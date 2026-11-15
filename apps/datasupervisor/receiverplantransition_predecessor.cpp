#include "receiverplantransition.h"
#include "receiverplantransition_p.h"
#include "receivercycleevidence.h"
#include "controljobs.h"
#include "supervisorio.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <atomic>
#include <map>
#include <mutex>
#include <stdexcept>
#include <sys/file.h>
#include <unistd.h>

namespace GetDAQ {
using namespace SupervisorIO;
using namespace ReceiverPlanTransitionInternal;

class SupervisorPlanPredecessor::Impl {
public:
    std::string path,sha,scope,epoch,nextPlan,root,journalBytes;
    Directory directory;
    BoundFile proof,request,copy,cycleFile,policy,journal;
    std::unique_ptr<ReceiverCycleEvidence> cycle;
    std::unique_ptr<BoundFile> activation;
    mutable std::atomic<bool> faulted{false};
    std::atomic<bool> activated{false};
    mutable std::mutex mutex;
    static std::string OldRoot(const std::string &file) {
        Path(file);const auto bytes=ReadFile(file,4*1024*1024);JsonLimits limits;limits.stringBytes=1024*1024;limits.arrayItems=131072;
        const auto value=ParseStrictJson(bytes,limits);const auto root=String(value.At("deployment_root"));Path(root);return root;
    }
    Impl(const std::string &file,const SupervisorControlOptions &next) : path(file),root(OldRoot(file)),
        directory(file.substr(0,file.find_last_of('/'))),proof(file,4*1024*1024,0400),
        request(directory.Path()+"/request.json",8192,0600),copy(directory.Path()+"/successor-plan.json",1024*1024,0400),
        cycleFile(directory.Path()+"/cycle-evidence.json",16*1024*1024,0400),
        policy(root+"/control/policy.json",65536,0600),journal(root+"/control/operations.jsonl",64*1024*1024,0600) {
        JsonLimits limits;limits.stringBytes=1024*1024;limits.arrayItems=131072;const auto value=ParseStrictJson(proof.Bytes(),limits);
        ExactSupervisorFields(value,{"schema","kind","proof_file","deployment_root","agent_id","request_key","request","successor_plan","lifecycle","cycle_evidence_sha256"});
        Require(Unsigned(value.At("schema"))==1 && String(value.At("kind"))=="getdaq.supervisor-plan-retirement" && String(value.At("proof_file"))==path,
            "Unsupported or moved original plan retirement proof");
        const auto original=DecodeSupervisorControlRequest(request.Bytes());
        Require(original.command==SupervisorCommand::RetirePlan && EncodeSupervisorControlRequest(original)==request.Bytes() &&
            String(value.At("request"))==request.Bytes() && String(value.At("request_key"))==SupervisorControlRequestKey(original),
            "Original plan retirement request/key differs");
        Require(directory.Path()==root+"/control/operations/"+original.operationId && String(value.At("agent_id"))==next.agentId &&
            original.controllerId==next.controllerId,"Plan retirement agent/controller/original path differs");
        const auto oldPlan=DecodeSupervisorPlan(ReadFile(root+"/plan.json",1024*1024));
        Require(ConfigurationSha256(EncodeOptions(oldPlan.options,oldPlan.executableSha256))==original.planSha256 && oldPlan.options.auditRoot==root &&
            oldPlan.options.configurationSha256==original.configurationSha256 && oldPlan.options.artifactSha256==original.artifactSha256,
            "Original data plan differs from retirement request");
        ValidateNextPlan(copy.Bytes(),oldPlan.options,oldPlan.executableSha256);
        nextPlan=ConfigurationSha256(copy.Bytes());
        Require(String(value.At("successor_plan"))==copy.Bytes() && nextPlan==original.successorPlanSha256 &&
            EncodeOptions(next.receiver,next.executableSha256)==copy.Bytes(),"Actual successor service plan differs from original retirement target");
        const auto status=DecodeSupervisorStatus(value.At("lifecycle"));
        Require(status.state==DataSupervisorStatus::Stopped && !status.needsReconciliation && !status.receivers.empty() &&
            status.sessionId==original.expectedSessionId && status.planSha256==original.planSha256,"Retirement lacks the original normal completed cycle");
        for (const auto &receiver:status.receivers) Require(receiver.reaped && receiver.exitCode==0 && receiver.sealVerified,"Retirement contains incomplete receivers");
        const auto frozenPolicy=ParseStrictJson(policy.Bytes());
        Require(String(frozenPolicy.At("agent_id"))==next.agentId && String(frozenPolicy.At("controller_id"))==next.controllerId &&
            String(frozenPolicy.At("plan_sha256"))==original.planSha256,"Original data service policy differs");
        sha=ConfigurationSha256(proof.Bytes());scope=original.successorControllerSnapshotSha256;epoch=original.successorControllerEpoch;
        journalBytes=journal.Bytes();Require(journalBytes.empty() || journalBytes.back()=='\n',"Truncated original data control journal");
        bool accepted=false,complete=false,failed=false;std::string instance;std::uint64_t sequence=0;
        std::map<std::string,SupervisorControlRequest> recoveries;std::map<std::string,std::string> recoveryPhases;
        for (std::size_t begin=0;begin<journalBytes.size();) {
            const auto end=journalBytes.find('\n',begin);const auto row=ParseStrictJson(journalBytes.substr(begin,end-begin),limits);begin=end+1;
            Require(Unsigned(row.At("sequence"))==sequence++ && String(row.At("plan_sha256"))==original.planSha256,"Original retirement journal scope/sequence differs");
            const auto event=String(row.At("event"));if (event=="opened") instance=String(row.At("instance_id"));
            if (event=="accepted") {
                const auto id=String(row.At("operation_id"));
                if (!accepted) {
                    if (id==original.operationId) {
                        Require(instance==original.instanceId && String(row.At("request"))==request.Bytes() && String(row.At("request_key"))==SupervisorControlRequestKey(original),"Original retirement intent differs");accepted=true;
                    }
                } else {
                    Require(failed && !complete,"Original control journal contains a new request after plan retirement");
                    const auto recovery=DecodeSupervisorControlRequest(String(row.At("request")));
                    Require(recovery.command==SupervisorCommand::CompletePlanRetirement && recovery.operationId==id &&
                        recovery.instanceId==instance && instance==original.instanceId &&
                        EncodeSupervisorControlRequest(OriginalPlanRetirementRequest(recovery))==request.Bytes() &&
                        String(row.At("request_key"))==SupervisorControlRequestKey(recovery) && !recoveries.count(id),
                        "Retirement completion changed original target/cycle/owner");
                    for (const auto &prior:recoveryPhases) Require(prior.second=="failed","Retirement completion overlaps an unresolved earlier attempt");
                    recoveries.emplace(id,recovery);recoveryPhases[id]="accepted";
                }
            }
            if (event=="failed" && String(row.At("operation_id"))==original.operationId) {
                Require(accepted && !failed && !complete && String(row.At("request_key"))==SupervisorControlRequestKey(original) &&
                    String(row.At("result"))==EncodeSupervisorStatus(status),"Failed retirement does not retain the original normal cycle");failed=true;
            }
            if (event=="complete" && String(row.At("operation_id"))==original.operationId) {
                Require(accepted && !failed && !complete && String(row.At("request_key"))==SupervisorControlRequestKey(original) &&
                    String(row.At("plan_retirement_proof_sha256"))==sha && String(row.At("result"))==EncodeSupervisorStatus(status),"Original retirement completion is not bound to proof/cycle");complete=true;
            }
            if (event=="running" || event=="failed" || event=="unknown" || event=="complete") {
                const auto id=String(row.At("operation_id"));const auto found=recoveries.find(id);
                if (found!=recoveries.end()) {
                    auto &phase=recoveryPhases[id];
                    Require(!complete && (phase=="accepted" || phase=="running") &&
                        String(row.At("request_key"))==SupervisorControlRequestKey(found->second),"Invalid retirement completion transition/key");
                    if (event=="running") Require(phase=="accepted","Repeated completion running event");
                    else if (event=="complete") {
                        Require(failed && String(row.At("plan_retirement_proof_sha256"))==sha && String(row.At("result"))==EncodeSupervisorStatus(status),
                            "Retirement completion does not bind the original proof/cycle");complete=true;
                    }
                    phase=event;
                }
            }
        }
        Require(complete,"Original plan retirement has no complete durable result");
        Require(ConfigurationSha256(cycleFile.Bytes())==String(value.At("cycle_evidence_sha256")),"Completed cycle evidence SHA differs");
        const auto cycleDocument=ParseStrictJson(cycleFile.Bytes(),limits);Require(String(cycleDocument.At("deployment_root"))==root,"Completed cycle deployment root differs");
        cycle.reset(new ReceiverCycleEvidence(cycleFile.Bytes()));CheckLocked();
    }
    void CheckLocked() const {
        if (faulted) throw std::runtime_error("Original plan predecessor proof fault remains latched");
        try {
            directory.Check();proof.Check();request.Check();copy.Check();cycleFile.Check();policy.Check();journal.Check();
            if (activated) cycle->CheckReservation(); else cycle->Check();
            if(activation)activation->Check();
        }
        catch (...) {faulted=true;throw;}
    }
};
SupervisorPlanPredecessor::SupervisorPlanPredecessor(const std::string &file,const SupervisorControlOptions &next) : fImpl(new Impl(file,next)) {}
SupervisorPlanPredecessor::~SupervisorPlanPredecessor() {}
void SupervisorPlanPredecessor::Activate() {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.mutex);p.CheckLocked();Require(!p.activated,"Original plan retirement already activated");
    Require(!::flock(p.proof.Descriptor(),LOCK_EX|LOCK_NB),"Original retirement proof is held by another activation");
    try {
        p.cycle->ReserveRetiredDeployment();
        const auto next=DecodeSupervisorPlan(p.copy.Bytes());EnsureAbsent(next.options.auditRoot);EnsureAbsent(p.path+".activation.json");
        const auto bytes="{\"schema\":1,\"kind\":\"getdaq.supervisor-plan-activation\",\"proof_sha256\":"+EncodeJsonString(p.sha)+
            ",\"successor_plan_sha256\":"+EncodeJsonString(p.nextPlan)+",\"activation_id\":"+EncodeJsonString(RandomSession())+"}\n";
        WriteNew(p.path+".activation.json",bytes);SealMetadata(p.path+".activation.json");
        Require(!::fsync(p.directory.Descriptor()),"Cannot sync original plan activation marker");
        p.activation.reset(new BoundFile(p.path+".activation.json",65536,0400));p.activated=true;p.CheckLocked();
    } catch (...) {p.faulted=true;throw;}
}
void SupervisorPlanPredecessor::Check() const {std::lock_guard<std::mutex> lock(fImpl->mutex);fImpl->CheckLocked();}
bool SupervisorPlanPredecessor::Activated() const {return fImpl->activated;}
std::string SupervisorPlanPredecessor::Sha256() const {return fImpl->sha;}
std::string SupervisorPlanPredecessor::Json() const {Check();return fImpl->proof.Bytes();}
std::string SupervisorPlanPredecessor::OwnerScopeSha256() const {return fImpl->scope;}
std::string SupervisorPlanPredecessor::OwnerEpoch() const {return fImpl->epoch;}
std::string SupervisorPlanPredecessor::NextPlanSha256() const {return fImpl->nextPlan;}

}
