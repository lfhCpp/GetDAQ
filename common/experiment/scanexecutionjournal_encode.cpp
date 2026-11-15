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

std::string ScanExecutionJournal::Encode() const
{
    Require(fEvents.size() <= 4096, "Scan journal event limit exceeded");
    const unsigned int schemaVersion = fSourceSnapshotSha256.empty() ? 1U :
        std::max(2U, fSchemaVersion);
    std::string bytes = "{\"schema_version\":" + std::to_string(schemaVersion) +
        ",\"kind\":\"getdaq.scan-execution-journal\",\"execution_id\":\"" +
        Escape(fExecutionId) + "\",\"plan_sha256\":\"" + fSourcePlanSha256 +
        "\"";
    if (schemaVersion >= 2U)
        bytes += ",\"snapshot_sha256\":\"" + fSourceSnapshotSha256 + "\"";
    bytes += ",\"point_count\":" + std::to_string(fState.PointCount()) +
        ",\"events\":[";
    for (std::size_t i = 0; i < fEvents.size(); ++i) {
        if (i) bytes += ',';
        const Event &event = fEvents[i];
        if (event.type == Event::NextPoint) {
            bytes += "{\"type\":\"next-point\",\"from_point\":" +
                std::to_string(event.point) + ",\"to_point\":" +
                std::to_string(event.point + 1) + "}";
        } else if (event.type == Event::SnapshotBound) {
            bytes += "{\"type\":\"snapshot-bound\",\"point_index\":" +
                std::to_string(event.point) + ",\"state\":\"" +
                ScanPointStateName(event.from) + "\",\"snapshot_sha256\":\"" +
                event.requestSha256 + "\"}";
        } else if (event.type == Event::RuntimeIdentityBound) {
            bytes += "{\"type\":\"runtime-identity-bound\",\"point_index\":" +
                std::to_string(event.point) + ",\"state\":\"" +
                ScanPointStateName(event.from) + "\",\"identity_sha256\":\"" +
                event.requestSha256 + "\"}";
        } else if (event.type == Event::ManagedServiceStartIntent) {
            bytes += "{\"type\":\"managed-service-start-intent\",\"point_index\":" +
                std::to_string(event.point) + ",\"state\":\"" +
                ScanPointStateName(event.from) + "\",\"operation_id\":\"" +
                Escape(event.operationId) + "\",\"executable_sha256\":\"" +
                event.executableSha256 + "\",\"service_configuration_sha256\":\"" +
                event.serviceConfigurationSha256 + "\"}";
        } else if (event.type == Event::ManagedServiceStarted) {
            bytes += "{\"type\":\"managed-service-started\",\"point_index\":" +
                std::to_string(event.point) + ",\"from\":\"" +
                ScanPointStateName(event.from) + "\",\"to\":\"" +
                ScanPointStateName(event.to) + "\",\"operation_id\":\"" +
                Escape(event.operationId) + "\",\"process_id\":" +
                std::to_string(event.processId) + ",\"executable_sha256\":\"" +
                event.executableSha256 + "\",\"service_configuration_sha256\":\"" +
                event.serviceConfigurationSha256 + "\"}";
        } else if (event.type == Event::ManagedServiceStartUnknown) {
            bytes += "{\"type\":\"managed-service-start-unknown\",\"point_index\":" +
                std::to_string(event.point) + ",\"state\":\"" +
                ScanPointStateName(event.from) + "\",\"operation_id\":\"" +
                Escape(event.operationId) + "\",\"reason_sha256\":\"" +
                event.responseSha256 + "\"}";
        } else if (event.type == Event::ManagedServiceShutdownIntent) {
            bytes += "{\"type\":\"managed-service-shutdown-intent\",\"point_index\":" +
                std::to_string(event.point) + ",\"state\":\"" +
                ScanPointStateName(event.from) + "\",\"operation_id\":\"" +
                Escape(event.operationId) + "\",\"process_id\":" +
                std::to_string(event.processId) + ",\"service_configuration_sha256\":\"" +
                event.serviceConfigurationSha256 + "\",\"instance_id\":\"" +
                event.instanceId + "\",\"service_snapshot_sha256\":\"" +
                event.serviceSnapshotSha256 + "\",\"handoff_operation_id\":\"" +
                Escape(event.handoffOperationId) + "\",\"shutdown_request_sha256\":\"" +
                event.requestSha256 + "\",\"runtime_identity_sha256\":\"" +
                event.runtimeIdentitySha256 + "\"}";
        } else if (event.type == Event::ManagedServiceShutdownUnknown) {
            bytes += "{\"type\":\"managed-service-shutdown-unknown\",\"point_index\":" +
                std::to_string(event.point) + ",\"state\":\"" +
                ScanPointStateName(event.from) + "\",\"operation_id\":\"" +
                Escape(event.operationId) + "\",\"reason_sha256\":\"" +
                event.responseSha256 + "\"}";
        } else if (event.type == Event::ProcessRetirement) {
            bytes += "{\"type\":\"managed-service-retired\",\"point_index\":" +
                std::to_string(event.point) + ",\"from\":\"" +
                ScanPointStateName(event.from) + "\",\"to\":\"" +
                ScanPointStateName(event.to) + "\",\"operation_id\":\"" +
                Escape(event.operationId) + "\",\"process_id\":" +
                std::to_string(event.processId) + ",\"exit_code\":" +
                std::to_string(event.exitCode) + ",\"service_configuration_sha256\":\"" +
                event.serviceConfigurationSha256 + "\",\"instance_id\":\"" +
                event.instanceId + "\",\"service_snapshot_sha256\":\"" +
                event.serviceSnapshotSha256 + "\",\"handoff_operation_id\":\"" +
                Escape(event.handoffOperationId) + "\",\"shutdown_request_sha256\":\"" +
                event.requestSha256 + "\",\"shutdown_response_sha256\":\"" +
                event.responseSha256 + "\",\"runtime_identity_sha256\":\"" +
                event.runtimeIdentitySha256 + "\"}";
        } else if (event.type == Event::EventCountBaselineBound) {
            const ScanEventCountBaseline &baseline = event.eventCountBaseline;
            bytes += "{\"type\":\"event-count-baseline-bound\",\"point_index\":" +
                std::to_string(event.point) + ",\"state\":\"" +
                ScanPointStateName(event.from) + "\",\"runtime_identity_sha256\":\"" +
                event.requestSha256 + "\",\"instance_id\":\"" + baseline.instanceId +
                "\",\"snapshot_sha256\":\"" + baseline.snapshotSha256 +
                "\",\"data_binding_sha256\":\"" + baseline.dataBindingSha256 +
                "\",\"agent_id\":\"" + Escape(baseline.agentId) +
                "\",\"plan_sha256\":\"" + baseline.planSha256 +
                "\",\"receiver_instance_id\":\"" + baseline.receiverInstanceId +
                "\",\"source_id\":\"" + Escape(baseline.sourceId) +
                "\",\"session_id\":\"" + baseline.sessionId +
                "\",\"pid\":" + std::to_string(baseline.pid) +
                ",\"available\":true,\"acquisition_events\":\"" +
                std::to_string(static_cast<unsigned long long>(baseline.acquisitionEvents)) +
                "\",\"observed_at_unix_ms\":\"" +
                std::to_string(static_cast<unsigned long long>(baseline.observedAtUnixMs)) +
                "\"}";
        } else if (event.type == Event::Transition) {
            bytes += "{\"type\":\"transition\",\"point_index\":" +
                std::to_string(event.point) + ",\"from\":\"" +
                ScanPointStateName(event.from) + "\",\"to\":\"" +
                ScanPointStateName(event.to) + "\"}";
        } else if (event.type == Event::OperationIntent) {
            bytes += "{\"type\":\"operation-intent\",\"point_index\":" +
                std::to_string(event.point) + ",\"state\":\"" +
                ScanPointStateName(event.from) + "\",\"operation_id\":\"" +
                Escape(event.operationId) + "\",\"operation\":\"" + Escape(event.operation) +
                "\",\"request_sha256\":\"" + event.requestSha256 + "\"}";
        } else if (event.type == Event::OperationComplete) {
            bytes += "{\"type\":\"operation-complete\",\"point_index\":" +
                std::to_string(event.point) + ",\"operation_id\":\"" +
                Escape(event.operationId) + "\",\"from\":\"" +
                ScanPointStateName(event.from) + "\",\"to\":\"" +
                ScanPointStateName(event.to) + "\",\"response_sha256\":\"" +
                event.responseSha256 + "\"}";
        } else if (event.type == Event::OperationUnknown) {
            bytes += "{\"type\":\"operation-unknown\",\"point_index\":" +
                std::to_string(event.point) + ",\"state\":\"" +
                ScanPointStateName(event.from) + "\",\"operation_id\":\"" +
                Escape(event.operationId) + "\",\"reason_sha256\":\"" +
                event.responseSha256 + "\"}";
        } else if (event.type == Event::OperationObservedComplete) {
            bytes += "{\"type\":\"operation-observed-complete\",\"point_index\":" +
                std::to_string(event.point) + ",\"operation_id\":\"" +
                Escape(event.operationId) + "\",\"from\":\"" +
                ScanPointStateName(event.from) + "\",\"to\":\"" +
                ScanPointStateName(event.to) + "\",\"response_sha256\":\"" +
                event.responseSha256 + "\"}";
        } else {
            bytes += "{\"type\":\"operation-reconciled\",\"point_index\":" +
                std::to_string(event.point) + ",\"from\":\"" +
                ScanPointStateName(event.from) + "\",\"to\":\"" +
                ScanPointStateName(event.to) + "\",\"operation_id\":\"" +
                Escape(event.operationId) + "\",\"operator_id\":\"" +
                Escape(event.actorId) + "\",\"disposition\":\"" +
                (event.operationApplied ? "confirmed-applied" : "confirmed-not-sent") +
                "\",\"evidence_sha256\":\"" + event.responseSha256 + "\"}";
        }
        Require(bytes.size() <= 1024U * 1024U, "Scan journal exceeds 1 MiB");
    }
    bytes += "]}";
    Require(bytes.size() <= 1024U * 1024U, "Scan journal exceeds 1 MiB");
    return bytes;
}

} // namespace GetDAQ
