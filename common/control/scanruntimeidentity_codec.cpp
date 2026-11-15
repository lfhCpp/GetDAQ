#include "scanruntimeidentity.h"
#include "scanruntimeidentityinternal_p.h"

#include "eccprotocol.h"
#include "strictjson.h"

#include <set>
#include <stdexcept>

namespace GetDAQ {
namespace {
using namespace ScanRuntimeIdentityInternal;
}

std::string EncodeScanRuntimePointIdentityRequest(
    const ScanRuntimePointIdentityRequest &request)
{
    Require(request.mode == "adopt" || request.mode == "revalidate",
            "Scan runtime identity request mode is unsupported");
    Require(request.expectedControllerState >= 1 &&
            request.expectedControllerState <= 4,
            "Expected ECC controller state is outside the scan lifecycle");
    Require((request.mode == "adopt" && request.expectedControllerState == 1) ||
            request.mode == "revalidate",
            "ECC adoption is only valid from the Idle state");
    RequireDigest(request.executionSnapshotSha256,
                  "Scan execution snapshot SHA-256 is invalid");
    Require(request.pointIndex <= 255,
            "Scan runtime identity point index is out of range");
    RequireDigest(request.configurationSha256,
                  "Scan point configuration SHA-256 is invalid");
    RequireDigest(request.artifactSha256,
                  "Scan point artifact SHA-256 is invalid");
    return "{\"schema_version\":2,\"kind\":\"getdaq.scan-runtime-point-identity-request\",\"mode\":" +
        Quote(request.mode) + ",\"expected_controller_state\":" +
        std::to_string(request.expectedControllerState) + ",\"execution_snapshot_sha256\":" +
        Quote(request.executionSnapshotSha256) + ",\"point_index\":" +
        std::to_string(request.pointIndex) + ",\"configuration_sha256\":" +
        Quote(request.configurationSha256) + ",\"artifact_sha256\":" +
        Quote(request.artifactSha256) + "}\n";
}

ScanRuntimePointIdentityRequest DecodeScanRuntimePointIdentityRequest(
    const std::string &bytes)
{
    JsonLimits limits; limits.stringBytes = 1024; limits.arrayItems = 8;
    const JsonValue root = ParseStrictJson(bytes, limits);
    const unsigned int version = Number(root.At("schema_version"), 2U);
    if (version == 1U)
        ExactKeys(root, {"schema_version", "kind", "execution_snapshot_sha256",
                         "point_index", "configuration_sha256", "artifact_sha256"});
    else
        ExactKeys(root, {"schema_version", "kind", "mode", "expected_controller_state",
                         "execution_snapshot_sha256", "point_index",
                         "configuration_sha256", "artifact_sha256"});
    Require((version == 1U || version == 2U) &&
            Text(root.At("kind")) == "getdaq.scan-runtime-point-identity-request",
            "Unsupported scan runtime point identity request");
    ScanRuntimePointIdentityRequest request;
    if (version == 2U) {
        request.mode = Text(root.At("mode"));
        request.expectedControllerState = Number(root.At("expected_controller_state"), 4U);
    }
    request.executionSnapshotSha256 = Text(root.At("execution_snapshot_sha256"));
    request.pointIndex = Number(root.At("point_index"), 255U);
    request.configurationSha256 = Text(root.At("configuration_sha256"));
    request.artifactSha256 = Text(root.At("artifact_sha256"));
    (void)EncodeScanRuntimePointIdentityRequest(request);
    return request;
}

std::string EncodeScanRuntimeIdentityExpectation(
    const ScanRuntimeIdentityExpectation &expected)
{
    ValidateExpectationFields(expected);
    std::string bytes = "{\"schema_version\":1,\"kind\":\"getdaq.scan-runtime-identity-expectation\",\"execution_snapshot_sha256\":" +
        Quote(expected.executionSnapshotSha256) + ",\"point_index\":" +
        std::to_string(expected.pointIndex) + ",\"service_id\":" + Quote(expected.serviceId) +
        ",\"client_id\":" + Quote(expected.clientId) + ",\"instance_id\":" + Quote(expected.instanceId) +
        ",\"snapshot_sha256\":" + Quote(expected.snapshotSha256) +
        ",\"input_binding_sha256\":" + Quote(expected.inputBindingSha256) +
        ",\"configuration_sha256\":" + Quote(expected.configurationSha256) +
        ",\"artifact_sha256\":" + Quote(expected.artifactSha256) +
        ",\"data_binding_sha256\":" + Quote(expected.dataBindingSha256) +
        ",\"data_scope_sha256\":" + Quote(expected.dataScopeSha256) +
        ",\"data_epoch\":" + Quote(expected.dataEpoch) + ",\"data_nodes\":[";
    for (const ScanRuntimeDataNodeExpectation &node : expected.dataNodes) {
        if (bytes.back() != '[') bytes += ',';
        bytes += "{\"agent_id\":" + Quote(node.agentId) +
            ",\"instance_id\":" + Quote(node.instanceId) +
            ",\"plan_sha256\":" + Quote(node.planSha256) + "}";
    }
    return bytes + "]}\n";
}

ScanRuntimeIdentityExpectation DecodeScanRuntimeIdentityExpectation(
    const std::string &bytes)
{
    JsonLimits limits; limits.stringBytes = 16384; limits.arrayItems = 256;
    const JsonValue root = ParseStrictJson(bytes, limits);
    ExactKeys(root, {"schema_version", "kind", "execution_snapshot_sha256",
                     "point_index", "service_id", "client_id", "instance_id",
                     "snapshot_sha256", "input_binding_sha256", "configuration_sha256",
                     "artifact_sha256", "data_binding_sha256", "data_scope_sha256",
                     "data_epoch", "data_nodes"});
    Require(Number(root.At("schema_version"), 1U) == 1U &&
            Text(root.At("kind")) == "getdaq.scan-runtime-identity-expectation",
            "Unsupported scan runtime identity expectation");
    ScanRuntimeIdentityExpectation expected;
    expected.executionSnapshotSha256 = Text(root.At("execution_snapshot_sha256"));
    expected.pointIndex = Number(root.At("point_index"), 255U);
    expected.serviceId = Text(root.At("service_id"));
    expected.clientId = Text(root.At("client_id"));
    expected.instanceId = Text(root.At("instance_id"));
    expected.snapshotSha256 = Text(root.At("snapshot_sha256"));
    expected.inputBindingSha256 = Text(root.At("input_binding_sha256"));
    expected.configurationSha256 = Text(root.At("configuration_sha256"));
    expected.artifactSha256 = Text(root.At("artifact_sha256"));
    expected.dataBindingSha256 = Text(root.At("data_binding_sha256"));
    expected.dataScopeSha256 = Text(root.At("data_scope_sha256"));
    expected.dataEpoch = Text(root.At("data_epoch"));
    const JsonValue &nodes = root.At("data_nodes");
    Require(nodes.type == JsonValue::Array && !nodes.items.empty() &&
            nodes.items.size() <= 195, "Invalid scan runtime identity node set");
    for (const JsonValue &row : nodes.items) {
        ExactKeys(row, {"agent_id", "instance_id", "plan_sha256"});
        ScanRuntimeDataNodeExpectation node;
        node.agentId = Text(row.At("agent_id"));
        node.instanceId = Text(row.At("instance_id"));
        node.planSha256 = Text(row.At("plan_sha256"));
        expected.dataNodes.push_back(node);
    }
    ValidateExpectationFields(expected);
    return expected;
}

std::string EncodeScanRuntimePointIdentityResponse(
    const ScanRuntimePointIdentityResponse &response)
{
    Require(!response.identityJson.empty() && response.identityJson.size() <= 64 * 1024 &&
            !response.statusJson.empty() && response.statusJson.size() <= 1024 * 1024 &&
            !response.receiversJson.empty() && response.receiversJson.size() <= 1024 * 1024,
            "Scan runtime identity observations exceed their metadata budgets");
    (void)DecodeEccControlIdentity(response.identityJson);
    (void)DecodeEccControlSystemStatus(response.statusJson);
    (void)DecodeEccControlReceiverObservation(response.receiversJson);
    const std::string expected = EncodeScanRuntimeIdentityExpectation(response.expectation);
    if (response.mode == "adopt") {
        Require(response.observedControllerState == 1U,
                "ECC adoption response must observe Idle");
        (void)ObserveScanRuntimeIdentity(response.identityJson, response.statusJson,
                                         response.receiversJson, response.expectation);
    } else {
        Require(response.mode == "revalidate",
                "Scan runtime identity response mode is unsupported");
        (void)ObserveScanRuntimeIdentityForRevalidation(
            response.identityJson, response.statusJson, response.receiversJson,
            response.expectation, response.observedControllerState);
    }
    const std::string bytes = "{\"schema_version\":2,\"kind\":\"getdaq.scan-runtime-point-identity-response\",\"mode\":" +
        Quote(response.mode) + ",\"observed_controller_state\":" +
        std::to_string(response.observedControllerState) + ",\"identity_json\":" +
        Quote(response.identityJson) + ",\"status_json\":" + Quote(response.statusJson) +
        ",\"receivers_json\":" + Quote(response.receiversJson) +
        ",\"expectation_json\":" + Quote(expected) + "}\n";
    Require(bytes.size() <= 8 * 1024 * 1024,
            "Scan runtime identity response exceeds its wire budget");
    return bytes;
}

ScanRuntimePointIdentityResponse DecodeScanRuntimePointIdentityResponse(
    const std::string &bytes)
{
    Require(bytes.size() <= 8 * 1024 * 1024,
            "Scan runtime identity response exceeds its wire budget");
    JsonLimits limits; limits.stringBytes = 1024 * 1024; limits.arrayItems = 16;
    const JsonValue root = ParseStrictJson(bytes, limits);
    const unsigned int version = Number(root.At("schema_version"), 2U);
    if (version == 1U)
        ExactKeys(root, {"schema_version", "kind", "identity_json", "status_json",
                         "receivers_json", "expectation_json"});
    else
        ExactKeys(root, {"schema_version", "kind", "mode", "observed_controller_state",
                         "identity_json", "status_json", "receivers_json", "expectation_json"});
    Require((version == 1U || version == 2U) &&
            Text(root.At("kind")) == "getdaq.scan-runtime-point-identity-response",
            "Unsupported scan runtime point identity response");
    ScanRuntimePointIdentityResponse response;
    if (version == 2U) {
        response.mode = Text(root.At("mode"));
        response.observedControllerState = Number(root.At("observed_controller_state"), 4U);
    }
    response.identityJson = Text(root.At("identity_json"));
    response.statusJson = Text(root.At("status_json"));
    response.receiversJson = Text(root.At("receivers_json"));
    response.expectation = DecodeScanRuntimeIdentityExpectation(Text(root.At("expectation_json")));
    Require(response.identityJson.size() <= 64 * 1024 &&
            response.statusJson.size() <= 1024 * 1024 &&
            response.receiversJson.size() <= 1024 * 1024,
            "Scan runtime identity observations exceed their metadata budgets");
    (void)DecodeEccControlIdentity(response.identityJson);
    (void)DecodeEccControlSystemStatus(response.statusJson);
    (void)DecodeEccControlReceiverObservation(response.receiversJson);
    if (response.mode == "adopt") {
        Require(response.observedControllerState == 1U,
                "ECC adoption response must observe Idle");
        (void)ObserveScanRuntimeIdentity(response.identityJson, response.statusJson,
                                         response.receiversJson, response.expectation);
    } else {
        Require(response.mode == "revalidate",
                "Scan runtime identity response mode is unsupported");
        (void)ObserveScanRuntimeIdentityForRevalidation(
            response.identityJson, response.statusJson, response.receiversJson,
            response.expectation, response.observedControllerState);
    }
    return response;
}


}
