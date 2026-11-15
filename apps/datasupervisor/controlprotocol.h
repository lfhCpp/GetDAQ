#ifndef GETDAQ_SUPERVISORCONTROLPROTOCOL_H
#define GETDAQ_SUPERVISORCONTROLPROTOCOL_H
#include "datasupervisor.h"
#include "strictjson.h"
namespace GetDAQ {
enum class SupervisorCommand { Start, Finish, AbortStartup, Validate, VerifyReady, VerifyStopped, Catalog, AbortFailedSession, RetireFailedSession, HandoffControl, StageSuccessorPlan, RetirePlan, CompletePlanRetirement, CompleteControlHandoff };
std::string SupervisorCommandName(SupervisorCommand command);
SupervisorCommand ParseSupervisorCommand(const std::string &name);
bool SupervisorCommandMutates(SupervisorCommand command);
bool SupervisorCommandRetiresPlan(SupervisorCommand command);
struct SupervisorControlRequest {
    std::string operationId,planSha256,instanceId,controllerId,controllerSnapshotSha256,controllerEpoch;
    std::string configurationSha256,artifactSha256,parentAuditReference;
    std::string catalogExperiment,catalogSessionId;
    std::string catalogPhase="single",catalogReplacementId;
    std::string expectedSessionId; // Required for explicit failed-session abort/retirement.
    std::string parentFailureOperationId; // Optional original-owner declaration of system failure; not hardware verification.
    // Explicit original-owner handoff, same immutable plan; never an expiry/takeover.
    std::string successorControllerSnapshotSha256,successorControllerEpoch;
    std::string successorPlanFile,successorPlanSha256; // Target plan path/hash; retire-plan or stage-successor-plan.
    std::string successorPlanJson; // Canonical plan body only for authenticated stage-successor-plan.
    std::string failedRetirementOperationId; // Explicit metadata completion of the exact failed retirement.
    std::string failedHandoffOperationId; // Live-instance completion of the exact failed control handoff.
    unsigned int catalogRunNumber=0;
    bool catalogReplaceExisting=false;
    std::vector<std::string> unusedSourceIds; // 仅Finish，非空进入规范请求/摘要；不是自动猜测无数据。
    SupervisorCommand command=SupervisorCommand::Validate;
};
void ValidateSupervisorControlRequest(const SupervisorControlRequest &request);
std::size_t SupervisorControlRequestMaxBytes(SupervisorCommand command);
SupervisorControlRequest DecodeSupervisorControlRequest(const std::string &bytes);
std::string EncodeSupervisorControlRequest(const SupervisorControlRequest &request);
std::string SupervisorControlRequestKey(const SupervisorControlRequest &request);
SupervisorControlRequest OriginalPlanRetirementRequest(const SupervisorControlRequest &completion);
SupervisorControlRequest OriginalControlHandoffRequest(const SupervisorControlRequest &completion);
std::string EncodeSupervisorStatus(const DataSupervisorStatus &status);
DataSupervisorStatus DecodeSupervisorStatus(const JsonValue &value);
std::string EncodeSupervisorObservation(const DataSupervisorObservation &observation);
DataSupervisorObservation DecodeSupervisorObservation(const JsonValue &value);
void SupervisorOperationId(const std::string &id);
}
#endif
