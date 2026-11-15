#include "ecccontroljobs_p.h"
#include <stdexcept>
namespace GetDAQ {
EccControlOperation EccControlJobs::Impl::AcceptLocked(const EccControlRequest &r,bool automatic) {
    auto &p=*this; Validate(r);const bool negative=r.command=="stop-production-after-failure" || r.command=="abort-failed-receivers" || r.command=="retire-failed-receivers" || r.command=="retire-failed-sources" || r.command=="unprepare-acquisition-after-failure" || (r.command=="unprepare-after-failure" || r.command=="breakup-after-failure");
    bool inputRejected=false;
    try{p.Check(negative);}catch(...){if(!negative && p.inputFault && !p.storageFault){p.Check(true);inputRejected=true;}else throw;}
    if (r.instanceId!=p.instance || r.snapshotSha256!=p.snapshot || r.clientId!=p.configuration.ClientId()) throw std::invalid_argument("ECC instance/snapshot/client precondition differs");
    const auto key=EccControlRequestKey(r);
    {
        std::lock_guard<std::mutex> lock(p.stateMutex);
        const auto found=p.jobs.find(r.operationId);
        if (found!=p.jobs.end()) { if (found->second.requestKey!=key) throw std::invalid_argument("ECC operation ID has a different request key"); return found->second; }
        if (p.active || p.catalogRecoveryInProgress) throw std::runtime_error("ECC command busy; not queued");
        if(p.handoffFenced) {
            if(r.command!="complete-data-handoff")throw std::runtime_error("ECC data control handoff already accepted; original instance is permanently fenced");
            p.CheckHandoffCompletion(r);
        } else if(r.command=="complete-data-handoff")throw std::runtime_error("No original failed handoff to complete");
        if (!p.ownerEpoch.empty() && p.ownerEpoch!=r.clientEpoch) throw std::runtime_error("ECC client epoch differs; no automatic takeover");
        const bool catalogRecovery=r.command=="resume-catalog-replacement";
        if (catalogRecovery) {
            const auto original=p.jobs.find(r.failedOperationId);
            if (!p.needsReconciliation) throw std::runtime_error("Catalog recovery requires a latched reconciliation state");
            if (p.catalogRecoveryOriginalId!=r.failedOperationId) throw std::runtime_error("Catalog recovery failed-operation ID is not the retained replacement");
            if (original==p.jobs.end()) throw std::runtime_error("Catalog recovery original operation is not retained");
            if (original->second.phase!="failed" || original->second.request.command!="catalog" || !original->second.request.catalogReplaceExisting)
                throw std::runtime_error("Catalog recovery requires the exact failed replacement catalog operation");
            if (original->second.request.instanceId!=r.instanceId || original->second.request.snapshotSha256!=r.snapshotSha256 ||
                original->second.request.clientId!=r.clientId || original->second.request.clientEpoch!=r.clientEpoch)
                throw std::runtime_error("Catalog recovery identity differs from the original replacement");
            if (inputRejected || p.inputFault || p.storageFault || p.alarmHandled || p.receiverHandled)
                throw std::runtime_error("Catalog recovery is blocked by an independent input/storage/alarm/receiver hold");
            const auto hardware=p.controller.ControlStatus();
            if (hardware.state!=EccState::Prepared || hardware.transition || hardware.error || hardware.needsReconciliation || hardware.outcomeUnknown)
                throw std::runtime_error("Catalog recovery requires the same known Prepared ECC controller");
            const auto originalPath=p.configuration.ControllerOptions().auditRoot+"/operations/"+r.failedOperationId+"/catalog_publish";
            if (!p.data.HasRetainedCatalogPublishAudit(originalPath))
                throw std::runtime_error("Original Publish proof is not retained by this live ECC process; cold recovery is unsupported");
            p.catalogRecoveryInProgress=true;
        }
        if (p.needsReconciliation && !negative && r.command!="complete-data-handoff" && r.command!="release-stopped-alarm" && r.command!="reconcile-description" && r.command!="reconcile-preparation" && r.command!="reconcile-preparation-with-alarm" && r.command!="reconcile-acquisition" && r.command!="reconcile-acquisition-with-sources" && r.command!="reconcile-acquisition-with-alarm" && !catalogRecovery && !automatic && !inputRejected) throw std::runtime_error("ECC network requires reconciliation; no positive replay");
        if (automatic && r.command!="stop" && r.command!="stop-production-after-failure") throw std::logic_error("Automatic fault action must only stop production");
        if (p.jobs.size()>=8192) throw std::runtime_error("ECC network operation budget exhausted");
    }
    if (!inputRejected && r.command=="catalog" && (p.controller.Status().state!=EccState::Prepared || !p.data.CanCatalog())) throw std::invalid_argument("Catalog requires current Prepared/sealed sessions and enabled fixed data roots");
    if(!inputRejected && (r.command=="reprepare-cobo" || r.command=="configure-cobo")) {
        const auto status=p.controller.Status();const auto targets=p.controller.MaintenanceTargets();
        bool selected=false;for(const auto &target:targets)if(target.first==r.coboId)selected=true;
        if(!selected || status.state!=EccState::Prepared || status.transition || status.error || status.outcomeUnknown || status.needsReconciliation)
            throw std::invalid_argument("CoBo operation requires a fixed active target and known Prepared controller");
        const auto lifecycle=p.ProjectedTargetLifecycle(r.coboId);
        if(r.command=="configure-cobo" && lifecycle!="prepared")
            throw std::invalid_argument("CoBo Configure requires the selected board's known Prepared lifecycle state");
        if(r.command=="reprepare-cobo" && lifecycle!="prepared" && lifecycle!="configured")
            throw std::invalid_argument("CoBo reprepare requires the selected board's known Prepared or Configured lifecycle state");
    }
    if(!inputRejected && (r.command=="set-cobo-led" || r.command=="blink-cobo-leds" || r.command=="flip-cobo-led" || r.command=="pulse-cobo-led")) {
        const auto status=p.controller.Status();const auto targets=p.controller.MaintenanceTargets();
        bool selected=false;for(const auto &target:targets)if(target.first==r.coboId)selected=true;
        if(!selected || status.state!=EccState::Prepared || status.transition || status.error || status.outcomeUnknown || status.needsReconciliation)
            throw std::invalid_argument("CoBo LED maintenance requires a fixed active target and known Prepared controller");
        const auto lifecycle=p.ProjectedTargetLifecycle(r.coboId);
        if(lifecycle!="prepared" && lifecycle!="configured")
            throw std::invalid_argument("CoBo LED maintenance requires the selected board's known Prepared or Configured lifecycle state");
    }
    if(!inputRejected && r.command=="trigger-asad-pulser") {
        const auto status=p.controller.Status();const auto targets=p.controller.MaintenanceTargets();
        bool selected=false;for(const auto &target:targets)if(target.first==r.coboId)selected=true;
        if(!selected || status.state!=EccState::Prepared || status.transition || status.error || status.outcomeUnknown || status.needsReconciliation)
            throw std::invalid_argument("Single pulser trigger requires a fixed active target and known Prepared controller");
        const auto lifecycle=p.ProjectedTargetLifecycle(r.coboId);
        if(lifecycle!="prepared" && lifecycle!="configured")
            throw std::invalid_argument("Single pulser trigger requires the selected board's known Prepared or Configured lifecycle state");
    }
    if (!inputRejected && (r.command=="handoff-data-control" || r.command=="retire-data-plans")) {
        if (r.successorDataBindingSha256!=(r.command=="retire-data-plans" ? p.data.SuccessorBindingSha256(r.successorReceiverPlans) : p.data.BindingSha256()))
            throw std::invalid_argument("Data control handoff requires the same immutable receiver binding; changed plans are unsupported");
        const auto status=p.controller.Status();const auto data=p.data.Status();
        if ((status.state!=EccState::Described && status.state!=EccState::Idle) || status.transition || status.needsReconciliation || status.outcomeUnknown || !NormalSealedData(data))
            throw std::invalid_argument("Data control handoff requires known Idle/Described after normal Reset and sealed original data");
        if (r.successorScopeSha256==data.controllerScopeSha256 || r.successorEpoch==data.controllerEpoch)
            throw std::invalid_argument("Successor data scope and epoch must both differ from original");
        p.data.VerifyStopped();
    }
    if (p.worker.joinable()) p.worker.join();
    std::shared_ptr<EccSuccessorReview> reviewedSuccessor;
    if(!r.successorPlanFile.empty())reviewedSuccessor.reset(new EccSuccessorReview(r));
    const auto path=p.directory+"/operations/"+r.operationId;
    try {
        CreateDirectory(path); WriteNew(path+"/request.json",EncodeEccControlRequest(r));
    const auto proof=std::make_shared<EccControlRequestProof>(path,r,false,reviewedSuccessor); p.requestProofs.emplace(r.operationId,proof);
        if ((r.command=="handoff-data-control" || r.command=="retire-data-plans") && !inputRejected) {
            std::lock_guard<std::mutex> lock(p.stateMutex);p.handoffFenced=true;p.handoffOperationId=r.operationId;
            p.needsReconciliation=true;p.message="Data control handoff accepted; original ECC instance fenced, no replay";
        }
        if (p.ownerEpoch.empty()) { p.Record("owner",",\"client_epoch\":"+EncodeJsonString(r.clientEpoch),negative || inputRejected); std::lock_guard<std::mutex> lock(p.stateMutex); p.ownerEpoch=r.clientEpoch; }
        p.Record("accepted",",\"operation_id\":"+EncodeJsonString(r.operationId)+",\"request_key\":"+EncodeJsonString(key)+",\"request\":"+EncodeJsonString(EncodeEccControlRequest(r)),negative || inputRejected);
        EccControlOperation accepted; accepted.request=r; accepted.requestKey=key; accepted.phase="accepted";
        if(inputRejected) {
            accepted.phase="failed";accepted.message="Fixed external input fault: requested command was not executed; explicitly inspect/request retained production stop";accepted.resultJson=p.ControllerJson();
            p.Record("failed",",\"operation_id\":"+EncodeJsonString(r.operationId)+",\"request_key\":"+EncodeJsonString(key)+",\"message\":"+EncodeJsonString(accepted.message)+",\"result\":"+EncodeJsonString(accepted.resultJson),true);
            std::lock_guard<std::mutex> lock(p.stateMutex);p.jobs.emplace(r.operationId,accepted);p.operationOrder.push_back(r.operationId);p.needsReconciliation=true;
            if(r.operationId==p.alarmOperationId)p.alarmPhase="failed";
            if(r.operationId==p.receiverOperationId)p.receiverPhase="failed";
            return accepted;
        }
        { std::lock_guard<std::mutex> lock(p.stateMutex); p.jobs.emplace(r.operationId,accepted); p.operationOrder.push_back(r.operationId); p.active=true; p.activeId=r.operationId;
          if(r.command=="complete-data-handoff")p.handoffCompletionOperationId=r.operationId; }
        p.worker=std::thread([&p,r,proof] { p.Run(r,proof); }); return accepted;
    } catch (...) { std::lock_guard<std::mutex> lock(p.stateMutex); if(r.command=="resume-catalog-replacement"){p.catalogRecoveryOriginalId.clear();p.catalogRecoveryInProgress=false;} p.needsReconciliation=true; p.active=false; p.message="ECC acceptance/thread launch failed; preserve intent, no retry"; const auto found=p.jobs.find(r.operationId); if (found!=p.jobs.end()) found->second.phase="unknown"; throw; }
}
} // namespace GetDAQ
