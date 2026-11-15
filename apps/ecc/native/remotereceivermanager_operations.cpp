#include "remotereceivermanager_p.h"
#include <stdexcept>

namespace GetDAQ {
using namespace RemoteReceiverManagerInternal;

OperationResult RemoteReceiverManager::Impl::WaitOperation(std::size_t index,const SupervisorControlRequest &request,OperationResult result,
                                  const Clock::time_point &deadline){
        auto &peer=*peers[index];
        for (;;) {
            if (result.hasData) { peer.observed=result.data; Publish(index); }
            if (result.phase=="complete") {
                const bool planRetirement=SupervisorCommandRetiresPlan(request.command);
                Observe(index,deadline,request.command==SupervisorCommand::AbortFailedSession || request.command==SupervisorCommand::RetireFailedSession || planRetirement,
                    planRetirement ? "" : request.successorControllerSnapshotSha256,planRetirement ? "" : request.successorControllerEpoch); CheckRoot();
                if(request.command==SupervisorCommand::AbortFailedSession) {
                    SameCycle(peer,peer.observed,false);
                    if(peer.observed.state!=DataSupervisorStatus::Aborted || !peer.observed.needsReconciliation)
                        throw std::runtime_error("Fresh cleanup status differs from completed aborted session");
                }
                if(request.command==SupervisorCommand::RetireFailedSession) {
                    SameCycle(peer,peer.observed,false);
                    if(peer.observed.state!=DataSupervisorStatus::Retired || peer.observed.needsReconciliation)
                        throw std::runtime_error("Fresh retirement status differs from the original retired session");
                }
                return result;
            }
            if (result.phase=="failed" || result.phase=="unknown") throw std::runtime_error("Remote operation "+result.phase+": "+result.message);
            Deadline(deadline); std::this_thread::sleep_for(std::chrono::milliseconds(options.pollIntervalMs)); CheckRoot(); Deadline(deadline);
            result=DecodeOperation(Response(peer.client.Operation(request.operationId),200),request);
        }
    }

OperationResult RemoteReceiverManager::Impl::ObserveCompletedRetirement(std::size_t index,const SupervisorControlRequest &original,
        const Clock::time_point &deadline,const std::function<void(const std::string &)> &record){
        auto &peer=*peers[index];Deadline(deadline);CheckRoot();
        // Only the operator's already-completed node repair may bridge a failed
        // child. The parent never creates or POSTs a repair on its own.
        const auto value=Response(peer.client.PlanRetirementCompletion(),200);CheckRoot();Deadline(deadline);
        ExactSupervisorFields(value,{"schema","kind","agent_id","request","operation"});
        const auto completion=DecodeSupervisorControlRequest(String(value.At("request")));
        if (Unsigned(value.At("schema"))!=1 || String(value.At("kind"))!="getdaq.supervisor-plan-retirement-completion" ||
            String(value.At("agent_id"))!=peer.endpoint.agentId ||
            completion.command!=SupervisorCommand::CompletePlanRetirement ||
            EncodeSupervisorControlRequest(OriginalPlanRetirementRequest(completion))!=EncodeSupervisorControlRequest(original))
            throw std::runtime_error("Explicit node retirement completion differs from the original failed request");
        auto result=DecodeOperation(value.At("operation"),completion);
        if (result.phase!="complete") throw std::runtime_error("Explicit node retirement completion is not complete; observe that ID");
        result=WaitOperation(index,completion,result,deadline);
        const auto response=peer.client.PlanRetirement();CheckRoot();Deadline(deadline);
        if (response.code!=200 || ConfigurationSha256(response.body)!=result.handoffProofSha256)
            throw std::runtime_error("Explicit node completion proof differs from its completed operation");
        JsonLimits limits;limits.stringBytes=1024*1024;limits.arrayItems=131072;
        const auto proof=ParseStrictJson(response.body,limits);
        ExactSupervisorFields(proof,{"schema","kind","proof_file","deployment_root","agent_id","request_key","request","successor_plan","lifecycle","cycle_evidence_sha256"});
        if (Unsigned(proof.At("schema"))!=1 || String(proof.At("kind"))!="getdaq.supervisor-plan-retirement" ||
            String(proof.At("agent_id"))!=peer.endpoint.agentId || String(proof.At("request_key"))!=SupervisorControlRequestKey(original) ||
            String(proof.At("request"))!=EncodeSupervisorControlRequest(original) ||
            ConfigurationSha256(String(proof.At("successor_plan")))!=original.successorPlanSha256 ||
            EncodeSupervisorStatus(DecodeSupervisorStatus(proof.At("lifecycle")))!=EncodeSupervisorStatus(result.data))
            throw std::runtime_error("Explicit node retirement proof lost its original target or sealed lifecycle");
        record("{\"phase\":\"node_failed_retirement_completed\",\"agent_id\":"+EncodeJsonString(peer.endpoint.agentId)+
            ",\"operation_id\":"+EncodeJsonString(original.operationId)+",\"request_key\":"+EncodeJsonString(SupervisorControlRequestKey(original))+
            ",\"completion_request\":"+EncodeJsonString(EncodeSupervisorControlRequest(completion))+
            ",\"completion_request_key\":"+EncodeJsonString(SupervisorControlRequestKey(completion))+
            ",\"proof_sha256\":"+EncodeJsonString(result.handoffProofSha256)+",\"no_post_sent\":true}");
        return result;
    }

OperationResult RemoteReceiverManager::Impl::ObserveCompletedHandoff(std::size_t index,const SupervisorControlRequest &original,
        const Clock::time_point &deadline,const std::function<void(const std::string &)> &record){
        auto &peer=*peers[index];Deadline(deadline);CheckRoot();
        const auto response=peer.client.ControlHandoffCompletion();
        if (response.code!=200) throw std::runtime_error("Original node lacks an explicit live failed-handoff completion");
        CheckRoot();Deadline(deadline);const auto value=ParseStrictJson(response.body);
        ExactSupervisorFields(value,{"schema","kind","agent_id","original_request","original_operation","request","operation","proof"});
        const auto failedRequest=DecodeSupervisorControlRequest(String(value.At("original_request")));
        if (Unsigned(value.At("schema"))!=1 || String(value.At("kind"))!="getdaq.supervisor-control-handoff-completion" ||
            String(value.At("agent_id"))!=peer.endpoint.agentId || EncodeSupervisorControlRequest(failedRequest)!=EncodeSupervisorControlRequest(original))
            throw std::runtime_error("Explicit node handoff completion differs from the original request");
        const auto failed=DecodeOperation(value.At("original_operation"),original);
        if (failed.phase!="failed" || !failed.hasData || failed.data.state!=DataSupervisorStatus::Stopped || failed.data.needsReconciliation)
            throw std::runtime_error("Node completion does not retain a normal stopped original failure");
        const auto request=DecodeSupervisorControlRequest(String(value.At("request")));
        if (request.command!=SupervisorCommand::CompleteControlHandoff || request.instanceId!=peer.instance ||
            EncodeSupervisorControlRequest(OriginalControlHandoffRequest(request))!=EncodeSupervisorControlRequest(original))
            throw std::runtime_error("Node completion request is not bound to the exact original handoff");
        auto result=DecodeOperation(value.At("operation"),request);
        if (result.phase!="complete") throw std::runtime_error("Explicit node handoff completion is not complete; observe that ID");
        result=WaitOperation(index,request,result,deadline);
        const auto proofBytes=String(value.At("proof"));
        if (proofBytes.empty() || ConfigurationSha256(proofBytes)!=result.handoffProofSha256)
            throw std::runtime_error("Explicit node handoff proof digest differs from its completed operation");
        JsonLimits limits;limits.stringBytes=1024*1024;const auto proof=ParseStrictJson(proofBytes,limits);
        ExactSupervisorFields(proof,{"schema","kind","operation_id","request_key","instance_id","plan_sha256","controller_id","controller_snapshot_sha256","controller_epoch","successor_controller_snapshot_sha256","successor_controller_epoch","lifecycle"});
        if (Unsigned(proof.At("schema"))!=1 || String(proof.At("kind"))!="getdaq.supervisor-control-handoff" ||
            String(proof.At("operation_id"))!=original.operationId || String(proof.At("request_key"))!=SupervisorControlRequestKey(original) ||
            String(proof.At("instance_id"))!=original.instanceId || String(proof.At("plan_sha256"))!=original.planSha256 ||
            String(proof.At("controller_id"))!=original.controllerId || String(proof.At("controller_snapshot_sha256"))!=original.controllerSnapshotSha256 ||
            String(proof.At("controller_epoch"))!=original.controllerEpoch ||
            String(proof.At("successor_controller_snapshot_sha256"))!=original.successorControllerSnapshotSha256 ||
            String(proof.At("successor_controller_epoch"))!=original.successorControllerEpoch ||
            EncodeSupervisorStatus(DecodeSupervisorStatus(proof.At("lifecycle")))!=EncodeSupervisorStatus(result.data) ||
            EncodeSupervisorStatus(result.data)!=EncodeSupervisorStatus(failed.data))
            throw std::runtime_error("Explicit node handoff proof does not bind original request, owner, successor or sealed cycle");
        result.completionOperationId=request.operationId;result.completionRequestJson=EncodeSupervisorControlRequest(request);
        record("{\"phase\":\"node_failed_handoff_completed\",\"agent_id\":"+EncodeJsonString(peer.endpoint.agentId)+
            ",\"operation_id\":"+EncodeJsonString(original.operationId)+",\"request_key\":"+EncodeJsonString(SupervisorControlRequestKey(original))+
            ",\"completion_request\":"+EncodeJsonString(result.completionRequestJson)+
            ",\"completion_request_key\":"+EncodeJsonString(SupervisorControlRequestKey(request))+
            ",\"proof_sha256\":"+EncodeJsonString(result.handoffProofSha256)+",\"no_post_sent\":true}");
        return result;
    }

OperationResult RemoteReceiverManager::Impl::Execute(std::size_t index,const SupervisorControlRequest &request,const Clock::time_point &deadline,
                            const std::function<void(const std::string &)> &record,unsigned int *rejection){
        Deadline(deadline); CheckRoot(); auto &peer=*peers[index];
        // Only one POST per explicit attempt. Lost replies are observed by GET.
        if(rejection)*rejection=0;
        const auto response=peer.client.Submit(request);
        if(rejection && (response.code==409 || response.code==412)) {
            *rejection=response.code;
            if(record)record("{\"phase\":\"explicit_rejection\",\"operation_id\":"+EncodeJsonString(request.operationId)+
                ",\"request_key\":"+EncodeJsonString(SupervisorControlRequestKey(request))+",\"http_status\":"+std::to_string(response.code)+"}");
        }
        const auto result=DecodeOperation(Response(response,202),request);
        if(record)record("{\"phase\":\"accepted_reply\",\"operation_id\":"+EncodeJsonString(request.operationId)+"}");
        return WaitOperation(index,request,result,deadline);
    }

} // namespace GetDAQ
