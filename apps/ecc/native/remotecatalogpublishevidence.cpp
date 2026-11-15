#include "remotehandoffevidence_p.h"
#include "planio.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace GetDAQ {
using namespace RemoteEvidenceInternal;
class RemoteCatalogPublishEvidence::Impl {
public:
    std::string directory,epoch,intentSha,experiment,replacementId,binding,scope;
    unsigned int run=0;
    std::vector<RemoteCatalogPublishNodeEvidence> nodes;
    std::vector<std::unique_ptr<SupervisorIO::Directory>> directories;
    std::vector<std::unique_ptr<EvidenceFile>> files;
    std::size_t totalBytes=0;
    mutable bool faulted=false;
    const std::string &File(const std::string &path) {
        Require(totalBytes<4*1024*1024,"Catalog Publish evidence exceeds metadata budget");
        std::unique_ptr<EvidenceFile> file(new EvidenceFile(path));totalBytes+=file->bytes.size();
        Require(totalBytes<=4*1024*1024,"Catalog Publish evidence exceeds metadata budget");
        files.push_back(std::move(file));return files.back()->bytes;
    }
    SupervisorIO::Directory &Dir(const std::string &path) {
        directories.emplace_back(new SupervisorIO::Directory(path));return *directories.back();
    }
    static SupervisorControlRequest Request(const JsonValue &value) {
        Require(value.type==JsonValue::Object,"Publish request is not a JSON object");
        std::string json="{";
        for(const auto &field:value.members) {
            if(json.size()>1)json+=',';
            json+=EncodeJsonString(field.first)+':';
            if(field.second.type==JsonValue::String)json+=EncodeJsonString(field.second.text);
            else if(field.second.type==JsonValue::Number || field.second.type==JsonValue::Boolean)json+=field.second.text;
            else if(field.second.type==JsonValue::Array) {
                json+='[';for(const auto &item:field.second.items) {if(json.back()!='[')json+=',';Require(item.type==JsonValue::String,"Publish request array contains non-string");json+=EncodeJsonString(item.text);}json+=']';
            } else throw std::runtime_error("Publish request contains an unsupported JSON field");
        }
        return DecodeSupervisorControlRequest(json+'}');
    }
    Impl(const std::string &path,const std::string &expectedBinding,const std::string &expectedScope,
         const std::vector<std::string> &agentIds) : directory(path),binding(expectedBinding),scope(expectedScope) {
        Path(path);Digest(binding);Digest(scope);
        Require(agentIds.size()>0 && agentIds.size()<=195,"Catalog Publish evidence requires the fixed bounded node set");
        std::set<std::string> unique;
        for(const auto &agent:agentIds) {Text(agent,128);Require(!agent.empty() && unique.insert(agent).second,"Catalog Publish fixed node IDs are empty or duplicate");}
        Require(std::is_sorted(agentIds.begin(),agentIds.end()),"Catalog Publish fixed node IDs must be canonical/sorted");
        const auto operation=Parent(path),operations=Parent(operation),root=Parent(operations);
        Require(operations.substr(operations.find_last_of('/')+1)=="operations" &&
            path.size()>std::string("_publish").size() && path.compare(path.size()-std::string("_publish").size(),std::string("_publish").size(),"_publish")==0,
            ("Catalog Publish evidence is outside its canonical parent operation path: "+path).c_str());
        SupervisorOperationId(operation.substr(operation.find_last_of('/')+1));
        Dir(root);Dir(operations);Dir(operation);Dir(path);
        const auto &snapshot=File(root+"/snapshot.json");
        Require(ConfigurationSha256("getdaq.data-controller:1\n"+ConfigurationSha256(snapshot)+'\n'+root)==scope,"Catalog Publish controller root/snapshot differs");
        const auto &intentBytes=File(path+"/intent.json");intentSha=ConfigurationSha256(intentBytes);const auto intent=ParseStrictJson(intentBytes);
        ExactSupervisorFields(intent,{"schema","kind","binding_sha256","requests"});
        Require(Unsigned(intent.At("schema"))==1 && String(intent.At("kind"))=="getdaq.remote-receiver-intent" &&
            String(intent.At("binding_sha256"))==binding && intent.At("requests").type==JsonValue::Array &&
            intent.At("requests").items.size()==agentIds.size(),"Catalog Publish intent differs from the fixed binding/node count");
        const auto &requests=intent.At("requests").items;
        std::vector<SupervisorControlRequest> decodedRequests;std::string controllerId;
        for(std::size_t i=0;i<requests.size();++i) {
            const auto request=Request(requests[i]);RemoteCatalogPublishNodeEvidence node;node.agentId=agentIds[i];
            node.instanceId=request.instanceId;node.planSha256=request.planSha256;node.operationId=request.operationId;
            node.requestKey=SupervisorControlRequestKey(request);node.request=request;
            Require(request.command==SupervisorCommand::Catalog && request.catalogPhase=="publish" && request.catalogReplaceExisting &&
                request.parentAuditReference==path && request.controllerSnapshotSha256==scope && !request.controllerEpoch.empty(),
                "Catalog Publish request does not bind replacement, parent root, and epoch");
            Digest(request.instanceId);Digest(request.planSha256);Digest(request.controllerEpoch);
            if(i==0)controllerId=request.controllerId;
            Require(request.controllerId==controllerId,"Catalog Publish nodes disagree on the fixed controller ID");
            Require(request.operationId==ConfigurationSha256("getdaq.remote-data-operation:1\n"+scope+'\n'+request.controllerEpoch+'\n'+path+"\ncatalog\n"+agentIds[i]),
                "Catalog Publish operation ID is not bound to its fixed node");
            if(i==0) {epoch=request.controllerEpoch;experiment=request.catalogExperiment;run=request.catalogRunNumber;}
            Require(request.controllerEpoch==epoch && request.catalogExperiment==experiment && request.catalogRunNumber==run,
                "Catalog Publish nodes disagree on the run replacement identity");
            nodes.push_back(node);decodedRequests.push_back(request);
        }
        const std::string basePath=path.substr(0,path.size()-std::string("_publish").size());
        replacementId=ConfigurationSha256("getdaq.remote-catalog-replacement:1\n"+scope+'\n'+epoch+'\n'+basePath+'\n'+experiment+'\n'+std::to_string(run));
        for(std::size_t i=0;i<decodedRequests.size();++i)
            Require(decodedRequests[i].catalogReplacementId==ConfigurationSha256("getdaq.remote-catalog-replacement-node:1\n"+replacementId+'\n'+agentIds[i]),
                "Catalog Publish replacement ID differs from its fixed node identity");
        const auto journal=Journal(File(path+"/remote.jsonl"));
        Require(!journal.empty(),"Catalog Publish journal is empty");
        const auto &head=journal.front();ExactSupervisorFields(head,{"phase","binding_sha256","automatic_retry"});
        Require(String(head.At("phase"))=="intent" && String(head.At("binding_sha256"))==binding && !Boolean(head.At("automatic_retry")),
            "Catalog Publish journal lacks its committed fixed-binding intent");
        std::size_t next=0;bool ended=false,failed=false;std::vector<unsigned int> state(nodes.size(),0);
        std::vector<bool> catalog(nodes.size(),false),accepted(nodes.size(),false),rejected(nodes.size(),false);
        for(std::size_t rowIndex=1;rowIndex<journal.size();++rowIndex) {
            const auto &row=journal[rowIndex];const auto phase=String(row.At("phase"));Require(!ended,"Catalog Publish journal continues after terminal marker");
            if(phase=="before_post") {
                ExactSupervisorFields(row,{"phase","agent_id","operation_id","request_key"});
                Require(!failed && next<nodes.size() && state[next]==0 && String(row.At("agent_id"))==nodes[next].agentId &&
                    String(row.At("operation_id"))==nodes[next].operationId && String(row.At("request_key"))==nodes[next].requestKey,
                    "Catalog Publish submission marker differs from fixed node order/request");state[next]=1;
            } else if(phase=="explicit_rejection") {
                ExactSupervisorFields(row,{"phase","operation_id","request_key","http_status"});
                const auto code=Unsigned(row.At("http_status"));
                Require(next<nodes.size() && state[next]==1 && !accepted[next] && !rejected[next] && (code==409 || code==412) &&
                    String(row.At("operation_id"))==nodes[next].operationId && String(row.At("request_key"))==nodes[next].requestKey,
                    "Publish rejection is not bound to its exact earlier POST");rejected[next]=true;
            } else if(phase=="accepted_reply") {
                ExactSupervisorFields(row,{"phase","operation_id"});
                Require(next<nodes.size() && state[next]==1 && !accepted[next] && !rejected[next] &&
                    String(row.At("operation_id"))==nodes[next].operationId,"Publish accepted reply lacks its exact earlier POST");accepted[next]=true;
            } else if(phase=="catalog_complete") {
                ExactSupervisorFields(row,{"phase","agent_id","catalog"});
                Require(next<nodes.size() && state[next]==1 && accepted[next] && !catalog[next] && String(row.At("agent_id"))==nodes[next].agentId &&
                    row.At("catalog").type==JsonValue::Object,"Catalog receipt is out of order or unbound");catalog[next]=true;
            } else if(phase=="node_complete") {
                ExactSupervisorFields(row,{"phase","agent_id","result"});
                Require(next<nodes.size() && state[next]==1 && accepted[next] && catalog[next] && String(row.At("agent_id"))==nodes[next].agentId,
                    "Completed Publish node lacks preceding bound receipt");state[next]=2;nodes[next].outcome="complete";++next;
            } else if(phase=="node_unknown") {
                ExactSupervisorFields(row,{"phase","agent_id","operation_id"});
                Require(next<nodes.size() && state[next]==1 && String(row.At("agent_id"))==nodes[next].agentId &&
                    String(row.At("operation_id"))==nodes[next].operationId,"Unknown Publish node differs from submitted fixed request");
                state[next]=3;nodes[next].outcome="unknown";++next;failed=true;
            } else if(phase=="node_not_attempted") {
                ExactSupervisorFields(row,{"phase","agent_id","operation_id","reason","post_sent"});
                Require(failed && next<nodes.size() && state[next]==0 && String(row.At("agent_id"))==nodes[next].agentId &&
                    String(row.At("operation_id"))==nodes[next].operationId && !Boolean(row.At("post_sent")),
                    "Unattempted Publish node has no preceding failure or claims a sent request");
                const auto reason=String(row.At("reason"));Require(reason=="prior_publish_uncertain" || reason=="prior_finalize_uncertain","Unknown unattempted-node reason");
                state[next]=4;nodes[next].outcome="not_attempted";++next;
            } else if(phase=="unknown") {
                ExactSupervisorFields(row,{"phase","automatic_retry","automatic_cleanup"});
                Require(!Boolean(row.At("automatic_retry")) && !Boolean(row.At("automatic_cleanup")),"Unknown terminal event authorizes automatic action");ended=true;
            } else if(phase=="complete") {
                ExactSupervisorFields(row,{"phase","raw_data_forwarded"});
                Require(!Boolean(row.At("raw_data_forwarded")) && next==nodes.size() &&
                    std::all_of(state.begin(),state.end(),[](unsigned int value){return value==2;}),"Complete marker lacks every fixed Publish receipt");ended=true;
            } else throw std::runtime_error("Unknown catalog Publish journal phase: "+phase);
        }
        Require(ended,"Catalog Publish journal lacks a terminal classification");
        for(std::size_t i=0;i<nodes.size();++i) if(nodes[i].outcome.empty())nodes[i].outcome="not_started";
        Check();
    }
    void Check() const {
        Require(!faulted,"Catalog Publish evidence fault remains latched");
        try {for(const auto &directory:directories)directory->Check();for(const auto &file:files)file->Check();}
        catch(...) {faulted=true;throw;}
    }
};
RemoteCatalogPublishEvidence::RemoteCatalogPublishEvidence(const std::string &path,const std::string &binding,const std::string &scope,
    const std::vector<std::string> &agentIds) : fImpl(new Impl(path,binding,scope,agentIds)) {}
RemoteCatalogPublishEvidence::~RemoteCatalogPublishEvidence() {}
void RemoteCatalogPublishEvidence::Check() const {fImpl->Check();}
const std::string &RemoteCatalogPublishEvidence::Directory() const {Check();return fImpl->directory;}
const std::string &RemoteCatalogPublishEvidence::Epoch() const {Check();return fImpl->epoch;}
const std::string &RemoteCatalogPublishEvidence::IntentSha256() const {Check();return fImpl->intentSha;}
const std::string &RemoteCatalogPublishEvidence::Experiment() const {Check();return fImpl->experiment;}
unsigned int RemoteCatalogPublishEvidence::RunNumber() const {Check();return fImpl->run;}
const std::string &RemoteCatalogPublishEvidence::ReplacementId() const {Check();return fImpl->replacementId;}
const std::vector<RemoteCatalogPublishNodeEvidence> &RemoteCatalogPublishEvidence::Nodes() const {Check();return fImpl->nodes;}
}
