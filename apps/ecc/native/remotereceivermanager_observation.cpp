#include "remotereceivermanager_p.h"
#include <stdexcept>

namespace GetDAQ {
using namespace RemoteReceiverManagerInternal;

void RemoteReceiverManager::Impl::Observe(std::size_t index,const Clock::time_point &deadline,bool failureCleanup,
                 const std::string &successorScope,const std::string &successorEpoch){
        Deadline(deadline); CheckRoot(); auto &peer=*peers[index]; const auto v=Response(peer.client.Status(),200);
        ExactSupervisorFields(v,{"schema","service","agent_id","instance_id","plan_sha256","operation_active","operation_id","control_needs_reconciliation","message","controller_snapshot_sha256","controller_epoch","lifecycle"});
        if (Unsigned(v.At("schema"))!=1 || String(v.At("service"))!="getdaq-supervisor" || String(v.At("agent_id"))!=peer.endpoint.agentId ||
            String(v.At("plan_sha256"))!=peer.endpoint.planSha256 || String(v.At("instance_id"))!=peer.instance) {
            Latch("Remote status identity/instance differs"); throw std::runtime_error("Remote status identity/instance differs");
        }
        const auto owner=String(v.At("controller_snapshot_sha256")),ownerEpoch=String(v.At("controller_epoch"));
        const auto &expectedScope=successorScope.empty() ? scope : successorScope;
        const auto &expectedEpoch=successorScope.empty() ? epoch : successorEpoch;
        if((failureCleanup || !successorScope.empty()) && (owner!=expectedScope || ownerEpoch!=expectedEpoch)) throw std::runtime_error("Cleanup requires the retained remote controller owner");
        if ((owner.empty()!=ownerEpoch.empty()) || (!owner.empty() && (owner!=expectedScope || ownerEpoch!=expectedEpoch))) {
            Latch("Remote controller owner scope/epoch differs; no takeover"); throw std::runtime_error("Remote controller owner scope/epoch differs");
        }
        peer.observed=DecodeSupervisorStatus(v.At("lifecycle"));
        peer.serviceNeedsReconciliation=Boolean(v.At("control_needs_reconciliation"));
        if (peer.observed.planSha256!=peer.endpoint.planSha256) throw std::runtime_error("Remote lifecycle plan differs");
        Publish(index);
        if (!failureCleanup && (peer.serviceNeedsReconciliation || peer.observed.needsReconciliation)) {
            Latch("Remote service/data requires reconciliation"); throw std::runtime_error("Remote service/data requires reconciliation");
        }
        if (Boolean(v.At("operation_active"))) throw RemoteBusy();
        CheckRoot(); Deadline(deadline);
    }

void RemoteReceiverManager::Impl::AdoptPredecessorCycle(std::size_t index,const Clock::time_point &deadline){
        if (predecessorChangedPlans) { VerifyPlanPredecessor(index,deadline);return; }
        if (predecessorHandoffJson.empty() || peers[index]->hasCycle) return;
        auto &peer=*peers[index];
        const auto &nodes=predecessorHandoff.At("nodes");
        const JsonValue *node=nullptr;
        for (const auto &entry:nodes.items) if (String(entry.At("agent_id"))==peer.endpoint.agentId) node=&entry;
        if (!node) throw std::runtime_error("Predecessor handoff lacks this exact data agent");
        const auto request=DecodeSupervisorControlRequest(String(node->At("request")));
        if (request.command!=SupervisorCommand::HandoffControl || request.instanceId!=peer.instance ||
            request.planSha256!=peer.endpoint.planSha256 || request.controllerId!=options.controllerId ||
            request.controllerSnapshotSha256!=String(predecessorHandoff.At("original_scope_sha256")) ||
            request.controllerEpoch!=String(predecessorHandoff.At("original_epoch")) ||
            request.successorControllerSnapshotSha256!=scope || request.successorControllerEpoch!=epoch ||
            String(node->At("instance_id"))!=peer.instance || String(node->At("operation_id"))!=request.operationId ||
            String(node->At("request_key"))!=SupervisorControlRequestKey(request))
            throw std::runtime_error("Predecessor handoff does not bind this successor and original plan/instance");
        Deadline(deadline); CheckRoot();
        const auto original=DecodeOperation(Response(peer.client.Operation(request.operationId),200),request);
        OperationResult result;
        const auto completionId=node->members.count("completion_operation_id") ? String(node->At("completion_operation_id")) : std::string();
        const auto completionBytes=node->members.count("completion_request") ? String(node->At("completion_request")) : std::string();
        if (original.phase=="complete") result=original;
        else if (original.phase=="failed" && !completionId.empty() && !completionBytes.empty()) {
            result=ObserveCompletedHandoff(index,request,deadline,[](const std::string &) {});
            if (result.completionOperationId!=completionId || result.completionRequestJson!=completionBytes)
                throw std::runtime_error("Successor node completion differs from the retained parent receipt");
        } else throw std::runtime_error("Original data service cannot confirm the completed predecessor handoff");
        if (result.phase!="complete" || result.handoffProofSha256!=String(node->At("proof_sha256")) ||
            result.data.state!=DataSupervisorStatus::Stopped || result.data.needsReconciliation)
            throw std::runtime_error("Original data service cannot confirm the completed predecessor handoff");
        peer.cycle=result.data;peer.hasCycle=true;peer.retiredCycle=false;
        SameCycle(peer,peer.observed,true);Stopped(peer,peer.observed);CheckRoot();Deadline(deadline);
    }

void RemoteReceiverManager::Impl::VerifyPlanPredecessor(std::size_t index,const Clock::time_point &deadline){
        auto &peer=*peers[index];
        const JsonValue *node=nullptr;
        for (const auto &entry:predecessorHandoff.At("nodes").items)
            if (String(entry.At("agent_id"))==peer.endpoint.agentId) node=&entry;
        if (!node) throw std::runtime_error("Retirement receipt lacks this data agent");
        const auto request=DecodeSupervisorControlRequest(String(node->At("request")));
        if (request.command!=SupervisorCommand::RetirePlan || request.controllerId!=options.controllerId ||
            request.controllerSnapshotSha256!=String(predecessorHandoff.At("original_scope_sha256")) ||
            request.controllerEpoch!=String(predecessorHandoff.At("original_epoch")) || request.successorPlanSha256!=peer.endpoint.planSha256 ||
            request.successorControllerSnapshotSha256!=scope || request.successorControllerEpoch!=epoch ||
            String(node->At("instance_id"))!=request.instanceId || request.instanceId==peer.instance ||
            String(node->At("operation_id"))!=request.operationId || String(node->At("request_key"))!=SupervisorControlRequestKey(request))
            throw std::runtime_error("Original retirement does not authorize this data plan/instance/owner");
        Deadline(deadline);CheckRoot();const auto response=peer.client.Predecessor();
        if (response.code!=200) throw std::runtime_error("New data service cannot confirm its original retirement proof");
        JsonLimits limits;limits.stringBytes=1024*1024;limits.arrayItems=131072;
        const auto value=ParseStrictJson(response.body,limits);
        ExactSupervisorFields(value,{"schema","kind","agent_id","instance_id","plan_sha256","proof_sha256","proof"});
        if (Unsigned(value.At("schema"))!=1 || String(value.At("kind"))!="getdaq.supervisor-plan-predecessor" ||
            String(value.At("agent_id"))!=peer.endpoint.agentId || String(value.At("instance_id"))!=peer.instance ||
            String(value.At("plan_sha256"))!=peer.endpoint.planSha256 || String(value.At("proof_sha256"))!=String(node->At("proof_sha256")))
            throw std::runtime_error("New data service predecessor identity differs");
        const auto &proof=value.At("proof");
        ExactSupervisorFields(proof,{"schema","kind","proof_file","deployment_root","agent_id","request_key","request","successor_plan","lifecycle","cycle_evidence_sha256"});
        const auto planBytes=String(proof.At("successor_plan"));
        const auto oldCycle=DecodeSupervisorStatus(proof.At("lifecycle"));
        if (Unsigned(proof.At("schema"))!=1 || String(proof.At("kind"))!="getdaq.supervisor-plan-retirement" ||
            String(proof.At("agent_id"))!=peer.endpoint.agentId || String(proof.At("request_key"))!=SupervisorControlRequestKey(request) ||
            String(proof.At("request"))!=EncodeSupervisorControlRequest(request) || ConfigurationSha256(planBytes)!=peer.endpoint.planSha256 ||
            oldCycle.planSha256!=request.planSha256 || oldCycle.sessionId!=request.expectedSessionId ||
            oldCycle.state!=DataSupervisorStatus::Stopped || oldCycle.needsReconciliation || oldCycle.receivers.empty())
            throw std::runtime_error("New data service retirement content differs");
        for (const auto &receiver:oldCycle.receivers)
            if (!receiver.reaped || receiver.exitCode!=0 || !receiver.sealVerified) throw std::runtime_error("Retirement lacks normal sealed original cycle");
        if (!predecessorVerifiedAgents.count(peer.endpoint.agentId)) {
            if (peer.hasCycle || peer.observed.state!=DataSupervisorStatus::Fresh || !peer.observed.sessionId.empty() || !peer.observed.receivers.empty())
                throw std::runtime_error("Planned data service already used before explicit ECC adoption");
            predecessorVerifiedAgents.insert(peer.endpoint.agentId);
        }
        CheckRoot();Deadline(deadline);
    }

void RemoteReceiverManager::Impl::Refresh(const Clock::time_point &deadline,bool allowCatalogRecovery){
        Gate(false,allowCatalogRecovery); CheckRoot();
        try {
            for (std::size_t i=0;i<peers.size();++i) {
                Identity(i,deadline);
                Observe(i,deadline,allowCatalogRecovery,predecessorHandoffJson.empty() ? "" : scope,predecessorHandoffJson.empty() ? "" : epoch);
                AdoptPredecessorCycle(i,deadline);
            }
        } catch (const RemoteBusy &) { throw; }
        catch (...) {
            if (!predecessorHandoffJson.empty() || std::any_of(peers.begin(),peers.end(),[](const std::unique_ptr<Peer> &peer) { return peer->hasCycle; }))
                Latch("Previously started remote cycle could not be observed; verified recovery required");
            throw;
        }
    }

} // namespace GetDAQ
