#include "remotereceivermanager_p.h"
#include <stdexcept>

namespace GetDAQ {
using namespace RemoteReceiverManagerInternal;

void RemoteReceiverManager::CompletePendingHandoff(const std::string &path) {
    auto &p=*fImpl;std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if(!operation.owns_lock())throw std::runtime_error("Remote handoff completion busy; not queued");
    if(!p.handoffAttempted || !p.handoffProof || !p.handoffCompleteJson.empty() ||
       p.handoffRequests.size()!=p.peers.size() || p.handoffRejections.size()!=p.peers.size())
        throw std::runtime_error("Completion requires the retained original unfinished handoff; no cold adoption");
    p.CheckRoot();Path(path);
    const auto prefix=p.rootPath+"/operations/";
    if(path.compare(0,prefix.size(),prefix))throw std::invalid_argument("Completion audit is outside the original parent controller");
    const auto slash=path.find('/',prefix.size());
    if(slash==std::string::npos || path.find('/',slash+1)!=std::string::npos)
        throw std::invalid_argument("Completion audit requires parent operation and action directory");
    SupervisorOperationId(path.substr(prefix.size(),slash-prefix.size()));
    const auto &first=p.handoffRequests.front();
    const bool changedPlans=first.command==SupervisorCommand::RetirePlan;
    if(!changedPlans && first.command!=SupervisorCommand::HandoffControl)
        throw std::runtime_error("Retained request is not a control handoff or plan retirement");
    std::string intent="{\"schema\":1,\"kind\":\"getdaq.remote-handoff-completion-intent\",\"binding_sha256\":"+EncodeJsonString(p.binding)+
        ",\"original_audit_directory\":"+EncodeJsonString(p.handoffProof->path)+",\"original_intent_sha256\":"+EncodeJsonString(ConfigurationSha256(p.handoffProof->intent))+",\"requests\":[";
    for(std::size_t i=0;i<p.handoffRequests.size();++i) {
        if(i)intent+=',';
        intent+=EncodeSupervisorControlRequest(p.handoffRequests[i]);
    }
    intent+="]}\n";
    if(p.coldAuditPath!=path)CreateDirectory(path);
    std::unique_ptr<Impl::AuditProof> next(new Impl::AuditProof);next->path=path;next->intent=intent;next->directory.reset(new Directory(path));
    WriteNew(path+"/intent.json",intent);
    if(::lstat((path+"/intent.json").c_str(),&next->intentIdentity))throw std::runtime_error("Cannot bind completion intent");
    next->audit.reset(new HardwareAudit(path+"/remote.jsonl"));
    if(::lstat((path+"/remote.jsonl").c_str(),&next->journalIdentity))throw std::runtime_error("Cannot bind completion journal");
    p.proofs.push_back(std::move(next));auto &proof=*p.proofs.back();
    const auto record=[&](const std::string &line) {p.CheckRoot();proof.audit->Append(line);proof.journal+=line+'\n';p.CheckRoot();};
    const auto deadline=Clock::now()+std::chrono::milliseconds(p.options.commandTimeoutMs);
    try {
        record("{\"phase\":\"intent\",\"automatic_retry\":false,\"original_control_remains_fenced\":true}");
        std::vector<bool> missing(p.peers.size(),false);std::vector<OperationResult> results(p.peers.size());
        // Inspect every original instance before allowing a previously rejected
        // request to be submitted. Accepted/uncertain requests are GET-only.
        for(std::size_t i=0;i<p.peers.size();++i) {
            p.Identity(i,deadline);p.Deadline(deadline);const auto &request=p.handoffRequests[i];
            if(request.configurationSha256!=p.peers[i]->plan.options.configurationSha256 ||
               request.artifactSha256!=p.peers[i]->plan.options.artifactSha256)
                throw std::runtime_error("Original handoff configuration/artifact differs from current original plan");
            const auto reply=p.peers[i]->client.Operation(request.operationId);p.CheckRoot();
            if(reply.code==404) {
                const auto rejected=p.handoffRejections[i];
                if(rejected!=1 && rejected!=409 && rejected!=412)
                    throw std::runtime_error("Original delivery was uncertain; a missing operation cannot authorize another POST");
                p.Observe(i,deadline);auto &peer=*p.peers[i];p.Stopped(peer,peer.observed);
                if(!peer.hasCycle || peer.retiredCycle || peer.observed.state!=DataSupervisorStatus::Stopped)
                    throw std::runtime_error("Unaccepted handoff lacks the original normal sealed cycle");
                missing[i]=true;
                record("{\"phase\":\"node_original_not_accepted\",\"agent_id\":"+EncodeJsonString(peer.endpoint.agentId)+",\"operation_id\":"+EncodeJsonString(request.operationId)+",\"prior_rejection\":"+std::to_string(rejected)+"}");
            } else {
                auto original=DecodeOperation(Response(reply,200),request);
                const bool repaired=original.phase=="failed";
                results[i]=repaired ? (changedPlans ? p.ObserveCompletedRetirement(i,request,deadline,record) :
                    p.ObserveCompletedHandoff(i,request,deadline,record)) : p.WaitOperation(i,request,original,deadline);
                const auto &result=results[i];auto &peer=*p.peers[i];
                if(!result.hasData || result.data.state!=DataSupervisorStatus::Stopped || result.data.needsReconciliation)
                    throw std::runtime_error("Original handoff result lacks normal stopped data");
                p.SameCycle(peer,result.data,true);p.Stopped(peer,peer.observed);
                if (!repaired) record("{\"phase\":\"node_original_observed\",\"agent_id\":"+EncodeJsonString(p.peers[i]->endpoint.agentId)+",\"operation_id\":"+EncodeJsonString(request.operationId)+",\"no_post_sent\":true}");
            }
        }
        std::string nodes="[";
        for(std::size_t i=0;i<p.peers.size();++i) {
            const auto &request=p.handoffRequests[i];auto &peer=*p.peers[i];
            if(missing[i]) {
                record("{\"phase\":\"node_explicit_submit\",\"agent_id\":"+EncodeJsonString(peer.endpoint.agentId)+",\"operation_id\":"+EncodeJsonString(request.operationId)+",\"request_key\":"+EncodeJsonString(SupervisorControlRequestKey(request))+"}");
                results[i]=p.Execute(i,request,deadline,record,&p.handoffRejections[i]);
            }
            const auto &result=results[i];
            if(!result.hasData || result.data.state!=DataSupervisorStatus::Stopped || result.data.needsReconciliation)
                throw std::runtime_error("Original handoff result lacks normal stopped data");
            p.SameCycle(peer,result.data,true);p.SameCycle(peer,peer.observed,true);
            const auto node="{\"agent_id\":"+EncodeJsonString(peer.endpoint.agentId)+",\"instance_id\":"+EncodeJsonString(peer.instance)+
                ",\"operation_id\":"+EncodeJsonString(request.operationId)+",\"request_key\":"+EncodeJsonString(SupervisorControlRequestKey(request))+
                ",\"proof_sha256\":"+EncodeJsonString(result.handoffProofSha256)+",\"request\":"+EncodeJsonString(EncodeSupervisorControlRequest(request))+
                ",\"completion_operation_id\":"+EncodeJsonString(result.completionOperationId)+
                ",\"completion_request\":"+EncodeJsonString(result.completionRequestJson)+"}";
            if(i)nodes+=',';
            nodes+=node;
            record("{\"phase\":\"node_control_handoff_complete\",\"receipt\":"+node+"}");
        }
        const auto receipt="{\"schema\":1,\"kind\":"+EncodeJsonString(changedPlans ? "getdaq.remote-plan-retirement" : "getdaq.remote-control-handoff")+
            ",\"original_scope_sha256\":"+EncodeJsonString(p.scope)+",\"original_epoch\":"+EncodeJsonString(p.epoch)+
            ",\"successor_scope_sha256\":"+EncodeJsonString(first.successorControllerSnapshotSha256)+",\"successor_epoch\":"+EncodeJsonString(first.successorControllerEpoch)+
            ",\"audit_directory\":"+EncodeJsonString(p.handoffProof->path)+",\"nodes\":"+nodes+"]}\n";
        record("{\"phase\":\"complete\",\"receipt_sha256\":"+EncodeJsonString(ConfigurationSha256(receipt))+",\"original_control_remains_fenced\":true,\"raw_data_forwarded\":false}");
        p.handoffCompleteJson=receipt;p.Latch("Original handoff explicitly completed; original manager remains permanently fenced");
    } catch(...) {
        p.Latch("Explicit handoff completion failed; preserve original requests and completed nodes, no automatic resend");
        try {record("{\"phase\":\"unknown\",\"automatic_retry\":false,\"original_control_remains_fenced\":true}");}catch(...) {}
        throw;
    }
}
std::string RemoteReceiverManager::CompletedControlHandoffJson() const {
    auto &p=*fImpl;
    std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Remote controller handoff observation busy");
    p.CheckRoot();
    if (p.handoffCompleteJson.empty()) throw std::runtime_error("No complete original remote control handoff proof");
    return p.handoffCompleteJson;
}
void RemoteReceiverManager::AdoptControlHandoff(const std::string &bytes) {
    auto &p=*fImpl;
    std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Successor handoff binding busy; not queued");
    p.Gate(true);p.CheckRoot();
    if (p.validated || p.handoffAttempted || !p.predecessorHandoffJson.empty() || !p.proofs.empty() ||
        std::any_of(p.peers.begin(),p.peers.end(),[](const std::unique_ptr<Impl::Peer> &peer){return peer->hasPlan || peer->hasCycle;}))
        throw std::runtime_error("Only a fresh bound successor manager can adopt an explicit handoff");
    if (bytes.empty() || bytes.size()>1024*1024) throw std::invalid_argument("Handoff receipt exceeds 1MiB");
    JsonLimits limits;limits.stringBytes=8192;
    const auto receipt=ParseStrictJson(bytes,limits);
    ExactSupervisorFields(receipt,{"schema","kind","original_scope_sha256","original_epoch","successor_scope_sha256","successor_epoch","audit_directory","nodes"});
    const auto kind=String(receipt.At("kind"));
    if (Unsigned(receipt.At("schema"))!=1 || (kind!="getdaq.remote-control-handoff" && kind!="getdaq.remote-plan-retirement") ||
        String(receipt.At("successor_scope_sha256"))!=p.scope || String(receipt.At("successor_epoch"))!=p.epoch)
        throw std::invalid_argument("Explicit handoff is not addressed to this bound successor");
    Digest(String(receipt.At("original_scope_sha256")));Digest(String(receipt.At("original_epoch")));
    Path(String(receipt.At("audit_directory")));
    const auto &nodes=receipt.At("nodes");
    if (nodes.type!=JsonValue::Array || nodes.items.size()!=p.peers.size()) throw std::invalid_argument("Handoff must cover every exact data agent");
    std::set<std::string> ids;
    for (const auto &node:nodes.items) {
        std::set<std::string> nodeFields={"agent_id","instance_id","operation_id","request_key","proof_sha256","request"};
        if (node.members.count("completion_operation_id")) nodeFields.insert("completion_operation_id");
        if (node.members.count("completion_request")) nodeFields.insert("completion_request");
        if (node.members.count("completion_operation_id")!=node.members.count("completion_request"))
            throw std::invalid_argument("Handoff receipt has incomplete explicit completion binding");
        ExactSupervisorFields(node,nodeFields);
        if (!ids.insert(String(node.At("agent_id"))).second) throw std::invalid_argument("Duplicate data agent in handoff receipt");
        Digest(String(node.At("instance_id")));Digest(String(node.At("request_key")));Digest(String(node.At("proof_sha256")));
        SupervisorOperationId(String(node.At("operation_id")));
        const auto request=DecodeSupervisorControlRequest(String(node.At("request")));
        if(node.members.count("completion_operation_id")) {
            const auto id=String(node.At("completion_operation_id")),bytes=String(node.At("completion_request"));
            if(id.empty()!=bytes.empty())throw std::invalid_argument("Handoff receipt completion identity/request pair differs");
            if(!id.empty()) {
                SupervisorOperationId(id);const auto completion=DecodeSupervisorControlRequest(bytes);
                if(request.command!=SupervisorCommand::HandoffControl || completion.command!=SupervisorCommand::CompleteControlHandoff ||
                    completion.operationId!=id || EncodeSupervisorControlRequest(OriginalControlHandoffRequest(completion))!=EncodeSupervisorControlRequest(request))
                    throw std::invalid_argument("Handoff receipt explicit completion does not match its original request");
            }
        }
    }
    for (const auto &peer:p.peers) if (!ids.count(peer->endpoint.agentId)) throw std::invalid_argument("Handoff data agent set differs from successor");
    try {
        const auto path=p.rootPath+"/controller-handoff.json";
        WriteNew(path,bytes);
        p.predecessorHandoffFd.value=::open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
        if (p.predecessorHandoffFd.value<0 || ::fstat(p.predecessorHandoffFd.value,&p.predecessorHandoffIdentity) ||
            ::flock(p.predecessorHandoffFd.value,LOCK_EX|LOCK_NB))
            throw std::runtime_error("Cannot exclusively bind successor handoff receipt");
        p.predecessorHandoff=receipt;p.predecessorHandoffJson=bytes;p.predecessorChangedPlans=kind=="getdaq.remote-plan-retirement";p.CheckRoot();
    } catch (...) {p.Latch("Successor handoff binding failed; preserve receipt and do not retry");throw;}
}

} // namespace GetDAQ
