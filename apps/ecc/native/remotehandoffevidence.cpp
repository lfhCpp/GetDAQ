#include "remotehandoffevidence_p.h"
#include "remotehandoffevidence_delivery_p.h"
#include "planio.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <stdexcept>
namespace GetDAQ {
using namespace RemoteEvidenceInternal;
class RemoteHandoffEvidence::Impl {
public:
    std::string directory,epoch,intentSha,binding,scope;
    std::vector<RemoteHandoffNodeEvidence> nodes;
    std::vector<std::unique_ptr<SupervisorIO::Directory>> directories;
    std::vector<std::unique_ptr<EvidenceFile>> files;
    std::vector<std::pair<const SupervisorIO::Directory *,std::vector<std::string>>> listings;
    std::size_t totalBytes=0;
    mutable bool faulted=false;
    const std::string &File(const std::string &path) {
        Require(totalBytes<64*1024*1024,"Handoff evidence exceeds total metadata budget");
        std::unique_ptr<EvidenceFile> file(new EvidenceFile(path));totalBytes+=file->bytes.size();
        Require(totalBytes<=64*1024*1024,"Handoff evidence exceeds total metadata budget");
        files.push_back(std::move(file));return files.back()->bytes;
    }
    SupervisorIO::Directory &Dir(const std::string &path) {
        directories.emplace_back(new SupervisorIO::Directory(path));return *directories.back();
    }
    // A later uncertain attempt always wins over earlier explicit rejections.
    // This is historical classification only, not permission to send a request.

    Impl(const std::string &path,const std::string &expectedBinding,const std::string &expectedScope) : directory(path),binding(expectedBinding),scope(expectedScope) {
        Path(path);Digest(binding);Digest(scope);
        const auto operation=Parent(path),operations=Parent(operation),root=Parent(operations);
        Require(operations.substr(operations.find_last_of('/')+1)=="operations","Handoff evidence is outside a parent controller operations root");
        SupervisorOperationId(operation.substr(operation.find_last_of('/')+1));Dir(root);auto &all=Dir(operations);Dir(operation);Dir(path);
        const auto &snapshot=File(root+"/snapshot.json");
        Require(ConfigurationSha256("getdaq.data-controller:1\n"+ConfigurationSha256(snapshot)+'\n'+root)==scope,"Handoff evidence parent snapshot/root differs");
        const auto &intentBytes=File(path+"/intent.json");intentSha=ConfigurationSha256(intentBytes);const auto intent=ParseStrictJson(intentBytes);
        ExactSupervisorFields(intent,{"schema","kind","binding_sha256","requests"});
        Require(Unsigned(intent.At("schema"))==1 && String(intent.At("kind"))=="getdaq.remote-receiver-intent" && String(intent.At("binding_sha256"))==binding,"Original handoff intent binding differs");
        const auto contextBytes=File(path+"/handoff-context.json");const auto context=ParseStrictJson(contextBytes);
        ExactSupervisorFields(context,{"schema","kind","binding_sha256","original_scope_sha256","original_epoch","audit_directory","intent_sha256","nodes"});
        Require(Unsigned(context.At("schema"))==1 && String(context.At("kind"))=="getdaq.remote-handoff-context" &&
            String(context.At("binding_sha256"))==binding && String(context.At("original_scope_sha256"))==scope &&
            String(context.At("audit_directory"))==path && String(context.At("intent_sha256"))==intentSha,"Original handoff context binding differs");
        epoch=String(context.At("original_epoch"));Digest(epoch);const auto &entries=context.At("nodes"),&requests=intent.At("requests");
        Require(entries.type==JsonValue::Array && requests.type==JsonValue::Array && !entries.items.empty() && entries.items.size()<=195 && entries.items.size()==requests.items.size(),"Original handoff lacks exact node set");
        std::string previous;std::set<std::string> operationsSeen;
        for(std::size_t i=0;i<entries.items.size();++i) {
            const auto &entry=entries.items[i];ExactSupervisorFields(entry,{"agent_id","instance_id","plan_sha256","lifecycle"});RemoteHandoffNodeEvidence node;
            node.agentId=String(entry.At("agent_id"));Text(node.agentId,128);Require(!node.agentId.empty() && (i==0 || node.agentId>previous),"Handoff nodes must be sorted and unique");previous=node.agentId;
            node.instanceId=String(entry.At("instance_id"));node.planSha256=String(entry.At("plan_sha256"));Digest(node.instanceId);Digest(node.planSha256);
            node.stopped=DecodeSupervisorStatus(entry.At("lifecycle"));node.request=DecodeSupervisorControlRequest(EncodeValue(requests.items[i]));
            Require((node.request.command==SupervisorCommand::RetirePlan || node.request.command==SupervisorCommand::HandoffControl) &&
                (i==0 || node.request.command==nodes[0].request.command) && operationsSeen.insert(node.request.operationId).second && node.request.instanceId==node.instanceId &&
                node.request.planSha256==node.planSha256 && node.request.controllerSnapshotSha256==scope && node.request.controllerEpoch==epoch && node.request.parentAuditReference==path &&
                node.stopped.planSha256==node.planSha256 && node.stopped.state==DataSupervisorStatus::Stopped && !node.stopped.needsReconciliation && !node.stopped.receivers.empty(),"Original handoff request/cycle differs");
            Require(node.request.operationId==ConfigurationSha256("getdaq.remote-data-operation:1\n"+scope+'\n'+epoch+'\n'+path+'\n'+SupervisorCommandName(node.request.command)+'\n'+node.agentId),"Original handoff operation/agent binding differs");
            if(i)Require(node.request.controllerId==nodes[0].request.controllerId &&
                node.request.successorControllerSnapshotSha256==nodes[0].request.successorControllerSnapshotSha256 &&
                node.request.successorControllerEpoch==nodes[0].request.successorControllerEpoch,"Original handoff nodes disagree on successor");
            Digest(node.stopped.sessionId);
            if(node.request.command==SupervisorCommand::RetirePlan)Require(node.request.expectedSessionId==node.stopped.sessionId,"Original handoff session differs");
            for(const auto &receiver:node.stopped.receivers)Require(receiver.pid>0 && !receiver.outputDirectory.empty() && receiver.sessionId==node.stopped.sessionId &&
                receiver.reaped && receiver.exitCode==0 && receiver.sealVerified,"Original handoff lacks a normal receiver seal");
            nodes.push_back(node);
        }
        const auto rows=Journal(File(path+"/remote.jsonl"));Require(!rows.empty(),"Handoff context was not committed before submission");
        ExactSupervisorFields(rows.front(),{"phase","binding_sha256","handoff_context_sha256","automatic_retry"});
        Require(String(rows.front().At("phase"))=="intent" && String(rows.front().At("binding_sha256"))==binding && !Boolean(rows.front().At("automatic_retry")) &&
            String(rows.front().At("handoff_context_sha256"))==ConfigurationSha256(contextBytes),"Handoff context was not committed before submission");ApplyDeliveryHistory(nodes,rows,true);
        const auto names=Names(all);
        for(const auto &name:names) {
            auto &parent=Dir(operations+'/'+name);const auto actions=Names(parent);
            for(const auto &action:actions) {
                const auto candidate=operations+'/'+name+'/'+action;struct stat st;Require(!::lstat(candidate.c_str(),&st) && !S_ISLNK(st.st_mode),"Handoff evidence contains a replaced path");
                if(!S_ISDIR(st.st_mode) || candidate==path)continue;
                Dir(candidate);
                const auto file=candidate+"/intent.json";if(::lstat(file.c_str(),&st)) {Require(errno==ENOENT,"Cannot inspect possible completion intent");continue;}
                const auto value=ParseStrictJson(File(file));const auto kind=value.members.find("kind");
                if(kind==value.members.end() || String(kind->second)!="getdaq.remote-handoff-completion-intent")continue;
                if(String(value.At("original_audit_directory"))!=path)continue;
                ExactSupervisorFields(value,{"schema","kind","binding_sha256","original_audit_directory","original_intent_sha256","requests"});
                Require(Unsigned(value.At("schema"))==1 && String(value.At("binding_sha256"))==binding && String(value.At("original_intent_sha256"))==intentSha,"Later handoff completion binding differs");
                const auto &attemptRequests=value.At("requests");Require(attemptRequests.items.size()==nodes.size(),"Later handoff completion changed nodes");
                for(std::size_t i=0;i<nodes.size();++i)Require(EncodeValue(attemptRequests.items[i])==EncodeSupervisorControlRequest(nodes[i].request),"Later handoff completion changed original requests");
                const auto attemptRows=Journal(File(candidate+"/remote.jsonl"));
                if(!attemptRows.empty()) {
                    ExactSupervisorFields(attemptRows.front(),{"phase","automatic_retry","original_control_remains_fenced"});
                    Require(String(attemptRows.front().At("phase"))=="intent" && !Boolean(attemptRows.front().At("automatic_retry")) &&
                        Boolean(attemptRows.front().At("original_control_remains_fenced")),"Later completion lacks its original fenced intent");
                }
                ApplyDeliveryHistory(nodes,attemptRows,false);
            }
            Require(Names(parent)==actions,"Handoff evidence changed during inspection");listings.push_back(std::make_pair(&parent,actions));
        }
        Require(Names(all)==names,"Handoff operations changed during inspection");listings.push_back(std::make_pair(&all,names));Check();
    }
    static std::string EncodeValue(const JsonValue &value) {
        // Supervisor requests have a small, fixed field set. Preserve the
        // protocol's canonical field order through its strict decoder.
        std::string json="{";for(const auto &item:value.members) {
            if(json.size()>1)json+=',';
            json+=EncodeJsonString(item.first)+':';
            if(item.second.type==JsonValue::String)json+=EncodeJsonString(item.second.text);
            else if(item.second.type==JsonValue::Number || item.second.type==JsonValue::Boolean)json+=item.second.text;
            else throw std::runtime_error("Unsupported field in original handoff request");
        }
        const auto request=DecodeSupervisorControlRequest(json+'}');return EncodeSupervisorControlRequest(request);
    }
    void Check() const {
        Require(!faulted,"Handoff evidence fault remains latched");
        try {
        for(const auto &directory:directories)directory->Check();
        for(const auto &file:files)file->Check();
            for(const auto &listing:listings)Require(Names(*listing.first)==listing.second,"Handoff attempt set changed; inspect fresh evidence");
        }
        catch(...){faulted=true;throw;}
    }
};
RemoteHandoffEvidence::RemoteHandoffEvidence(const std::string &path,const std::string &binding,const std::string &scope) : fImpl(new Impl(path,binding,scope)) {}
RemoteHandoffEvidence::~RemoteHandoffEvidence() {}
void RemoteHandoffEvidence::Check() const {fImpl->Check();}
const std::string &RemoteHandoffEvidence::Directory() const {Check();return fImpl->directory;}
const std::string &RemoteHandoffEvidence::Epoch() const {Check();return fImpl->epoch;}
const std::string &RemoteHandoffEvidence::IntentSha256() const {Check();return fImpl->intentSha;}
const std::vector<RemoteHandoffNodeEvidence> &RemoteHandoffEvidence::Nodes() const {Check();return fImpl->nodes;}


}
