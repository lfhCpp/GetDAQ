#include "eccsuccessorplan.h"
#include "coldhandoff.h"
#include "handoffrecoverycommands_p.h"
#include "fixedeccconfiguration.h"
#include "eccprotocol.h"
#include "controlprotocol.h"
#include "planio.h"
#include "configurationsha256.h"
#include "supervisorio.h"
#include "systemexperimentconfiguration.h"
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <sys/file.h>
namespace {
using namespace GetDAQ;
using namespace SupervisorIO;
void Require(bool value,const std::string &message) {if (!value) throw std::runtime_error(message);}
void Missing(const std::string &file) {
    struct stat st;Require(::lstat(file.c_str(),&st)!=0 && errno==ENOENT,"Successor already has audit/activation history; no automatic reuse");
}
void SaveOrCheck(const std::string &file,const std::string &bytes) {
    struct stat st;
    if (::lstat(file.c_str(),&st)==0) Require(ReadFile(file,1024*1024)==bytes,"Existing handoff evidence differs; never overwrite");
    else {Require(errno==ENOENT,"Cannot inspect handoff evidence");WriteNew(file,bytes);}
}


}

int main(int argc,char **argv) {
    using namespace GetDAQ;using namespace SupervisorIO;
    if (argc==2 && std::string(argv[1])=="--help") {
        std::cout<<"Usage: getdaq-ecc-handoff submit|inspect OLD_CONFIG NEW_CONFIG SUCCESSOR_PLAN CLIENT_EPOCH OPERATION_ID EVIDENCE_DIRECTORY\n"
            <<"Changed plans: retire-submit|retire-inspect with the same arguments followed by TARGETS_JSON\n"
            <<"Partial original handoff: complete-submit|complete-inspect OLD_CONFIG NEW_CONFIG SUCCESSOR_PLAN ORIGINAL_EVIDENCE NEW_OPERATION_ID NEW_EVIDENCE\n"
            <<"Cold data completion: cold-complete-submit|cold-complete-inspect with the same seven arguments (old ECC must be stopped)\n"
            <<"Read-only cold evidence: review-cold OLD_CONFIG ORIGINAL_HANDOFF_AUDIT NEW_REPORT_DIRECTORY (no network or recovery)\n"
            <<"submit creates a new private evidence directory and POSTs once; inspect never POSTs. No raw payload is transferred.\n";return 0;
    }
    try {
        if(argc>1 && (std::string(argv[1])=="cold-complete-submit" || std::string(argv[1])=="cold-complete-inspect"))return RunColdDataHandoff(argc,argv);
        if(argc>1 && std::string(argv[1])=="review-cold")return GetDAQ::HandoffRecovery::ReviewCold(argc,argv);
        if(argc>1 && (std::string(argv[1])=="complete-submit" || std::string(argv[1])=="complete-inspect"))return GetDAQ::HandoffRecovery::Complete(argc,argv);
        Require(argc==8 || argc==9,"Use --help for explicit original-ECC handoff arguments");
        const std::string requestedAction=argv[1],directory=argv[7];
        const bool changed=requestedAction=="retire-submit" || requestedAction=="retire-inspect";
        const std::string action=requestedAction=="retire-submit" ? "submit" : requestedAction=="retire-inspect" ? "inspect" : requestedAction;
        Require((action=="submit" || action=="inspect") && argc==(changed ? 9 : 8),"Choose submit/inspect or retire-submit/retire-inspect with targets");
        Path(directory);Digest(argv[5]);SupervisorOperationId(argv[6]);
        FixedEccConfiguration original(argv[2]),next(argv[3]);EccSuccessorPlan target(argv[4],next);
        RemoteReceiverManager plannedOriginal(original.ReceiverOptions());
        const auto parent=PlanNativeEccConfiguration(original.ControllerOptions(),plannedOriginal.BindingSha256());
        if (!changed) Require(target.DataBindingSha256()==plannedOriginal.BindingSha256(),"Changed receiver plans require retire-submit/retire-inspect");
        const auto beforeArtifact=DecodeSystemExperimentArtifact(original.ControllerOptions().experimentArtifactBytes);
        const auto afterArtifact=DecodeSystemExperimentArtifact(next.ControllerOptions().experimentArtifactBytes);
        Require(changed || (beforeArtifact.configurationSha256==afterArtifact.configurationSha256 && beforeArtifact.artifactSha256==afterArtifact.artifactSha256),
            "This handoff preserves the current experiment/receiver plan; changed experiment activation is not implemented");
        SupervisorHttpOptions endpoint;endpoint.address=original.BindAddress()=="0.0.0.0" ? "127.0.0.1" : original.BindAddress();
        endpoint.port=original.Port();endpoint.credentialFile=original.CredentialFile();endpoint.timeoutMs=2000;
        NativeControlHttpClient client(endpoint);const auto identityReply=client.Identity();Require(identityReply.code==200,"Original ECC identity unavailable");
        const auto identity=DecodeEccControlIdentity(identityReply.body);
        Require(identity.serviceId==original.ServiceId() && identity.clientId==original.ClientId() && identity.inputBindingSha256==original.BindingSha256() &&
            identity.snapshotSha256==parent.snapshotSha256 && identity.dataBindingSha256==plannedOriginal.BindingSha256(),"Original ECC identity differs from frozen inputs");
        EccControlRequest request;request.command=changed ? "retire-data-plans" : "handoff-data-control";request.operationId=argv[6];request.clientEpoch=argv[5];
        request.clientId=identity.clientId;request.instanceId=identity.instanceId;request.snapshotSha256=identity.snapshotSha256;
        request.successorScopeSha256=target.ScopeSha256();request.successorEpoch=target.Epoch();request.successorPlanSha256=target.Sha256();request.successorDataBindingSha256=target.DataBindingSha256();
        if (changed) {
            const auto document=ParseStrictJson(ReadFile(argv[8],8192));
            ExactSupervisorFields(document,{"schema","kind","plans"});
            Require(Unsigned(document.At("schema"))==1 && String(document.At("kind"))=="getdaq.reviewed-data-plan-targets","Unsupported reviewed data targets");
            const auto &plans=document.At("plans");Require(plans.type==JsonValue::Array && plans.items.size()<=195,"Invalid reviewed data targets");
            for (const auto &entry:plans.items) {
                ExactSupervisorFields(entry,{"agent_id","plan_sha256","plan_file"});EccReceiverPlanTarget value;
                value.agentId=String(entry.At("agent_id"));value.planSha256=String(entry.At("plan_sha256"));value.planFile=String(entry.At("plan_file"));request.successorReceiverPlans.push_back(value);
            }
            ValidateEccControlRequest(request);
            Require(plannedOriginal.SuccessorBindingSha256(request.successorReceiverPlans)==target.DataBindingSha256(),"Reviewed targets differ from exact successor data binding/endpoints/time policy");
        }
        const auto bytes=EncodeEccControlRequest(request),key=EccControlRequestKey(request);
        Fd targetReservation;
        if (action=="submit") {
            targetReservation.value=::open(argv[4],O_RDONLY|O_NOFOLLOW|O_CLOEXEC);struct stat held,named;
            Require(targetReservation.value>=0 && !::fstat(targetReservation.value,&held) && !::lstat(argv[4],&named) &&
                Same(held,named) && !::flock(targetReservation.value,LOCK_SH|LOCK_NB),
                "Successor plan is unavailable or held by startup; no handoff was submitted");
            target.Check();Missing(target.AuditRoot());Missing(std::string(argv[4])+".activation.json");CreateDirectory(directory);
        }
        Directory evidence(directory);original.Check();next.Check();target.Check();
        if (action=="submit") {
            const auto statusReply=client.Status();Require(statusReply.code==200,"Original ECC status unavailable");
            const auto status=DecodeEccControlSystemStatus(statusReply.body);
            Require(status.instanceId==identity.instanceId && status.snapshotSha256==identity.snapshotSha256 && !status.operationActive &&
                !status.controlNeedsReconciliation && !status.controller.needsReconciliation && !status.controller.outcomeUnknown &&
                (status.controller.state==unsigned(EccState::Described) || status.controller.state==unsigned(EccState::Idle)) && !status.dataNeedsReconciliation &&
                status.clientEpoch==request.clientEpoch,"Handoff requires the original client, normal Reset and healthy sealed data");
            WriteNew(directory+"/request.json",bytes);WriteNew(directory+"/target.json",target.Json());WriteNew(directory+"/original-status.json",statusReply.body);
            evidence.Check();original.Check();next.Check();target.Check();
            Missing(target.AuditRoot());Missing(std::string(argv[4])+".activation.json");
            try {
                const auto response=client.PostOperation(bytes);
                WriteNew(directory+"/submission.json","{\"http_status\":"+std::to_string(response.code)+",\"body\":"+EncodeJsonString(response.body,1024*1024)+"}\n");
                Require(response.code==202,"Original ECC did not accept handoff; inspect the preserved evidence, do not retry POST");
            } catch (...) {
                SaveOrCheck(directory+"/submission-held.json","{\"schema\":1,\"post_attempted_once\":true,\"automatic_retry\":false}\n");throw;
            }
        } else {
            Require(ReadFile(directory+"/request.json",65536)==bytes && ReadFile(directory+"/target.json",65536)==target.Json(),
                "Inspect requires the exact original instance, request, target and evidence directory");
        }
        const auto originalStatus=DecodeEccControlSystemStatus(ReadFile(directory+"/original-status.json",1024*1024));
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(original.ReceiverOptions().commandTimeoutMs+5000);
        for (;;) {
            evidence.Check();Require(ReadFile(directory+"/request.json",65536)==bytes,"Handoff request file changed");
            const auto reply=client.Operation(request.operationId);Require(reply.code==200,"Original handoff operation unavailable; no POST was retried");
            const auto operation=DecodeEccControlOperation(reply.body);
            Require(operation.requestKey==key && EncodeEccControlRequest(operation.request)==bytes,"Original operation request/key differs");
            if (operation.phase=="complete") {
                const auto receipt=client.ControlHandoff();Require(receipt.code==200,"Original live ECC receipt unavailable; preserve operation and inspect again");
                GetDAQ::HandoffRecovery::VerifyReceipt(receipt.body,request,originalStatus,original.ReceiverOptions());
                original.Check();next.Check();target.Check();evidence.Check();
                SaveOrCheck(directory+"/operation.json",reply.body);SaveOrCheck(directory+"/receipt.json",receipt.body);
                std::cout<<"Complete original handoff verified; receipt: "<<directory<<"/receipt.json; successor startup remains explicit\n";return 0;
            }
            Require(operation.phase=="accepted" || operation.phase=="running","Handoff held/failed; do not replay or roll back");
            Require(std::chrono::steady_clock::now()<deadline,"Original handoff still pending; use inspect, never retry POST");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } catch (const std::exception &e) {std::cerr<<e.what()<<'\n';return 2;}
}
