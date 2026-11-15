#include "scanexecutionjournal.h"
#include "scanexecutionjournalinternal.h"
#include "strictjson.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"

#include <algorithm>

namespace GetDAQ {
using namespace ScanExecutionJournalInternal;

ScanExecutionJournal ScanExecutionJournal::WithManagedServiceStartIntent(
    const std::string &operationId, const std::string &executableSha256,
    const std::string &serviceConfigurationSha256) const
{
    Require(fSchemaVersion >= 3 && !fSourceSnapshotSha256.empty() &&
            fState.State() == ScanPointState::Reviewed && !HasPendingOperation() &&
            !fPendingUnknown && fRuntimeIdentitySha256.empty(),
            "Managed ECC startup requires a reviewed, snapshot-bound scan point");
    Require(IsSafeId(operationId) && IsSha256(executableSha256) &&
            IsSha256(serviceConfigurationSha256),
            "Managed ECC startup intent has invalid deployment identity");
    for (const Event &event : fEvents)
        if (!event.operationId.empty())
            Require(event.operationId != operationId,
                    "Managed ECC startup operation ID has already been used");
    Require(fEvents.size() < 4096, "Scan journal event limit reached");
    ScanExecutionJournal result(*this);
    Event event;
    event.type = Event::ManagedServiceStartIntent;
    event.point = fState.PointIndex();
    event.from = ScanPointState::Reviewed;
    event.operationId = operationId;
    event.executableSha256 = executableSha256;
    event.serviceConfigurationSha256 = serviceConfigurationSha256;
    result.fEvents.push_back(event);
    result.fSchemaVersion = 8;
    return result;
}

ScanExecutionJournal ScanExecutionJournal::WithManagedServiceStarted(
    const EccServiceProcessStartEvidence &evidence) const
{
    Require(fSchemaVersion >= 8 && fState.State() == ScanPointState::Reviewed &&
            !HasPendingOperation() && !fPendingUnknown && !fEvents.empty() &&
            fEvents.back().type == Event::ManagedServiceStartIntent,
            "Managed ECC start evidence requires its durable startup intent");
    const Event &intent = fEvents.back();
    Require(evidence.ProcessId() > 0 && evidence.OperationId() == intent.operationId &&
            evidence.ExecutableSha256() == intent.executableSha256 &&
            evidence.ServiceConfigurationSha256() == intent.serviceConfigurationSha256 &&
            evidence.ExecutionId() == fExecutionId &&
            evidence.ExecutionSnapshotSha256() == fSourceSnapshotSha256 &&
            evidence.PointIndex() == fState.PointIndex(),
            "Managed ECC start evidence belongs to another deployment or scan point");
    Require(fEvents.size() < 4096, "Scan journal event limit reached");
    ScanExecutionState updated = fState;
    std::string error;
    Require(updated.Transition(ScanPointState::Reviewed, ScanPointState::EccStarted, &error),
            "Managed ECC startup cannot advance this scan point");
    ScanExecutionJournal result(*this);
    Event event;
    event.type = Event::ManagedServiceStarted;
    event.point = fState.PointIndex();
    event.from = ScanPointState::Reviewed;
    event.to = ScanPointState::EccStarted;
    event.operationId = evidence.OperationId();
    event.processId = evidence.ProcessId();
    event.executableSha256 = evidence.ExecutableSha256();
    event.serviceConfigurationSha256 = evidence.ServiceConfigurationSha256();
    result.fEvents.push_back(event);
    result.fState = updated;
    return result;
}

ScanExecutionJournal ScanExecutionJournal::WithManagedServiceStartUnknown(
    const std::string &reasonSha256) const
{
    Require(fSchemaVersion >= 8 && fState.State() == ScanPointState::Reviewed &&
            !HasPendingOperation() && !fPendingUnknown && !fEvents.empty() &&
            fEvents.back().type == Event::ManagedServiceStartIntent &&
            IsSha256(reasonSha256),
            "Unknown managed ECC startup must match a durable startup intent");
    Require(fEvents.size() < 4096, "Scan journal event limit reached");
    ScanExecutionState updated = fState;
    std::string error;
    Require(updated.Transition(ScanPointState::Reviewed,
                               ScanPointState::NeedsReconciliation, &error),
            "Cannot quarantine an unknown managed ECC startup");
    ScanExecutionJournal result(*this);
    Event event;
    event.type = Event::ManagedServiceStartUnknown;
    event.point = fState.PointIndex();
    event.from = ScanPointState::Reviewed;
    event.operationId = fEvents.back().operationId;
    event.responseSha256 = reasonSha256;
    result.fEvents.push_back(event);
    result.fState = updated;
    return result;
}

ScanExecutionJournal ScanExecutionJournal::WithManagedServiceRetirement(
    const EccServiceProcessExitEvidence &evidence) const
{
    Require(fSchemaVersion >= 3 && !fSourceSnapshotSha256.empty() &&
            fState.State() == ScanPointState::Bound &&
            !HasPendingOperation() && !fPendingUnknown,
            "Managed ECC service retirement requires a resolved, snapshot-bound Bound point");
    Require(!fEvents.empty() &&
            fEvents.back().type == Event::ManagedServiceShutdownIntent,
            "Managed ECC service shutdown intent must be durable before retirement");
    const Event &intent = fEvents.back();
    Require(evidence.ProcessId() > 0 && evidence.ExitCode() == 0 &&
            IsSha256(evidence.ServiceConfigurationSha256()) &&
            IsSha256(evidence.InstanceId()) && IsSha256(evidence.ServiceSnapshotSha256()) &&
            IsSafeId(evidence.HandoffOperationId()) &&
            evidence.RetirementOperationId() == intent.operationId &&
            evidence.ProcessId() == intent.processId &&
            evidence.ServiceConfigurationSha256() == intent.serviceConfigurationSha256 &&
            evidence.InstanceId() == intent.instanceId &&
            evidence.ServiceSnapshotSha256() == intent.serviceSnapshotSha256 &&
            evidence.HandoffOperationId() == intent.handoffOperationId &&
            IsSha256(evidence.ShutdownRequestSha256()) &&
            IsSha256(evidence.ShutdownResponseSha256()) &&
            evidence.ExecutionId() == fExecutionId &&
            evidence.ExecutionSnapshotSha256() == fSourceSnapshotSha256 &&
            evidence.PointIndex() == fState.PointIndex() &&
            evidence.RuntimeIdentitySha256() == fRuntimeIdentitySha256 &&
            evidence.ShutdownRequestSha256() == ConfigurationSha256(ShutdownRequestBytes(
                evidence.InstanceId(), evidence.ServiceSnapshotSha256(),
                evidence.HandoffOperationId())) &&
            evidence.ShutdownResponseSha256() == ConfigurationSha256(ShutdownAcceptedBytes(
                evidence.InstanceId(), evidence.ServiceSnapshotSha256(),
                evidence.HandoffOperationId())) &&
            !fRuntimeIdentitySha256.empty(),
            "Managed ECC process exit evidence belongs to another scan point or lacks identity");
    Require(fEvents.size() < 4096, "Scan journal event limit reached");
    ScanExecutionState updated = fState;
    std::string error;
    Require(updated.RetireAfterManagedProcessExit(&error),
            "Managed ECC process exit cannot retire the current scan point");
    ScanExecutionJournal result(*this);
    Event event;
    event.type = Event::ProcessRetirement;
    event.point = fState.PointIndex();
    event.operationId = evidence.RetirementOperationId();
    event.from = ScanPointState::Bound;
    event.to = ScanPointState::Retired;
    event.processId = evidence.ProcessId();
    event.exitCode = evidence.ExitCode();
    event.serviceConfigurationSha256 = evidence.ServiceConfigurationSha256();
    event.instanceId = evidence.InstanceId();
    event.serviceSnapshotSha256 = evidence.ServiceSnapshotSha256();
    event.handoffOperationId = evidence.HandoffOperationId();
    event.requestSha256 = evidence.ShutdownRequestSha256();
    event.responseSha256 = evidence.ShutdownResponseSha256();
    event.runtimeIdentitySha256 = evidence.RuntimeIdentitySha256();
    result.fEvents.push_back(event);
    result.fState = updated;
    result.fSchemaVersion = std::max(7U, result.fSchemaVersion);
    return result;
}

ScanExecutionJournal ScanExecutionJournal::WithManagedServiceShutdownIntent(
    const std::string &retirementOperationId, int processId,
    const std::string &serviceConfigurationSha256,
    const std::string &instanceId, const std::string &serviceSnapshotSha256,
    const std::string &handoffOperationId) const
{
    Require(fSchemaVersion >= 3 && !fSourceSnapshotSha256.empty() &&
            fState.State() == ScanPointState::Bound &&
            !HasPendingOperation() && !fPendingUnknown && fHasEventCountBaseline &&
            !fRuntimeIdentitySha256.empty() &&
            fEventCountBaseline.instanceId == instanceId &&
            fEventCountBaseline.snapshotSha256 == serviceSnapshotSha256,
            "Managed ECC shutdown intent requires a fully bound Run point");
    Require(fEvents.empty() || fEvents.back().type != Event::ManagedServiceShutdownIntent,
            "A managed ECC shutdown intent is already pending");
    Require(IsSafeId(retirementOperationId) && processId > 0 &&
            IsSha256(serviceConfigurationSha256) && IsSha256(instanceId) &&
            IsSha256(serviceSnapshotSha256) && IsSafeId(handoffOperationId),
            "Invalid managed ECC shutdown intent identity");
    for (const Event &previous : fEvents)
        if (previous.type == Event::ManagedServiceShutdownIntent)
            Require(previous.operationId != retirementOperationId,
                    "Managed ECC retirement operation ID has already been used");
    Require(fEvents.size() < 4096, "Scan journal event limit reached");
    ScanExecutionJournal result(*this);
    Event event;
    event.type = Event::ManagedServiceShutdownIntent;
    event.point = fState.PointIndex();
    event.from = ScanPointState::Bound;
    event.operationId = retirementOperationId;
    event.processId = processId;
    event.serviceConfigurationSha256 = serviceConfigurationSha256;
    event.instanceId = instanceId;
    event.serviceSnapshotSha256 = serviceSnapshotSha256;
    event.handoffOperationId = handoffOperationId;
    event.runtimeIdentitySha256 = fRuntimeIdentitySha256;
    event.requestSha256 = ConfigurationSha256(ShutdownRequestBytes(
        instanceId, serviceSnapshotSha256, handoffOperationId));
    result.fEvents.push_back(event);
    result.fSchemaVersion = std::max(7U, result.fSchemaVersion);
    return result;
}

ScanExecutionJournal ScanExecutionJournal::WithManagedServiceShutdownUnknown(
    const std::string &reasonSha256) const
{
    Require(fSchemaVersion >= 7 && fState.State() == ScanPointState::Bound &&
            !HasPendingOperation() && !fPendingUnknown && !fEvents.empty() &&
            fEvents.back().type == Event::ManagedServiceShutdownIntent &&
            IsSha256(reasonSha256),
            "Unknown managed ECC shutdown must match a persisted shutdown intent");
    Require(fEvents.size() < 4096, "Scan journal event limit reached");
    ScanExecutionState updated = fState;
    std::string error;
    Require(updated.Transition(ScanPointState::Bound,
                               ScanPointState::NeedsReconciliation, &error),
            "Cannot quarantine an unknown managed ECC shutdown");
    ScanExecutionJournal result(*this);
    Event event;
    event.type = Event::ManagedServiceShutdownUnknown;
    event.point = fState.PointIndex();
    event.from = ScanPointState::Bound;
    event.operationId = fEvents.back().operationId;
    event.responseSha256 = reasonSha256;
    result.fEvents.push_back(event);
    result.fState = updated;
    return result;
}


} // namespace GetDAQ
