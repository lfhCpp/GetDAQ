#include "controljobs.h"
#include "controljobs_p.h"
#include "controlproofs_p.h"
#include "receivercatalog.h"
#include "receiverplantransition.h"
#include "planio.h"
#include "supervisorio.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include "../../common/text/utf8.h"
#include <cerrno>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unistd.h>
namespace GetDAQ {
using namespace SupervisorIO;
std::string EncodeSupervisorControlOperation(const SupervisorControlOperation &op) {
    return "{\"schema\":1,\"operation_id\":"+EncodeJsonString(op.request.operationId)+",\"command\":"+EncodeJsonString(SupervisorCommandName(op.request.command))+
        ",\"request_key\":"+EncodeJsonString(op.requestKey)+",\"phase\":"+EncodeJsonString(op.phase)+",\"message\":"+EncodeJsonString(op.message)+
        ",\"instance_id\":"+EncodeJsonString(op.request.instanceId)+",\"plan_sha256\":"+EncodeJsonString(op.request.planSha256)+
        ",\"controller_id\":"+EncodeJsonString(op.request.controllerId)+",\"controller_snapshot_sha256\":"+EncodeJsonString(op.request.controllerSnapshotSha256)+
        ",\"controller_epoch\":"+EncodeJsonString(op.request.controllerEpoch)+",\"result\":"+(op.resultStatusJson.empty() ? "null" : op.resultStatusJson)+
        ((op.request.command==SupervisorCommand::HandoffControl || op.request.command==SupervisorCommand::CompleteControlHandoff) ? ",\"handoff\":{\"successor_controller_snapshot_sha256\":"+EncodeJsonString(op.request.successorControllerSnapshotSha256)+
            ",\"successor_controller_epoch\":"+EncodeJsonString(op.request.successorControllerEpoch)+",\"proof_sha256\":"+EncodeJsonString(op.handoffProofSha256)+"}" : "")+
        (SupervisorCommandRetiresPlan(op.request.command) ? ",\"plan_retirement\":{\"successor_plan_sha256\":"+EncodeJsonString(op.request.successorPlanSha256)+
            ",\"successor_controller_snapshot_sha256\":"+EncodeJsonString(op.request.successorControllerSnapshotSha256)+",\"successor_controller_epoch\":"+EncodeJsonString(op.request.successorControllerEpoch)+
            ",\"proof_sha256\":"+EncodeJsonString(op.planRetirementProofSha256)+"}" : "")+
        (op.request.command==SupervisorCommand::Catalog ? ",\"catalog\":"+(op.catalogResultJson.empty() ? "null" : op.catalogResultJson) : "")+"}\n";
}
SupervisorControl::SupervisorControl(const SupervisorControlOptions &o) : SupervisorControl(o,nullptr) {}
SupervisorControl::SupervisorControl(const SupervisorControlOptions &o,std::shared_ptr<const SupervisorPlanPredecessor> predecessor) : fImpl(new Impl(o,predecessor)) {}
SupervisorControl::~SupervisorControl() {}
SupervisorControlOperation SupervisorControl::Submit(const SupervisorControlRequest &r) {
    auto &p=*fImpl; ValidateSupervisorControlRequest(r); bool catalogRecovery=false;
    if (r.instanceId!=p.instance || r.planSha256!=p.planHash || r.controllerId!=p.options.controllerId || r.configurationSha256!=p.options.receiver.configurationSha256 || r.artifactSha256!=p.options.receiver.artifactSha256)
        throw std::invalid_argument("Supervisor instance/plan/controller/configuration precondition differs");
    const auto key=SupervisorControlRequestKey(r);
    {
        std::lock_guard<std::mutex> lock(p.stateMutex); const auto found=p.jobs.find(r.operationId);
        if (found!=p.jobs.end()) { if (found->second.requestKey!=key) throw std::invalid_argument("Supervisor operation ID has a different request key"); return found->second; }
        if (p.active) throw std::runtime_error("Supervisor control operation busy; not queued");
        if (p.planRetired && r.command!=SupervisorCommand::CompletePlanRetirement) throw std::runtime_error("Original data plan retirement is accepted; deployment is permanently fenced");
    }
    std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Supervisor control submission busy; not queued");
    p.CheckStore();
    {
        std::lock_guard<std::mutex> lock(p.stateMutex);
        const auto found=p.jobs.find(r.operationId);
        if (found!=p.jobs.end()) { if (found->second.requestKey!=key) throw std::invalid_argument("Supervisor operation ID has a different request key"); return found->second; }
        if (p.active) throw std::runtime_error("Supervisor control operation busy; not queued");
        if (!p.ownerSnapshot.empty() && (r.controllerSnapshotSha256!=p.ownerSnapshot || r.controllerEpoch!=p.ownerEpoch)) throw std::runtime_error("Supervisor control owner scope/epoch differs; no takeover");
        if (r.command==SupervisorCommand::CompletePlanRetirement) { p.CheckRetirementCompletion(r); if(p.storageFault) throw std::runtime_error("Retirement completion cannot clear a storage fault"); }
        else if (r.command==SupervisorCommand::CompleteControlHandoff) p.CheckControlHandoffCompletion(r,true);
        else if (p.planRetired) throw std::runtime_error("Original data plan is retired; no new control or replay");
        if((r.command==SupervisorCommand::AbortFailedSession || r.command==SupervisorCommand::RetireFailedSession) && (p.ownerSnapshot.empty() || p.storageFault))
            throw std::runtime_error("Failed-session abort requires retained controller ownership and intact storage");
        if(r.command==SupervisorCommand::RetireFailedSession && (!p.retirementEligible || p.independentHold))
            throw std::runtime_error("Session retirement cannot clear an independent control hold or adopt a restarted service");
        catalogRecovery=r.command==SupervisorCommand::Catalog && r.catalogPhase==p.catalogRecoveryPhase && p.catalogRecoveryAllowed &&
            !p.catalogRecoveryInProgress && !p.storageFault && !p.independentHold && r.catalogReplacementId==p.catalogRecoveryReplacementId &&
            r.catalogSessionId==p.catalogRecoverySession && r.catalogExperiment==p.catalogRecoveryExperiment &&
            r.catalogRunNumber==p.catalogRecoveryRun;
        if (p.needsReconciliation && SupervisorCommandMutates(r.command) &&
            !((r.command==SupervisorCommand::CompletePlanRetirement && !p.storageFault) ||
              (r.command==SupervisorCommand::CompleteControlHandoff && p.liveHandoffCompletionAllowed && !p.storageFault) ||
              (r.command==SupervisorCommand::AbortStartup && p.startFailed && !p.storageFault) ||
              ((r.command==SupervisorCommand::AbortFailedSession || r.command==SupervisorCommand::RetireFailedSession) && !p.storageFault) ||
              catalogRecovery)) {
            if (r.command==SupervisorCommand::Catalog && (r.catalogPhase=="publish" || r.catalogPhase=="finalize"))
                throw std::runtime_error("Catalog recovery is not eligible (phase="+r.catalogPhase+",allowed="+std::string(p.catalogRecoveryAllowed ? "true" : "false")+
                    ",in_progress="+(p.catalogRecoveryInProgress ? "true" : "false")+
                    ",storage_fault="+(p.storageFault ? "true" : "false")+
                    ",replacement_match="+(r.catalogReplacementId==p.catalogRecoveryReplacementId ? "true" : "false")+
                    ",session_match="+(r.catalogSessionId==p.catalogRecoverySession ? "true" : "false")+
                    ",experiment_match="+(r.catalogExperiment==p.catalogRecoveryExperiment ? "true" : "false")+
                    ",run_match="+(r.catalogRunNumber==p.catalogRecoveryRun ? "true" : "false")+")");
            throw std::runtime_error("Supervisor control requires reconciliation; no positive replay");
        }
        if ((r.command==SupervisorCommand::HandoffControl || r.command==SupervisorCommand::RetirePlan) && (p.ownerSnapshot.empty() || p.storageFault || p.independentHold))
            throw std::runtime_error("Control handoff requires the original healthy live controller owner");
        if (p.jobs.size()>=8192) throw std::runtime_error("Supervisor operation history budget exhausted");
    }
    if((r.command==SupervisorCommand::AbortFailedSession || r.command==SupervisorCommand::RetireFailedSession || SupervisorCommandRetiresPlan(r.command)) && r.expectedSessionId!=p.data.Status().sessionId)
        throw std::invalid_argument("Failed-session abort session precondition differs; no operation was accepted");
    if (r.command==SupervisorCommand::HandoffControl || r.command==SupervisorCommand::CompleteControlHandoff || SupervisorCommandRetiresPlan(r.command)) {
        try { p.data.VerifyControlHandoff(); }
        catch (...) {
            if (p.data.Status().needsReconciliation) {
                std::lock_guard<std::mutex> state(p.stateMutex);
                p.needsReconciliation=p.independentHold=true;
                p.message="Original completed receiver proof changed; no control handoff";
            }
            throw;
        }
    }
    if (r.command==SupervisorCommand::Catalog) {
        const auto status=p.data.Status();
        const bool dataResume=catalogRecovery && p.data.CanResumeCatalogReplacement(r.catalogReplacementId,r.catalogExperiment,r.catalogRunNumber,r.catalogSessionId,r.catalogPhase);
        if (p.options.receiver.catalogRoot.empty() || status.sessionId!=r.catalogSessionId ||
            (!dataResume && (status.state!=DataSupervisorStatus::Stopped || status.needsReconciliation)))
            throw std::invalid_argument("Catalog requires an enabled fixed root and the current Stopped session");
        if (catalogRecovery && !dataResume) throw std::runtime_error("Catalog recovery evidence changed before acceptance");
    }
    if (r.command==SupervisorCommand::StageSuccessorPlan)
        PreflightScanSuccessorPlanBytes(r.successorPlanJson,r.successorPlanFile,p.options.receiver,p.options.executableSha256);
    std::shared_ptr<SupervisorNextPlan> next;
    if (SupervisorCommandRetiresPlan(r.command)) next=std::make_shared<SupervisorNextPlan>(r.successorPlanFile,r.successorPlanSha256,p.options.receiver,p.options.executableSha256);
    if (p.worker.joinable()) p.worker.join();
    const auto path=p.directory+"/operations/"+r.operationId;
    try {
        CreateDirectory(path); WriteNew(path+"/request.json",EncodeSupervisorControlRequest(r));
        if (p.ownerSnapshot.empty() && SupervisorCommandMutates(r.command)) {
            p.Record("owner",",\"controller_snapshot_sha256\":"+EncodeJsonString(r.controllerSnapshotSha256)+",\"controller_epoch\":"+EncodeJsonString(r.controllerEpoch));
            std::lock_guard<std::mutex> lock(p.stateMutex); p.ownerSnapshot=r.controllerSnapshotSha256; p.ownerEpoch=r.controllerEpoch;
        }
        if (r.command==SupervisorCommand::RetirePlan) {
            std::lock_guard<std::mutex> state(p.stateMutex);p.planRetired=true;p.planRetirementOperation=r.operationId;
            p.needsReconciliation=true;p.message="Original plan retirement accepted; no replay or new control";
        }
        if (r.command==SupervisorCommand::CompletePlanRetirement) {
            std::lock_guard<std::mutex> state(p.stateMutex);p.retirementCompletionUncertain=true;
        }
        p.Record("accepted",",\"operation_id\":"+EncodeJsonString(r.operationId)+",\"request_key\":"+EncodeJsonString(key)+",\"request\":"+EncodeJsonString(EncodeSupervisorControlRequest(r),4U*1024U*1024U));
        SupervisorControlOperation accepted; accepted.request=r; accepted.requestKey=key; accepted.phase="accepted";
        { std::lock_guard<std::mutex> lock(p.stateMutex); p.jobs.emplace(r.operationId,accepted); p.active=true; p.currentOperation=r.operationId;
          if (catalogRecovery) { p.catalogRecoveryAllowed=false;p.catalogRecoveryInProgress=true; } }
        p.worker=std::thread([&p,r,next] { p.Run(r,next); }); return accepted;
    } catch (...) {
        std::lock_guard<std::mutex> lock(p.stateMutex); p.needsReconciliation=true; p.message="Control acceptance/thread launch failed; preserve intent and do not retry";
        if (catalogRecovery) p.catalogRecoveryInProgress=false;
        auto found=p.jobs.find(r.operationId); if (found!=p.jobs.end()) { found->second.phase="unknown"; found->second.message=p.message; } p.active=false; throw;
    }
}
}
