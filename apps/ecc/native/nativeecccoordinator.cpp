#include "nativeecccoordinator.h"
#include "nativeecccoordinator_p.h"
#include "nativeeccplancompiler_p.h"
#include "nativeeccpreflight.h"
#include "nativeeccfilesystemproof_p.h"
#include "hostcontrolownership.h"
#include "../../../common/text/utf8.h"
#include <set>
#include <Ice/Ice.h>
#include "systemacquisition.h"
#include <chrono>
#include "hardwareaudit.h"
#include "configurationsha256.h"
#include "strictjson.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <map>
#include <mutex>
#include <stdexcept>
#include <sys/stat.h>
namespace GetDAQ {
using namespace NativeEccFilesystemProof;
namespace {
void OperationId(const std::string &id) {
    if (id.empty() || id.size()>128 || id.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos)
        throw std::invalid_argument("Native ECC operation ID requires 1..128 ASCII letters/digits/underscore/hyphen");
}

}

NativeEccCoordinator::Impl::Impl(const NativeEccOptions &o,EccDataLifecycle &d) : options(o),data(d) {
    auto planned=CompileNativeEccPlan(o,data.BindingSha256());
    bound=std::move(planned.bound);senders=std::move(planned.senders);dataBinding=std::move(planned.dataBinding);
    snapshot=std::move(planned.plan.snapshotJson);snapshotHash=std::move(planned.plan.snapshotSha256);
    if (!o.hostControlOwnershipRoot.empty()) {
        Ice::InitializationData init; init.properties=Ice::createProperties();
        init.properties->setProperty("Ice.RetryIntervals","-1");
        const auto communicator=Ice::initialize(init);
        std::vector<std::string> addresses;
        try {
            const auto add=[&](const std::string &text) {
                const auto proxy=communicator->stringToProxy(text);
                if (!proxy || proxy->ice_getRouter() || proxy->ice_getLocator() || !proxy->ice_getAdapterId().empty())
                    throw std::invalid_argument("Host ownership requires direct board TCP proxies");
                const auto endpoints=proxy->ice_getEndpoints();
                if (endpoints.empty() || endpoints.size()>16) throw std::invalid_argument("Invalid ownership endpoint count");
                for (const auto &endpoint:endpoints) {
                    const auto info=Ice::TCPEndpointInfoPtr::dynamicCast(endpoint->getInfo());
                    if (!info || info->port<1 || info->port>65535) throw std::invalid_argument("Host ownership requires explicit TCP endpoints");
                    addresses.push_back(info->host);
                }
            };
            for (const auto &node:o.description.nodes) add(node.nodeProxy);
            for (const auto &sender:senders) add(sender.controlProxy);
            communicator->destroy();
        } catch (...) { try { communicator->destroy(); } catch (...) {} throw; }
        ownership.reset(new HostControlOwnership(o.hostControlOwnershipRoot,addresses,o.deploymentId,snapshotHash));
    }
    struct stat status;
    const bool exists=::lstat(o.auditRoot.c_str(),&status)==0;
    if (!exists && errno!=ENOENT) throw std::runtime_error("Cannot inspect native ECC root");
    if (!exists) CreateHardwareAuditDirectory(o.auditRoot);
    root.reset(new Directory(o.auditRoot));
    if (!exists) {
        CreateHardwareAuditDirectory(o.auditRoot+"/operations");
        WriteNewFile(o.auditRoot+"/experiment.json",o.experimentArtifactBytes);
        WriteNewFile(o.auditRoot+"/snapshot.json",snapshot);
    } else if (::lstat((o.auditRoot+"/ecc.jsonl").c_str(),&status)!=0)
        throw std::runtime_error("Existing native ECC root is incomplete; do not recreate its journal");
    operations.reset(new Directory(o.auditRoot+"/operations"));
    artifactFile.reset(new ImmutableFile(o.auditRoot+"/experiment.json",o.experimentArtifactBytes));
    snapshotFile.reset(new ImmutableFile(o.auditRoot+"/snapshot.json",snapshot));
    journal.reset(new EccJournal(o.auditRoot+"/ecc.jsonl","getdaq.native-ecc:1:"+snapshotHash));
    machine.reset(new EccStateMachine(*this,*journal));
    data.BindController(snapshotHash,o.auditRoot);
}

void NativeEccCoordinator::Impl::CheckBinding() {
    if (storageFault) throw std::runtime_error("Native ECC storage fault is latched; reconciliation required");
    try { if (ownership) ownership->Check(); root->Check(); operations->Check(); artifactFile->Check(); snapshotFile->Check(); }
    catch (...) { storageFault=true; throw; }
}

void NativeEccCoordinator::Impl::Match(EccCommand c,EccState from,EccState to) {
    if (context.id.empty() || context.command!=c || context.from!=from || context.to!=to || context.path!=options.auditRoot+"/operations/"+context.id)
        throw std::logic_error("Native ECC command context does not match its journal intent");
    CheckBinding();
}

void NativeEccCoordinator::Impl::CheckUnusedPath(const std::string &id) const {
    struct stat st; const auto path=options.auditRoot+"/operations/"+id;
    if (::lstat(path.c_str(),&st)==0) throw std::runtime_error("Native ECC operation ID already has an audit directory");
    if (errno!=ENOENT) throw std::runtime_error("Cannot inspect native ECC operation ID directory");
}

NativeEccCoordinator::NativeEccCoordinator(const NativeEccOptions &o,EccDataLifecycle &d) : fImpl(new Impl(o,d)) {}
NativeEccCoordinator::~NativeEccCoordinator() {}
void NativeEccCoordinator::Execute(EccCommand c,const std::string &id) {ExecuteChecked(c,id,false);}
void NativeEccCoordinator::ReprepareCoBo(const std::string &id,const std::string &node) {ExecuteChecked(EccCommand::ReprepareCoBo,id,false,node);}
void NativeEccCoordinator::ConfigureCoBo(const std::string &id,const std::string &node) {ExecuteChecked(EccCommand::ConfigureCoBo,id,false,node);}
void NativeEccCoordinator::SetCoBoLed(const std::string &id,const std::string &node,unsigned int asadId,unsigned int type,unsigned int state) {
    if(asadId>3 || type>unsigned(HardwareStep::LedD) || state>unsigned(HardwareStep::LedFastPulse))
        throw std::invalid_argument("Invalid selected CoBo LED operation");
    ExecuteChecked(EccCommand::SetCoBoLed,id,false,node,asadId,type,state);
}
void NativeEccCoordinator::BlinkCoBoLeds(const std::string &id,const std::string &node) {
    ExecuteChecked(EccCommand::BlinkCoBoLeds,id,false,node);
}
void NativeEccCoordinator::FlipCoBoLed(const std::string &id,const std::string &node,unsigned int ledId) {
    if(ledId>16)throw std::invalid_argument("LED flip selection must be 0..15 or 16 for all");
    ExecuteChecked(EccCommand::FlipCoBoLed,id,false,node,0,0,0,0,ledId);
}
void NativeEccCoordinator::PulseCoBoLed(const std::string &id,const std::string &node,unsigned int ledId,unsigned int periodMs) {
    if(ledId>16 || periodMs>unsigned(std::numeric_limits<int>::max()))throw std::invalid_argument("LED pulse requires LED 0..15 or 16 for all and period_ms in int range");
    ExecuteChecked(EccCommand::PulseCoBoLed,id,false,node,0,0,0,0,0,ledId,periodMs);
}
void NativeEccCoordinator::TriggerAsAdPulser(const std::string &id,const std::string &node,int voltageMv) {
    if(voltageMv < -1000 || voltageMv > 1000) throw std::invalid_argument("Single pulser voltage must be within -1000..1000 mV");
    ExecuteChecked(EccCommand::TriggerAsAdPulser,id,false,node,0,0,0,voltageMv);
}
std::vector<std::pair<std::string,std::string>> NativeEccCoordinator::MaintenanceTargets() const {
    std::vector<std::pair<std::string,std::string>> targets;
    for(const auto &node:fImpl->bound.cobos)if(node.enabled)targets.emplace_back(node.id,node.prepare.expectedNodeName);
    return targets;
}
void NativeEccCoordinator::ExecuteChecked(EccCommand c,const std::string &id,bool verifyAlarmRelease,const std::string &node,
                                          unsigned int asadId,unsigned int ledType,unsigned int ledState,int pulserVoltageMv,unsigned int ledFlipId,unsigned int ledPulseId,unsigned int ledPulsePeriodMs) {
    auto &p=*fImpl; std::unique_lock<std::mutex> lock(p.command,std::try_to_lock);
    if (!lock.owns_lock()) throw std::runtime_error("Native ECC command already in progress; request was not queued");
    ++p.observationEpoch;
    OperationId(id); p.CheckBinding(); const auto status=Status();
    if (status.outcomeUnknown) throw std::runtime_error("ECC outcome UNKNOWN; reconcile hardware and operation logs before further commands");
    if (status.needsReconciliation) throw std::runtime_error("ECC startup/storage requires hardware reconciliation before commands");
    if(c==EccCommand::ReprepareCoBo || c==EccCommand::ConfigureCoBo || c==EccCommand::SetCoBoLed || c==EccCommand::TriggerAsAdPulser || c==EccCommand::BlinkCoBoLeds || c==EccCommand::FlipCoBoLed || c==EccCommand::PulseCoBoLed) {
        const auto selected=std::find_if(p.bound.cobos.begin(),p.bound.cobos.end(),[&](const SystemCoBoSettings &entry) {return entry.id==node && entry.enabled;});
        if(node.empty() || selected==p.bound.cobos.end())throw std::invalid_argument("Native CoBo maintenance requires an explicit active node from the fixed configuration");
    } else if(!node.empty()) throw std::invalid_argument("Selected maintenance node requires an explicit maintenance command");
    p.CheckUnusedPath(id);
    p.context.maintenanceNodeId=node;
    p.context.maintenanceAsadId=asadId;p.context.maintenanceLedType=ledType;p.context.maintenanceLedState=ledState;
    p.context.pulserVoltageMv=pulserVoltageMv;p.context.maintenanceLedFlipId=ledFlipId;
    p.context.maintenanceLedPulseId=ledPulseId;p.context.maintenanceLedPulsePeriodMs=ledPulsePeriodMs;
    p.context.verifyAlarmRelease=verifyAlarmRelease;
    p.context.command=c; p.context.from=status.state; p.context.to=EccStateMachine::Target(status.state,c);
    p.context.id=id; p.context.path=p.options.auditRoot+"/operations/"+id;
    p.context.key=GetDAQ::ConfigurationSha256("getdaq.native-ecc-operation:1\n"+p.snapshotHash+'\n'+id+'\n'+std::to_string(int(c))+'\n'+p.context.path);
    if(c==EccCommand::ReprepareCoBo)p.context.key=GetDAQ::ConfigurationSha256("getdaq.native-ecc-maintenance:1\n"+p.context.key+'\n'+node);
    if(c==EccCommand::ConfigureCoBo)p.context.key=GetDAQ::ConfigurationSha256("getdaq.native-ecc-configure-cobo:1\n"+p.context.key+'\n'+node);
    if(c==EccCommand::SetCoBoLed)p.context.key=GetDAQ::ConfigurationSha256("getdaq.native-ecc-led:1\n"+p.context.key+'\n'+node+'\n'+std::to_string(asadId)+'\n'+std::to_string(ledType)+'\n'+std::to_string(ledState));
    if(c==EccCommand::BlinkCoBoLeds)p.context.key=GetDAQ::ConfigurationSha256("getdaq.native-ecc-led-blink:1\n"+p.context.key+'\n'+node+"\nsequential-once");
    if(c==EccCommand::FlipCoBoLed)p.context.key=GetDAQ::ConfigurationSha256("getdaq.native-ecc-led-flip:1\n"+p.context.key+'\n'+node+'\n'+std::to_string(ledFlipId));
    if(c==EccCommand::PulseCoBoLed)p.context.key=GetDAQ::ConfigurationSha256("getdaq.native-ecc-led-pulse:1\n"+p.context.key+'\n'+node+'\n'+std::to_string(ledPulseId)+'\n'+std::to_string(ledPulsePeriodMs));
    if(c==EccCommand::TriggerAsAdPulser)p.context.key=GetDAQ::ConfigurationSha256("getdaq.native-ecc-pulser:1\n"+p.context.key+'\n'+node+'\n'+std::to_string(pulserVoltageMv));
    if (c==EccCommand::Configure) {
        std::lock_guard<std::mutex> guard(p.readbackMutex);p.readbackOperation=id;p.readbackPhase="pending";p.readbackEntries="[]";
    }
    try {p.machine->Execute(c,id,p.context.key);}
    catch(...) {
        if (c==EccCommand::Configure) {std::lock_guard<std::mutex> guard(p.readbackMutex);if(p.readbackPhase=="pending")p.readbackPhase="unavailable";}
        if(p.acquisition && p.acquisition->IceSourceOutcomeUnknown())p.sourceFault=true;
        throw;
    }
}
void NativeEccCoordinator::ReleaseStoppedAlarm(const std::string &id,const std::string &stoppedId) {
    auto &p=*fImpl;OperationId(id);OperationId(stoppedId);
    {
        std::unique_lock<std::mutex> lock(p.command,std::try_to_lock);
        if(!lock.owns_lock())throw std::runtime_error("Stopped alarm release busy; not queued");
        const auto status=Status();p.CheckBinding();
        if(status.state!=EccState::Ready || status.transition || status.outcomeUnknown || status.needsReconciliation || status.operationId!=stoppedId ||
           !p.acquisition || !p.preparation || p.knownOperationId!=stoppedId || !p.knownOperationAudit || !p.knownOperationDirectory ||
           (p.context.command!=EccCommand::Stop && p.context.command!=EccCommand::Configure))
            throw std::logic_error("Alarm release requires the same known stopped Ready object; no UNKNOWN or cold takeover");
        try{p.knownOperationAudit->Check();p.knownOperationDirectory->Check();}catch(...){p.storageFault=true;throw;}
        p.CheckUnusedPath(id);
        p.releaseOriginalAudit=p.knownOperationAudit;p.releaseOriginalDirectory=p.knownOperationDirectory;
    }
    const auto breakup="alarm-breakup-"+GetDAQ::ConfigurationSha256("getdaq.stopped-alarm-breakup:1\n"+p.snapshotHash+'\n'+id);
    Execute(EccCommand::Breakup,breakup);
    ExecuteChecked(EccCommand::Reset,id,true);
}
void NativeEccCoordinator::CheckHandoffCompletionBoundary() const {
    auto &p=*fImpl;std::unique_lock<std::mutex> guard(p.command,std::try_to_lock);
    if(!guard.owns_lock())throw std::runtime_error("Original control boundary busy; not queued");
    p.CheckBinding();const auto state=p.machine->Status();
    if((state.state!=EccState::Idle && state.state!=EccState::Described) || state.transition || state.error || state.outcomeUnknown || state.needsReconciliation ||
       p.controlInputFault || p.sourceFault || p.receiverFault || p.storageFault || (p.ownership && p.ownership->Faulted()))
        throw std::runtime_error("Original live control boundary is not known released; independent holds remain");
}

}
