#include "remotehandoffevidence.h"
#include "supervisorio.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
namespace {
using namespace GetDAQ;namespace IO=SupervisorIO;
void Require(bool value,const char *message) {if(!value)throw std::runtime_error(message);}
void Replace(const std::string &path,const std::string &bytes) {std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<bytes;file.close();Require(bool(file),"Test evidence replacement failed");}
unsigned int passed=0;
void Reject(const std::string &name,const std::function<void()> &action) {
    bool rejected=false;try{action();}catch(const std::exception &){rejected=true;}Require(rejected,name.c_str());++passed;std::cout<<"PASS "<<name<<'\n';
}
struct Fixture {
    std::string root,path,binding=std::string(64,'b'),scope,epoch=std::string(64,'e'),intent,context;
    SupervisorControlRequest request;
    explicit Fixture(const std::string &directory) : root(directory),path(root+"/operations/original/data-handoff") {
        IO::CreateDirectory(root);IO::CreateDirectory(root+"/operations");IO::CreateDirectory(root+"/operations/original");IO::CreateDirectory(path);
        const std::string snapshot="{\"synthetic_test_only\":true}\n";IO::WriteNew(root+"/snapshot.json",snapshot);
        scope=ConfigurationSha256("getdaq.data-controller:1\n"+ConfigurationSha256(snapshot)+'\n'+root);
        request.command=SupervisorCommand::RetirePlan;request.controllerId="test-controller";request.controllerSnapshotSha256=scope;request.controllerEpoch=epoch;
        request.operationId=ConfigurationSha256("getdaq.remote-data-operation:1\n"+scope+'\n'+epoch+'\n'+path+"\nretire-plan\nnode-0");
        request.planSha256=std::string(64,'a');request.instanceId=std::string(64,'c');request.configurationSha256=std::string(64,'d');request.artifactSha256=std::string(64,'f');
        request.parentAuditReference=path;request.successorControllerSnapshotSha256=std::string(64,'1');request.successorControllerEpoch=std::string(64,'2');
        request.successorPlanFile="/synthetic/next-plan.json";request.successorPlanSha256=std::string(64,'3');request.expectedSessionId=std::string(64,'4');
        intent="{\"schema\":1,\"kind\":\"getdaq.remote-receiver-intent\",\"binding_sha256\":"+EncodeJsonString(binding)+",\"requests\":["+EncodeSupervisorControlRequest(request)+"]}\n";
        IO::WriteNew(path+"/intent.json",intent);
        DataSupervisorStatus stopped;stopped.state=DataSupervisorStatus::Stopped;stopped.planSha256=request.planSha256;stopped.sessionId=request.expectedSessionId;
        ManagedReceiverStatus receiver;receiver.id="C0";receiver.sessionId=stopped.sessionId;receiver.outputDirectory="/synthetic/no-raw-payload";receiver.pid=123;
        receiver.exitCode=0;receiver.reaped=receiver.sealVerified=true;stopped.receivers.push_back(receiver);
        context="{\"schema\":1,\"kind\":\"getdaq.remote-handoff-context\",\"binding_sha256\":"+EncodeJsonString(binding)+",\"original_scope_sha256\":"+EncodeJsonString(scope)+
            ",\"original_epoch\":"+EncodeJsonString(epoch)+",\"audit_directory\":"+EncodeJsonString(path)+",\"intent_sha256\":"+EncodeJsonString(ConfigurationSha256(intent))+
            ",\"nodes\":[{\"agent_id\":\"node-0\",\"instance_id\":"+EncodeJsonString(request.instanceId)+",\"plan_sha256\":"+EncodeJsonString(request.planSha256)+",\"lifecycle\":"+EncodeSupervisorStatus(stopped)+"}]}\n";
        IO::WriteNew(path+"/handoff-context.json",context);IO::WriteNew(path+"/remote.jsonl",Journal());
    }
    std::string Start() const {return "{\"phase\":\"intent\",\"binding_sha256\":"+EncodeJsonString(binding)+",\"handoff_context_sha256\":"+EncodeJsonString(ConfigurationSha256(context))+",\"automatic_retry\":false}\n";}
    std::string Submit(bool original) const {return "{\"phase\":"+EncodeJsonString(original ? "before_post" : "node_explicit_submit")+",\"agent_id\":\"node-0\",\"operation_id\":"+EncodeJsonString(request.operationId)+",\"request_key\":"+EncodeJsonString(SupervisorControlRequestKey(request))+"}\n";}
    std::string Rejected(unsigned int code=409) const {return "{\"phase\":\"explicit_rejection\",\"operation_id\":"+EncodeJsonString(request.operationId)+",\"request_key\":"+EncodeJsonString(SupervisorControlRequestKey(request))+",\"http_status\":"+std::to_string(code)+"}\n";}
    std::string Journal() const {return Start()+Submit(true)+Rejected()+"{\"phase\":\"unknown\"}\n";}
    void Attempt(const std::string &name,bool rejected) {
        const auto directory=root+"/operations/"+name;IO::CreateDirectory(directory);IO::CreateDirectory(directory+"/data-handoff");
        const auto intentBytes="{\"schema\":1,\"kind\":\"getdaq.remote-handoff-completion-intent\",\"binding_sha256\":"+EncodeJsonString(binding)+
            ",\"original_audit_directory\":"+EncodeJsonString(path)+",\"original_intent_sha256\":"+EncodeJsonString(ConfigurationSha256(intent))+",\"requests\":["+EncodeSupervisorControlRequest(request)+"]}\n";
        IO::WriteNew(directory+"/data-handoff/intent.json",intentBytes);
        IO::WriteNew(directory+"/data-handoff/remote.jsonl","{\"phase\":\"intent\",\"automatic_retry\":false,\"original_control_remains_fenced\":true}\n"+Submit(false)+(rejected ? Rejected(412) : "")+"{\"phase\":\"unknown\"}\n");
    }
    void Check(unsigned int expected,const char *name) const {
        RemoteHandoffEvidence evidence(path,binding,scope);Require(evidence.Nodes().size()==1 && evidence.Nodes()[0].rejection==expected && evidence.Epoch()==epoch,name);
        ++passed;std::cout<<"PASS "<<name<<'\n';
    }
};
struct CatalogFixture {
    std::string root,path,binding=std::string(64,'b'),scope,epoch=std::string(64,'e'),replacementId;
    std::vector<std::string> agents{"node-0","node-1"};std::vector<SupervisorControlRequest> requests;
    explicit CatalogFixture(const std::string &directory) : root(directory),path(root+"/operations/original-op/catalog_publish") {
        IO::CreateDirectory(root);IO::CreateDirectory(root+"/operations");IO::CreateDirectory(root+"/operations/original-op");IO::CreateDirectory(path);
        const std::string snapshot="{\"synthetic_test_only\":true}\n";IO::WriteNew(root+"/snapshot.json",snapshot);
        scope=ConfigurationSha256("getdaq.data-controller:1\n"+ConfigurationSha256(snapshot)+'\n'+root);
        const auto base=path.substr(0,path.size()-std::string("_publish").size());
        replacementId=ConfigurationSha256("getdaq.remote-catalog-replacement:1\n"+scope+'\n'+epoch+'\n'+base+"\nExp\n12");
        for(const auto &agent:agents) {
            SupervisorControlRequest request;request.command=SupervisorCommand::Catalog;request.controllerId="test-controller";
            request.controllerSnapshotSha256=scope;request.controllerEpoch=epoch;request.parentAuditReference=path;
            request.planSha256=std::string(64,'a');request.instanceId=std::string(64,'c');request.configurationSha256=std::string(64,'d');request.artifactSha256=std::string(64,'f');
            request.catalogExperiment="Exp";request.catalogRunNumber=12;request.catalogSessionId=std::string(64,'4');request.catalogReplaceExisting=true;
            request.catalogPhase="publish";request.catalogReplacementId=ConfigurationSha256("getdaq.remote-catalog-replacement-node:1\n"+replacementId+'\n'+agent);
            request.operationId=ConfigurationSha256("getdaq.remote-data-operation:1\n"+scope+'\n'+epoch+'\n'+path+"\ncatalog\n"+agent);
            ValidateSupervisorControlRequest(request);requests.push_back(request);
        }
        std::string intent="{\"schema\":1,\"kind\":\"getdaq.remote-receiver-intent\",\"binding_sha256\":"+EncodeJsonString(binding)+",\"requests\":[";
        for(std::size_t i=0;i<requests.size();++i){if(i)intent+=',';intent+=EncodeSupervisorControlRequest(requests[i]);}intent+="]}\n";
        IO::WriteNew(path+"/intent.json",intent);IO::WriteNew(path+"/remote.jsonl","");
    }
    std::string Head() const {return "{\"phase\":\"intent\",\"binding_sha256\":"+EncodeJsonString(binding)+",\"automatic_retry\":false}\n";}
    std::string Before(std::size_t i) const {return "{\"phase\":\"before_post\",\"agent_id\":"+EncodeJsonString(agents[i])+",\"operation_id\":"+EncodeJsonString(requests[i].operationId)+",\"request_key\":"+EncodeJsonString(SupervisorControlRequestKey(requests[i]))+"}\n";}
    std::string CompleteNode(std::size_t i) const {return "{\"phase\":\"accepted_reply\",\"operation_id\":"+EncodeJsonString(requests[i].operationId)+"}\n"+
        "{\"phase\":\"catalog_complete\",\"agent_id\":"+EncodeJsonString(agents[i])+",\"catalog\":{}}\n{\"phase\":\"node_complete\",\"agent_id\":"+EncodeJsonString(agents[i])+",\"result\":{}}\n";}
    std::string FailedFirst() const {return Head()+Before(0)+"{\"phase\":\"node_unknown\",\"agent_id\":\"node-0\",\"operation_id\":"+EncodeJsonString(requests[0].operationId)+"}\n"+
        "{\"phase\":\"node_not_attempted\",\"agent_id\":\"node-1\",\"operation_id\":"+EncodeJsonString(requests[1].operationId)+",\"reason\":\"prior_publish_uncertain\",\"post_sent\":false}\n"+
        "{\"phase\":\"unknown\",\"automatic_retry\":false,\"automatic_cleanup\":false}\n";}
    void CheckInterrupted() const {
        RemoteCatalogPublishEvidence evidence(path,binding,scope,agents);
        Require(evidence.Epoch()==epoch && evidence.Experiment()=="Exp" && evidence.RunNumber()==12 && evidence.ReplacementId()==replacementId &&
            evidence.Nodes().size()==2 && evidence.Nodes()[0].outcome=="unknown" && evidence.Nodes()[1].outcome=="not_attempted" &&
            EncodeSupervisorControlRequest(evidence.Nodes()[0].request)==EncodeSupervisorControlRequest(requests[0]) &&
            EncodeSupervisorControlRequest(evidence.Nodes()[1].request)==EncodeSupervisorControlRequest(requests[1]) &&
            evidence.Nodes()[0].requestKey==SupervisorControlRequestKey(evidence.Nodes()[0].request),
            "Interrupted Publish proof did not preserve the exact validated requests and classifications");++passed;std::cout<<"PASS exact Publish requests and node classification\n";
    }
};
}
int main(int argc,char **argv) {
    try {
        Require(argc==2,"Pass an owned empty metadata test root");const std::string root=argv[1];
        Fixture basic(root+"/basic");basic.Check(409,"durable explicit rejection");
        Reject("foreign binding",[&]{RemoteHandoffEvidence x(basic.path,std::string(64,'0'),basic.scope);});
        Reject("foreign scope",[&]{RemoteHandoffEvidence x(basic.path,basic.binding,std::string(64,'0'));});
        basic.Attempt("z-rejected",true);basic.Check(412,"later explicit rejection remains classified");
        RemoteHandoffEvidence before(basic.path,basic.binding,basic.scope);basic.Attempt("a-uncertain",false);
        Reject("new attempt invalidates old review",[&]{before.Check();});basic.Check(0,"uncertain attempt dominates later-sorted rejection");
        Fixture unsent(root+"/unsent");Replace(unsent.path+"/remote.jsonl",unsent.Start()+"{\"phase\":\"unknown\"}\n");unsent.Check(1,"no recorded submit");
        Fixture lost(root+"/lost");Replace(lost.path+"/remote.jsonl",lost.Start()+lost.Submit(true)+"{\"phase\":\"unknown\"}\n");lost.Check(0,"lost result never authorizes replay");
        Fixture truncated(root+"/truncated");Replace(truncated.path+"/remote.jsonl",truncated.Journal()+"{");
        Reject("partial journal",[&]{RemoteHandoffEvidence x(truncated.path,truncated.binding,truncated.scope);});
        Fixture orphan(root+"/orphan");Replace(orphan.path+"/remote.jsonl",orphan.Submit(true)+orphan.Rejected()+"{\"phase\":\"unknown\"}\n");
        Reject("context lacks earlier commitment",[&]{RemoteHandoffEvidence x(orphan.path,orphan.binding,orphan.scope);});
        Fixture missing(root+"/missing");Require(!::unlink((missing.path+"/handoff-context.json").c_str()),"Remove owned test context");
        Reject("legacy missing context is not invented",[&]{RemoteHandoffEvidence x(missing.path,missing.binding,missing.scope);});
        Fixture changed(root+"/changed");RemoteHandoffEvidence bound(changed.path,changed.binding,changed.scope);Replace(changed.path+"/handoff-context.json",changed.context+" ");
        Reject("context content drift",[&]{bound.Check();});Replace(changed.path+"/handoff-context.json",changed.context);Reject("observed fault remains latched",[&]{bound.Check();});
        Fixture badCode(root+"/bad-code");Replace(badCode.path+"/remote.jsonl",badCode.Start()+badCode.Submit(true)+badCode.Rejected(500)+"{\"phase\":\"unknown\"}\n");
        Reject("500 is not explicit nonacceptance",[&]{RemoteHandoffEvidence x(badCode.path,badCode.binding,badCode.scope);});
        Fixture early(root+"/early");Replace(early.path+"/remote.jsonl",early.Start()+early.Rejected()+"{\"phase\":\"unknown\"}\n");
        Reject("rejection without earlier submit marker",[&]{RemoteHandoffEvidence x(early.path,early.binding,early.scope);});
        Fixture duplicate(root+"/duplicate");Replace(duplicate.path+"/remote.jsonl",duplicate.Start()+duplicate.Submit(true)+duplicate.Submit(true)+"{\"phase\":\"unknown\"}\n");
        Reject("repeated submission evidence",[&]{RemoteHandoffEvidence x(duplicate.path,duplicate.binding,duplicate.scope);});
        Fixture falseComplete(root+"/false-complete");Replace(falseComplete.path+"/remote.jsonl",falseComplete.Start()+falseComplete.Submit(true)+"{\"phase\":\"complete\"}\n");
        Reject("claimed completion without node proofs",[&]{RemoteHandoffEvidence x(falseComplete.path,falseComplete.binding,falseComplete.scope);});
        Fixture badSeal(root+"/bad-seal");const auto at=badSeal.context.find("\"seal_verified\":true");Require(at!=std::string::npos,"Synthetic seal field absent");
        badSeal.context.replace(at,std::string("\"seal_verified\":true").size(),"\"seal_verified\":false");Replace(badSeal.path+"/handoff-context.json",badSeal.context);Replace(badSeal.path+"/remote.jsonl",badSeal.Journal());
        Reject("unsealed context is never a normal cycle",[&]{RemoteHandoffEvidence x(badSeal.path,badSeal.binding,badSeal.scope);});
        Fixture linked(root+"/linked");Require(!::link((linked.path+"/handoff-context.json").c_str(),(linked.root+"/context-alias.json").c_str()),"Create owned test hardlink");
        Reject("multiply linked evidence",[&]{RemoteHandoffEvidence x(linked.path,linked.binding,linked.scope);});
        Fixture trailing(root+"/trailing");Replace(trailing.path+"/remote.jsonl",trailing.Journal()+trailing.Submit(true));
        Reject("events after terminal outcome",[&]{RemoteHandoffEvidence x(trailing.path,trailing.binding,trailing.scope);});
        CatalogFixture catalog(root+"/catalog");Replace(catalog.path+"/remote.jsonl",catalog.FailedFirst());catalog.CheckInterrupted();
        CatalogFixture sent(root+"/catalog-sent");auto badUnsent=sent.FailedFirst();const auto postSent=badUnsent.find("\"post_sent\":false");
        Require(postSent!=std::string::npos,"Synthetic not-attempted marker absent");badUnsent.replace(postSent,std::string("\"post_sent\":false").size(),"\"post_sent\":true");
        Replace(sent.path+"/remote.jsonl",badUnsent);
        Reject("Publish audit cannot claim a sent operation was not attempted",[&]{RemoteCatalogPublishEvidence x(sent.path,sent.binding,sent.scope,sent.agents);});
        CatalogFixture order(root+"/catalog-order");Replace(order.path+"/remote.jsonl",order.Head()+order.Before(1));
        Reject("Publish audit enforces fixed node order",[&]{RemoteCatalogPublishEvidence x(order.path,order.binding,order.scope,order.agents);});
        CatalogFixture complete(root+"/catalog-complete");Replace(complete.path+"/remote.jsonl",complete.Head()+complete.Before(0)+complete.CompleteNode(0)+
            complete.Before(1)+complete.CompleteNode(1)+"{\"phase\":\"complete\",\"raw_data_forwarded\":false}\n");
        RemoteCatalogPublishEvidence verified(complete.path,complete.binding,complete.scope,complete.agents);
        Require(verified.Nodes()[0].outcome=="complete" && verified.Nodes()[1].outcome=="complete" &&
            EncodeSupervisorControlRequest(verified.Nodes()[0].request)==EncodeSupervisorControlRequest(complete.requests[0]) &&
            EncodeSupervisorControlRequest(verified.Nodes()[1].request)==EncodeSupervisorControlRequest(complete.requests[1]),
            "Complete Publish proof lost the exact validated requests");
        ++passed;std::cout<<"PASS complete fixed-node Publish proof\n";
        CatalogFixture truncatedCatalog(root+"/catalog-truncated");Replace(truncatedCatalog.path+"/remote.jsonl",truncatedCatalog.FailedFirst()+"{");
        Reject("truncated Publish journal is not evidence",[&]{RemoteCatalogPublishEvidence x(truncatedCatalog.path,truncatedCatalog.binding,truncatedCatalog.scope,truncatedCatalog.agents);});
        std::cout<<"PASS cold remote audit evidence: "<<passed<<" metadata cases; no network, hardware or raw data\n";return 0;
    }catch(const std::exception &e){std::cerr<<"FAIL cold handoff evidence: "<<e.what()<<'\n';return 1;}
}
