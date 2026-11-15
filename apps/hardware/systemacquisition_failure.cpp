#include "systemacquisition_p.h"
#include <stdexcept>

namespace GetDAQ {
namespace {
HardwareStep Expect(const char *device,const char *reg,const char *field,std::uint64_t value) {
    HardwareStep s; s.kind = HardwareStep::ExpectValue; s.device = device; s.reg = reg; s.field = field;
    s.value = value; s.requireBoundDescription = true; return s;
}
}
void SystemAcquisitionClient::StopProductionAfterFailure(const std::string &path,bool parentUnknown) {
    auto &p = *fImpl; std::lock_guard<std::mutex> lock(p.operation);
    const auto from=p.state.load();
    if(p.brokenStopVerificationFailed)throw std::runtime_error("Broken production-stop verification failed; recovery required, no fallback or replay");
    if (from != State::Failed && !(parentUnknown && (from==State::Configured || from==State::Ready || from==State::Running || from==State::Paused || from==State::Stopped || from==State::Broken)))
        throw std::logic_error("StopProductionAfterFailure requires Failed or a confirmed UNKNOWN parent run");
    p.Begin(path,"stop-production-after-failure");
    p.state=State::Failed;
    try {
        p.Record("{\"phase\":\"negative_stop_context\",\"parent_ecc_unknown_confirmed\":"+std::string(parentUnknown ? "true" : "false")+",\"previous_run_state\":"+std::to_string(unsigned(from))+"}");
        if(from==State::Broken || p.brokenStopContext) {
            p.brokenStopContext=true;
            try {p.VerifyBrokenProductionStopped();}
            catch(...) {p.brokenStopVerificationFailed=true;throw;}
            p.Complete("production-stopped-readback-recovery-pending",State::Failed);return;
        }
        p.Hardware();
        const bool hardwareStopped=p.StopProducers();bool sourcesStopped=true;
        for(const auto &source:p.senderOptions)if(source.protocol=="ICE") {
            auto &client=p.links->Sender(source.senderId);const auto status=client.Snapshot();
            if(status.state==DataSenderSnapshot::Fresh || status.state==DataSenderSnapshot::Disconnected)continue;
            if(status.failureStopIssued) {sourcesStopped=sourcesStopped && status.failureStopAcknowledged;continue;}
            try {client.StopIceProductionAfterFailure(p.File(source.senderId,"ice-source-failure-stop"),true);}
            catch(...) {sourcesStopped=false;}
        }
        if(!hardwareStopped || !sourcesStopped)throw std::runtime_error("Producer/source stop unconfirmed; no repeated command, source End or receiver cleanup");
        p.Record("{\"phase\":\"production_stop_confirmed\",\"sender_reconciled\":false,\"ice_source_threads_stopped\":true,\"connections_closed\":false,\"recovery_required\":true}");
        p.Complete("production-stopped-recovery-pending",State::Failed);
    } catch (...) { p.Fail(); throw; }
}
void SystemAcquisitionClient::RetireFailedIceSources(const std::string &path,bool parentUnknown) {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);
    if(!parentUnknown || p.state!=State::Failed || p.sourceRetirementAttempted || p.failureBreakupAttempted)
        throw std::logic_error("Source retirement requires the same stopped failed acquisition and no earlier attempt");
    std::vector<std::string> pending;
    for(const auto &source:p.senderOptions)if(source.protocol=="ICE") {
        auto &sender=p.links->Sender(source.senderId);const auto status=sender.Snapshot();
        if(status.state==DataSenderSnapshot::Fresh || status.state==DataSenderSnapshot::Disconnected)continue;
        sender.ValidateIceFailureRetirement();pending.push_back(source.senderId);
    }
    if(pending.empty())throw std::logic_error("No original failed ICE source requires retirement");
    p.Begin(path,"retire-failed-ice-sources");p.sourceRetirementAttempted=true;p.sourceRetirementChains.clear();
    try {
        bool complete=true;
        for(const auto &id:pending){auto &sender=p.links->Sender(id);const auto file=p.File(id,"retire-failed-ice-source");
            try{sender.RetireIceSourceAfterFailure(file);p.sourceRetirementChains.emplace_back(file,sender.CompletedFailureRetirementAuditChainSha256());}
            catch(const std::exception &e){complete=false;p.Record("{\"phase\":\"source_retirement_unconfirmed\",\"id\":"+JsonString(id)+",\"error\":"+JsonString(e.what())+"}");}}
        if(!complete)throw std::runtime_error("Source retirement incomplete; independent targets attempted, no replay or End");
        p.Complete("failed-source-connections-retired-unknown-preserved",State::Failed);
        p.sourceRetirementChains.emplace_back(path+"/system-run.jsonl",p.audit->ChainSha256());p.sourceRetirementComplete=true;
    }catch(...){p.Fail();throw;}
}
std::string SystemAcquisitionClient::VerifyRetiredIceSources() {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);
    if(!p.sourceRetirementComplete || p.sourceRetirementChains.empty())throw std::logic_error("Missing complete original source retirement");
    std::string observations="[";
    for(const auto &source:p.senderOptions)if(source.protocol=="ICE") {
        auto &sender=p.links->Sender(source.senderId);const auto status=sender.Snapshot();
        if(status.state==DataSenderSnapshot::Fresh || status.state==DataSenderSnapshot::Disconnected)continue;
        if(observations.size()>1) observations+=',';
        observations+=sender.VerifyIceFailureRetirement();
    }
    return observations+']';
}
std::vector<std::pair<std::string,std::string>> SystemAcquisitionClient::SourceRetirementAuditChains() const {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);
    if(!p.sourceRetirementComplete || p.sourceRetirementChains.empty())
        throw std::logic_error("Missing source retirement audit chains");
    return p.sourceRetirementChains;
}
void SystemAcquisitionClient::BreakupAfterFailure(const std::string &path,bool parentUnknown)
{
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);
    if(!parentUnknown || p.state!=State::Failed || p.failureBreakupAttempted || p.brokenStopVerificationFailed)
        throw std::logic_error("Failure Breakup requires the same failed acquisition and no earlier cleanup attempt");
    const bool alreadyBroken=p.brokenStopContext;
    if(p.prepared.GetState()!=(alreadyBroken ? SystemPreparationClient::State::Prepared : SystemPreparationClient::State::Configured))
        throw std::logic_error("Failure Breakup lost the original preparation stage");
    const auto confirmed=[&](const std::string &id,const char *stage) {
        const auto found=p.productionStops.find(std::make_pair(id,std::string(stage)));
        if(found==p.productionStops.end() || !found->second)
            throw std::logic_error("Failure Breakup requires every original producer stop confirmed");
    };
    for(const auto &c:p.options.cobos)if(c.enabled)
        for(const char *stage:{"cobo-stop-pulser","cobo-stop-firmware","cobo-stop-interrupts"})confirmed(c.id,stage);
    for(const auto &m:p.options.mutants)confirmed(m.endpoint.id,"mutant-stop-production");
    p.Hardware();
    const std::vector<HardwareStep> stopped={Expect("ctrl","pipeCtrl","firmwareEnable",0),Expect("ctrl","pipeCtrl","intEnable",0)};
    for(std::size_t i=0;i<p.options.cobos.size();++i) {
        p.Name(*p.cobos[i],p.options.cobos[i].prepare.expectedNodeName);p.cobos[i]->Validate(stopped);
        if(p.cobos[i]->Read("ctrl","pipeCtrl","firmwareEnable") || p.cobos[i]->Read("ctrl","pipeCtrl","intEnable"))
            throw std::runtime_error("Failure Breakup found production enabled");
    }
    for(std::size_t i=0;i<p.options.mutants.size();++i) {
        p.Name(*p.mutants[i],p.options.mutants[i].endpoint.expectedNodeName);
        if(p.mutants[i]->Read("CARD_B_EMC","TRIGGER_CTRL","run"))throw std::runtime_error("Failure Breakup found MuTaNT running");
    }
    for(const auto &source:p.senderOptions) {
        auto &sender=p.links->Sender(source.senderId);const auto status=sender.Snapshot();
        const bool retired=source.protocol=="ICE" && status.outcomeUnknown && status.failureRetirementAcknowledged;
        if((status.outcomeUnknown && !retired) || (!retired && status.state!=DataSenderSnapshot::Fresh && status.state!=DataSenderSnapshot::Disconnected &&
           status.state!=DataSenderSnapshot::Connected && status.state!=DataSenderSnapshot::Running && status.state!=DataSenderSnapshot::Stopped))
            throw std::logic_error("Failure Breakup cannot adopt or repeat an unknown sender");
        if(alreadyBroken && !retired && status.state!=DataSenderSnapshot::Fresh && status.state!=DataSenderSnapshot::Disconnected)
            throw std::logic_error("Previously Broken acquisition has an unconfirmed disconnect");
        if(retired){if(!p.sourceRetirementComplete)throw std::logic_error("Failed source retirement was not committed");sender.VerifyIceFailureRetirement();}
        else if(status.state!=DataSenderSnapshot::Fresh && status.state!=DataSenderSnapshot::Disconnected)sender.Validate();
    }
    p.Begin(path,"breakup-after-failure");p.failureBreakupAttempted=true;p.failureBreakupChains.clear();p.captureFailureHardware=true;
    try {
        if(alreadyBroken) {
            p.VerifyBrokenProductionStopped();
            for(const auto &entry:p.prepared.ConfigurationInvalidationAuditChains())p.failureBreakupChains.push_back(entry);
        } else {
            bool complete=true;
            for(const auto &source:p.senderOptions) {
                auto &sender=p.links->Sender(source.senderId);auto status=sender.Snapshot();
                if(source.protocol=="ICE" && status.outcomeUnknown && status.failureRetirementAcknowledged){
                    sender.VerifyIceFailureRetirement();p.Record("{\"phase\":\"retired_failed_source_not_replayed\",\"id\":"+JsonString(source.senderId)+",\"source_end_requested\":false}");continue;
                }
                if(status.state==DataSenderSnapshot::Fresh || status.state==DataSenderSnapshot::Disconnected) {
                    p.Record("{\"phase\":\"sender_not_repeated\",\"id\":"+JsonString(source.senderId)+",\"state\":"+std::to_string(unsigned(status.state))+"}");continue;
                }
                if(status.state==DataSenderSnapshot::Running) {
                    const auto file=p.File(source.senderId,"failure-sender-stop");
                    try {sender.Stop(file);p.failureBreakupChains.emplace_back(file,sender.CompletedCommandAuditChainSha256());}
                    catch(const std::exception &e){complete=false;p.Record("{\"phase\":\"sender_stop_unconfirmed\",\"id\":"+JsonString(source.senderId)+",\"error\":"+JsonString(e.what())+"}");continue;}
                }
                const auto file=p.File(source.senderId,"failure-sender-disconnect");
                try {sender.Disconnect(file);p.failureBreakupChains.emplace_back(file,sender.CompletedCommandAuditChainSha256());}
                catch(const std::exception &e){complete=false;p.Record("{\"phase\":\"sender_disconnect_unconfirmed\",\"id\":"+JsonString(source.senderId)+",\"error\":"+JsonString(e.what())+"}");}
            }
            if(!complete)throw std::runtime_error("Failure Breakup sender cleanup unconfirmed; independent targets attempted, no replay");
            const auto lifecycleBefore=p.coboLifecycle->Snapshot();
            p.prepared.InvalidateConfiguration(path+"/configuration");
            p.RecordLifecycleMirror(lifecycleBefore);
            for(const auto &entry:p.prepared.ConfigurationInvalidationAuditChains())p.failureBreakupChains.push_back(entry);
        }
        if(p.sourceRetirementComplete)for(const auto &entry:p.sourceRetirementChains)p.failureBreakupChains.push_back(entry);
        p.LifecycleAll(CoBoLifecycleState::Prepared);
        p.Complete("failure-connections-closed-ecc-and-data-recovery-pending",State::Broken);
        p.failureBreakupChains.emplace_back(path+"/system-run.jsonl",p.audit->ChainSha256());p.failureBreakupComplete=true;p.captureFailureHardware=false;
    } catch(...) {p.captureFailureHardware=false;p.Fail();throw;}
}
std::vector<std::pair<std::string,std::string>> SystemAcquisitionClient::FailureBreakupAuditChains() const
{
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);
    if(p.state!=State::Broken || !p.failureBreakupComplete || p.failureBreakupChains.empty())
        throw std::logic_error("No confirmed retained failure Breakup proof");
    return p.failureBreakupChains;
}
bool SystemAcquisitionClient::IceSourceOutcomeUnknown() const {
    for(const auto &source:fImpl->senderOptions)if(source.protocol=="ICE" && fImpl->links->Sender(source.senderId).Snapshot().outcomeUnknown)return true;
    return false;
}
bool SystemAcquisitionClient::CheckIceSourceFailures() {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);
    for(const auto &source:p.senderOptions)if(source.protocol=="ICE") {
        auto &client=p.links->Sender(source.senderId);const auto state=client.Snapshot().state;
        if(state==DataSenderSnapshot::Fresh || state==DataSenderSnapshot::Disconnected)continue;
        try {client.Validate();}catch(...) {} // The original bound control client latches unknown; never issue a mutation here.
    }
    return IceSourceOutcomeUnknown();
}
}
