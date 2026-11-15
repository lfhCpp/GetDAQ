#include "controlprotocol.h"
#include "supervisorcommon.h"
#include "planio.h"
#include "systemexperimentconfiguration.h"
#include "configurationsha256.h"
#include <limits>
#include <stdexcept>

namespace GetDAQ {
std::string SupervisorCommandName(SupervisorCommand c) {
    switch(c) {
    case SupervisorCommand::Start:return "start";
    case SupervisorCommand::Finish:return "finish";
    case SupervisorCommand::AbortStartup:return "abort-startup";
    case SupervisorCommand::Validate:return "validate";
    case SupervisorCommand::VerifyReady:return "verify-ready";
    case SupervisorCommand::VerifyStopped:return "verify-stopped";
    case SupervisorCommand::Catalog:return "catalog";
    case SupervisorCommand::AbortFailedSession:return "abort-failed-session";
    case SupervisorCommand::RetireFailedSession:return "retire-failed-session";
    case SupervisorCommand::HandoffControl:return "handoff-control";
    case SupervisorCommand::StageSuccessorPlan:return "stage-successor-plan";
    case SupervisorCommand::RetirePlan:return "retire-plan";
    case SupervisorCommand::CompletePlanRetirement:return "complete-plan-retirement";
    case SupervisorCommand::CompleteControlHandoff:return "complete-control-handoff";
    }
    throw std::invalid_argument("Invalid supervisor command");
}
SupervisorCommand ParseSupervisorCommand(const std::string &s) {
    for (const auto c:{SupervisorCommand::Start,SupervisorCommand::Finish,SupervisorCommand::AbortStartup,SupervisorCommand::Validate,SupervisorCommand::VerifyReady,SupervisorCommand::VerifyStopped,SupervisorCommand::Catalog,SupervisorCommand::AbortFailedSession,SupervisorCommand::RetireFailedSession,SupervisorCommand::HandoffControl,SupervisorCommand::StageSuccessorPlan,SupervisorCommand::RetirePlan,SupervisorCommand::CompletePlanRetirement,SupervisorCommand::CompleteControlHandoff})
        if (s==SupervisorCommandName(c)) return c;
    throw std::invalid_argument("Unsupported supervisor command");
}
bool SupervisorCommandRetiresPlan(SupervisorCommand c) { return c==SupervisorCommand::RetirePlan || c==SupervisorCommand::CompletePlanRetirement; }
bool SupervisorCommandMutates(SupervisorCommand c) { return c==SupervisorCommand::Start || c==SupervisorCommand::Finish || c==SupervisorCommand::AbortStartup || c==SupervisorCommand::Catalog || c==SupervisorCommand::AbortFailedSession || c==SupervisorCommand::RetireFailedSession || c==SupervisorCommand::HandoffControl || c==SupervisorCommand::StageSuccessorPlan || c==SupervisorCommand::CompleteControlHandoff || SupervisorCommandRetiresPlan(c); }
std::size_t SupervisorControlRequestMaxBytes(SupervisorCommand command) { return command==SupervisorCommand::StageSuccessorPlan ? 2U*1024U*1024U : 8192U; }
void SupervisorOperationId(const std::string &s) {
    if (s.empty() || s.size()>128 || s.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos)
        throw std::invalid_argument("Invalid supervisor operation ID");
}
void ValidateSupervisorControlRequest(const SupervisorControlRequest &r) {
    using namespace SupervisorIO;
    SupervisorOperationId(r.operationId); Digest(r.planSha256); Digest(r.instanceId); Digest(r.controllerSnapshotSha256); Digest(r.controllerEpoch);
    Digest(r.configurationSha256); Digest(r.artifactSha256); Text(r.controllerId,128); Text(r.parentAuditReference,3500); SupervisorCommandName(r.command);
    if(r.command==SupervisorCommand::AbortFailedSession || r.command==SupervisorCommand::RetireFailedSession || SupervisorCommandRetiresPlan(r.command)) {Digest(r.expectedSessionId);if(!r.parentFailureOperationId.empty()) SupervisorOperationId(r.parentFailureOperationId);}
    else if(!r.expectedSessionId.empty() || !r.parentFailureOperationId.empty()) throw std::invalid_argument("Expected session/parent failure requires failed-session abort/retirement");
    if (SupervisorCommandRetiresPlan(r.command) || r.command==SupervisorCommand::StageSuccessorPlan) {
        Path(r.successorPlanFile);Digest(r.successorPlanSha256);
        if (r.successorPlanSha256==r.planSha256) throw std::invalid_argument("Successor plan must have a different identity");
        if (r.command==SupervisorCommand::RetirePlan && !r.parentFailureOperationId.empty()) throw std::invalid_argument("Plan retirement requires a normal completed cycle");
        if (r.command==SupervisorCommand::StageSuccessorPlan) {
            if (r.successorPlanJson.empty() || r.successorPlanJson.size()>1024U*1024U) throw std::invalid_argument("Successor plan body exceeds its 1 MiB bound");
            const auto plan=DecodeSupervisorPlan(r.successorPlanJson);ValidatePlanOptions(plan.options);
            if (EncodeOptions(plan.options,plan.executableSha256)!=r.successorPlanJson || ConfigurationSha256(r.successorPlanJson)!=r.successorPlanSha256)
                throw std::invalid_argument("Staged successor plan must be canonical and match its SHA-256");
        } else if (!r.successorPlanJson.empty()) throw std::invalid_argument("Plan bytes are only carried by stage-successor-plan");
    } else if (!r.successorPlanFile.empty() || !r.successorPlanSha256.empty() || !r.successorPlanJson.empty()) throw std::invalid_argument("Successor plan fields require a staging or retirement command");
    if (r.command==SupervisorCommand::HandoffControl || r.command==SupervisorCommand::CompleteControlHandoff || SupervisorCommandRetiresPlan(r.command)) {
        Digest(r.successorControllerSnapshotSha256); Digest(r.successorControllerEpoch);
        if (r.successorControllerSnapshotSha256==r.controllerSnapshotSha256 && r.successorControllerEpoch==r.controllerEpoch)
            throw std::invalid_argument("Control handoff requires a different explicit successor");
        if (SupervisorCommandRetiresPlan(r.command) && (r.successorControllerSnapshotSha256==r.controllerSnapshotSha256 || r.successorControllerEpoch==r.controllerEpoch))
            throw std::invalid_argument("Plan retirement requires new successor scope and epoch");
    } else if (!r.successorControllerSnapshotSha256.empty() || !r.successorControllerEpoch.empty()) {
        throw std::invalid_argument("Successor fields require handoff-control");
    }
    if (r.command==SupervisorCommand::CompletePlanRetirement) {
        SupervisorOperationId(r.failedRetirementOperationId);
        if (r.failedRetirementOperationId==r.operationId) throw std::invalid_argument("Completion needs a distinct original failed operation");
    } else if (!r.failedRetirementOperationId.empty()) throw std::invalid_argument("Failed retirement reference requires explicit completion");
    if (r.command==SupervisorCommand::CompleteControlHandoff) {
        SupervisorOperationId(r.failedHandoffOperationId);
        if (r.failedHandoffOperationId==r.operationId) throw std::invalid_argument("Handoff completion needs a distinct original operation");
    } else if (!r.failedHandoffOperationId.empty()) throw std::invalid_argument("Failed handoff reference requires explicit completion");
    if (!r.unusedSourceIds.empty()) {
        if (r.command!=SupervisorCommand::Finish || r.unusedSourceIds.size()>195) throw std::invalid_argument("Unused sources require bounded Finish request");
        std::string previous;
        for (const auto &id:r.unusedSourceIds) { Text(id,128); if (!previous.empty() && id<=previous) throw std::invalid_argument("Unused source IDs must be sorted and unique"); previous=id; }
    }
    if (r.command==SupervisorCommand::Catalog) {
        Text(r.catalogExperiment,80); Digest(r.catalogSessionId);
        if (r.catalogRunNumber>=unsigned(std::numeric_limits<int>::max()) || r.catalogExperiment.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos ||
            r.catalogExperiment[0]=='_' || r.catalogExperiment[0]=='-') throw std::invalid_argument("Catalog requires a safe experiment/Run number");
        if (r.catalogPhase!="single" && r.catalogPhase!="prepare" && r.catalogPhase!="publish" && r.catalogPhase!="finalize") throw std::invalid_argument("Unsupported catalog phase");
        if (!r.catalogReplaceExisting && (r.catalogPhase!="single" || !r.catalogReplacementId.empty())) throw std::invalid_argument("Catalog replacement phase requires explicit replacement authorization");
        if (r.catalogPhase!="single") { SupervisorOperationId(r.catalogReplacementId); }
        else if (!r.catalogReplacementId.empty()) throw std::invalid_argument("Single-phase catalog must not carry a replacement ID");
    } else if (!r.catalogExperiment.empty() || !r.catalogSessionId.empty() || r.catalogRunNumber || r.catalogReplaceExisting) throw std::invalid_argument("Catalog fields require the catalog command");
}
}
