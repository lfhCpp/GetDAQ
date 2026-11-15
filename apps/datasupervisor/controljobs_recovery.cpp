#include "controljobs_p.h"
#include "planio.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include "../../common/text/utf8.h"
#include <map>
#include <set>
#include <stdexcept>
#include <unistd.h>

namespace GetDAQ {
using namespace SupervisorIO;
void SupervisorControl::Impl::CheckRetirementCompletion(const SupervisorControlRequest &r) const {
        const auto found=jobs.find(r.failedRetirementOperationId);
        if (r.command!=SupervisorCommand::CompletePlanRetirement || !planRetired || retirementCompletionUncertain ||
            r.failedRetirementOperationId!=planRetirementOperation || !planRetirementCompletionOperation.empty() ||
            found==jobs.end() || found->second.phase!="failed" || found->second.request.command!=SupervisorCommand::RetirePlan ||
            EncodeSupervisorControlRequest(OriginalPlanRetirementRequest(r))!=EncodeSupervisorControlRequest(found->second.request))
            throw std::runtime_error("Completion requires the exact original failed retirement; target/cycle/owner cannot change");
        for (const auto &entry:jobs)
            if (entry.second.request.command==SupervisorCommand::CompletePlanRetirement &&
                entry.second.request.failedRetirementOperationId==r.failedRetirementOperationId && entry.second.phase!="failed")
                throw std::runtime_error("An earlier retirement completion is unresolved or already complete; observe that ID");
    }

void SupervisorControl::Impl::CheckControlHandoffCompletion(const SupervisorControlRequest &r,bool requireLive) const {
        const auto found=jobs.find(r.failedHandoffOperationId);
        if (r.command!=SupervisorCommand::CompleteControlHandoff ||
            (requireLive && (!liveHandoffCompletionAllowed || storageFault)) ||
            found==jobs.end() || found->second.phase!="failed" || found->second.request.command!=SupervisorCommand::HandoffControl ||
            EncodeSupervisorControlRequest(OriginalControlHandoffRequest(r))!=EncodeSupervisorControlRequest(found->second.request))
            throw std::runtime_error("Completion requires the exact original failed live control handoff; target/plan/owner cannot change");
        for (const auto &entry:jobs)
            if (entry.second.request.command==SupervisorCommand::CompleteControlHandoff &&
                entry.second.request.failedHandoffOperationId==r.failedHandoffOperationId && entry.second.phase!="failed")
                throw std::runtime_error("An earlier handoff completion is unresolved or already complete; observe that ID");
    }

void SupervisorControl::Impl::Replay(const JsonValue &v) {
        if (Unsigned(v.At("schema"))!=1 || Unsigned(v.At("sequence"))!=sequence++ || String(v.At("plan_sha256"))!=planHash) throw std::runtime_error("Control journal scope/sequence differs");
        const auto event=String(v.At("event"));
        if (event=="opened") {
            ExactSupervisorFields(v,{"schema","sequence","plan_sha256","event","instance_id"});
            replayInstance=String(v.At("instance_id")); Digest(replayInstance); return;
        }
        if (event=="owner") {
            ExactSupervisorFields(v,{"schema","sequence","plan_sha256","event","controller_snapshot_sha256","controller_epoch"});
            const auto snapshot=String(v.At("controller_snapshot_sha256")),epoch=String(v.At("controller_epoch")); Digest(snapshot); Digest(epoch);
            if (!ownerSnapshot.empty()) throw std::runtime_error("Control owner repeated/changed without verified recovery");
            ownerSnapshot=snapshot; ownerEpoch=epoch; return;
        }
        const auto id=String(v.At("operation_id")); SupervisorOperationId(id);
        if (event=="accepted") {
            ExactSupervisorFields(v,{"schema","sequence","plan_sha256","event","operation_id","request_key","request"});
            SupervisorControlOperation op; op.request=DecodeSupervisorControlRequest(String(v.At("request"))); op.requestKey=String(v.At("request_key")); op.phase="accepted";
            if (op.request.command==SupervisorCommand::CompletePlanRetirement) CheckRetirementCompletion(op.request);
            else if (op.request.command==SupervisorCommand::CompleteControlHandoff) CheckControlHandoffCompletion(op.request,false);
            else if (planRetired) throw std::runtime_error("Control journal contains a new request after plan retirement");
            if (op.request.instanceId!=replayInstance || (SupervisorCommandMutates(op.request.command) && ownerSnapshot.empty()) ||
                (!ownerSnapshot.empty() && (op.request.controllerSnapshotSha256!=ownerSnapshot || op.request.controllerEpoch!=ownerEpoch)) ||
                op.request.operationId!=id || op.request.planSha256!=planHash || op.request.controllerId!=options.controllerId || op.request.configurationSha256!=options.receiver.configurationSha256 ||
                op.request.artifactSha256!=options.receiver.artifactSha256 || op.requestKey!=SupervisorControlRequestKey(op.request) || !jobs.emplace(id,op).second) throw std::runtime_error("Invalid/repeated control operation");
            if (op.request.command==SupervisorCommand::RetirePlan) {planRetired=true;planRetirementOperation=id;}
            if (op.request.command==SupervisorCommand::CompletePlanRetirement) retirementCompletionUncertain=true;
            return;
        }
        auto found=jobs.find(id); if (found==jobs.end() || String(v.At("request_key"))!=found->second.requestKey) throw std::runtime_error("Control result without bound intent");
        auto &op=found->second;
        if (event=="running" && op.phase=="accepted") {
            ExactSupervisorFields(v,{"schema","sequence","plan_sha256","event","operation_id","request_key"}); op.phase="running"; return;
        }
        if ((event=="complete" || event=="failed" || event=="unknown") && (op.phase=="accepted" || op.phase=="running")) {
            std::set<std::string> fields={"schema","sequence","plan_sha256","event","operation_id","request_key","message","result"};
            if (op.request.command==SupervisorCommand::Catalog) fields.insert("catalog");
            if ((op.request.command==SupervisorCommand::HandoffControl || op.request.command==SupervisorCommand::CompleteControlHandoff) && event=="complete") fields.insert("handoff_proof_sha256");
            if (SupervisorCommandRetiresPlan(op.request.command) && event=="complete") fields.insert("plan_retirement_proof_sha256");
            ExactSupervisorFields(v,fields);
            op.phase=event; op.message=String(v.At("message")); op.resultStatusJson=String(v.At("result"));
            if (op.request.command==SupervisorCommand::CompletePlanRetirement) retirementCompletionUncertain=event=="unknown";
            if (op.request.command==SupervisorCommand::Catalog) { op.catalogResultJson=String(v.At("catalog")); if (!op.catalogResultJson.empty()) ParseStrictJson(op.catalogResultJson); }
            if (DecodeSupervisorStatus(ParseStrictJson(op.resultStatusJson)).planSha256!=planHash) throw std::runtime_error("Control result plan differs");
            if (SupervisorCommandRetiresPlan(op.request.command) && event=="complete") {
                op.planRetirementProofSha256=String(v.At("plan_retirement_proof_sha256"));Digest(op.planRetirementProofSha256);
                const auto original=op.request.command==SupervisorCommand::CompletePlanRetirement ? op.request.failedRetirementOperationId : id;
                const auto path=directory+"/operations/"+original+"/plan-retirement.json";
                handoffProofs.emplace(id,std::make_shared<SupervisorHandoffProof>(path,op.planRetirementProofSha256));
                if (op.request.command==SupervisorCommand::CompletePlanRetirement) planRetirementCompletionOperation=id;
            }
            if ((op.request.command==SupervisorCommand::HandoffControl || op.request.command==SupervisorCommand::CompleteControlHandoff) && event=="complete") {
                const auto status=DecodeSupervisorStatus(ParseStrictJson(op.resultStatusJson));
                if (status.state!=DataSupervisorStatus::Stopped || status.needsReconciliation || status.receivers.empty())
                    throw std::runtime_error("Replayed handoff lacks original normally stopped session");
                for (const auto &receiver:status.receivers) if (!receiver.reaped || !receiver.sealVerified || receiver.exitCode!=0)
                    throw std::runtime_error("Replayed handoff contains unsealed or running receiver");
                op.handoffProofSha256=String(v.At("handoff_proof_sha256"));
                const auto original=op.request.command==SupervisorCommand::CompleteControlHandoff ? op.request.failedHandoffOperationId : id;
                const auto originalRequest=op.request.command==SupervisorCommand::CompleteControlHandoff ? OriginalControlHandoffRequest(op.request) : op.request;
                const auto path=directory+"/operations/"+original+"/handoff-proof.json";
                handoffProofs.emplace(original,std::make_shared<SupervisorHandoffProof>(path,op.handoffProofSha256));
                const auto proof=ParseStrictJson(ReadFile(path,1024*1024));
                ExactSupervisorFields(proof,{"schema","kind","operation_id","request_key","instance_id","plan_sha256","controller_id","controller_snapshot_sha256","controller_epoch","successor_controller_snapshot_sha256","successor_controller_epoch","lifecycle"});
                if (Unsigned(proof.At("schema"))!=1 || String(proof.At("kind"))!="getdaq.supervisor-control-handoff" ||
                    String(proof.At("operation_id"))!=original || String(proof.At("request_key"))!=SupervisorControlRequestKey(originalRequest) ||
                    String(proof.At("instance_id"))!=originalRequest.instanceId || String(proof.At("plan_sha256"))!=planHash ||
                    String(proof.At("controller_id"))!=options.controllerId || String(proof.At("controller_snapshot_sha256"))!=originalRequest.controllerSnapshotSha256 ||
                    String(proof.At("controller_epoch"))!=originalRequest.controllerEpoch || String(proof.At("successor_controller_snapshot_sha256"))!=originalRequest.successorControllerSnapshotSha256 ||
                    String(proof.At("successor_controller_epoch"))!=originalRequest.successorControllerEpoch ||
                    EncodeSupervisorStatus(DecodeSupervisorStatus(proof.At("lifecycle")))!=op.resultStatusJson)
                    throw std::runtime_error("Replayed controller handoff proof binding differs");
                if (op.request.command==SupervisorCommand::HandoffControl && op.requestKey!=SupervisorControlRequestKey(originalRequest))
                    throw std::runtime_error("Original handoff proof request key differs");
                ownerSnapshot=originalRequest.successorControllerSnapshotSha256;
                ownerEpoch=originalRequest.successorControllerEpoch;
            }

            return;
        }
        throw std::runtime_error("Invalid control journal event/transition");
    }

void SupervisorControl::Impl::CheckStore() {
        if (storageFault) throw std::runtime_error("Supervisor control storage fault is latched");
        try {
            if (predecessor) predecessor->Check();
            for (const auto &entry:handoffProofs) entry.second->Check();
            journal->Check();
        } catch (...) {
            std::lock_guard<std::mutex> lock(stateMutex); storageFault=needsReconciliation=true; message="Supervisor control storage changed; restoration does not clear gate"; throw;
        }
    }


}
