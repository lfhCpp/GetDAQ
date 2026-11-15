#include "scanexecutionjournal.h"
#include "scanexecutionjournalinternal.h"
#include "strictjson.h"

#include <stdexcept>

namespace GetDAQ {
using namespace ScanExecutionJournalInternal;

bool ScanExecutionJournal::ReplayOperationEvent(
    const std::string &type, const JsonValue &row, ReplayContext &context, Event &event)
{
    const unsigned int schemaVersion = context.schemaVersion;
    const unsigned int pointCount = context.pointCount;
    std::vector<Event> &events = context.events;
    ScanExecutionState &state = context.state;
    std::string &pendingOperationId = context.pendingOperationId;
    std::string &pendingOperation = context.pendingOperation;
    std::string &pendingRequestSha256 = context.pendingRequestSha256;
    bool &pendingUnknown = context.pendingUnknown;
    bool &snapshotBindingSeen = context.snapshotBindingSeen;
    std::string &runtimeIdentitySha256 = context.runtimeIdentitySha256;
    bool &hasEventCountBaseline = context.hasEventCountBaseline;
    bool &legacyRetirementHistory = context.legacyRetirementHistory;
        if (type == "operation-intent") {
            ExactKeys(row, {"type", "point_index", "state", "operation_id",
                            "operation", "request_sha256"});
            event.type = Event::OperationIntent;
            event.point = Number(row.At("point_index"), pointCount - 1);
            event.from = ParseState(Text(row.At("state")));
            event.operationId = Text(row.At("operation_id"));
            event.operation = Text(row.At("operation"));
            event.requestSha256 = Text(row.At("request_sha256"));
            Require((schemaVersion == 1 || snapshotBindingSeen) &&
                    !pendingOperationId.size() && !pendingUnknown &&
                    event.point == state.PointIndex() && event.from == state.State() &&
                    event.from != ScanPointState::Retired &&
                    event.from != ScanPointState::NeedsReconciliation &&
                    IsSafeId(event.operationId) && IsSafeOperation(event.operation) &&
                    IsSha256(event.requestSha256), "Invalid or overlapping scan operation intent");
            Require(schemaVersion < 3 || event.operation == "start-ecc" ||
                    event.operation == "adopt-ecc" ||
                    !runtimeIdentitySha256.empty(),
                    "Scan operation intent precedes runtime identity binding");
            Require(schemaVersion < 3 || (event.operation != "start-ecc" &&
                    event.operation != "adopt-ecc") ||
                    runtimeIdentitySha256.empty(),
                    "ECC startup intent follows runtime identity binding");
            Require(schemaVersion < 4 || event.operation != "run-start" ||
                    hasEventCountBaseline,
                    "Scan Run intent has no durable receiver event baseline");
            Require(schemaVersion >= 5 || event.operation != "adopt-ecc",
                    "ECC instance adoption requires scan journal schema 5");
            (void)OperationTarget(event.from, event.operation);
            for (const Event &previous : events)
                if (previous.type == Event::OperationIntent)
                    Require(previous.operationId != event.operationId, "Duplicate scan operation ID");
            pendingOperationId = event.operationId;
            pendingOperation = event.operation;
            pendingRequestSha256 = event.requestSha256;
        } else if (type == "operation-complete") {
            ExactKeys(row, {"type", "point_index", "operation_id", "from", "to",
                            "response_sha256"});
            event.type = Event::OperationComplete;
            event.point = Number(row.At("point_index"), pointCount - 1);
            event.operationId = Text(row.At("operation_id"));
            event.from = ParseState(Text(row.At("from")));
            event.to = ParseState(Text(row.At("to")));
            event.responseSha256 = Text(row.At("response_sha256"));
            Require(!pendingOperationId.empty() && !pendingUnknown &&
                    event.operationId == pendingOperationId && event.point == state.PointIndex() &&
                    event.from == state.State() && IsSha256(event.responseSha256),
                    "Scan operation completion has no matching intent");
            Require(OperationTarget(event.from, pendingOperation) == event.to,
                    "Scan operation result differs from its command/lifecycle mapping");
            std::string error;
            if (pendingOperation == "retire") {
                Require(event.to == ScanPointState::Retired &&
                        state.ReplayLegacyRetirement(&error),
                        "Legacy scan retirement completion is invalid");
                legacyRetirementHistory = true;
            } else {
                Require(state.Transition(event.from, event.to, &error),
                        "Scan operation completion contains an illegal transition");
            }
            pendingOperationId.clear(); pendingOperation.clear(); pendingRequestSha256.clear();
        } else if (type == "operation-unknown") {
            ExactKeys(row, {"type", "point_index", "state", "operation_id", "reason_sha256"});
            event.type = Event::OperationUnknown;
            event.point = Number(row.At("point_index"), pointCount - 1);
            event.from = ParseState(Text(row.At("state")));
            event.operationId = Text(row.At("operation_id"));
            event.responseSha256 = Text(row.At("reason_sha256"));
            Require(!pendingOperationId.empty() && !pendingUnknown &&
                    event.operationId == pendingOperationId && event.point == state.PointIndex() &&
                    event.from == state.State() && IsSha256(event.responseSha256),
                    "Unknown scan operation has no matching intent");
            std::string error;
            Require(state.Transition(event.from, ScanPointState::NeedsReconciliation, &error),
                    "Cannot quarantine unknown scan operation");
            pendingUnknown = true;
        } else if (type == "operation-observed-complete") {
            ExactKeys(row, {"type", "point_index", "operation_id", "from", "to",
                            "response_sha256"});
            event.type = Event::OperationObservedComplete;
            event.point = Number(row.At("point_index"), pointCount - 1);
            event.operationId = Text(row.At("operation_id"));
            event.from = ParseState(Text(row.At("from")));
            event.to = ParseState(Text(row.At("to")));
            event.responseSha256 = Text(row.At("response_sha256"));
            const bool lifecycle = pendingOperation == "describe" ||
                pendingOperation == "prepare" || pendingOperation == "configure";
            const bool quarantined = pendingUnknown &&
                state.State() == ScanPointState::NeedsReconciliation;
            const bool recoveredIntent = !pendingUnknown &&
                state.State() == event.from;
            const Event *intent = nullptr;
            for (auto it = events.rbegin(); it != events.rend(); ++it)
                if (it->type == Event::OperationIntent &&
                    it->operationId == pendingOperationId) { intent = &*it; break; }
            Require(schemaVersion >= 6 && (quarantined || recoveredIntent) && lifecycle &&
                    !pendingOperationId.empty() &&
                    event.operationId == pendingOperationId && intent != nullptr &&
                    event.from == intent->from && event.point == state.PointIndex() &&
                    event.to == OperationTarget(event.from, pendingOperation) &&
                    IsSha256(event.responseSha256),
                    "Observed completion does not resolve the exact quarantined lifecycle intent");
            std::string error;
            if (quarantined)
                Require(state.ResolveNeedsReconciliation(event.to, &error),
                        "Cannot resolve observed scan lifecycle completion");
            else
                Require(state.Transition(event.from, event.to, &error),
                        "Cannot apply observed scan lifecycle completion");
            pendingOperationId.clear(); pendingOperation.clear();
            pendingRequestSha256.clear(); pendingUnknown = false;
        } else if (type == "operation-reconciled") {
            ExactKeys(row, {"type", "point_index", "from", "to", "operation_id", "operator_id",
                            "disposition", "evidence_sha256"});
            event.type = Event::OperationReconciled;
            event.point = Number(row.At("point_index"), pointCount - 1);
            event.from = ParseState(Text(row.At("from")));
            event.to = ParseState(Text(row.At("to")));
            event.operationId = Text(row.At("operation_id"));
            event.actorId = Text(row.At("operator_id"));
            const std::string disposition = Text(row.At("disposition"));
            event.responseSha256 = Text(row.At("evidence_sha256"));
            const bool quarantined = pendingUnknown &&
                state.State() == ScanPointState::NeedsReconciliation;
            const bool recoveredIntent = !pendingUnknown &&
                state.State() == event.from;
            Require(!pendingOperationId.empty() &&
                    event.operationId == pendingOperationId &&
                    event.point == state.PointIndex() && (quarantined || recoveredIntent) &&
                    IsSafeId(event.actorId) && IsSha256(event.responseSha256),
                    "Scan operation reconciliation has no matching unknown intent");
            const Event *intent = nullptr;
            for (auto it = events.rbegin(); it != events.rend(); ++it)
                if (it->type == Event::OperationIntent &&
                    it->operationId == pendingOperationId) { intent = &*it; break; }
            Require(intent != nullptr && event.from == intent->from,
                    "Scan reconciliation origin differs from its intent");
            if (disposition == "confirmed-applied") {
                event.operationApplied = true;
                Require(OperationTarget(event.from, pendingOperation) == event.to,
                        "Applied reconciliation differs from operation mapping");
            } else if (disposition == "confirmed-not-sent") {
                event.operationApplied = false;
                Require(event.to == event.from,
                        "Not-sent reconciliation must restore the pre-operation state");
            } else {
                throw std::invalid_argument("Unsupported scan reconciliation disposition");
            }
            std::string error;
            if (quarantined) {
                if (event.operationApplied && pendingOperation == "retire") {
                    Require(event.to == ScanPointState::Retired &&
                            state.ResolveNeedsReconciliation(event.to, &error),
                            "Legacy reconciled retirement is invalid");
                    legacyRetirementHistory = true;
                } else {
                    Require(state.ResolveNeedsReconciliation(event.to, &error),
                            "Cannot resolve scan journal quarantine");
                }
            } else if (event.operationApplied) {
                if (pendingOperation == "retire") {
                    Require(event.to == ScanPointState::Retired &&
                            state.ReplayLegacyRetirement(&error),
                            "Legacy reconciled retirement is invalid");
                    legacyRetirementHistory = true;
                } else {
                    Require(state.Transition(event.from, event.to, &error),
                            "Cannot apply reconciled scan operation");
                }
            }
            pendingOperationId.clear(); pendingOperation.clear();
            pendingRequestSha256.clear(); pendingUnknown = false;
        } else {
            return false;
        }
        return true;

}

} // namespace GetDAQ
