#include "scanruntimeidentity.h"
#include "scanruntimeidentityinternal_p.h"

#include "configurationsha256.h"
#include "eccprotocol.h"

#include <algorithm>
#include <map>
#include <set>

namespace GetDAQ {
using namespace ScanRuntimeIdentityInternal;

ScanRuntimeIdentityDeployment CaptureScanRuntimeIdentityDeployment(
    const std::string &fixedServiceId,
    const std::string &fixedClientId,
    const std::string &fixedSnapshotSha256,
    const std::string &fixedInputBindingSha256,
    const std::vector<std::string> &configuredAgentIds,
    const std::string &identityBytes,
    const std::string &statusBytes,
    const std::string &receiverObservationBytes,
    bool allowResolvedLifecycle)
{
    const EccControlIdentity identity = DecodeEccControlIdentity(identityBytes);
    const EccControlSystemStatus status = DecodeEccControlSystemStatus(statusBytes);
    const EccControlReceiverObservation receivers =
        DecodeEccControlReceiverObservation(receiverObservationBytes);
    Require(identity.serviceId == fixedServiceId && identity.clientId == fixedClientId &&
            identity.snapshotSha256 == fixedSnapshotSha256 &&
            identity.inputBindingSha256 == fixedInputBindingSha256,
            "ECC identity differs from the fixed client configuration");
    Require(status.instanceId == identity.instanceId &&
            status.snapshotSha256 == identity.snapshotSha256 &&
            !status.operationActive && !status.controlNeedsReconciliation &&
            status.controller.transition == 0 && status.controller.error == 0 &&
            !status.controller.outcomeUnknown && !status.controller.needsReconciliation &&
            !status.dataNeedsReconciliation &&
            ((!allowResolvedLifecycle && status.controller.state == 1 &&
              status.operationId.empty() && status.controller.operationId.empty()) ||
             (allowResolvedLifecycle && status.controller.state >= 1 &&
              status.controller.state <= 4)),
            "ECC status is not a resolved observation of the fixed instance");
    Require(!status.dataScopeSha256.empty() && !status.dataEpoch.empty(),
            "ECC status omits the active data scope or epoch");
    Require(receivers.instanceId == identity.instanceId &&
            receivers.snapshotSha256 == identity.snapshotSha256 &&
            receivers.dataBindingSha256 == identity.dataBindingSha256,
            "Receiver observation differs from the fixed ECC identity");

    std::set<std::string> configured(configuredAgentIds.begin(), configuredAgentIds.end());
    Require(!configured.empty() && configured.size() == configuredAgentIds.size(),
            "Configured receiver agent IDs are empty or duplicated");
    std::map<std::string, const EccControlDataNodeStatus *> statusByAgent;
    for (const EccControlDataNodeStatus &node : status.nodes)
        Require(statusByAgent.insert(std::make_pair(node.agentId, &node)).second,
                "ECC status contains duplicate receiver agents");
    std::set<std::string> observed;
    ScanRuntimeIdentityDeployment deployment;
    deployment.serviceId = identity.serviceId;
    deployment.clientId = identity.clientId;
    deployment.instanceId = identity.instanceId;
    deployment.snapshotSha256 = identity.snapshotSha256;
    deployment.inputBindingSha256 = identity.inputBindingSha256;
    deployment.dataBindingSha256 = identity.dataBindingSha256;
    deployment.dataScopeSha256 = status.dataScopeSha256;
    deployment.dataEpoch = status.dataEpoch;
    for (const EccControlReceiverNodeObservation &receiver : receivers.nodes) {
        Require(configured.count(receiver.agentId) == 1 &&
                observed.insert(receiver.agentId).second,
                "Receiver observation contains an unexpected or duplicate agent");
        const auto found = statusByAgent.find(receiver.agentId);
        Require(receiver.available && !receiver.busy && receiver.observedAtUnixMs != 0 &&
                !receiver.lifecycle.needsReconciliation &&
                receiver.lifecycle.planSha256 == receiver.planSha256 &&
                found != statusByAgent.end() &&
                found->second->instanceId == receiver.instanceId &&
                found->second->planSha256 == receiver.planSha256 &&
                found->second->state == receiver.lifecycle.state &&
                found->second->sessionId == receiver.lifecycle.sessionId,
                "Receiver observation differs from the ECC data-node lifecycle");
        if (!allowResolvedLifecycle || status.controller.state < 4U) {
            Require(receiver.lifecycle.state == "fresh" &&
                    receiver.lifecycle.sessionId.empty() &&
                    receiver.lifecycle.receivers.empty() && receiver.receivers.empty() &&
                    found->second->state == "fresh" && found->second->sessionId.empty() &&
                    found->second->receivers.empty(),
                    "Receiver agent is active before scan Configure is complete");
        } else {
            Require(receiver.lifecycle.state == "ready" &&
                    !receiver.lifecycle.sessionId.empty() &&
                    !receiver.receivers.empty() &&
                    receiver.receivers.size() == receiver.lifecycle.receivers.size() &&
                    receiver.receivers.size() == found->second->receivers.size(),
                    "Configured point lacks a matching Ready receiver lifecycle");
            std::map<std::string, const EccControlReceiverProcess *> processesById;
            std::map<std::string, const EccControlReceiverStatus *> lifecycleById;
            for (const auto &process : receiver.receivers)
                Require(processesById.insert(std::make_pair(process.id, &process)).second,
                        "Configured receiver observation has duplicate process IDs");
            for (const auto &lifecycle : receiver.lifecycle.receivers)
                Require(lifecycleById.insert(std::make_pair(lifecycle.id, &lifecycle)).second,
                        "Configured receiver lifecycle has duplicate process IDs");
            Require(processesById.size() == lifecycleById.size(),
                    "Configured receiver process sets differ from lifecycle evidence");
            for (const auto &entry : processesById) {
                const auto lifecycleIt = lifecycleById.find(entry.first);
                Require(lifecycleIt != lifecycleById.end(),
                        "Configured receiver process is missing lifecycle evidence");
                const auto &process = *entry.second;
                const auto &lifecycle = *lifecycleIt->second;
                Require(process.state == "running" && process.pid > 0 &&
                        lifecycle.pid == process.pid && !lifecycle.reaped &&
                        lifecycle.exitCode == -1 && !lifecycle.sealVerified &&
                        lifecycle.sessionId == receiver.lifecycle.sessionId,
                        "Configured receiver process is not in its bound Ready session");
            }
        }
        deployment.dataNodes.push_back({receiver.agentId, receiver.instanceId,
                                        receiver.planSha256});
    }
    Require(observed == configured && statusByAgent.size() == configured.size(),
            "Receiver agent set differs from the configured whole system");
    std::sort(deployment.dataNodes.begin(), deployment.dataNodes.end(),
        [](const ScanRuntimeDataNodeExpectation &left,
           const ScanRuntimeDataNodeExpectation &right) {
            return left.agentId < right.agentId;
        });
    RequireDigest(deployment.instanceId, "ECC instance ID is invalid");
    RequireDigest(deployment.snapshotSha256, "ECC snapshot SHA-256 is invalid");
    RequireDigest(deployment.inputBindingSha256, "ECC input binding SHA-256 is invalid");
    RequireDigest(deployment.dataBindingSha256, "ECC data binding SHA-256 is invalid");
    RequireDigest(deployment.dataScopeSha256, "ECC data scope SHA-256 is invalid");
    RequireDigest(deployment.dataEpoch, "ECC data epoch is invalid");
    for (const ScanRuntimeDataNodeExpectation &node : deployment.dataNodes) {
        RequireDigest(node.instanceId, "Receiver instance ID is invalid");
        RequireDigest(node.planSha256, "Receiver plan SHA-256 is invalid");
    }
    return deployment;
}

ScanRuntimeIdentityExpectation BindScanRuntimeIdentityExpectation(
    const ScanExecutionSnapshot &snapshot,
    unsigned int pointIndex,
    const ScanRuntimeIdentityDeployment &deployment)
{
    Require(pointIndex < snapshot.Points().size() && pointIndex <= 255,
            "Scan runtime identity point is outside the frozen execution snapshot");
    Require(!deployment.serviceId.empty() && deployment.serviceId.size() <= 128,
            "Scan runtime deployment service ID is invalid");
    Require(!deployment.clientId.empty() && deployment.clientId.size() <= 128,
            "Scan runtime deployment client ID is invalid");

    const ScanExecutionSnapshotPoint &point = snapshot.Points()[pointIndex];
    ScanRuntimeIdentityExpectation expected;
    expected.executionSnapshotSha256 = ConfigurationSha256(snapshot.Encode());
    expected.pointIndex = pointIndex;
    expected.serviceId = deployment.serviceId;
    expected.clientId = deployment.clientId;
    expected.instanceId = deployment.instanceId;
    expected.snapshotSha256 = deployment.snapshotSha256;
    expected.inputBindingSha256 = deployment.inputBindingSha256;
    expected.configurationSha256 = point.configurationSha256;
    expected.artifactSha256 = point.artifactSha256;
    expected.dataBindingSha256 = deployment.dataBindingSha256;
    expected.dataScopeSha256 = deployment.dataScopeSha256;
    expected.dataEpoch = deployment.dataEpoch;
    expected.dataNodes = deployment.dataNodes;
    return expected;
}


}
