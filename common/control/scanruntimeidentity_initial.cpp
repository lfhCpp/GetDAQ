#include "scanruntimeidentity.h"
#include "scanruntimeidentityinternal_p.h"

#include "configurationsha256.h"
#include "eccprotocol.h"
#include "systemexperimentconfiguration.h"

#include <algorithm>
#include <set>

namespace GetDAQ {
namespace ScanRuntimeIdentityInternal {
ScanRuntimeIdentityObservation StableIdentityObservation(
    const std::string &identityBytes,
    const std::string &statusBytes,
    const ScanRuntimeIdentityExpectation &expected)
{
    ValidateExpectationFields(expected);
    const EccControlIdentity identity = DecodeEccControlIdentity(identityBytes);
    const EccControlSystemStatus status = DecodeEccControlSystemStatus(statusBytes);
    Require(identity.serviceId == expected.serviceId &&
            identity.clientId == expected.clientId &&
            identity.instanceId == expected.instanceId &&
            identity.snapshotSha256 == expected.snapshotSha256 &&
            identity.inputBindingSha256 == expected.inputBindingSha256 &&
            identity.configurationSha256 == expected.configurationSha256 &&
            identity.artifactSha256 == expected.artifactSha256 &&
            identity.dataBindingSha256 == expected.dataBindingSha256,
            "ECC identity differs from the frozen scan deployment");
    Require(status.instanceId == expected.instanceId &&
            status.snapshotSha256 == expected.snapshotSha256 &&
            status.dataScopeSha256 == expected.dataScopeSha256 &&
            status.dataEpoch == expected.dataEpoch &&
            status.nodes.size() == expected.dataNodes.size(),
            "ECC status differs from the frozen scan deployment");

    std::vector<ScanRuntimeDataNodeExpectation> expectedNodes = expected.dataNodes;
    std::vector<EccControlDataNodeStatus> observedNodes = status.nodes;
    std::sort(expectedNodes.begin(), expectedNodes.end(),
        [](const ScanRuntimeDataNodeExpectation &left,
           const ScanRuntimeDataNodeExpectation &right) {
            return left.agentId < right.agentId;
        });
    std::sort(observedNodes.begin(), observedNodes.end(),
        [](const EccControlDataNodeStatus &left,
           const EccControlDataNodeStatus &right) {
            return left.agentId < right.agentId;
        });
    for (std::size_t i = 0; i < expectedNodes.size(); ++i) {
        const auto &want = expectedNodes[i];
        const auto &have = observedNodes[i];
        Require(have.agentId == want.agentId && have.instanceId == want.instanceId &&
                have.planSha256 == want.planSha256,
                "ECC receiver node identity differs from the frozen scan deployment");
    }

    ScanRuntimeIdentityObservation result;
    result.canonicalBytes = std::string("getdaq.scan-runtime-identity-observation:4\n") +
        "execution_snapshot_sha256=" + Quote(expected.executionSnapshotSha256) + "\n" +
        "point_index=" + std::to_string(expected.pointIndex) + "\n" +
        "service_id=" + Quote(identity.serviceId) + "\n" +
        "client_id=" + Quote(identity.clientId) + "\n" +
        "instance_id=" + Quote(identity.instanceId) + "\n" +
        "snapshot_sha256=" + Quote(identity.snapshotSha256) + "\n" +
        "input_binding_sha256=" + Quote(identity.inputBindingSha256) + "\n" +
        "configuration_sha256=" + Quote(identity.configurationSha256) + "\n" +
        "artifact_sha256=" + Quote(identity.artifactSha256) + "\n" +
        "data_binding_sha256=" + Quote(identity.dataBindingSha256) + "\n" +
        "data_scope_sha256=" + Quote(status.dataScopeSha256) + "\n" +
        "data_epoch=" + Quote(status.dataEpoch) + "\n";
    for (const auto &node : observedNodes)
        result.canonicalBytes += "node=" + Quote(node.agentId) + "," +
            Quote(node.instanceId) + "," + Quote(node.planSha256) + "\n";
    result.sha256 = ConfigurationSha256(result.canonicalBytes);
    return result;
}

}

using namespace ScanRuntimeIdentityInternal;

ScanRuntimeIdentityObservation ObserveScanRuntimeIdentity(
    const std::string &identityBytes,
    const std::string &statusBytes,
    const ScanRuntimeIdentityExpectation &expected)
{
    const ScanRuntimeIdentityObservation stable = StableIdentityObservation(
        identityBytes, statusBytes, expected);
    RequireDigest(expected.executionSnapshotSha256,
                  "Expected scan execution snapshot SHA-256 is invalid");
    Require(expected.pointIndex <= 255,
            "Expected scan point index is out of range");
    Require(!expected.serviceId.empty() && expected.serviceId.size() <= 128,
            "Expected ECC service ID is invalid");
    Require(!expected.clientId.empty() && expected.clientId.size() <= 128,
            "Expected ECC client ID is invalid");
    RequireDigest(expected.instanceId, "Expected ECC instance ID is invalid");
    RequireDigest(expected.snapshotSha256, "Expected ECC snapshot SHA-256 is invalid");
    RequireDigest(expected.inputBindingSha256, "Expected ECC input binding SHA-256 is invalid");
    RequireDigest(expected.configurationSha256, "Expected configuration SHA-256 is invalid");
    RequireDigest(expected.artifactSha256, "Expected artifact SHA-256 is invalid");
    RequireDigest(expected.dataBindingSha256, "Expected data binding SHA-256 is invalid");
    RequireDigest(expected.dataScopeSha256, "Expected data controller scope SHA-256 is invalid");
    RequireDigest(expected.dataEpoch, "Expected data controller epoch is invalid");
    Require(!expected.dataNodes.empty() && expected.dataNodes.size() <= 195,
            "Expected data node set is empty or exceeds its limit");

    const EccControlIdentity identity = DecodeEccControlIdentity(identityBytes);
    const EccControlSystemStatus status = DecodeEccControlSystemStatus(statusBytes);
    Require(identity.serviceId == expected.serviceId,
            "ECC service ID differs from the reviewed point");
    Require(identity.clientId == expected.clientId,
            "ECC client ID differs from the reviewed point");
    Require(identity.instanceId == expected.instanceId && status.instanceId == expected.instanceId,
            "ECC identity/status instance differs from the reviewed point");
    Require(identity.snapshotSha256 == expected.snapshotSha256 &&
            status.snapshotSha256 == expected.snapshotSha256,
            "ECC identity/status snapshot differs from the reviewed point");
    Require(identity.inputBindingSha256 == expected.inputBindingSha256,
            "ECC input binding differs from the reviewed point");
    Require(identity.configurationSha256 == expected.configurationSha256 &&
            identity.artifactSha256 == expected.artifactSha256,
            "ECC configuration/artifact differs from the reviewed scan point");
    Require(identity.dataBindingSha256 == expected.dataBindingSha256,
            "ECC data binding differs from the reviewed point");

    // Before describe, the newly created controller must still be idle. A
    // busy, failed, unknown, or held instance is never accepted as a fresh point.
    Require(!status.operationActive && status.operationId.empty() &&
            !status.controlNeedsReconciliation && status.controller.state == 1 &&
            status.controller.transition == 0 && status.controller.error == 0 &&
            !status.controller.outcomeUnknown && !status.controller.needsReconciliation &&
            status.controller.operationId.empty(),
            "ECC controller is not in a fresh, known idle state");
    Require(!status.dataNeedsReconciliation &&
            status.dataScopeSha256 == expected.dataScopeSha256 &&
            status.dataEpoch == expected.dataEpoch,
            "ECC data controller scope/epoch differs or needs reconciliation");
    Require(status.nodes.size() == expected.dataNodes.size(),
            "ECC data node set differs from the reviewed point");

    std::vector<ScanRuntimeDataNodeExpectation> expectedNodes = expected.dataNodes;
    std::sort(expectedNodes.begin(), expectedNodes.end(),
              [](const ScanRuntimeDataNodeExpectation &left,
                 const ScanRuntimeDataNodeExpectation &right) {
                  return left.agentId < right.agentId;
              });
    std::set<std::string> seen;
    for (const ScanRuntimeDataNodeExpectation &node : expectedNodes) {
        Require(!node.agentId.empty() && node.agentId.size() <= 128 &&
                seen.insert(node.agentId).second,
                "Expected data node IDs are invalid or duplicated");
        RequireDigest(node.instanceId, "Expected data node instance ID is invalid");
        RequireDigest(node.planSha256, "Expected receiver plan SHA-256 is invalid");
    }

    std::vector<EccControlDataNodeStatus> observedNodes = status.nodes;
    std::sort(observedNodes.begin(), observedNodes.end(),
              [](const EccControlDataNodeStatus &left,
                 const EccControlDataNodeStatus &right) {
                  return left.agentId < right.agentId;
              });
    for (std::size_t i = 0; i < expectedNodes.size(); ++i) {
        const ScanRuntimeDataNodeExpectation &want = expectedNodes[i];
        const EccControlDataNodeStatus &have = observedNodes[i];
        Require(have.agentId == want.agentId && have.instanceId == want.instanceId &&
                have.planSha256 == want.planSha256,
                "ECC data node identity or receiver plan differs from the reviewed point");
        Require(have.state == "fresh" && !have.needsReconciliation &&
                have.sessionId.empty() && have.receivers.empty(),
                "ECC data node is not fresh before scan-point description");
    }

    return stable;
}

ScanRuntimeIdentityObservation ObserveScanRuntimeIdentity(
    const std::string &identityBytes,
    const std::string &statusBytes,
    const std::string &receiverObservationBytes,
    const ScanRuntimeIdentityExpectation &expected)
{
    ScanRuntimeIdentityObservation result =
        ObserveScanRuntimeIdentity(identityBytes, statusBytes, expected);
    const EccControlIdentity identity = DecodeEccControlIdentity(identityBytes);
    const EccControlReceiverObservation receivers =
        DecodeEccControlReceiverObservation(receiverObservationBytes);
    Require(receivers.instanceId == identity.instanceId &&
            receivers.snapshotSha256 == identity.snapshotSha256 &&
            receivers.dataBindingSha256 == expected.dataBindingSha256,
            "Fresh receiver observation differs from the ECC runtime identity");
    Require(receivers.nodes.size() == expected.dataNodes.size(),
            "Fresh receiver observation node set differs from the fixed point deployment");

    std::vector<ScanRuntimeDataNodeExpectation> expectedNodes = expected.dataNodes;
    std::sort(expectedNodes.begin(), expectedNodes.end(),
              [](const ScanRuntimeDataNodeExpectation &left,
                 const ScanRuntimeDataNodeExpectation &right) {
                  return left.agentId < right.agentId;
              });
    std::vector<EccControlReceiverNodeObservation> observedNodes = receivers.nodes;
    std::sort(observedNodes.begin(), observedNodes.end(),
              [](const EccControlReceiverNodeObservation &left,
                 const EccControlReceiverNodeObservation &right) {
                  return left.agentId < right.agentId;
              });
    for (std::size_t i = 0; i < expectedNodes.size(); ++i) {
        const ScanRuntimeDataNodeExpectation &want = expectedNodes[i];
        const EccControlReceiverNodeObservation &have = observedNodes[i];
        Require(have.agentId == want.agentId && have.instanceId == want.instanceId &&
                have.planSha256 == want.planSha256 && have.available && !have.busy &&
                have.observedAtUnixMs != 0 &&
                have.lifecycle.agentId == want.agentId &&
                have.lifecycle.instanceId == want.instanceId &&
                have.lifecycle.planSha256 == want.planSha256 &&
                have.lifecycle.state == "fresh" &&
                !have.lifecycle.needsReconciliation &&
                have.lifecycle.sessionId.empty() &&
                have.lifecycle.receivers.empty() && have.receivers.empty(),
                "Receiver node is unavailable, stale, busy, changed, or has a live session");
    }

    return result;
}


} // namespace GetDAQ
