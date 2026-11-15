#include "controljobs_p.h"
#include <stdexcept>
#include "receivercatalog.h"
#include "planio.h"
#include "configurationsha256.h"
#include "controlproofs_p.h"
#include "systemexperimentconfiguration.h"
#include <mutex>

namespace GetDAQ {
using namespace SupervisorIO;

void SupervisorControl::Impl::Dispatch(const SupervisorControlRequest &r,
    std::shared_ptr<SupervisorNextPlan> next,std::string &result,bool &retirement,bool &handoff) {
    const auto opDir=directory+"/operations/"+r.operationId;
    const auto dataDir=opDir+"/data";
    switch (r.command) {
    case SupervisorCommand::StageSuccessorPlan:
        result=StageScanSuccessorPlanBytes(r.successorPlanJson,r.successorPlanFile,options.receiver,options.executableSha256); break;
    case SupervisorCommand::RetirePlan:
    case SupervisorCommand::CompletePlanRetirement: {
        const bool repair=r.command==SupervisorCommand::CompletePlanRetirement;
        const auto sourceId=repair ? r.failedRetirementOperationId : r.operationId;
        const auto base=directory+"/operations/"+sourceId;
        if (repair && ReadFile(base+"/request.json",2U*1024U*1024U)!=EncodeSupervisorControlRequest(OriginalPlanRetirementRequest(r)))
            throw std::runtime_error("Original retirement request changed");
        if (next) next->Check();
        data.VerifyControlHandoff();
        const auto status=data.Status();
        if (status.state!=DataSupervisorStatus::Stopped || status.needsReconciliation || status.sessionId!=r.expectedSessionId || status.receivers.empty())
            throw std::runtime_error("Plan retirement requires the original normally completed cycle");
        for (const auto &receiver:status.receivers) if (!receiver.reaped || !receiver.sealVerified || receiver.exitCode!=0)
            throw std::runtime_error("Plan retirement contains an incomplete receiver");
        const auto successor=next ? next->Bytes() : ReadFile(r.successorPlanFile,1024*1024);
        const auto cycle=data.CompletedCycleEvidenceJson();
        const auto proof="{\"schema\":1,\"kind\":\"getdaq.supervisor-plan-retirement\",\"proof_file\":"+EncodeJsonString(base+"/plan-retirement.json")+
            ",\"deployment_root\":"+EncodeJsonString(options.receiver.auditRoot)+",\"agent_id\":"+EncodeJsonString(options.agentId)+
            ",\"request_key\":"+EncodeJsonString(SupervisorControlRequestKey(repair ? OriginalPlanRetirementRequest(r) : r))+
            ",\"request\":"+EncodeJsonString(EncodeSupervisorControlRequest(repair ? OriginalPlanRetirementRequest(r) : r))+
            ",\"successor_plan\":"+EncodeJsonString(successor)+",\"lifecycle\":"+EncodeSupervisorStatus(status)+
            ",\"cycle_evidence_sha256\":"+EncodeJsonString(ConfigurationSha256(cycle))+"}\n";
        // Preflight every existing path before publishing any metadata. A
        // conflict in a later file must not leave an earlier file behind.
        WriteOrVerifyRetirementFile(base+"/successor-plan.json",successor,repair,false);
        WriteOrVerifyRetirementFile(base+"/cycle-evidence.json",cycle,repair,false);
        WriteOrVerifyRetirementFile(base+"/plan-retirement.json",proof,repair,false);
        WriteOrVerifyRetirementFile(base+"/successor-plan.json",successor,repair,true);
        WriteOrVerifyRetirementFile(base+"/cycle-evidence.json",cycle,repair,true);
        WriteOrVerifyRetirementFile(base+"/plan-retirement.json",proof,repair,true);
        // Bind the reviewed source through publication. The input can change
        // while cycle evidence and retirement metadata are being written.
        if (next) next->Check();
        result=EncodeSupervisorStatus(status);retirement=true;break;
    }
    case SupervisorCommand::HandoffControl:
    case SupervisorCommand::CompleteControlHandoff: {
        const bool repair=r.command==SupervisorCommand::CompleteControlHandoff;
        const auto original=repair ? OriginalControlHandoffRequest(r) : r;
        const auto sourceId=repair ? r.failedHandoffOperationId : r.operationId;
        const auto base=directory+"/operations/"+sourceId;
        if (repair && ReadFile(base+"/request.json",2U*1024U*1024U)!=EncodeSupervisorControlRequest(original))
            throw std::runtime_error("Original control handoff request changed");
        data.VerifyControlHandoff();
        const auto status=data.Status();
        if (status.state!=DataSupervisorStatus::Stopped || status.needsReconciliation || status.receivers.empty())
            throw std::runtime_error("Control handoff requires the original normally completed cycle");
        for (const auto &receiver:status.receivers) if (!receiver.reaped || !receiver.sealVerified || receiver.exitCode!=0)
            throw std::runtime_error("Control handoff contains an incomplete receiver");
        const auto proof="{\"schema\":1,\"kind\":\"getdaq.supervisor-control-handoff\",\"operation_id\":"+EncodeJsonString(sourceId)+
            ",\"request_key\":"+EncodeJsonString(SupervisorControlRequestKey(original))+",\"instance_id\":"+EncodeJsonString(original.instanceId)+
            ",\"plan_sha256\":"+EncodeJsonString(planHash)+",\"controller_id\":"+EncodeJsonString(options.controllerId)+
            ",\"controller_snapshot_sha256\":"+EncodeJsonString(original.controllerSnapshotSha256)+",\"controller_epoch\":"+EncodeJsonString(original.controllerEpoch)+
            ",\"successor_controller_snapshot_sha256\":"+EncodeJsonString(original.successorControllerSnapshotSha256)+
            ",\"successor_controller_epoch\":"+EncodeJsonString(original.successorControllerEpoch)+",\"lifecycle\":"+EncodeSupervisorStatus(status)+"}\n";
        WriteNew(base+"/handoff-proof.json",proof);
        result=EncodeSupervisorStatus(status);handoff=true;break;
    }
    case SupervisorCommand::Start: data.Start(dataDir); result=EncodeSupervisorStatus(data.Status()); break;
    case SupervisorCommand::Finish: data.Finish(dataDir,r.unusedSourceIds); result=EncodeSupervisorStatus(data.Status()); break;
    case SupervisorCommand::AbortStartup: data.AbortStartup(dataDir); result=EncodeSupervisorStatus(data.Status()); break;
    case SupervisorCommand::AbortFailedSession: data.AbortFailedSession(dataDir,r.expectedSessionId,r.parentFailureOperationId); result=EncodeSupervisorStatus(data.Status()); break;
    case SupervisorCommand::RetireFailedSession: data.RetireFailedSession(dataDir,r.expectedSessionId,r.parentFailureOperationId); result=EncodeSupervisorStatus(data.Status()); break;
    case SupervisorCommand::Validate: data.Validate(); result=EncodeSupervisorStatus(data.Status()); break;
    case SupervisorCommand::VerifyReady: data.VerifyReady(); result=EncodeSupervisorStatus(data.Status()); break;
    case SupervisorCommand::VerifyStopped: data.VerifyStopped(); result=EncodeSupervisorStatus(data.Status()); break;
    case SupervisorCommand::Catalog:
        { const auto catalog=data.Catalog(dataDir,r.operationId,r.catalogExperiment,r.catalogRunNumber,r.catalogSessionId,r.catalogReplaceExisting,r.catalogPhase,r.catalogReplacementId);
          { std::lock_guard<std::mutex> state(stateMutex); jobs.at(r.operationId).catalogResultJson=catalog; }
          result=EncodeSupervisorStatus(data.Status()); }
        break;
    }
}
}
