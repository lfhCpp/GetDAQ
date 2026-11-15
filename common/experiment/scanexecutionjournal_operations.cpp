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

ScanExecutionJournal ScanExecutionJournal::WithOperationIntent(
    const std::string &operationId, const std::string &operation,
    const std::string &requestSha256) const
{
    Require(!fSourceSnapshotSha256.empty() &&
            (fEvents.empty() || fEvents.back().type != Event::ManagedServiceStartIntent),
            "A validated execution snapshot must be bound before an operation");
    Require(fSchemaVersion >= 3 || operation != "adopt-ecc",
            "ECC instance adoption requires a frozen execution snapshot");
    Require(!HasPendingOperation() && fState.State() != ScanPointState::Retired &&
            fState.State() != ScanPointState::NeedsReconciliation,
            "Scan journal cannot begin another operation");
    // A point is not retired merely because its catalog output was bound. The
    // future retirement path must verify ECC shutdown/ownership release and
    // preserve that evidence before it can advance the journal. Keep decoding
    // historical retirement events, but do not let new generic intents claim it.
    Require(operation != "retire",
            "Scan point retirement is unavailable until ECC shutdown evidence is bound");
    Require(fSchemaVersion < 4 || operation != "run-start" || fHasEventCountBaseline,
            "A receiver event baseline must be bound before Run start");
    Require(IsSafeId(operationId) && IsSafeOperation(operation) && IsSha256(requestSha256),
            "Invalid scan operation identity or request digest");
    (void)OperationTarget(fState.State(), operation);
    Require(fSchemaVersion < 3 || operation == "start-ecc" || operation == "adopt-ecc" ||
            !fRuntimeIdentitySha256.empty(),
            "Runtime ECC identity must be bound before point operations");
    Require(fSchemaVersion < 3 || (operation != "start-ecc" && operation != "adopt-ecc") ||
            fRuntimeIdentitySha256.empty(),
            "ECC startup intent follows runtime identity binding");
    for (const Event &event : fEvents)
        if (event.type == Event::OperationIntent)
            Require(event.operationId != operationId, "Scan operation ID already exists; query it instead of replaying");
    Require(fEvents.size() < 4096, "Scan journal event limit reached");
    ScanExecutionJournal result(*this);
    Event event;
    event.type = Event::OperationIntent;
    event.point = fState.PointIndex();
    event.from = fState.State();
    event.operationId = operationId;
    event.operation = operation;
    event.requestSha256 = requestSha256;
    result.fEvents.push_back(event);
    if (operation == "adopt-ecc") result.fSchemaVersion = 5;
    result.fPendingOperationId = operationId;
    result.fPendingOperation = operation;
    result.fPendingRequestSha256 = requestSha256;
    result.fPendingUnknown = false;
    return result;
}

ScanExecutionJournal ScanExecutionJournal::WithOperationComplete(
    const std::string &operationId, const std::string &responseSha256,
    ScanPointState next) const
{
    Require(HasPendingOperation() && !fPendingUnknown &&
            operationId == fPendingOperationId && IsSha256(responseSha256),
            "Scan operation completion does not match its pending intent");
    Require(fPendingOperation != "retire",
            "A generic response cannot prove scan point retirement");
    Require(OperationTarget(fState.State(), fPendingOperation) == next,
            "Scan operation result differs from its command/lifecycle mapping");
    ScanExecutionState updated = fState;
    std::string error;
    Require(updated.Transition(fState.State(), next, &error),
            "Scan operation completion has an illegal resulting state");
    Require(fEvents.size() < 4096, "Scan journal event limit reached");
    ScanExecutionJournal result(*this);
    Event event;
    event.type = Event::OperationComplete;
    event.point = fState.PointIndex();
    event.from = fState.State();
    event.to = next;
    event.operationId = operationId;
    event.responseSha256 = responseSha256;
    result.fEvents.push_back(event);
    result.fState = updated;
    result.fPendingOperationId.clear();
    result.fPendingOperation.clear();
    result.fPendingRequestSha256.clear();
    result.fPendingUnknown = false;
    return result;
}

ScanPointState ScanExecutionJournal::PendingOperationTarget() const
{
    Require(HasPendingOperation() && !fPendingUnknown,
            "Scan journal has no resolvable operation target");
    return OperationTarget(fState.State(), fPendingOperation);
}

}
