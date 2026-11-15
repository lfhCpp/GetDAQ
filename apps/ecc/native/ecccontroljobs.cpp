#include "ecccontroljobs_p.h"
#include <stdexcept>
namespace GetDAQ {
EccControlJobs::EccControlJobs(FixedEccConfiguration &configuration) : EccControlJobs(configuration,nullptr,"") {}
EccControlJobs::EccControlJobs(FixedEccConfiguration &configuration,std::shared_ptr<const EccSuccessorPlan> successor,const std::string &receipt) : fImpl(new Impl(configuration,successor,receipt)) {
    if (configuration.AlarmStopEnabled()) fImpl->alarmWorker=std::thread([this] { fImpl->MonitorAlarms(); });
    if (configuration.ReceiverStopEnabled()) fImpl->receiverWorker=std::thread([this] { fImpl->MonitorReceivers(); });
}
EccControlJobs::~EccControlJobs() {}
EccControlJobs::Impl::~Impl() {
    alarmShutdown=true; alarmWait.notify_all(); if (alarmWorker.joinable()) alarmWorker.join();
    if (receiverWorker.joinable()) receiverWorker.join();
    if (worker.joinable()) worker.join();
}
EccControlOperation EccControlJobs::Operation(const std::string &id) const { SupervisorOperationId(id); std::lock_guard<std::mutex> lock(fImpl->stateMutex); const auto found=fImpl->jobs.find(id); if (found==fImpl->jobs.end()) throw std::out_of_range("ECC operation not found"); return found->second; }
EccControlOperation EccControlJobs::Submit(const EccControlRequest &r) {
    auto &p=*fImpl; Validate(r);
    if (r.operationId.compare(0,15,"internal-alarm-")==0) throw std::invalid_argument("Reserved ECC alarm operation ID prefix");
    if (r.operationId.compare(0,18,"internal-receiver-")==0) throw std::invalid_argument("Reserved ECC receiver operation ID prefix");
    if (r.instanceId!=p.instance || r.snapshotSha256!=p.snapshot || r.clientId!=p.configuration.ClientId()) throw std::invalid_argument("ECC instance/snapshot/client precondition differs");
    const auto key=EccControlRequestKey(r);
    { std::lock_guard<std::mutex> lock(p.stateMutex); const auto found=p.jobs.find(r.operationId); if (found!=p.jobs.end()) { if (found->second.requestKey!=key) throw std::invalid_argument("ECC operation ID has a different request key"); return found->second; } if (p.active) throw std::runtime_error("ECC command busy; not queued"); }
    std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock); if (!operation.owns_lock()) throw std::runtime_error("ECC acceptance busy; not queued");
    return p.AcceptLocked(r,false);
}
void EccControlJobs::Impl::MonitorAlarms() {
    while (!alarmShutdown) {
        try {
            NativeEccAlarms observation;
            if (controller.ObserveAlarms(observation)) {
                auto evidence=AlarmObservation(observation);
                { std::lock_guard<std::mutex> lock(stateMutex); if (!alarmHandled) alarmObservation=evidence; }
                if (observation.stopRequested) {
                    std::unique_lock<std::mutex> operation(operationMutex,std::try_to_lock);
                    if (operation.owns_lock() && !alarmShutdown) {
                        // 持有命令入口锁后重新观察，不能用并发Stop/Reset前的旧状态选动作。
                        if (!controller.ObserveAlarms(observation) || !observation.stopRequested) continue;
                        evidence=AlarmObservation(observation);
                        bool handle=false;
                        { std::lock_guard<std::mutex> lock(stateMutex); if (!active && !alarmHandled && !handoffFenced) { alarmHandled=true; needsReconciliation=true; handle=true; alarmObservation=evidence; alarmPhase="held"; message="Hardware alarm held; stop pending, recovery required"; } }
                        if (handle) {
                            Check();
                            EccControlRequest request;
                            if (observation.controller.outcomeUnknown) request.command="stop-production-after-failure";
                            else if (observation.controller.state==EccState::Running || observation.controller.state==EccState::Paused) request.command="stop";
                            if (!request.command.empty()) {
                                request.operationId="internal-alarm-"+RandomSession(); request.instanceId=instance; request.snapshotSha256=snapshot; request.clientId=configuration.ClientId();
                                std::lock_guard<std::mutex> lock(stateMutex); request.clientEpoch=ownerEpoch; alarmOperationId=request.operationId;
                            }
                            const auto key=ConfigurationSha256("getdaq.ecc-alarm-observation:1\n"+evidence);
                            Record("alarm-held",",\"operation_id\":"+EncodeJsonString(request.operationId)+",\"evidence_sha256\":"+EncodeJsonString(key)+",\"evidence\":"+EncodeJsonString(evidence));
                            if (!request.command.empty()) { AcceptLocked(request,true); std::lock_guard<std::mutex> lock(stateMutex); if (alarmPhase=="held") alarmPhase="accepted"; }
                            else { std::lock_guard<std::mutex> lock(stateMutex); alarmPhase="held-no-run"; message="Hardware alarm held outside active production; inspect before commands"; }
                        }
                    }
                }
            }
        } catch (...) {
            std::lock_guard<std::mutex> lock(stateMutex); needsReconciliation=true; alarmHandled=true; alarmPhase="unknown"; message="Alarm observation/intent failed; gate retained, no automatic replay";
        }
        std::unique_lock<std::mutex> wait(alarmWaitMutex);
        alarmWait.wait_for(wait,std::chrono::milliseconds(configuration.AlarmPollIntervalMs()),[this] { return alarmShutdown.load(); });
    }
}
void EccControlJobs::Impl::MonitorReceivers() {
    while (!alarmShutdown) {
        bool held=false;
        try {
            bool handled;
            { std::lock_guard<std::mutex> lock(stateMutex); handled=receiverHandled || handoffFenced; }
            const auto before=controller.Status();
            if (!handled && !before.transition && (before.state==EccState::Ready || before.state==EccState::Running || before.state==EccState::Paused)) {
                // Independent read-only clients; never keep the command-entry lock over HTTP.
                const auto observation="{\"schema\":1,\"kind\":\"getdaq.ecc-receivers\",\"instance_id\":"+EncodeJsonString(instance)+
                    ",\"snapshot_sha256\":"+EncodeJsonString(snapshot)+",\"data_binding_sha256\":"+EncodeJsonString(data.BindingSha256())+
                    ",\"nodes\":"+data.ObserveReceiversJson()+"}\n";
                const auto observed=DecodeEccControlReceiverObservation(observation);
                std::unique_lock<std::mutex> operation(operationMutex,std::try_to_lock);
                if (operation.owns_lock() && !alarmShutdown) {
                    const auto boundBytes=StatusJsonForPolicy();
                    const auto bound=DecodeEccControlSystemStatus(boundBytes);
                    if (bound.controller.operationId==before.operationId && bound.controller.state==unsigned(before.state) &&
                        !FailedBoundReceivers(bound,observed,data.BindingSha256()).empty()) {
                        EccControlRequest request; request.command="stop-production-after-failure";
                        request.operationId="internal-receiver-"+RandomSession(); request.instanceId=instance;
                        request.snapshotSha256=snapshot; request.clientId=configuration.ClientId();
                        {
                            std::lock_guard<std::mutex> lock(stateMutex);
                            if (receiverHandled || active) continue;
                            receiverHandled=true; needsReconciliation=true; held=true;
                            receiverPhase="held"; receiverOperationId=request.operationId; request.clientEpoch=ownerEpoch;
                            receiverEvidence="{\"bound_status\":"+EncodeJsonString(boundBytes,1024*1024)+",\"observation\":"+EncodeJsonString(observation,1024*1024)+"}";
                            message="Bound receiver failed; production stop pending, recovery required";
                        }
                        Check();
                        Record("receiver-held",",\"operation_id\":"+EncodeJsonString(request.operationId)+
                            ",\"evidence_sha256\":"+EncodeJsonString(ConfigurationSha256("getdaq.ecc-receiver-failure:1\n"+receiverEvidence))+
                            ",\"evidence\":"+EncodeJsonString(receiverEvidence,1024*1024));
                        controller.HoldReceiverFailure(bound.controller.operationId);
                        AcceptLocked(request,true);
                        std::lock_guard<std::mutex> lock(stateMutex);
                        if (receiverPhase=="held") receiverPhase="accepted";
                    }
                }
            }
        } catch (...) {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (held) {
                needsReconciliation=receiverHandled=true; receiverPhase="unknown";
                message="Receiver failure intent/stop acceptance unconfirmed; gate retained, no replay";
            } else if (!receiverHandled) receiverPhase="observation-unavailable";
        }
        std::unique_lock<std::mutex> wait(alarmWaitMutex);
        alarmWait.wait_for(wait,std::chrono::milliseconds(configuration.ReceiverPollIntervalMs()),[this] { return alarmShutdown.load(); });
    }
}
void EccControlJobs::ValidateServiceShutdown(const std::string &instanceId,const std::string &snapshotSha256,
                                           const std::string &handoffOperationId) const {
    auto &p=*fImpl;
    SupervisorOperationId(handoffOperationId);
    if(instanceId!=p.instance || snapshotSha256!=p.snapshot || handoffOperationId!=p.handoffOperationId)
        throw std::runtime_error("ECC shutdown must bind this instance, snapshot and original data-handoff operation");
    {
        std::lock_guard<std::mutex> lock(p.stateMutex);
        const auto found=p.jobs.find(handoffOperationId);
        bool complete=found!=p.jobs.end() && found->second.phase=="complete";
        if(!complete && found!=p.jobs.end() && found->second.phase=="failed" && !p.handoffCompletionOperationId.empty()) {
            const auto completion=p.jobs.find(p.handoffCompletionOperationId);
            complete=completion!=p.jobs.end() && completion->second.phase=="complete" &&
                completion->second.request.command=="complete-data-handoff" &&
                completion->second.request.failedOperationId==handoffOperationId &&
                completion->second.request.instanceId==p.instance;
        }
        if(!p.handoffFenced || p.active || found==p.jobs.end() || !complete ||
           found->second.request.instanceId!=p.instance ||
           (found->second.request.command!="handoff-data-control" && found->second.request.command!="retire-data-plans"))
            throw std::runtime_error("ECC service shutdown requires a completed original data handoff and no active operation");
    }
    // The public Status() intentionally folds the expected post-handoff data
    // reconciliation fence into the controller status. Inspect hardware/control
    // state separately; receiver completion and the exact handoff receipt are
    // independently checked below.
    const auto status=p.controller.ControlStatus();
    if((status.state!=EccState::Idle && status.state!=EccState::Described) || status.transition || status.error ||
       status.outcomeUnknown || status.needsReconciliation)
        throw std::runtime_error("ECC service shutdown requires a known Idle or Described controller after Reset");
    // A completed handoff deliberately latches RemoteReceiverManager as fenced,
    // so its current aggregate status is no longer a normal Stopped status.
    // The handoff operation itself is accepted only after NormalSealedData and
    // completes only after each node's stopped cycle and seal are reverified.
    // Recheck its exact retained receipt here instead of rejecting that fence.
    (void)ControlHandoffJson();
}

}
