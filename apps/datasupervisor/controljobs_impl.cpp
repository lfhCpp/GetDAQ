#include "controljobs_p.h"
#include "receivercatalog.h"
#include "planio.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include "../../common/text/utf8.h"
#include <cerrno>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unistd.h>

namespace GetDAQ {
namespace {
using namespace SupervisorIO;
DataSupervisorOptions CheckedOptions(const SupervisorControlOptions &o) {
    Text(o.agentId,128); Text(o.controllerId,128); Digest(o.executableSha256); ValidateOptions(o.receiver);
    if (ConfigurationSha256(ReadFile(o.receiver.receiverExecutable,64*1024*1024))!=o.executableSha256) throw std::invalid_argument("Supervisor executable differs from service plan");
    return o.receiver;
}
DataSupervisorOptions CheckedSuccessor(const SupervisorControlOptions &options,const std::shared_ptr<const SupervisorPlanPredecessor> &predecessor) {
    const auto checked=CheckedOptions(options);
    if (predecessor) {
        predecessor->Check();
        if (!predecessor->Activated() || predecessor->NextPlanSha256()!=ConfigurationSha256(EncodeOptions(checked,options.executableSha256)))
            throw std::invalid_argument("Data successor plan is not the activated original retirement target");
        struct stat st;if (::lstat(checked.auditRoot.c_str(),&st)==0 || errno!=ENOENT)
            throw std::runtime_error("Data successor requires a fresh absent audit root");
    }
    return checked;
}

}
SupervisorControl::Impl::Impl(const SupervisorControlOptions &o,std::shared_ptr<const SupervisorPlanPredecessor> previous) : options(o),predecessor(previous),data(CheckedSuccessor(o,previous)),instance(RandomSession()),planHash(data.Status().planSha256) {
        if (DecodeSupervisorPlan(data.PlanBytes()).executableSha256!=o.executableSha256) throw std::runtime_error("Receiver executable changed during control construction");
        directory=o.receiver.auditRoot+"/control";
        std::string controlPolicy="{\"schema\":1,\"kind\":\"getdaq.supervisor-control\",\"agent_id\":"+EncodeJsonString(o.agentId)+",\"controller_id\":"+EncodeJsonString(o.controllerId)+",\"plan_sha256\":"+EncodeJsonString(planHash)+"}\n";
        if (predecessor) controlPolicy.insert(controlPolicy.size()-2,",\"predecessor_proof_sha256\":"+EncodeJsonString(predecessor->Sha256()));
        journal.reset(new SupervisorControlJournal(directory,controlPolicy));
        const bool existing=journal->Existing();
        const auto &journalContent=journal->Content();
        std::size_t begin=0;
        while (begin<journalContent.size()) {
            const auto end=journalContent.find('\n',begin);
            const auto row=journalContent.substr(begin,end-begin);
            JsonLimits limits;limits.stringBytes=4*1024*1024;limits.arrayItems=131072;Replay(ParseStrictJson(row,limits)); begin=end+1;
        }
        needsReconciliation=existing || data.Status().needsReconciliation || planRetired;
        independentHold=needsReconciliation;
        for (auto &entry:jobs) if (entry.second.phase=="accepted" || entry.second.phase=="running") { entry.second.phase="unknown"; entry.second.message="Service restarted with unfinished operation; never replay"; }
        Record("opened",",\"instance_id\":"+EncodeJsonString(instance));
        if (predecessor) {
            ownerSnapshot=predecessor->OwnerScopeSha256();ownerEpoch=predecessor->OwnerEpoch();
            Record("owner",",\"controller_snapshot_sha256\":"+EncodeJsonString(ownerSnapshot)+",\"controller_epoch\":"+EncodeJsonString(ownerEpoch));
        }
    }

SupervisorControl::Impl::~Impl() { if (worker.joinable()) worker.join(); }

void SupervisorControl::Impl::Record(const std::string &event,const std::string &fields) {
        CheckStore();
        const auto row="{\"schema\":1,\"sequence\":"+std::to_string(sequence)+",\"plan_sha256\":"+EncodeJsonString(planHash)+",\"event\":"+EncodeJsonString(event)+fields+"}\n";
        journal->Append(row); ++sequence; CheckStore();
    }

void SupervisorControl::Impl::Run(const SupervisorControlRequest &r,std::shared_ptr<SupervisorNextPlan> next) {
    std::unique_lock<std::mutex> operation(operationMutex);
    const auto key=SupervisorControlRequestKey(r);
    std::string result,message;
    bool complete=false,retirement=false,handoff=false;
    try {
        CheckStore();
        const auto intentPath=directory+"/operations/"+r.operationId+"/request.json";
        const auto checkIntent=[&] { struct stat requestInfo;
            if (::lstat(intentPath.c_str(),&requestInfo) || !S_ISREG(requestInfo.st_mode) || requestInfo.st_uid!=::geteuid() ||
                requestInfo.st_nlink!=1 || (requestInfo.st_mode&0777)!=0600 ||
                ReadFile(intentPath,2U*1024U*1024U)!=EncodeSupervisorControlRequest(r))
                throw std::runtime_error("Accepted control request identity/content changed"); };
        checkIntent();
        Record("running",",\"operation_id\":"+EncodeJsonString(r.operationId)+",\"request_key\":"+EncodeJsonString(key));
        { std::lock_guard<std::mutex> state(stateMutex); jobs.at(r.operationId).phase="running"; }
        Dispatch(r,next,result,retirement,handoff);
        checkIntent();
        complete=true;
    } catch (const std::exception &e) {
        message=EccErrorText(e.what());
        try { result=EncodeSupervisorStatus(data.Status()); } catch (...) { result="{}"; }
    } catch (...) {
        message="Nonstandard supervisor control operation failure";
        try { result=EncodeSupervisorStatus(data.Status()); } catch (...) { result="{}"; }
    }
    bool catalogRecoveryEligible=false;
    if (!complete && r.command==SupervisorCommand::Catalog && r.catalogReplaceExisting &&
        (r.catalogPhase=="publish" || r.catalogPhase=="finalize")) {
        try {
            catalogRecoveryEligible=data.CanResumeCatalogReplacement(r.catalogReplacementId,r.catalogExperiment,
                r.catalogRunNumber,r.catalogSessionId,r.catalogPhase);
        } catch (...) {}
    }
    try {
        std::string fields=",\"operation_id\":"+EncodeJsonString(r.operationId)+",\"request_key\":"+EncodeJsonString(key)+
            ",\"message\":"+EncodeJsonString(message)+",\"result\":"+EncodeJsonString(result);
        if (r.command==SupervisorCommand::Catalog) { std::lock_guard<std::mutex> state(stateMutex);fields+=",\"catalog\":"+EncodeJsonString(jobs.at(r.operationId).catalogResultJson); }
        const auto proofOperation=r.command==SupervisorCommand::CompletePlanRetirement ? r.failedRetirementOperationId : r.command==SupervisorCommand::CompleteControlHandoff ? r.failedHandoffOperationId : r.operationId;
        if (complete && retirement) fields+=",\"plan_retirement_proof_sha256\":"+EncodeJsonString(ConfigurationSha256(ReadFile(directory+"/operations/"+proofOperation+"/plan-retirement.json",4*1024*1024)));
        if (complete && handoff) fields+=",\"handoff_proof_sha256\":"+EncodeJsonString(ConfigurationSha256(ReadFile(directory+"/operations/"+proofOperation+"/handoff-proof.json",1024*1024)));
        Record(complete ? "complete" : "failed",fields);
        std::lock_guard<std::mutex> state(stateMutex);auto &job=jobs.at(r.operationId);
        job.phase=complete ? "complete" : "failed";job.message=message;job.resultStatusJson=result;
        if (r.command==SupervisorCommand::Catalog) job.catalogResultJson=jobs.at(r.operationId).catalogResultJson;
        if (complete && retirement) {
            job.planRetirementProofSha256=ConfigurationSha256(ReadFile(directory+"/operations/"+proofOperation+"/plan-retirement.json",4*1024*1024));
            handoffProofs.emplace(r.operationId,std::make_shared<SupervisorHandoffProof>(directory+"/operations/"+proofOperation+"/plan-retirement.json",job.planRetirementProofSha256));
            if (r.command==SupervisorCommand::CompletePlanRetirement) planRetirementCompletionOperation=r.operationId;
        }
        if (complete && handoff) {
            job.handoffProofSha256=ConfigurationSha256(ReadFile(directory+"/operations/"+proofOperation+"/handoff-proof.json",1024*1024));
            handoffProofs.emplace(proofOperation,std::make_shared<SupervisorHandoffProof>(directory+"/operations/"+proofOperation+"/handoff-proof.json",job.handoffProofSha256));
            ownerSnapshot=r.successorControllerSnapshotSha256;ownerEpoch=r.successorControllerEpoch;
            needsReconciliation=false;independentHold=false;
        }
        // A normally completed retirement is resolved, while a repaired
        // failed retirement retains its reconciliation hold. Both stay
        // permanently fenced by planRetired and cannot accept new commands.
        if (complete && retirement) {
            const bool repairedFailure=r.command==SupervisorCommand::CompletePlanRetirement;
            needsReconciliation=repairedFailure;
            independentHold=repairedFailure;
        }
        if (r.command==SupervisorCommand::CompletePlanRetirement) retirementCompletionUncertain=!complete;
        if (r.command==SupervisorCommand::CompleteControlHandoff) liveHandoffCompletionAllowed=false;
        if (r.command==SupervisorCommand::Catalog && catalogRecoveryInProgress) {
            catalogRecoveryAllowed=false;catalogRecoveryInProgress=false;
            catalogRecoveryReplacementId.clear();catalogRecoverySession.clear();catalogRecoveryExperiment.clear();
            catalogRecoveryPhase.clear();catalogRecoveryRun=0;
            if (complete) { needsReconciliation=false;message.clear(); }
        } else if (r.command==SupervisorCommand::Catalog && catalogRecoveryEligible && !storageFault && !independentHold) {
            catalogRecoveryAllowed=true;catalogRecoveryReplacementId=r.catalogReplacementId;
            catalogRecoverySession=r.catalogSessionId;catalogRecoveryExperiment=r.catalogExperiment;
            catalogRecoveryPhase=r.catalogPhase;catalogRecoveryRun=r.catalogRunNumber;
        }
        if (r.command==SupervisorCommand::Start && !complete) startFailed=true;
        if (r.command==SupervisorCommand::HandoffControl && !complete) {
            const auto lifecycle=data.Status();
            liveHandoffCompletionAllowed=lifecycle.state==DataSupervisorStatus::Stopped && !lifecycle.needsReconciliation && !storageFault;
        }
        if (!complete && SupervisorCommandMutates(r.command)) needsReconciliation=true;
        active=false;currentOperation.clear();
    } catch (const std::exception &e) {
        std::lock_guard<std::mutex> state(stateMutex);storageFault=needsReconciliation=true;active=false;
        message=EccErrorText(e.what());auto found=jobs.find(r.operationId);if(found!=jobs.end()){found->second.phase="unknown";found->second.message=message;}
    }
}
}
