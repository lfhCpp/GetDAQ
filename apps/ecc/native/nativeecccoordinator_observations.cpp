// Read-only controller status and hardware/source observation projections.
#include "nativeecccoordinator_p.h"
#include "../../../common/text/utf8.h"
#include <algorithm>
#include <chrono>
#include <set>
#include <stdexcept>
#include <vector>
namespace GetDAQ {
EccStatus NativeEccCoordinator::Status() const {
    auto status=fImpl->machine->Status();
    if(fImpl->controlInputFault){status.outcomeUnknown=status.needsReconciliation=true;status.message="External control input fault held; original targets retained, explicit negative stop/reconciliation required";}
    if(fImpl->sourceFault) {status.outcomeUnknown=status.needsReconciliation=true;status.message="Retained ICE source outcome UNKNOWN; explicit production stop/recovery required";}
    if(fImpl->receiverFault) {status.outcomeUnknown=status.needsReconciliation=true;status.message="Bound receiver failed; production/data outcome requires reconciliation";}
    if (fImpl->data.NeedsReconciliation()) { status.needsReconciliation=true; if (status.message.empty()) status.message="Data lifecycle requires reconciliation"; }
    if (fImpl->storageFault) { status.needsReconciliation=true; status.message="Native ECC immutable binding/storage fault is latched"; }
    if (fImpl->ownership && fImpl->ownership->Faulted()) {
        status.outcomeUnknown=status.needsReconciliation=true;
        status.message="Host control reservation changed; hardware outcome requires reconciliation";
    }
    return status;
}
EccStatus NativeEccCoordinator::ControlStatus() const {
    auto status=fImpl->machine->Status();
    if(fImpl->controlInputFault){status.outcomeUnknown=status.needsReconciliation=true;status.message="External control input fault held; original targets retained, explicit negative stop/reconciliation required";}
    if(fImpl->sourceFault) {status.outcomeUnknown=status.needsReconciliation=true;status.message="Retained ICE source outcome UNKNOWN; explicit production stop/recovery required";}
    if(fImpl->receiverFault) {status.outcomeUnknown=status.needsReconciliation=true;status.message="Bound receiver failed; production/data outcome requires reconciliation";}
    if(fImpl->storageFault) {status.needsReconciliation=true;status.message="Native ECC immutable binding/storage fault is latched";}
    if(fImpl->ownership && fImpl->ownership->Faulted()) {
        status.outcomeUnknown=status.needsReconciliation=true;
        status.message="Host control reservation changed; hardware outcome requires reconciliation";
    }
    return status;
}
EccOperation NativeEccCoordinator::Operation(const std::string &id) const { return fImpl->journal->Operation(id); }
std::vector<EccOperation> NativeEccCoordinator::Operations() const { return fImpl->journal->Operations(); }
std::string NativeEccCoordinator::SnapshotSha256() const { return fImpl->snapshotHash; }
std::string NativeEccCoordinator::ConfigurationSha256() const { return fImpl->bound.configurationSha256; }
std::string NativeEccCoordinator::ArtifactSha256() const { return fImpl->bound.artifactSha256; }
std::string NativeEccCoordinator::OwnershipJson() const {
    const auto &p=*fImpl; std::string resources="[";
    if (p.ownership) for (const auto &resource:p.ownership->Resources()) {
        if (resources.size()>1) resources+=',';
        resources+=JsonString(resource);
    }
    return "{\"enabled\":"+std::string(p.ownership ? "true" : "false")+
        ",\"scope\":\"single-host-cooperating-ecc\",\"directory\":"+JsonString(p.options.hostControlOwnershipRoot)+
        ",\"held_at_last_check\":"+(p.ownership && !p.ownership->Faulted() ? "true" : "false")+
        ",\"fault_latched\":"+(p.ownership && p.ownership->Faulted() ? "true" : "false")+
        ",\"cached\":true,\"board_global_lock_verified\":false,\"cross_host_lock_verified\":false,\"resources\":"+resources+"]}";
}
std::string NativeEccCoordinator::ConfigureReadbacksJson() const {
    const auto &p=*fImpl;std::lock_guard<std::mutex> guard(p.readbackMutex);
    return "{\"phase\":"+JsonString(p.readbackPhase)+",\"configure_operation_id\":"+JsonString(p.readbackOperation)+
        ",\"historical\":true,\"live_hardware_checked\":false,\"actual_loaded_configuration_verified\":false,\"entries\":"+p.readbackEntries+"}";
}
bool NativeEccCoordinator::ObserveAlarms(NativeEccAlarms &observation) const {
    auto &p=*fImpl; std::unique_lock<std::mutex> lock(p.command,std::try_to_lock);
    if (!lock.owns_lock()) return false;
    NativeEccAlarms next; next.controller=Status();
    if (p.preparation) for (const auto &c:p.bound.cobos) if (c.enabled) {
        auto *client=p.preparation->Alarms(c.id);
        if (!client) continue;
        NativeEccCoBoAlarm node; node.id=c.id; node.snapshot=client->Snapshot();
        next.stopRequested=next.stopRequested || node.snapshot.stopRequested;
        next.nodes.push_back(node);
    }
    observation=std::move(next); return true;
}
std::string NativeEccCoordinator::ObserveSourcesJson(unsigned int budget) const {
    if(budget<100 || budget>5000)throw std::invalid_argument("Source observation budget must be 100..5000ms");
    auto &p=*fImpl;std::vector<std::shared_ptr<DataSenderClient>> clients;EccStatus controller;bool busy=false;
    {std::unique_lock<std::mutex> lock(p.command,std::try_to_lock);busy=!lock.owns_lock();controller=Status();if(!busy && p.acquisition)clients=p.acquisition->ObservationClients();}
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(budget);std::string rows="[";
    for(const auto &client:clients) {
        const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
        DataSourceObservation source;
        if(remaining<50)source=client->ObserveSource(0);
        else source=client->ObserveSource(unsigned(std::min<std::int64_t>(remaining,500)));
        if(rows.back()!='[')rows+=',';
        rows+="{\"id\":"+JsonString(source.senderId)+",\"protocol\":"+JsonString(source.protocol)+",\"phase\":"+JsonString(source.phase)+",\"message\":"+JsonString(source.message)+
            ",\"local_state\":"+std::to_string(source.localState)+",\"source_instance_id\":"+JsonString(source.sourceInstanceId)+",\"receiver_session\":"+JsonString(source.receiverSession)+
            ",\"observed_at_unix_ms\":"+JsonString(std::to_string(source.observedAtUnixMs))+",\"state\":"+JsonString(source.state)+",\"error\":"+JsonString(source.error)+
            ",\"issued_bytes\":"+JsonString(std::to_string(source.issuedBytes))+",\"locally_sent_bytes\":"+JsonString(std::to_string(source.locallySentBytes))+
            ",\"start_issued\":"+(source.startIssued ? "true" : "false")+",\"stop_acknowledged\":"+(source.stopAcknowledged ? "true" : "false")+
            ",\"drain_acknowledged\":"+(source.drainAcknowledged ? "true" : "false")+",\"finish_issued\":"+(source.finishIssued ? "true" : "false")+
            ",\"source_end_acknowledged\":"+(source.sourceEndAcknowledged ? "true" : "false")+",\"failure_stop_issued\":"+(source.failureStopIssued ? "true" : "false")+
            ",\"failure_stop_acknowledged\":"+(source.failureStopAcknowledged ? "true" : "false")+"}";
    }
    return "{\"schema\":1,\"kind\":\"getdaq.ecc-sources\",\"snapshot_sha256\":"+JsonString(p.snapshotHash)+",\"configuration_sha256\":"+JsonString(p.bound.configurationSha256)+
        ",\"artifact_sha256\":"+JsonString(p.bound.artifactSha256)+",\"controller_operation_id\":"+JsonString(controller.operationId)+",\"busy\":"+(busy ? "true" : "false")+",\"sources\":"+rows+"]}";
}
std::string NativeEccCoordinator::ObserveHardwareStateJson(unsigned int budget) const {
    if(budget<100 || budget>5000)throw std::invalid_argument("Hardware observation budget must be 100..5000ms");
    auto &p=*fImpl;std::unique_lock<std::mutex> lock(p.command,std::try_to_lock);const bool busy=!lock.owns_lock();EccStatus controller=Status();const auto initial=controller;const auto epoch=p.observationEpoch.load();if(lock.owns_lock())lock.unlock();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(budget);std::string rows="[";
    for(const auto &node:p.options.description.nodes) {
        std::string values="{}",message;std::uint64_t observed=0;bool available=false;
        if(busy)message="ECC command in progress; no hardware observation issued";
        else try {
            const auto checkpoint=[&]{if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Whole hardware observation budget exceeded");std::unique_lock<std::mutex> guard(p.command,std::try_to_lock);if(!guard.owns_lock() || p.observationEpoch.load()!=epoch)throw std::runtime_error("ECC command became active during observation");const auto now=Status();if(now.operationId!=initial.operationId || now.state!=initial.state || now.transition || now.outcomeUnknown!=initial.outcomeUnknown || now.needsReconciliation!=initial.needsReconciliation)throw std::runtime_error("ECC state changed during observation");};
            checkpoint();p.CheckBinding();const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
            if(remaining<100)throw std::runtime_error("Whole hardware observation budget exceeded; no next node RPC issued");
            HardwareClient hardware(node.nodeProxy,p.bound.encodingMinor,int(std::min<std::int64_t>(remaining,300)));
            checkpoint();if(hardware.Name()!=node.description.nodeId)throw std::runtime_error("Observed hardware node name differs");hardware.UseDescription(node.description);
            checkpoint();const auto devices=hardware.Devices();if(devices.size()!=node.description.devices.size())throw std::runtime_error("Observed device inventory differs");
            std::set<std::string> inventory(devices.begin(),devices.end());
            for(const auto &device:node.description.devices){checkpoint();if(!inventory.count(device.name))throw std::runtime_error("Observed device missing");hardware.Describe(device.name);}
            std::vector<std::pair<std::string,std::uint64_t>> bits;
            const auto read=[&](const char *device,const char *reg,const char *field){checkpoint();const auto value=hardware.Read(device,reg,field);if(value>1)throw std::runtime_error("Observed state bit is outside boolean range");bits.emplace_back(field,value);};
            if(node.role==SystemDescriptionNode::Role::CoBo){read("ctrl","pipeCtrl","firmwareEnable");read("ctrl","pipeCtrl","intEnable");read("ctrl","mutantConfig","initDone");read("ctrl","mutantConfig","mutantMode");}
            else read("CARD_B_EMC","TRIGGER_CTRL","run");
            checkpoint();p.CheckBinding();values="{";for(const auto &bit:bits){if(values.back()!='{')values+=',';values+=JsonString(bit.first)+':'+JsonString(std::to_string(bit.second));}values+='}';
            available=true;observed=std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        }catch(const std::exception &e){message=EccErrorText(e.what());if(message.size()>1024)message="Hardware observation failed; bounded diagnostic exceeded";}catch(...){message="Nonstandard hardware observation failure";}
        if(rows.back()!='[')rows+=',';
        rows+="{\"id\":"+JsonString(node.id)+",\"role\":"+JsonString(node.role==SystemDescriptionNode::Role::CoBo ? "cobo" : "mutant")+",\"available\":"+(available ? "true" : "false")+",\"message\":"+JsonString(message)+",\"observed_at_unix_ms\":"+JsonString(std::to_string(observed))+",\"values\":"+values+"}";
    }
    controller=Status();bool consistent=!busy && p.observationEpoch.load()==epoch && controller.operationId==initial.operationId && controller.state==initial.state && controller.transition==initial.transition && controller.error==initial.error && controller.outcomeUnknown==initial.outcomeUnknown && controller.needsReconciliation==initial.needsReconciliation;
    std::string coboLifecycle="[]";
    if(consistent) {
        std::unique_lock<std::mutex> guard(p.command,std::try_to_lock);
        if(!guard.owns_lock() || p.observationEpoch.load()!=epoch) consistent=false;
        else if(p.acquisition)coboLifecycle=p.acquisition->CoBoLifecycleJson();
        else if(p.preparation)coboLifecycle=p.preparation->CoBoLifecycleJson();
        else {
            coboLifecycle="[";
            const bool baselineKnown=!controller.outcomeUnknown && !controller.needsReconciliation;
            for(std::size_t i=0;i<p.options.endpoints.cobos.size();++i) {
                if(i)coboLifecycle+=',';
                const auto &board=p.options.endpoints.cobos[i];
                const auto state=!board.enabled ? "disabled" : baselineKnown ? "fresh" : "unknown";
                coboLifecycle+="{\"id\":"+JsonString(board.id)+",\"state\":"+JsonString(state)+"}";
            }
            coboLifecycle+=']';
        }
    }
    const auto coboLifecycleSource=busy || !consistent ? "unavailable" : (p.acquisition || p.preparation) ? "live-controller-cache" : "controller-baseline";
    if(!consistent && !busy){rows="[";for(const auto &node:p.options.description.nodes){if(rows.back()!='[')rows+=',';rows+="{\"id\":"+JsonString(node.id)+",\"role\":"+JsonString(node.role==SystemDescriptionNode::Role::CoBo ? "cobo" : "mutant")+",\"available\":false,\"message\":\"Controller changed during observation; all samples withdrawn\",\"observed_at_unix_ms\":\"0\",\"values\":{}}";}}
    if(!consistent)coboLifecycle="[]";
    return "{\"schema\":1,\"kind\":\"getdaq.hardware-state-observation\",\"snapshot_sha256\":"+JsonString(p.snapshotHash)+",\"configuration_sha256\":"+JsonString(p.bound.configurationSha256)+",\"artifact_sha256\":"+JsonString(p.bound.artifactSha256)+",\"controller_operation_id\":"+JsonString(controller.operationId)+",\"controller_state\":"+std::to_string(int(controller.state))+",\"controller_outcome_unknown\":"+(controller.outcomeUnknown ? "true" : "false")+",\"controller_needs_reconciliation\":"+(controller.needsReconciliation ? "true" : "false")+",\"busy\":"+(busy ? "true" : "false")+",\"consistent\":"+(consistent ? "true" : "false")+",\"recovery_verified\":false,\"actual_loaded_configuration_verified\":false,\"global_control_verified\":false,\"cobo_lifecycle_source\":"+JsonString(coboLifecycleSource)+",\"cobo_lifecycle_is_hardware_readback\":false,\"cobo_lifecycle\":"+coboLifecycle+",\"nodes\":"+rows+"]}";
}

}
