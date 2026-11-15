#include "remotereceivermanager_p.h"
#include <stdexcept>

namespace GetDAQ {
using namespace RemoteReceiverManagerInternal;

void RemoteReceiverManager::Impl::ExecuteMutation(SupervisorCommand command,const std::string &path,
    const std::string &experiment,unsigned int run,const std::set<std::string> &unused,
    const std::vector<SupervisorControlRequest> &requests,const Clock::time_point &deadline,
    bool handoff,bool cleanup,bool retirement,bool changedPlans,const std::string &parentFailure,
    const std::string &successorScope,const std::string &successorEpoch,
    const std::vector<bool> &alreadySealed,AuditProof &proof) {
        if(handoff) {
            handoffAttempted=true;handoffProof=&proof;handoffRequests=requests;
            handoffRejections.assign(requests.size(),1);
        }
        if(cleanup) failedAbortAttempted=true;
        if(retirement) retirementAttempted=true;
        const auto record=[&](const std::string &line) { CheckRoot(); proof.audit->Append(line); proof.journal+=line+'\n'; CheckRoot(); };
        try {
            record("{\"phase\":\"intent\",\"binding_sha256\":"+EncodeJsonString(binding)+
                (handoff ? ",\"handoff_context_sha256\":"+EncodeJsonString(ConfigurationSha256(proof.handoffContext)) : "")+",\"automatic_retry\":false}");
            std::string handoffNodes="[";
            std::exception_ptr firstFailure;
            for (std::size_t i=0;i<peers.size();++i) {
                if((cleanup || retirement) && alreadySealed[i]) {record("{\"phase\":\"node_already_sealed\",\"agent_id\":"+EncodeJsonString(peers[i]->endpoint.agentId)+",\"no_post_sent\":true}");continue;}
                record("{\"phase\":\"before_post\",\"agent_id\":"+EncodeJsonString(peers[i]->endpoint.agentId)+",\"operation_id\":"+EncodeJsonString(requests[i].operationId)+",\"request_key\":"+EncodeJsonString(SupervisorControlRequestKey(requests[i]))+"}");
                try {
                    if (command==SupervisorCommand::Start) { peers[i]->catalogJson.clear(); Publish(i); }
                    const auto result=Execute(i,requests[i],deadline,record,handoff ? &handoffRejections[i] : nullptr); auto &peer=*peers[i];
                    if (command==SupervisorCommand::Start) { Ready(peer,result.data,true); peer.cycle=result.data; peer.hasCycle=true; peer.retiredCycle=false; }
                    else if(cleanup) {
                        SameCycle(peer,result.data,false);
                        if(result.data.state!=DataSupervisorStatus::Aborted || !result.data.needsReconciliation ||
                           !std::all_of(result.data.receivers.begin(),result.data.receivers.end(),[](const ManagedReceiverStatus &r){return r.reaped;}))
                            throw std::runtime_error("Remote cleanup did not confirm original aborted/reaped session");
                    } else if(retirement) {
                        SameCycle(peer,result.data,false);
                        if(result.data.state!=DataSupervisorStatus::Retired || result.data.needsReconciliation)
                            throw std::runtime_error("Remote retirement did not verify the original retired session");
                        peer.retiredCycle=true;
                    } else { if (result.data.state!=DataSupervisorStatus::Stopped || result.data.needsReconciliation) throw std::runtime_error("Remote Finish did not verify stopped"); SameCycle(peer,result.data,true); }
                    if (command==SupervisorCommand::Finish) for (const auto &receiver:result.data.receivers)
                        if (receiver.sourceUnusedConfirmed!=(unused.count(receiver.id)!=0)) throw std::runtime_error("Remote unused-source result differs from parent intent");
                    if (command==SupervisorCommand::Catalog) {
                        ReceiverCatalogOptions expected; expected.receiver=peer.plan.options; expected.executableSha256=peer.plan.executableSha256; expected.stopped=result.data;
                        expected.operationId=requests[i].catalogReplaceExisting && requests[i].catalogPhase!="single" ? requests[i].catalogReplacementId : requests[i].operationId;
                        expected.experiment=experiment; expected.runNumber=run;
                        expected.stagedForReplacement=requests[i].catalogReplaceExisting;
                        const auto receipt=DecodeEccControlCatalogReceipt(result.catalogJson); std::ostringstream number; number<<"run_"<<std::setw(4)<<std::setfill('0')<<run;
                        if (receipt.operationId!=expected.operationId || receipt.requestKey!=ReceiverCatalogRequestKey(expected) ||
                            receipt.directory!=peer.plan.options.catalogRoot+'/'+experiment+'/'+number.str()+'/'+peer.plan.options.deploymentId)
                            throw std::runtime_error("Remote catalog request/experiment/Run/directory binding differs");
                        if (requests[i].catalogPhase!="prepare") {
                            peer.catalogJson=result.catalogJson; Publish(i);
                            record("{\"phase\":\"catalog_complete\",\"agent_id\":"+EncodeJsonString(peer.endpoint.agentId)+",\"catalog\":"+EmbeddedJsonObject(result.catalogJson)+"}");
                        } else record("{\"phase\":\"catalog_prepared\",\"agent_id\":"+EncodeJsonString(peer.endpoint.agentId)+",\"catalog\":"+EmbeddedJsonObject(result.catalogJson)+"}");
                    }
                    if (handoff) {
                        if (!handoffNodes.empty() && handoffNodes.back()!='[') handoffNodes+=',';
                        const auto receipt="{\"agent_id\":"+EncodeJsonString(peer.endpoint.agentId)+",\"instance_id\":"+EncodeJsonString(peer.instance)+
                            ",\"operation_id\":"+EncodeJsonString(requests[i].operationId)+",\"request_key\":"+EncodeJsonString(SupervisorControlRequestKey(requests[i]))+
                            ",\"proof_sha256\":"+EncodeJsonString(result.handoffProofSha256)+",\"request\":"+EncodeJsonString(EncodeSupervisorControlRequest(requests[i]))+"}";
                        handoffNodes+=receipt;
                        record("{\"phase\":\"node_control_handoff_complete\",\"receipt\":"+receipt+"}");
                    }
                    record("{\"phase\":\"node_complete\",\"agent_id\":"+EncodeJsonString(peer.endpoint.agentId)+",\"result\":"+EmbeddedJsonObject(EncodeSupervisorStatus(result.data))+"}");
                } catch (...) {
                    if (command==SupervisorCommand::Start) throw;
                    // Finish/Prepare独立观察节点；Publish首个未知结果后不再扩大跨节点部分提交。
                    // 父审计发生故障则立即停止后段，不能再以不可信意图发送命令。
                    CheckRoot(); if (!firstFailure) firstFailure=std::current_exception();
                    record("{\"phase\":\"node_unknown\",\"agent_id\":"+EncodeJsonString(peers[i]->endpoint.agentId)+",\"operation_id\":"+EncodeJsonString(requests[i].operationId)+"}");
                    if (command==SupervisorCommand::Catalog &&
                        (requests[i].catalogPhase=="publish" || requests[i].catalogPhase=="finalize")) {
                        for (std::size_t pending=i+1;pending<peers.size();++pending)
                            record("{\"phase\":\"node_not_attempted\",\"agent_id\":"+EncodeJsonString(peers[pending]->endpoint.agentId)+
                                ",\"operation_id\":"+EncodeJsonString(requests[pending].operationId)+",\"reason\":"+
                                EncodeJsonString(requests[i].catalogPhase=="publish" ? "prior_publish_uncertain" : "prior_finalize_uncertain")+",\"post_sent\":false}");
                        break;
                    }
                }
            }
            if (firstFailure) std::rethrow_exception(firstFailure);
            record("{\"phase\":\"complete\",\"raw_data_forwarded\":false}");
            if (handoff) {
                handoffCompleteJson="{\"schema\":1,\"kind\":"+EncodeJsonString(changedPlans ? "getdaq.remote-plan-retirement" : "getdaq.remote-control-handoff")+",\"original_scope_sha256\":"+EncodeJsonString(scope)+
                    ",\"original_epoch\":"+EncodeJsonString(epoch)+",\"successor_scope_sha256\":"+EncodeJsonString(successorScope)+
                    ",\"successor_epoch\":"+EncodeJsonString(successorEpoch)+",\"audit_directory\":"+EncodeJsonString(path)+",\"nodes\":"+handoffNodes+"]}\n";
                Latch("Control handed off explicitly; original remote manager is permanently fenced");
            }
            if(cleanup) {
                Latch("Failed receiver cleanup complete; acquisition incomplete, hardware reconciliation still required");
                failedAbortParent=parentFailure; failedAbortComplete=true;
            }
            if(retirement) {
                std::lock_guard<std::mutex> state(stateMutex); status.needsReconciliation=false;
                status.message="Original failed data sessions retired; old files and independent hardware/Run holds remain";
            }
            if(command==SupervisorCommand::Start) {
                failedAbortAttempted=false; failedAbortComplete=false; retirementAttempted=false;
                retirementBlocked=false; failedAbortParent.clear();
            }
        } catch (...) {
            Latch("Remote receiver mutation failed/uncertain; keep all node results, no retry or cleanup");
            try { record("{\"phase\":\"unknown\",\"automatic_retry\":false,\"automatic_cleanup\":false}"); } catch (...) {}
            throw;
        }
}

} // namespace GetDAQ
