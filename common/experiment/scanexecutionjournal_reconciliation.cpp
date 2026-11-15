#include "scanexecutionjournal.h"
#include "scanexecutionjournalinternal.h"

namespace GetDAQ {
using namespace ScanExecutionJournalInternal;

ScanExecutionJournal ScanExecutionJournal::WithObservedOperationComplete(
    const std::string &operationId, const std::string &responseSha256) const
{
    Require(HasPendingOperation() && fPendingUnknown &&
            fState.State() == ScanPointState::NeedsReconciliation &&
            operationId == fPendingOperationId && IsSha256(responseSha256),
            "Observed scan completion does not match an unknown pending intent");
    Require(fPendingOperation == "describe" || fPendingOperation == "prepare" ||
            fPendingOperation == "configure",
            "Automatic observation recovery is limited to whole-system lifecycle operations");
    ScanPointState origin = ScanPointState::Planned;
    bool foundOrigin = false;
    for (auto it = fEvents.rbegin(); it != fEvents.rend(); ++it) {
        if (it->type == Event::OperationIntent && it->operationId == operationId) {
            origin = it->from;
            foundOrigin = true;
            break;
        }
    }
    Require(foundOrigin, "Observed scan completion has no original intent");
    const ScanPointState target = OperationTarget(origin, fPendingOperation);
    ScanExecutionState updated = fState;
    std::string error;
    Require(updated.ResolveNeedsReconciliation(target, &error),
            "Cannot resolve observed scan operation state");
    Require(fEvents.size() < 4096, "Scan journal event limit reached");
    ScanExecutionJournal result(*this);
    Event event;
    event.type = Event::OperationObservedComplete;
    event.point = fState.PointIndex();
    event.from = origin;
    event.to = target;
    event.operationId = operationId;
    event.responseSha256 = responseSha256;
    result.fEvents.push_back(event);
    result.fState = updated;
    result.fSchemaVersion = 6;
    result.fPendingOperationId.clear();
    result.fPendingOperation.clear();
    result.fPendingRequestSha256.clear();
    result.fPendingUnknown = false;
    return result;
}

ScanExecutionJournal ScanExecutionJournal::WithOperationUnknown(
    const std::string &operationId, const std::string &reasonSha256) const
{
    Require(HasPendingOperation() && !fPendingUnknown &&
            operationId == fPendingOperationId && IsSha256(reasonSha256),
            "Unknown scan operation does not match its pending intent");
    ScanExecutionState updated = fState;
    std::string error;
    Require(updated.Transition(fState.State(), ScanPointState::NeedsReconciliation, &error),
            "Cannot quarantine scan operation from this state");
    Require(fEvents.size() < 4096, "Scan journal event limit reached");
    ScanExecutionJournal result(*this);
    Event event;
    event.type = Event::OperationUnknown;
    event.point = fState.PointIndex();
    event.from = fState.State();
    event.operationId = operationId;
    event.responseSha256 = reasonSha256;
    result.fEvents.push_back(event);
    result.fState = updated;
    result.fPendingUnknown = true;
    return result;
}

ScanExecutionJournal ScanExecutionJournal::WithOperationReconciled(
    const std::string &operationId, const std::string &operatorId,
    ScanOperationDisposition disposition,
    const std::string &evidenceSha256) const
{
    Require(HasPendingOperation() && fPendingUnknown &&
            operationId == fPendingOperationId && IsSafeId(operatorId) &&
            IsSha256(evidenceSha256),
            "Reconciliation does not match an unknown pending operation");
    ScanPointState origin = ScanPointState::Planned;
    bool foundOrigin = false;
    for (auto it = fEvents.rbegin(); it != fEvents.rend(); ++it) {
        if (it->type == Event::OperationIntent && it->operationId == operationId) {
            origin = it->from;
            foundOrigin = true;
            break;
        }
    }
    Require(foundOrigin, "Unknown operation has no original intent");
    ScanPointState resolved = origin;
    if (disposition == ScanOperationDisposition::ConfirmedApplied) {
        Require(fPendingOperation != "retire",
                "Human reconciliation cannot replace scan retirement evidence");
        resolved = OperationTarget(origin, fPendingOperation);
    } else {
        Require(disposition == ScanOperationDisposition::ConfirmedNotSent,
                "Unsupported scan operation reconciliation disposition");
    }

    ScanExecutionState updated = fState;
    std::string error;
    Require(updated.ResolveNeedsReconciliation(resolved, &error),
            "Cannot resolve scan journal quarantine");
    Require(fEvents.size() < 4096, "Scan journal event limit reached");
    ScanExecutionJournal result(*this);
    Event event;
    event.type = Event::OperationReconciled;
    event.point = fState.PointIndex();
    event.from = origin;
    event.to = resolved;
    event.operationId = operationId;
    event.actorId = operatorId;
    event.responseSha256 = evidenceSha256;
    event.operationApplied = disposition == ScanOperationDisposition::ConfirmedApplied;
    result.fEvents.push_back(event);
    result.fState = updated;
    result.fPendingOperationId.clear();
    result.fPendingOperation.clear();
    result.fPendingRequestSha256.clear();
    result.fPendingUnknown = false;
    return result;
}

} // namespace GetDAQ
