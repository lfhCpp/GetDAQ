#include "scanexecutionjournal.h"
#include "scanexecutionjournalinternal.h"

#include "strictjson.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

namespace GetDAQ {
using namespace ScanExecutionJournalInternal;


ScanExecutionJournal::ScanExecutionJournal(const std::string &executionId,
                                           const std::string &sourcePlanSha256,
                                           unsigned int pointCount)
    : fExecutionId(executionId), fSourcePlanSha256(sourcePlanSha256), fState(pointCount)
{
    Require(IsSafeId(executionId), "Invalid scan execution ID");
    Require(IsSha256(sourcePlanSha256), "Invalid source scan-plan SHA-256");
}

ScanExecutionJournal::ScanExecutionJournal(const std::string &executionId,
                                           const std::string &sourcePlanSha256,
                                           const std::string &sourceSnapshotSha256,
                                           unsigned int pointCount)
    : ScanExecutionJournal(executionId, sourcePlanSha256, pointCount)
{
    *this = WithSnapshotBinding(sourceSnapshotSha256);
}

ScanExecutionJournal::ScanExecutionJournal(const std::string &executionId,
                                           const std::string &sourcePlanSha256,
                                           const std::string &sourceSnapshotSha256,
                                           const std::string &runtimeIdentitySha256,
                                           const ScanEventCountBaseline &eventCountBaseline,
                                           bool hasEventCountBaseline,
                                           unsigned int schemaVersion,
                                           unsigned int pointCount,
                                           const std::vector<Event> &events,
                                           const ScanExecutionState &state,
                                           const std::string &pendingOperationId,
                                           const std::string &pendingOperation,
                                           const std::string &pendingRequestSha256,
                                           bool pendingUnknown,
                                           bool legacyRetirementHistory)
    : fExecutionId(executionId), fSourcePlanSha256(sourcePlanSha256),
      fSourceSnapshotSha256(sourceSnapshotSha256),
      fRuntimeIdentitySha256(runtimeIdentitySha256),
      fEventCountBaseline(eventCountBaseline),
      fHasEventCountBaseline(hasEventCountBaseline), fSchemaVersion(schemaVersion),
      fEvents(events), fState(state), fPendingOperationId(pendingOperationId),
      fPendingOperation(pendingOperation), fPendingRequestSha256(pendingRequestSha256),
      fPendingUnknown(pendingUnknown),
      fLegacyRetirementHistory(legacyRetirementHistory)
{
    Require(IsSafeId(executionId) && IsSha256(sourcePlanSha256),
            "Invalid scan execution journal identity");
    Require(pointCount == state.PointCount(), "Scan journal point count differs");
}

ScanExecutionJournal ScanExecutionJournal::WithTransition(
    ScanPointState expected, ScanPointState next) const
{
    Require(!fPendingOperationId.empty() ||
            (fPendingOperation.empty() && fPendingRequestSha256.empty() && !fPendingUnknown),
            "Incomplete scan journal pending operation");
    ScanExecutionState updated = fState;
    std::string error;
    Require(!HasPendingOperation(), "Cannot change scan state while an operation is pending");
    Require(expected == ScanPointState::Planned && next == ScanPointState::Reviewed,
            "Only operator review may advance without an operation intent");
    Require(updated.Transition(expected, next, &error), "Illegal scan journal transition");
    Require(fEvents.size() < 4096, "Scan journal event limit reached");
    ScanExecutionJournal result(*this);
    Event event;
    event.type = Event::Transition;
    event.point = fState.PointIndex();
    event.from = expected;
    event.to = next;
    result.fEvents.push_back(event);
    result.fState = updated;
    return result;
}

ScanExecutionJournal ScanExecutionJournal::WithNextPoint() const
{
    ScanExecutionState updated = fState;
    std::string error;
    Require(!fLegacyRetirementHistory,
            "Historical retirement cannot authorize advancing to another point");
    Require(!HasPendingOperation(), "Cannot advance while an operation is pending");
    Require(updated.BeginNextPoint(&error), "Cannot advance scan journal point");
    Require(fEvents.size() < 4096, "Scan journal event limit reached");
    ScanExecutionJournal result(*this);
    Event event;
    event.type = Event::NextPoint;
    event.point = fState.PointIndex();
    result.fEvents.push_back(event);
    result.fState = updated;
    result.fRuntimeIdentitySha256.clear();
    result.fEventCountBaseline = ScanEventCountBaseline();
    result.fHasEventCountBaseline = false;
    return result;
}

ScanExecutionJournal ScanExecutionJournal::WithSnapshotBinding(
    const std::string &snapshotSha256) const
{
    Require(IsSha256(snapshotSha256), "Invalid scan execution snapshot SHA-256");
    if (!fSourceSnapshotSha256.empty()) {
        Require(fSourceSnapshotSha256 == snapshotSha256,
                "Scan execution snapshot binding is immutable");
        return *this;
    }
    Require(!HasPendingOperation() &&
            (fState.State() == ScanPointState::Planned ||
             fState.State() == ScanPointState::Reviewed),
            "A snapshot can only be bound before scan execution begins");
    for (const Event &event : fEvents)
        Require(event.type != Event::OperationIntent &&
                event.type != Event::ManagedServiceStartIntent,
                "A scan journal with operations cannot be retroactively bound");
    Require(fEvents.size() < 4096, "Scan journal event limit reached");
    ScanExecutionJournal result(*this);
    result.fSourceSnapshotSha256 = snapshotSha256;
    result.fSchemaVersion = 3;
    Event event;
    event.type = Event::SnapshotBound;
    event.point = fState.PointIndex();
    event.from = fState.State();
    event.requestSha256 = snapshotSha256;
    result.fEvents.push_back(event);
    return result;
}

ScanExecutionJournal ScanExecutionJournal::WithRuntimeIdentityBinding(
    const std::string &identitySha256) const
{
    Require(fSchemaVersion >= 3 && !fSourceSnapshotSha256.empty(),
            "A version-3 execution snapshot must be bound before runtime identity");
    Require(IsSha256(identitySha256), "Invalid scan runtime identity SHA-256");
    Require(fState.State() == ScanPointState::EccStarted &&
            !HasPendingOperation() && fRuntimeIdentitySha256.empty(),
            "Runtime identity can only be bound once after ECC startup");
    Require(fEvents.size() < 4096, "Scan journal event limit reached");
    ScanExecutionJournal result(*this);
    result.fRuntimeIdentitySha256 = identitySha256;
    Event event;
    event.type = Event::RuntimeIdentityBound;
    event.point = fState.PointIndex();
    event.from = fState.State();
    event.requestSha256 = identitySha256;
    result.fEvents.push_back(event);
    return result;
}

ScanExecutionJournal ScanExecutionJournal::WithEventCountBaseline(
    const ScanEventCountBaseline &baseline) const
{
    ValidateEventCountBaseline(baseline);
    Require(fSchemaVersion >= 3 && !fRuntimeIdentitySha256.empty() &&
            fState.State() == ScanPointState::Configured &&
            !HasPendingOperation() && !fHasEventCountBaseline,
            "Event-count baseline can only be bound once before a configured Run");
    Require(fEvents.size() < 4096, "Scan journal event limit reached");
    ScanExecutionJournal result(*this);
    result.fEventCountBaseline = baseline;
    result.fHasEventCountBaseline = true;
    result.fSchemaVersion = std::max(4U, result.fSchemaVersion);
    Event event;
    event.type = Event::EventCountBaselineBound;
    event.point = fState.PointIndex();
    event.from = fState.State();
    event.eventCountBaseline = baseline;
    event.requestSha256 = fRuntimeIdentitySha256;
    result.fEvents.push_back(event);
    return result;
}



}
