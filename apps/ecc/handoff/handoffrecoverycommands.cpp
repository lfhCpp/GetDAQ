#include "handoffrecoverycommands_p.h"
#include "eccsuccessorplan.h"
#include "fixedeccconfiguration.h"
#include "remotehandoffevidence.h"
#include "remotereceivermanager.h"
#include "eccprotocol.h"
#include "controlprotocol.h"
#include "planio.h"
#include "configurationsha256.h"
#include "strictjson.h"
#include "supervisorio.h"
#include "systemexperimentconfiguration.h"
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <limits.h>
#include <set>
#include <stdexcept>
#include <thread>
#include <sys/file.h>
#include <sys/stat.h>

namespace GetDAQ {
namespace HandoffRecovery {
using namespace SupervisorIO;
namespace {

void Require(bool value, const std::string &message) {
    if (!value) throw std::runtime_error(message);
}

void Missing(const std::string &file) {
    struct stat st;
    Require(::lstat(file.c_str(), &st) != 0 && errno == ENOENT,
            "Successor already has audit/activation history; no automatic reuse");
}

void SaveOrCheck(const std::string &file, const std::string &bytes) {
    struct stat st;
    if (::lstat(file.c_str(), &st) == 0)
        Require(ReadFile(file, 1024 * 1024) == bytes, "Existing handoff evidence differs; never overwrite");
    else {
        Require(errno == ENOENT, "Cannot inspect handoff evidence");
        WriteNew(file, bytes);
    }
}
} // namespace

void VerifyReceipt(const std::string &bytes,const EccControlRequest &request,const EccControlSystemStatus &original,
             const RemoteReceiverOptions &options) {
    JsonLimits limits;limits.stringBytes=8192;const auto value=ParseStrictJson(bytes,limits);
    const bool changed=request.command=="retire-data-plans";
    Require(Unsigned(value.At("schema"))==1 && String(value.At("kind"))==(changed ? "getdaq.remote-plan-retirement" : "getdaq.remote-control-handoff") &&
        String(value.At("successor_scope_sha256"))==request.successorScopeSha256 && String(value.At("successor_epoch"))==request.successorEpoch &&
        String(value.At("original_scope_sha256"))==original.dataScopeSha256 && String(value.At("original_epoch"))==original.dataEpoch,
        "Original ECC receipt scope/epoch differs from reviewed request");
    const auto &nodes=value.At("nodes");Require(nodes.type==JsonValue::Array && nodes.items.size()==options.endpoints.size(),"Handoff receipt does not cover exact data agents");
    std::set<std::string> expected,seen;
    for (const auto &endpoint:options.endpoints) expected.insert(endpoint.agentId);
    for (const auto &node:nodes.items) {
        const auto id=String(node.At("agent_id"));Require(expected.count(id) && seen.insert(id).second,"Handoff receipt agent differs");
        const auto child=DecodeSupervisorControlRequest(String(node.At("request")));
        Require(child.command==(changed ? SupervisorCommand::RetirePlan : SupervisorCommand::HandoffControl) && child.operationId==String(node.At("operation_id")) &&
            SupervisorControlRequestKey(child)==String(node.At("request_key")) && child.instanceId==String(node.At("instance_id")) &&
            child.controllerSnapshotSha256==original.dataScopeSha256 && child.controllerEpoch==original.dataEpoch &&
            child.successorControllerSnapshotSha256==request.successorScopeSha256 && child.successorControllerEpoch==request.successorEpoch,
            "Original data-node handoff request differs");
        Digest(String(node.At("proof_sha256")));
        bool matched=false;
        for (const auto &old:original.nodes) if (old.agentId==id)
            matched=old.instanceId==child.instanceId && old.planSha256==child.planSha256;
        Require(matched,"Handoff receipt data instance/plan differs from original observation");
        if (changed) {
            bool targetMatched=false;
            for (const auto &target:request.successorReceiverPlans) if (target.agentId==id)
                targetMatched=child.successorPlanSha256==target.planSha256 && child.successorPlanFile==target.planFile;
            Require(targetMatched,"Retirement receipt target plan differs");
            for (const auto &old:original.nodes) if (old.agentId==id) Require(old.sessionId==child.expectedSessionId,"Retirement original session differs");
        }
    }
}

int Complete(int argc,char **argv) {
    Require(argc==8,"Use complete-submit|complete-inspect OLD_CONFIG NEW_CONFIG SUCCESSOR_PLAN ORIGINAL_EVIDENCE NEW_OPERATION_ID NEW_EVIDENCE");
    const bool submit=std::string(argv[1])=="complete-submit";
    const std::string oldDirectory=argv[5],directory=argv[7];Path(oldDirectory);Path(directory);SupervisorOperationId(argv[6]);
    Directory originalEvidence(oldDirectory);FixedEccConfiguration configuration(argv[2]),next(argv[3]);EccSuccessorPlan target(argv[4],next);
    const auto originalBytes=ReadFile(oldDirectory+"/request.json",8192),originalStatusBytes=ReadFile(oldDirectory+"/original-status.json",1024*1024);
    const auto originalRequest=DecodeEccControlRequest(originalBytes);
    Require(EncodeEccControlRequest(originalRequest)==originalBytes &&
        (originalRequest.command=="handoff-data-control" || originalRequest.command=="retire-data-plans"),"Original evidence lacks a canonical handoff/retirement request");
    Require(target.Sha256()==originalRequest.successorPlanSha256 && target.ScopeSha256()==originalRequest.successorScopeSha256 &&
        target.Epoch()==originalRequest.successorEpoch && target.DataBindingSha256()==originalRequest.successorDataBindingSha256,
        "Completion cannot change or consume the originally reviewed successor identity");
    Fd targetReservation(::open(argv[4],O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
    struct stat held,named;
    Require(targetReservation.value>=0 && !::fstat(targetReservation.value,&held) && !::lstat(argv[4],&named) && Same(held,named) &&
        !::flock(targetReservation.value,LOCK_SH|LOCK_NB),"Cannot hold the original successor identity during completion");
    target.Check();Missing(target.AuditRoot());Missing(std::string(argv[4])+".activation.json");
    const auto originalStatus=DecodeEccControlSystemStatus(originalStatusBytes);
    Require(originalStatus.instanceId==originalRequest.instanceId && originalStatus.snapshotSha256==originalRequest.snapshotSha256 &&
        originalStatus.clientEpoch==originalRequest.clientEpoch && !originalStatus.operationActive && !originalStatus.controlNeedsReconciliation &&
        !originalStatus.dataNeedsReconciliation && !originalStatus.controller.outcomeUnknown && !originalStatus.controller.needsReconciliation,
        "Original evidence is not the pre-handoff known controller/data binding");
    RemoteReceiverManager planned(configuration.ReceiverOptions());
    const auto parent=PlanNativeEccConfiguration(configuration.ControllerOptions(),planned.BindingSha256());
    SupervisorHttpOptions endpoint;endpoint.address=configuration.BindAddress()=="0.0.0.0" ? "127.0.0.1" : configuration.BindAddress();
    endpoint.port=configuration.Port();endpoint.credentialFile=configuration.CredentialFile();endpoint.timeoutMs=2000;
    NativeControlHttpClient client(endpoint);const auto identityReply=client.Identity();Require(identityReply.code==200,"Original live ECC identity unavailable");
    const auto identity=DecodeEccControlIdentity(identityReply.body);
    Require(identity.instanceId==originalRequest.instanceId && identity.snapshotSha256==originalRequest.snapshotSha256 && identity.snapshotSha256==parent.snapshotSha256 &&
        identity.inputBindingSha256==configuration.BindingSha256() && identity.dataBindingSha256==planned.BindingSha256() &&
        identity.serviceId==configuration.ServiceId() && identity.clientId==originalRequest.clientId && identity.clientId==configuration.ClientId(),
        "Completion requires the exact original live ECC, fixed inputs and saved client");
    const auto originalReply=client.Operation(originalRequest.operationId);Require(originalReply.code==200,"Original handoff operation unavailable");
    const auto failed=DecodeEccControlOperation(originalReply.body);
    Require(failed.phase=="failed" && EncodeEccControlRequest(failed.request)==originalBytes,"Completion requires the exact failed original operation");
    EccControlRequest request;request.command="complete-data-handoff";request.operationId=argv[6];request.failedOperationId=originalRequest.operationId;
    request.instanceId=originalRequest.instanceId;request.snapshotSha256=originalRequest.snapshotSha256;request.clientId=originalRequest.clientId;request.clientEpoch=originalRequest.clientEpoch;
    const auto bytes=EncodeEccControlRequest(request),key=EccControlRequestKey(request);
    if(submit) {
        Missing(directory);CreateDirectory(directory);
        WriteNew(directory+"/request.json",bytes);WriteNew(directory+"/original-request.json",originalBytes);
        WriteNew(directory+"/original-status.json",originalStatusBytes);WriteNew(directory+"/original-operation.json",originalReply.body);
    }
    Directory evidence(directory);
    Require(ReadFile(directory+"/request.json",8192)==bytes && ReadFile(directory+"/original-request.json",8192)==originalBytes &&
        ReadFile(directory+"/original-status.json",1024*1024)==originalStatusBytes,"Completion evidence differs from the original request/instance");
    configuration.Check();next.Check();target.Check();originalEvidence.Check();evidence.Check();
    if(submit) {
        try {
            const auto reply=client.PostOperation(bytes);
            WriteNew(directory+"/submission.json","{\"http_status\":"+std::to_string(reply.code)+",\"body\":"+EncodeJsonString(reply.body,1024*1024)+"}\n");
            Require(reply.code==202,"Completion not accepted; preserve intent and use complete-inspect, never automatic POST retry");
        }catch(...) {SaveOrCheck(directory+"/submission-held.json","{\"post_attempted_once\":true,\"automatic_retry\":false}\n");throw;}
    }
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(configuration.ReceiverOptions().commandTimeoutMs+5000);
    for(;;) {
        configuration.Check();next.Check();target.Check();Missing(target.AuditRoot());Missing(std::string(argv[4])+".activation.json");originalEvidence.Check();evidence.Check();
        Require(ReadFile(oldDirectory+"/request.json",8192)==originalBytes && ReadFile(directory+"/request.json",8192)==bytes &&
            ReadFile(directory+"/original-request.json",8192)==originalBytes && ReadFile(directory+"/original-status.json",1024*1024)==originalStatusBytes,
            "Completion evidence changed; no resubmission");
        const auto reply=client.Operation(request.operationId);Require(reply.code==200,"Original completion operation unavailable; no POST retried");
        const auto operation=DecodeEccControlOperation(reply.body);
        Require(operation.requestKey==key && EncodeEccControlRequest(operation.request)==bytes,"Completion result differs from its original saved intent");
        if(operation.phase=="complete") {
            const auto receipt=client.ControlHandoff();Require(receipt.code==200,"Completed original handoff receipt unavailable; inspect again");
            VerifyReceipt(receipt.body,originalRequest,originalStatus,configuration.ReceiverOptions());
            SaveOrCheck(directory+"/operation.json",reply.body);SaveOrCheck(directory+"/receipt.json",receipt.body);
            std::cout<<"Original handoff explicitly completed; old controller remains fenced; receipt: "<<directory<<"/receipt.json\n";return 0;
        }
        Require(operation.phase=="accepted" || operation.phase=="running","Explicit completion failed/unknown; preserve every original result");
        Require(std::chrono::steady_clock::now()<deadline,"Completion pending; use complete-inspect without POST");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int ReviewCold(int argc,char **argv) {
    Require(argc==5,"Use review-cold OLD_CONFIG ORIGINAL_HANDOFF_AUDIT NEW_REPORT_DIRECTORY");
    FixedEccConfiguration configuration(argv[2],FixedEccConfiguration::OfflineEvidenceOnly());
    const auto binding=RemoteReceiverBindingSha256(configuration.ReceiverOptions());
    const auto planned=PlanNativeEccConfiguration(configuration.ControllerOptions(),binding);
    RemoteHandoffEvidence evidence(argv[3],binding,planned.dataControllerScopeSha256);
    const std::string directory=argv[4],root=configuration.ControllerOptions().auditRoot;Path(directory);
    const auto slash=directory.find_last_of('/');const auto parent=directory.substr(0,slash ? slash : 1);char physicalParent[PATH_MAX];
    Require(::realpath(parent.c_str(),physicalParent) && parent==physicalParent,"Review output parent must be an existing canonical directory without aliases");
    Require(directory!=root && directory.compare(0,root.size()+1,root+'/')!=0,"Review output must be outside the immutable original controller root");
    std::string report="{\"schema\":1,\"kind\":\"getdaq.cold-handoff-review\",\"original_audit_directory\":"+EncodeJsonString(evidence.Directory())+
        ",\"original_intent_sha256\":"+EncodeJsonString(evidence.IntentSha256())+",\"snapshot_sha256\":"+EncodeJsonString(planned.snapshotSha256)+
        ",\"binding_sha256\":"+EncodeJsonString(binding)+",\"original_scope_sha256\":"+EncodeJsonString(planned.dataControllerScopeSha256)+
        ",\"original_epoch\":"+EncodeJsonString(evidence.Epoch())+",\"nodes\":[";
    bool first=true;
    for(const auto &node:evidence.Nodes()) {
        if(!first) report+=',';
        first=false;
        report+="{\"agent_id\":"+EncodeJsonString(node.agentId)+",\"instance_id\":"+EncodeJsonString(node.instanceId)+
            ",\"operation_id\":"+EncodeJsonString(node.request.operationId)+",\"request_key\":"+EncodeJsonString(SupervisorControlRequestKey(node.request))+
            ",\"plan_sha256\":"+EncodeJsonString(node.planSha256)+",\"session_id\":"+EncodeJsonString(node.stopped.sessionId)+
            ",\"historical_delivery\":"+EncodeJsonString(node.rejection==1 ? "never_submitted" : node.rejection==0 ? "uncertain_or_accepted" : "explicit_rejection")+
            ",\"http_rejection\":"+(node.rejection==409 || node.rejection==412 ? std::to_string(node.rejection) : "null")+"}";
    }
    report+="],\"current_remote_identity_checked\":false,\"control_granted\":false,\"hardware_unknown_cleared\":false,\"cold_recovery_executed\":false,\"raw_payload_read\":false}\n";
    Require(report.size()<=1024*1024,"Cold review report exceeds 1MiB");configuration.Check();evidence.Check();
    struct stat output;Require(::lstat(directory.c_str(),&output)!=0 && errno==ENOENT,"Review output already exists or cannot be inspected; choose a new report directory");
    CreateDirectory(directory);
    WriteNew(directory+"/review.json",report);configuration.Check();evidence.Check();
    std::cout<<"Historical handoff evidence reviewed; no network, hardware actions or recovery performed. Report: "<<directory<<"/review.json\n";return 0;
}

} // namespace HandoffRecovery
} // namespace GetDAQ
