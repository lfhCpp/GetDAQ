#include "scanexecutionjournal.h"
#include "scanexecutionjournalinternal.h"
#include "strictjson.h"
#include "configurationsha256.h"

#include <stdexcept>

namespace GetDAQ {
using namespace ScanExecutionJournalInternal;

bool ScanExecutionJournal::ReplayManagedServiceEvent(
    const std::string &type, const JsonValue &row, ReplayContext &context, Event &event)
{
    const unsigned int schemaVersion = context.schemaVersion;
    const unsigned int pointCount = context.pointCount;
    std::vector<Event> &events = context.events;
    ScanExecutionState &state = context.state;
    std::string &pendingOperationId = context.pendingOperationId;
    bool &pendingUnknown = context.pendingUnknown;
    bool &snapshotBindingSeen = context.snapshotBindingSeen;
    std::string &runtimeIdentitySha256 = context.runtimeIdentitySha256;
    bool &hasEventCountBaseline = context.hasEventCountBaseline;
    bool &managedServiceStartPending = context.managedServiceStartPending;
    bool &managedServiceShutdownPending = context.managedServiceShutdownPending;
    Event &managedServiceShutdownIntent = context.managedServiceShutdownIntent;
        if (type == "managed-service-start-intent") {
            ExactKeys(row, {"type", "point_index", "state", "operation_id",
                            "executable_sha256", "service_configuration_sha256"});
            event.type = Event::ManagedServiceStartIntent;
            event.point = Number(row.At("point_index"), pointCount - 1);
            event.from = ParseState(Text(row.At("state")));
            event.operationId = Text(row.At("operation_id"));
            event.executableSha256 = Text(row.At("executable_sha256"));
            event.serviceConfigurationSha256 = Text(row.At("service_configuration_sha256"));
            Require(schemaVersion >= 8 && !managedServiceStartPending && snapshotBindingSeen &&
                    pendingOperationId.empty() && !pendingUnknown && runtimeIdentitySha256.empty() &&
                    event.point == state.PointIndex() && event.from == ScanPointState::Reviewed &&
                    state.State() == ScanPointState::Reviewed && IsSafeId(event.operationId) &&
                    IsSha256(event.executableSha256) &&
                    IsSha256(event.serviceConfigurationSha256),
                    "Invalid or unbound managed ECC startup intent");
            for (const Event &previous : events)
                if (!previous.operationId.empty())
                    Require(previous.operationId != event.operationId,
                            "Duplicate managed ECC startup operation ID");
            managedServiceStartPending = true;
        } else if (type == "managed-service-started") {
            ExactKeys(row, {"type", "point_index", "from", "to", "operation_id",
                            "process_id", "executable_sha256", "service_configuration_sha256"});
            event.type = Event::ManagedServiceStarted;
            event.point = Number(row.At("point_index"), pointCount - 1);
            event.from = ParseState(Text(row.At("from")));
            event.to = ParseState(Text(row.At("to")));
            event.operationId = Text(row.At("operation_id"));
            event.processId = static_cast<int>(Number(row.At("process_id"), 2147483647U));
            event.executableSha256 = Text(row.At("executable_sha256"));
            event.serviceConfigurationSha256 = Text(row.At("service_configuration_sha256"));
            const Event *intent = events.empty() ? nullptr : &events.back();
            Require(schemaVersion >= 8 && managedServiceStartPending && intent &&
                    intent->type == Event::ManagedServiceStartIntent &&
                    event.point == state.PointIndex() && event.from == ScanPointState::Reviewed &&
                    event.to == ScanPointState::EccStarted && state.State() == event.from &&
                    event.operationId == intent->operationId && event.processId > 0 &&
                    event.executableSha256 == intent->executableSha256 &&
                    event.serviceConfigurationSha256 == intent->serviceConfigurationSha256,
                    "Managed ECC startup evidence does not match its persisted intent");
            std::string error;
            Require(state.Transition(event.from, event.to, &error),
                    "Cannot apply managed ECC startup event");
            managedServiceStartPending = false;
        } else if (type == "managed-service-start-unknown") {
            ExactKeys(row, {"type", "point_index", "state", "operation_id", "reason_sha256"});
            event.type = Event::ManagedServiceStartUnknown;
            event.point = Number(row.At("point_index"), pointCount - 1);
            event.from = ParseState(Text(row.At("state")));
            event.operationId = Text(row.At("operation_id"));
            event.responseSha256 = Text(row.At("reason_sha256"));
            const Event *intent = events.empty() ? nullptr : &events.back();
            Require(schemaVersion >= 8 && managedServiceStartPending && intent &&
                    intent->type == Event::ManagedServiceStartIntent &&
                    event.point == state.PointIndex() && event.from == ScanPointState::Reviewed &&
                    state.State() == event.from && event.operationId == intent->operationId &&
                    IsSha256(event.responseSha256),
                    "Unknown managed ECC startup does not match its durable intent");
            std::string error;
            Require(state.Transition(event.from, ScanPointState::NeedsReconciliation, &error),
                    "Cannot quarantine unknown managed ECC startup");
            managedServiceStartPending = false;
        } else if (type == "managed-service-shutdown-intent") {
            ExactKeys(row, {"type", "point_index", "state", "operation_id", "process_id",
                            "service_configuration_sha256", "instance_id",
                            "service_snapshot_sha256", "handoff_operation_id",
                            "shutdown_request_sha256", "runtime_identity_sha256"});
            event.type = Event::ManagedServiceShutdownIntent;
            event.point = Number(row.At("point_index"), pointCount - 1);
            event.from = ParseState(Text(row.At("state")));
            event.operationId = Text(row.At("operation_id"));
            event.processId = static_cast<int>(Number(row.At("process_id"), 2147483647U));
            event.serviceConfigurationSha256 = Text(row.At("service_configuration_sha256"));
            event.instanceId = Text(row.At("instance_id"));
            event.serviceSnapshotSha256 = Text(row.At("service_snapshot_sha256"));
            event.handoffOperationId = Text(row.At("handoff_operation_id"));
            event.requestSha256 = Text(row.At("shutdown_request_sha256"));
            event.runtimeIdentitySha256 = Text(row.At("runtime_identity_sha256"));
            Require(schemaVersion >= 7 && !managedServiceShutdownPending &&
                    snapshotBindingSeen && hasEventCountBaseline &&
                    !runtimeIdentitySha256.empty() && pendingOperationId.empty() && !pendingUnknown &&
                    event.point == state.PointIndex() && event.from == ScanPointState::Bound &&
                    state.State() == ScanPointState::Bound && event.processId > 0 &&
                    IsSafeId(event.operationId) && IsSha256(event.serviceConfigurationSha256) &&
                    IsSha256(event.instanceId) && IsSha256(event.serviceSnapshotSha256) &&
                    IsSafeId(event.handoffOperationId) && IsSha256(event.requestSha256) &&
                    event.runtimeIdentitySha256 == runtimeIdentitySha256 &&
                    event.requestSha256 == ConfigurationSha256(ShutdownRequestBytes(
                        event.instanceId, event.serviceSnapshotSha256, event.handoffOperationId)),
                    "Invalid or unbound managed ECC shutdown intent");
            for (const Event &previous : events)
                if (previous.type == Event::ManagedServiceShutdownIntent)
                    Require(previous.operationId != event.operationId,
                            "Managed ECC retirement operation ID was reused");
            managedServiceShutdownIntent = event;
            managedServiceShutdownPending = true;
        } else if (type == "managed-service-shutdown-unknown") {
            ExactKeys(row, {"type", "point_index", "state", "operation_id", "reason_sha256"});
            event.type = Event::ManagedServiceShutdownUnknown;
            event.point = Number(row.At("point_index"), pointCount - 1);
            event.from = ParseState(Text(row.At("state")));
            event.operationId = Text(row.At("operation_id"));
            event.responseSha256 = Text(row.At("reason_sha256"));
            Require(schemaVersion >= 7 && managedServiceShutdownPending &&
                    event.point == state.PointIndex() && event.from == ScanPointState::Bound &&
                    state.State() == ScanPointState::Bound &&
                    event.operationId == managedServiceShutdownIntent.operationId &&
                    IsSha256(event.responseSha256),
                    "Unknown managed ECC shutdown does not match its durable intent");
            std::string error;
            Require(state.Transition(ScanPointState::Bound,
                                    ScanPointState::NeedsReconciliation, &error),
                    "Cannot replay an unknown managed ECC shutdown");
            managedServiceShutdownPending = false;
        } else if (type == "managed-service-retired") {
            ExactKeys(row, {"type", "point_index", "from", "to", "operation_id", "process_id", "exit_code",
                            "service_configuration_sha256", "instance_id",
                            "service_snapshot_sha256", "handoff_operation_id",
                            "shutdown_request_sha256", "shutdown_response_sha256",
                            "runtime_identity_sha256"});
            event.type = Event::ProcessRetirement;
            event.point = Number(row.At("point_index"), pointCount - 1);
            event.from = ParseState(Text(row.At("from")));
            event.to = ParseState(Text(row.At("to")));
            event.operationId = Text(row.At("operation_id"));
            event.processId = static_cast<int>(Number(row.At("process_id"), 2147483647U));
            event.exitCode = static_cast<int>(Number(row.At("exit_code"), 255U));
            event.serviceConfigurationSha256 = Text(row.At("service_configuration_sha256"));
            event.instanceId = Text(row.At("instance_id"));
            event.serviceSnapshotSha256 = Text(row.At("service_snapshot_sha256"));
            event.handoffOperationId = Text(row.At("handoff_operation_id"));
            event.requestSha256 = Text(row.At("shutdown_request_sha256"));
            event.responseSha256 = Text(row.At("shutdown_response_sha256"));
            event.runtimeIdentitySha256 = Text(row.At("runtime_identity_sha256"));
            Require(schemaVersion >= 7 && snapshotBindingSeen && managedServiceShutdownPending &&
                    !runtimeIdentitySha256.empty() && pendingOperationId.empty() && !pendingUnknown &&
                    event.point == state.PointIndex() &&
                    event.from == ScanPointState::Bound && state.State() == ScanPointState::Bound &&
                    event.to == ScanPointState::Retired && event.processId > 0 && event.exitCode == 0 &&
                    IsSafeId(event.operationId) &&
                    event.operationId == managedServiceShutdownIntent.operationId &&
                    event.processId == managedServiceShutdownIntent.processId &&
                    event.serviceConfigurationSha256 == managedServiceShutdownIntent.serviceConfigurationSha256 &&
                    event.instanceId == managedServiceShutdownIntent.instanceId &&
                    event.serviceSnapshotSha256 == managedServiceShutdownIntent.serviceSnapshotSha256 &&
                    event.handoffOperationId == managedServiceShutdownIntent.handoffOperationId &&
                    IsSha256(event.serviceConfigurationSha256) && IsSha256(event.instanceId) &&
                    IsSha256(event.serviceSnapshotSha256) && IsSafeId(event.handoffOperationId) &&
                    IsSha256(event.requestSha256) && IsSha256(event.responseSha256) &&
                    event.requestSha256 == ConfigurationSha256(ShutdownRequestBytes(
                        event.instanceId, event.serviceSnapshotSha256, event.handoffOperationId)) &&
                    event.responseSha256 == ConfigurationSha256(ShutdownAcceptedBytes(
                        event.instanceId, event.serviceSnapshotSha256, event.handoffOperationId)) &&
                    event.runtimeIdentitySha256 == runtimeIdentitySha256,
                    "Invalid or unbound managed ECC service retirement evidence");
            std::string error;
            Require(state.RetireAfterManagedProcessExit(&error),
                    "Managed ECC service retirement has an invalid lifecycle state");
            managedServiceShutdownPending = false;
        } else {
            return false;
        }
        return true;

}

} // namespace GetDAQ
