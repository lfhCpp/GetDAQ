#include "remotereceivermanager_p.h"
#include <stdexcept>

namespace GetDAQ {
using namespace RemoteReceiverManagerInternal;

std::unique_ptr<RemoteReceiverManager::Impl::AuditProof>
RemoteReceiverManager::Impl::PersistMutationIntent(const std::string &path,bool handoff,
    const std::vector<SupervisorControlRequest> &requests) {
    std::unique_ptr<AuditProof> proof(new AuditProof);
    proof->path=path;proof->directory.reset(new Directory(path));
    std::string intent="{\"schema\":1,\"kind\":\"getdaq.remote-receiver-intent\",\"binding_sha256\":"+
        EncodeJsonString(binding)+",\"requests\":[";
    for(std::size_t i=0;i<requests.size();++i) {
        if(i)intent+=',';
        intent+=EncodeSupervisorControlRequest(requests[i]);
    }
    intent+="]}\n";proof->intent=intent;WriteNew(path+"/intent.json",intent);
    if(::lstat((path+"/intent.json").c_str(),&proof->intentIdentity))
        throw std::runtime_error("Cannot bind remote intent file");
    if(handoff) {
        std::string context="{\"schema\":1,\"kind\":\"getdaq.remote-handoff-context\",\"binding_sha256\":"+
            EncodeJsonString(binding)+",\"original_scope_sha256\":"+EncodeJsonString(scope)+
            ",\"original_epoch\":"+EncodeJsonString(epoch)+",\"audit_directory\":"+
            EncodeJsonString(path)+",\"intent_sha256\":"+EncodeJsonString(ConfigurationSha256(intent))+",\"nodes\":[";
        for(std::size_t i=0;i<peers.size();++i) {
            const auto &peer=*peers[i];Stopped(peer,peer.observed);
            if(!peer.hasCycle || peer.retiredCycle || peer.observed.state!=DataSupervisorStatus::Stopped)
                throw std::runtime_error("Handoff context requires every retained normally sealed original cycle");
            if(i)context+=',';
            context+="{\"agent_id\":"+EncodeJsonString(peer.endpoint.agentId)+
                ",\"instance_id\":"+EncodeJsonString(peer.instance)+
                ",\"plan_sha256\":"+EncodeJsonString(peer.endpoint.planSha256)+
                ",\"lifecycle\":"+EncodeSupervisorStatus(peer.observed)+"}";
        }
        context+="]}\n";
        if(context.size()>1024*1024)
            throw std::runtime_error("Original handoff context exceeds 1MiB; no request sent");
        WriteNew(path+"/handoff-context.json",context);proof->handoffContext=context;
        if(::lstat((path+"/handoff-context.json").c_str(),&proof->handoffContextIdentity))
            throw std::runtime_error("Cannot bind original handoff context");
    }
    proof->audit.reset(new HardwareAudit(path+"/remote.jsonl"));
    if(::lstat((path+"/remote.jsonl").c_str(),&proof->journalIdentity))
        throw std::runtime_error("Cannot bind remote result journal");
    return proof;
}

} // namespace GetDAQ
