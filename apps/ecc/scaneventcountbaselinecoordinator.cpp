#include "scaneventcountbaselinecoordinator.h"
#include "../../common/experiment/configurationsha256.h"

#include <stdexcept>

namespace GetDAQ {
namespace {

void Require(bool condition, const char *message)
{
    if (!condition) throw std::invalid_argument(message);
}

}

ScanEventCountBaselineCoordinator::ScanEventCountBaselineCoordinator(
    ScanExecutionJournalStore &store,
    const ScanExecutionSnapshot &snapshot,
    const ScanRuntimeIdentityExpectation &expected)
    : fStore(store), fSnapshot(snapshot), fExpected(expected)
{
    (void)PrepareQuery();
}

std::string ScanEventCountBaselineCoordinator::PrepareQuery() const
{
    const ScanExecutionJournal current = fStore.Current();
    Require(current.State() == ScanPointState::Configured &&
            !current.HasPendingOperation() && !current.PendingOperationUnknown(),
            "Receiver event baseline requires a durably configured point with no pending operation");
    Require(!current.RuntimeIdentitySha256().empty() &&
            !current.HasEventCountBaseline(),
            "Receiver event baseline requires a bound runtime identity and no prior baseline");
    Require(fSnapshot.ExecutionId() == current.ExecutionId() &&
            fSnapshot.SourcePlanSha256() == current.SourcePlanSha256() &&
            current.SourceSnapshotSha256() == ConfigurationSha256(fSnapshot.Encode()) &&
            fSnapshot.PointCount() == current.PointCount(),
            "Receiver event baseline snapshot differs from the journal's frozen execution input");
    Require(fExpected.executionSnapshotSha256 == current.SourceSnapshotSha256() &&
            fExpected.pointIndex == current.PointIndex() &&
            fExpected.pointIndex < fSnapshot.Points().size(),
            "Receiver event baseline expectation differs from the journal's current scan point");
    const ScanExecutionSnapshotPoint &point = fSnapshot.Points()[fExpected.pointIndex];
    Require(fExpected.configurationSha256 == point.configurationSha256 &&
            fExpected.artifactSha256 == point.artifactSha256,
            "Receiver event baseline expectation differs from the frozen point artifacts");
    Require(!fSnapshot.TargetCoBo().empty(),
            "Receiver event baseline scan point has no selected CoBo source");
    return fSnapshot.TargetCoBo();
}

ScanExecutionJournal ScanEventCountBaselineCoordinator::BindResponse(
    const std::string &responseBytes,
    const EccControlReceiverObservation &currentReceivers)
{
    const std::string sourceId = PrepareQuery();
    const EccControlReceiverEvents events =
        DecodeEccControlReceiverEvents(responseBytes);
    Require(events.instanceId == fExpected.instanceId &&
            events.snapshotSha256 == fExpected.snapshotSha256 &&
            events.dataBindingSha256 == fExpected.dataBindingSha256 &&
            events.sourceId == sourceId,
            "Receiver event response differs from the fixed ECC deployment or selected scan source");
    Require(events.available,
            "Receiver acquisition event counter is unavailable for this scan source");
    Require(currentReceivers.instanceId == fExpected.instanceId &&
            currentReceivers.snapshotSha256 == fExpected.snapshotSha256 &&
            currentReceivers.dataBindingSha256 == fExpected.dataBindingSha256,
            "Current receiver observation differs from the fixed ECC deployment");

    const ScanRuntimeDataNodeExpectation *matched = nullptr;
    for (const ScanRuntimeDataNodeExpectation &node : fExpected.dataNodes) {
        if (node.agentId == events.agentId &&
            node.instanceId == events.receiverInstanceId &&
            node.planSha256 == events.planSha256) {
            if (matched) throw std::invalid_argument(
                "Receiver event response matches multiple fixed data nodes");
            matched = &node;
        }
    }
    Require(matched != nullptr,
            "Receiver event response differs from every fixed agent/instance/plan binding");

    const EccControlReceiverNodeObservation *observedNode = nullptr;
    for (const EccControlReceiverNodeObservation &node : currentReceivers.nodes) {
        if (node.agentId != events.agentId) continue;
        if (observedNode) throw std::invalid_argument(
            "Receiver agent is duplicated in the current lifecycle observation");
        observedNode = &node;
    }
    Require(observedNode && observedNode->available && !observedNode->busy &&
            observedNode->observedAtUnixMs > 0 &&
            observedNode->instanceId == matched->instanceId &&
            observedNode->planSha256 == matched->planSha256 &&
            observedNode->lifecycle.agentId == matched->agentId &&
            observedNode->lifecycle.instanceId == matched->instanceId &&
            !observedNode->lifecycle.needsReconciliation &&
            observedNode->lifecycle.planSha256 == matched->planSha256 &&
            observedNode->lifecycle.state == "ready" &&
            !observedNode->lifecycle.sessionId.empty() &&
            observedNode->lifecycle.sessionId == events.sessionId,
            "Receiver event response session differs from the fresh, idle lifecycle observation");
    const EccControlReceiverProcess *process = nullptr;
    for (const EccControlReceiverProcess &candidate : observedNode->receivers) {
        if (candidate.id != events.sourceId) continue;
        if (process) throw std::invalid_argument(
            "Selected CoBo receiver is duplicated in the lifecycle observation");
        process = &candidate;
    }
    Require(process && process->state == "running" && process->pid > 0 &&
            process->pid == events.pid,
            "Receiver event response PID differs from the fresh running process observation");

    const ScanExecutionJournal current = fStore.Current();
    ScanEventCountBaseline baseline;
    baseline.instanceId = events.instanceId;
    baseline.snapshotSha256 = events.snapshotSha256;
    baseline.dataBindingSha256 = events.dataBindingSha256;
    baseline.agentId = events.agentId;
    baseline.planSha256 = events.planSha256;
    baseline.receiverInstanceId = events.receiverInstanceId;
    baseline.sourceId = events.sourceId;
    baseline.sessionId = events.sessionId;
    baseline.pid = events.pid;
    baseline.available = events.available;
    baseline.acquisitionEvents = events.acquisitionEvents;
    baseline.observedAtUnixMs = events.observedAtUnixMs;

    const ScanExecutionJournal next = current.WithEventCountBaseline(baseline);
    fStore.Persist(next);
    return next;
}

}
