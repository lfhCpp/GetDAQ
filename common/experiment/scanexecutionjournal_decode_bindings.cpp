#include "scanexecutionjournal.h"
#include "scanexecutionjournalinternal.h"
#include "strictjson.h"

#include <stdexcept>

namespace GetDAQ {
using namespace ScanExecutionJournalInternal;

bool ScanExecutionJournal::ReplayBindingEvent(
    const std::string &type, const JsonValue &row, ReplayContext &context, Event &event)
{
    const unsigned int schemaVersion = context.schemaVersion;
    const unsigned int pointCount = context.pointCount;
    const std::string &snapshotDigest = context.snapshotDigest;
    ScanExecutionState &state = context.state;
    std::string &pendingOperationId = context.pendingOperationId;
    bool &pendingUnknown = context.pendingUnknown;
    bool &snapshotBindingSeen = context.snapshotBindingSeen;
    std::string &runtimeIdentitySha256 = context.runtimeIdentitySha256;
    ScanEventCountBaseline &eventCountBaseline = context.eventCountBaseline;
    bool &hasEventCountBaseline = context.hasEventCountBaseline;
    bool &legacyRetirementHistory = context.legacyRetirementHistory;
        if (type == "snapshot-bound") {
            ExactKeys(row, {"type", "point_index", "state", "snapshot_sha256"});
            event.type = Event::SnapshotBound;
            event.point = Number(row.At("point_index"), pointCount - 1);
            event.from = ParseState(Text(row.At("state")));
            event.requestSha256 = Text(row.At("snapshot_sha256"));
            Require(schemaVersion >= 2 && !snapshotBindingSeen &&
                    event.point == state.PointIndex() && event.from == state.State() &&
                    (event.from == ScanPointState::Planned ||
                     event.from == ScanPointState::Reviewed) &&
                    event.requestSha256 == snapshotDigest,
                    "Invalid or conflicting scan snapshot binding");
            snapshotBindingSeen = true;
        } else if (type == "runtime-identity-bound") {
            ExactKeys(row, {"type", "point_index", "state", "identity_sha256"});
            event.type = Event::RuntimeIdentityBound;
            event.point = Number(row.At("point_index"), pointCount - 1);
            event.from = ParseState(Text(row.At("state")));
            event.requestSha256 = Text(row.At("identity_sha256"));
            Require(schemaVersion >= 3 && snapshotBindingSeen &&
                    runtimeIdentitySha256.empty() &&
                    pendingOperationId.empty() && !pendingUnknown &&
                    event.point == state.PointIndex() &&
                    event.from == ScanPointState::EccStarted &&
                    state.State() == ScanPointState::EccStarted &&
                    IsSha256(event.requestSha256),
                    "Invalid or repeated scan runtime identity binding");
            runtimeIdentitySha256 = event.requestSha256;
        } else if (type == "event-count-baseline-bound") {
            ExactKeys(row, {"type", "point_index", "state", "runtime_identity_sha256",
                            "instance_id", "snapshot_sha256", "data_binding_sha256",
                            "agent_id", "plan_sha256", "receiver_instance_id", "source_id",
                            "session_id", "pid", "available", "acquisition_events",
                            "observed_at_unix_ms"});
            event.type = Event::EventCountBaselineBound;
            event.point = Number(row.At("point_index"), pointCount - 1);
            event.from = ParseState(Text(row.At("state")));
            event.requestSha256 = Text(row.At("runtime_identity_sha256"));
            event.eventCountBaseline.instanceId = Text(row.At("instance_id"));
            event.eventCountBaseline.snapshotSha256 = Text(row.At("snapshot_sha256"));
            event.eventCountBaseline.dataBindingSha256 = Text(row.At("data_binding_sha256"));
            event.eventCountBaseline.agentId = Text(row.At("agent_id"));
            event.eventCountBaseline.planSha256 = Text(row.At("plan_sha256"));
            event.eventCountBaseline.receiverInstanceId = Text(row.At("receiver_instance_id"));
            event.eventCountBaseline.sourceId = Text(row.At("source_id"));
            event.eventCountBaseline.sessionId = Text(row.At("session_id"));
            event.eventCountBaseline.pid = int(Number(row.At("pid"), 2147483647U));
            event.eventCountBaseline.available = Boolean(row.At("available"));
            event.eventCountBaseline.acquisitionEvents = Number64(row.At("acquisition_events"));
            event.eventCountBaseline.observedAtUnixMs = Number64(row.At("observed_at_unix_ms"));
            ValidateEventCountBaseline(event.eventCountBaseline);
            Require(schemaVersion >= 4 && snapshotBindingSeen &&
                    !runtimeIdentitySha256.empty() && !hasEventCountBaseline &&
                    pendingOperationId.empty() && !pendingUnknown &&
                    event.point == state.PointIndex() &&
                    event.from == ScanPointState::Configured &&
                    state.State() == ScanPointState::Configured &&
                    event.requestSha256 == runtimeIdentitySha256,
                    "Invalid or repeated scan event-count baseline binding");
            eventCountBaseline = event.eventCountBaseline;
            hasEventCountBaseline = true;
        } else if (type == "transition") {
            ExactKeys(row, {"type", "point_index", "from", "to"});
            event.point = Number(row.At("point_index"), pointCount - 1);
            event.from = ParseState(Text(row.At("from")));
            event.to = ParseState(Text(row.At("to")));
            Require(event.point == state.PointIndex() && event.from == state.State(),
                    "Scan journal transition does not continue current state");
            Require(event.from == ScanPointState::Planned && event.to == ScanPointState::Reviewed,
                    "Only operator review may transition without an operation intent");
            std::string error;
            Require(state.Transition(event.from, event.to, &error),
                    "Scan journal contains an illegal transition");
        } else if (type == "next-point") {
            ExactKeys(row, {"type", "from_point", "to_point"});
            event.type = Event::NextPoint;
            event.point = Number(row.At("from_point"), pointCount - 1);
            const unsigned int toPoint = Number(row.At("to_point"), pointCount - 1);
            Require(event.point == state.PointIndex() && toPoint == event.point + 1,
                    "Scan journal next-point sequence is invalid");
            std::string error;
            Require(state.BeginNextPoint(&error), "Scan journal advances before retirement");
            legacyRetirementHistory = false;
            runtimeIdentitySha256.clear();
            eventCountBaseline = ScanEventCountBaseline();
            hasEventCountBaseline = false;
        } else {
            return false;
        }
        return true;

}

} // namespace GetDAQ
