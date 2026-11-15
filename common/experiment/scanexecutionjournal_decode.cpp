#include "scanexecutionjournal.h"
#include "scanexecutionjournalinternal.h"
#include "strictjson.h"
#include "configurationsha256.h"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace GetDAQ {
using namespace ScanExecutionJournalInternal;

ScanExecutionJournal ScanExecutionJournal::Decode(const std::string &bytes)
{
    Require(!bytes.empty() && bytes.size() <= 1024U * 1024U,
            "Scan journal is empty or exceeds 1 MiB");
    JsonLimits limits;
    limits.stringBytes = 4096;
    limits.arrayItems = 4096;
    const JsonValue root = ParseStrictJson(bytes, limits);
    const unsigned int schemaVersion = Number(root.At("schema_version"), 8);
    Require((schemaVersion >= 1 && schemaVersion <= 8) &&
            Text(root.At("kind")) == "getdaq.scan-execution-journal",
            "Unsupported scan journal schema");
    if (schemaVersion == 1)
        ExactKeys(root, {"schema_version", "kind", "execution_id", "plan_sha256",
                         "point_count", "events"});
    else
        ExactKeys(root, {"schema_version", "kind", "execution_id", "plan_sha256",
                         "snapshot_sha256", "point_count", "events"});
    const std::string executionId = Text(root.At("execution_id"));
    const std::string digest = Text(root.At("plan_sha256"));
    const std::string snapshotDigest = schemaVersion >= 2
        ? Text(root.At("snapshot_sha256")) : std::string();
    const unsigned int pointCount = Number(root.At("point_count"), 256);
    Require(IsSafeId(executionId) && IsSha256(digest) && pointCount > 0 &&
            (snapshotDigest.empty() || IsSha256(snapshotDigest)),
            "Invalid scan journal identity or point count");
    Require(root.At("events").type == JsonValue::Array &&
            root.At("events").items.size() <= 4096,
            "Invalid scan journal event list");

    std::vector<Event> events;
    ScanExecutionState state(pointCount);
    std::string pendingOperationId, pendingOperation, pendingRequestSha256;
    bool pendingUnknown = false;
    bool snapshotBindingSeen = false;
    std::string runtimeIdentitySha256;
    ScanEventCountBaseline eventCountBaseline;
    bool hasEventCountBaseline = false;
    bool legacyRetirementHistory = false;
    bool managedServiceStartPending = false;
    bool managedServiceShutdownPending = false;
    Event managedServiceShutdownIntent;
    ReplayContext replayContext = {schemaVersion, pointCount, snapshotDigest, events, state,
        pendingOperationId, pendingOperation, pendingRequestSha256, pendingUnknown,
        snapshotBindingSeen, runtimeIdentitySha256, eventCountBaseline, hasEventCountBaseline,
        legacyRetirementHistory, managedServiceStartPending, managedServiceShutdownPending,
        managedServiceShutdownIntent};
    for (const JsonValue &row : root.At("events").items) {
        const std::string type = Text(row.At("type"));
        Require(!managedServiceStartPending || type == "managed-service-started" ||
                type == "managed-service-start-unknown",
                "Managed ECC startup intent must be resolved before another journal event");
        Event event;
        if (ReplayManagedServiceEvent(type, row, replayContext, event)) {
            events.push_back(event);
            continue;
        }
        if (ReplayOperationEvent(type, row, replayContext, event)) {
            events.push_back(event);
            continue;
        }
        if (ReplayBindingEvent(type, row, replayContext, event)) {
            events.push_back(event);
            continue;
        }
        throw std::invalid_argument("Unsupported scan journal event type");
    }
    // A restart with a durable operation intent but no durable completion is
    // ambiguous. Quarantine it; never infer that the command was not sent.
    if (!pendingOperationId.empty() && !pendingUnknown) {
        std::string error;
        Require(state.Transition(state.State(), ScanPointState::NeedsReconciliation, &error),
                "Cannot quarantine incomplete scan operation");
        pendingUnknown = true;
    }
    if (managedServiceShutdownPending) {
        std::string error;
        Require(state.Transition(state.State(), ScanPointState::NeedsReconciliation, &error),
                "Cannot quarantine an unresolved managed ECC shutdown");
    }
    if (managedServiceStartPending) {
        std::string error;
        Require(state.Transition(state.State(), ScanPointState::NeedsReconciliation, &error),
                "Cannot quarantine an unresolved managed ECC startup");
    }
    Require((schemaVersion == 1 && snapshotDigest.empty() && !snapshotBindingSeen) ||
            ((schemaVersion >= 2 && schemaVersion <= 8) &&
             !snapshotDigest.empty() && snapshotBindingSeen),
            "Scan journal snapshot binding is missing");
    return ScanExecutionJournal(executionId, digest, snapshotDigest,
                                runtimeIdentitySha256, eventCountBaseline,
                                hasEventCountBaseline, schemaVersion, pointCount, events, state,
                                pendingOperationId, pendingOperation,
                                pendingRequestSha256, pendingUnknown,
                                legacyRetirementHistory);
}

} // namespace GetDAQ
