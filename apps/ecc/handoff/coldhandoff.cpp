#include "coldhandoff.h"
#include "coldhandoff_evidence_p.h"
#include "eccsuccessorplan.h"
#include "eccjournal.h"
#include "fixedeccconfiguration.h"
#include "remotehandoffevidence.h"
#include "eccprotocol.h"
#include "supervisorio.h"
#include "planio.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <sys/file.h>
#include <unistd.h>
namespace GetDAQ {
namespace {
using namespace SupervisorIO;
void Require(bool value,const std::string &message) {if(!value)throw std::runtime_error(message);}
void Missing(const std::string &path) {
    struct stat st;Require(::lstat(path.c_str(),&st)!=0 && errno==ENOENT,"Cold completion requires an unused successor and new evidence destination");
}
std::string CanonicalParent(const std::string &path) {
    Path(path);const auto parent=path.substr(0,path.find_last_of('/'));char *resolved=::realpath(parent.c_str(),nullptr);
    const bool same=resolved && parent==resolved;::free(resolved);
    Require(same,"Cold evidence requires an existing canonical absolute parent");return parent;
}
class File {
    std::string path;Fd fd;struct stat identity;
public:
    const std::string bytes;
    File(const std::string &name,std::size_t maximum) : path(name),fd(::open(name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC)),bytes(ReadFile(name,maximum)) {
        Require(fd.value>=0 && !::fstat(fd.value,&identity),"Cannot bind cold completion metadata");Check();
    }
    int Descriptor() const {return fd.value;}
    void Check() const {
        struct stat held,current;
        Require(!::fstat(fd.value,&held) && !::lstat(path.c_str(),&current) && Same(identity,held) && Same(identity,current) &&
            S_ISREG(current.st_mode) && current.st_uid==::geteuid() && (current.st_mode&0777)==0600 && current.st_nlink==1 &&
            ReadFile(path,bytes.size())==bytes,"Cold completion metadata identity/content changed");
    }
};
void Save(const std::string &path,const std::string &bytes) {
    struct stat st;
    if(::lstat(path.c_str(),&st)==0) {File existing(path,1024*1024);Require(existing.bytes==bytes,"Saved cold receipt differs; no overwrite");}
    else {Require(errno==ENOENT,"Cannot inspect cold receipt destination");WriteNew(path,bytes);}
}
}
int RunColdDataHandoff(int argc,char **argv) {
    Require(argc==8,"Use cold-complete-submit|cold-complete-inspect OLD_CONFIG NEW_CONFIG SUCCESSOR_PLAN ORIGINAL_EVIDENCE NEW_OPERATION_ID NEW_EVIDENCE");
    const bool submit=std::string(argv[1])=="cold-complete-submit";
    const std::string originalDirectory=argv[5],operationId=argv[6],directory=argv[7];
    SupervisorOperationId(operationId);Path(originalDirectory);Path(directory);
    FixedEccConfiguration configuration(argv[2]),next(argv[3]);EccSuccessorPlan target(argv[4],next);
    Directory oldEvidence(originalDirectory),outputParent(CanonicalParent(directory));
    File original(originalDirectory+"/request.json",8192),before(originalDirectory+"/original-status.json",1024*1024);
    const auto request=DecodeEccControlRequest(original.bytes);const auto status=DecodeEccControlSystemStatus(before.bytes);
    Require(EncodeEccControlRequest(request)==original.bytes && (request.command=="handoff-data-control" || request.command=="retire-data-plans"),"Original evidence is not a canonical data handoff");
    const auto options=configuration.ControllerOptions();
    const auto binding=RemoteReceiverBindingSha256(configuration.ReceiverOptions());
    const auto plan=PlanNativeEccConfiguration(options,binding);
    Require(status.instanceId==request.instanceId && status.snapshotSha256==request.snapshotSha256 && status.snapshotSha256==plan.snapshotSha256 &&
        status.clientEpoch==request.clientEpoch && request.clientId==configuration.ClientId() && !status.operationActive && !status.controlNeedsReconciliation &&
        !status.dataNeedsReconciliation && !status.controller.outcomeUnknown && !status.controller.needsReconciliation &&
        (status.controller.state==1 || status.controller.state==2) && !status.controller.error && !status.controller.transition &&
        status.dataScopeSha256==plan.dataControllerScopeSha256,"Original parent evidence lacks the known released controller/data binding");
    Require(target.Sha256()==request.successorPlanSha256 && target.ScopeSha256()==request.successorScopeSha256 && target.Epoch()==request.successorEpoch &&
        target.DataBindingSha256()==request.successorDataBindingSha256,"Cold completion cannot change the original successor");
    Fd reservation(::open(argv[4],O_RDONLY|O_NOFOLLOW|O_CLOEXEC));struct stat held,named;
    Require(reservation.value>=0 && !::fstat(reservation.value,&held) && !::lstat(argv[4],&named) && Same(held,named) &&
        !::flock(reservation.value,LOCK_SH|LOCK_NB),"Cannot reserve the exact original successor identity");
    target.Check();Missing(target.AuditRoot());Missing(std::string(argv[4])+".activation.json");
    for(const auto &root:{options.auditRoot,target.AuditRoot()})Require(directory!=root && directory.compare(0,root.size()+1,root+'/'),"Cold evidence must be outside controller roots");
    Directory network(options.auditRoot+"/network"),networkOps(options.auditRoot+"/network/operations"),parentOperation(options.auditRoot+"/network/operations/"+request.operationId);
    File parentRequest(parentOperation.Path()+"/request.json",8192),policy(network.Path()+"/policy.json",65536),history(network.Path()+"/operations.jsonl",64*1024*1024);
    File native(options.auditRoot+"/ecc.jsonl",64*1024*1024),experiment(options.auditRoot+"/experiment.json",1024*1024);
    Require(parentRequest.bytes==original.bytes && experiment.bytes==options.experimentArtifactBytes,"Original ECC parent request/experiment differs");
    const auto policyValue=ParseStrictJson(policy.bytes);
    std::set<std::string> policyFields={"schema","kind","service_id","client_id","input_binding_sha256","snapshot_sha256"};
    if(policyValue.members.count("successor_plan_sha256")) {
        policyFields.insert("successor_plan_sha256");policyFields.insert("receiver_controller_epoch");
        Digest(String(policyValue.At("successor_plan_sha256")));
        Require(String(policyValue.At("receiver_controller_epoch"))==status.dataEpoch,"Original planned controller epoch differs");
    }
    ExactSupervisorFields(policyValue,policyFields);
    Require(Unsigned(policyValue.At("schema"))==1 && String(policyValue.At("kind"))=="getdaq.ecc-network-policy" &&
        String(policyValue.At("service_id"))==configuration.ServiceId() && String(policyValue.At("client_id"))==request.clientId &&
        String(policyValue.At("input_binding_sha256"))==configuration.BindingSha256() && String(policyValue.At("snapshot_sha256"))==plan.snapshotSha256,
        "Original ECC fixed network policy differs");ValidateColdParentHistory(history.bytes,request);
    const auto originalPath=options.auditRoot+"/operations/"+request.operationId+"/data-handoff";
    RemoteHandoffEvidence prior(originalPath,binding,plan.dataControllerScopeSha256);const auto nodes=prior.Nodes();
    Require(prior.Epoch()==status.dataEpoch && nodes.size()==configuration.ReceiverOptions().endpoints.size() && nodes.size()==status.nodes.size(),"Original handoff data epoch/node set differs");
    for(const auto &node:nodes) {
        const auto &child=node.request;
        Require(child.controllerId==configuration.ReceiverOptions().controllerId && child.successorControllerSnapshotSha256==target.ScopeSha256() && child.successorControllerEpoch==target.Epoch() &&
            child.command==(request.command=="retire-data-plans" ? SupervisorCommand::RetirePlan : SupervisorCommand::HandoffControl),"Original child successor/command differs");
        bool matched=false;for(const auto &old:status.nodes)if(old.agentId==node.agentId)matched=old.instanceId==node.instanceId && old.planSha256==node.planSha256 && old.sessionId==node.stopped.sessionId;
        Require(matched,"Original parent and backend normal data cycle differ");
        if(child.command==SupervisorCommand::RetirePlan) {
            bool targetMatched=false;for(const auto &nextNode:request.successorReceiverPlans)if(nextNode.agentId==node.agentId)targetMatched=nextNode.planFile==child.successorPlanFile && nextNode.planSha256==child.successorPlanSha256;
            Require(targetMatched,"Original parent and child retirement targets differ");
        }
    }
    const auto intent="{\"schema\":1,\"kind\":\"getdaq.cold-data-handoff-completion\",\"operation_id\":"+EncodeJsonString(operationId)+
        ",\"original_request_sha256\":"+EncodeJsonString(ConfigurationSha256(original.bytes))+",\"original_evidence_directory\":"+EncodeJsonString(originalDirectory)+
        ",\"original_audit_directory\":"+EncodeJsonString(originalPath)+",\"original_intent_sha256\":"+EncodeJsonString(prior.IntentSha256())+
        ",\"successor_plan_sha256\":"+EncodeJsonString(target.Sha256())+"}\n";
    if(submit) {Missing(directory);CreateDirectory(directory);WriteNew(directory+"/cold-request.json",intent);WriteNew(directory+"/original-request.json",original.bytes);WriteNew(directory+"/original-status.json",before.bytes);}
    Directory output(directory);File saved(directory+"/cold-request.json",8192),savedOriginal(directory+"/original-request.json",8192),savedBefore(directory+"/original-status.json",1024*1024);
    Require(saved.bytes==intent && savedOriginal.bytes==original.bytes && savedBefore.bytes==before.bytes,"Cold evidence differs from saved original completion intent");
    Require(!::flock(saved.Descriptor(),LOCK_EX|LOCK_NB),"This cold evidence directory is already in use");
    const auto check=[&] {
        configuration.Check();next.Check();target.Check();Missing(target.AuditRoot());Missing(std::string(argv[4])+".activation.json");
        oldEvidence.Check();outputParent.Check();output.Check();network.Check();networkOps.Check();parentOperation.Check();
        original.Check();before.Check();parentRequest.Check();policy.Check();history.Check();native.Check();experiment.Check();saved.Check();savedOriginal.Check();savedBefore.Check();
    };
    check();std::string receipt;
    if(submit) {
        try {receipt=RemoteReceiverManager::CompleteColdHandoff(configuration.ReceiverOptions(),options,originalPath,operationId,check);}
        catch(...) {Save(directory+"/completion-held.json","{\"completion_invoked_once\":true,\"automatic_retry\":false,\"original_control_remains_fenced\":true}\n");throw;}
    }else {
        EccJournal reserve(options.auditRoot+"/ecc.jsonl","getdaq.native-ecc:1:"+plan.snapshotSha256,EccJournalMode::ExclusiveReadOnly);
        const auto state=reserve.Load();Require((state.state==EccState::Idle || state.state==EccState::Described) && !state.outcomeUnknown && !state.error && !state.transition,
            "Cold receipt inspection cannot bypass independent native UNKNOWN");
        check();RemoteHandoffEvidence evidence(originalPath,binding,plan.dataControllerScopeSha256);
        Directory attempt(options.auditRoot+"/operations/"+operationId),action(attempt.Path()+"/data-handoff");
        File auditIntent(action.Path()+"/intent.json",1024*1024),journal(action.Path()+"/remote.jsonl",1024*1024);
        const auto value=ParseStrictJson(auditIntent.bytes);Require(String(value.At("kind"))=="getdaq.remote-handoff-completion-intent" &&
            String(value.At("original_audit_directory"))==originalPath && String(value.At("original_intent_sha256"))==evidence.IntentSha256() &&
            String(value.At("binding_sha256"))==binding,"Cold observation is not the saved original attempt");
        receipt=BuildColdCompletionReceipt(plan.dataControllerScopeSha256,status.dataEpoch,originalPath,nodes,journal.bytes);
        evidence.Check();attempt.Check();action.Check();auditIntent.Check();journal.Check();check();
    }
    check();Save(directory+"/receipt.json",receipt);check();
    std::cout<<"Cold data handoff proof saved; original hardware controller remains fenced: "<<directory<<"/receipt.json\n";return 0;
}
}
