#include "scanruntimeidentity.h"
#include "scanruntimeidentityinternal_p.h"

#include "configurationsha256.h"
#include "eccprotocol.h"
#include "strictjson.h"
#include "systemexperimentconfiguration.h"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace GetDAQ {
using namespace ScanRuntimeIdentityInternal;

ScanRuntimeIdentityObservation ObserveScanRuntimeIdentityForRevalidation(
    const std::string &identityBytes,
    const std::string &statusBytes,
    const ScanRuntimeIdentityExpectation &expected,
    unsigned int controllerState)
{
    Require(controllerState >= 1U && controllerState <= 4U,
            "Scan identity revalidation state is outside Idle through Ready");
    ScanRuntimeIdentityObservation result = StableIdentityObservation(
        identityBytes, statusBytes, expected);
    const EccControlSystemStatus status = DecodeEccControlSystemStatus(statusBytes);
    Require(status.controller.state == controllerState &&
            !status.operationActive && status.controller.transition == 0 &&
            status.controller.error == 0 && !status.controlNeedsReconciliation &&
            !status.controller.outcomeUnknown && !status.controller.needsReconciliation &&
            !status.dataNeedsReconciliation,
            "ECC lifecycle is not resolved at the journal's expected state");
    std::vector<EccControlDataNodeStatus> nodes = status.nodes;
    std::sort(nodes.begin(), nodes.end(),
        [](const EccControlDataNodeStatus &left,
           const EccControlDataNodeStatus &right) {
            return left.agentId < right.agentId;
        });
    for (const auto &node : nodes) {
        if (controllerState < 4U)
            Require(node.state == "fresh" && !node.needsReconciliation &&
                    node.sessionId.empty() && node.receivers.empty(),
                    "Receiver session exists before scan Configure is complete");
        else
            Require(node.state == "ready" && !node.needsReconciliation &&
                    !node.sessionId.empty() && !node.receivers.empty(),
                    "Configured scan point has no Ready receiver lifecycle");
    }
    return result;
}

ScanRuntimeIdentityObservation ObserveScanRuntimeIdentityForRevalidation(
    const std::string &identityBytes,
    const std::string &statusBytes,
    const std::string &receiverObservationBytes,
    const ScanRuntimeIdentityExpectation &expected,
    unsigned int controllerState)
{
    ScanRuntimeIdentityObservation result = ObserveScanRuntimeIdentityForRevalidation(
        identityBytes, statusBytes, expected, controllerState);
    const EccControlIdentity identity = DecodeEccControlIdentity(identityBytes);
    const EccControlSystemStatus status = DecodeEccControlSystemStatus(statusBytes);
    const EccControlReceiverObservation receivers =
        DecodeEccControlReceiverObservation(receiverObservationBytes);
    Require(receivers.instanceId == identity.instanceId &&
            receivers.snapshotSha256 == identity.snapshotSha256 &&
            receivers.dataBindingSha256 == expected.dataBindingSha256 &&
            receivers.nodes.size() == expected.dataNodes.size(),
            "Receiver observation differs from the frozen scan deployment");

    std::vector<ScanRuntimeDataNodeExpectation> expectedNodes = expected.dataNodes;
    std::vector<EccControlDataNodeStatus> statusNodes = status.nodes;
    std::vector<EccControlReceiverNodeObservation> receiverNodes = receivers.nodes;
    std::sort(expectedNodes.begin(), expectedNodes.end(),
        [](const ScanRuntimeDataNodeExpectation &left,
           const ScanRuntimeDataNodeExpectation &right) {
            return left.agentId < right.agentId;
        });
    std::sort(statusNodes.begin(), statusNodes.end(),
        [](const EccControlDataNodeStatus &left,
           const EccControlDataNodeStatus &right) {
            return left.agentId < right.agentId;
        });
    std::sort(receiverNodes.begin(), receiverNodes.end(),
        [](const EccControlReceiverNodeObservation &left,
           const EccControlReceiverNodeObservation &right) {
            return left.agentId < right.agentId;
        });
    for (std::size_t i = 0; i < expectedNodes.size(); ++i) {
        const auto &want = expectedNodes[i];
        const auto &have = statusNodes[i];
        const auto &observed = receiverNodes[i];
        Require(have.agentId == want.agentId && have.instanceId == want.instanceId &&
                have.planSha256 == want.planSha256 && observed.agentId == want.agentId &&
                observed.instanceId == want.instanceId && observed.planSha256 == want.planSha256 &&
                observed.available && !observed.busy && observed.observedAtUnixMs != 0 &&
                !have.needsReconciliation && !observed.lifecycle.needsReconciliation &&
                observed.lifecycle.agentId == want.agentId &&
                observed.lifecycle.instanceId == want.instanceId &&
                observed.lifecycle.planSha256 == want.planSha256 &&
                observed.lifecycle.state == have.state &&
                observed.lifecycle.sessionId == have.sessionId,
                "Receiver lifecycle no longer matches the frozen scan deployment");
        if (controllerState < 4U) {
            Require(have.state == "fresh" && have.sessionId.empty() &&
                    have.receivers.empty() && observed.lifecycle.receivers.empty() &&
                    observed.receivers.empty(),
                    "Receiver session exists before scan Configure is complete");
            continue;
        }
        Require(have.state == "ready" && !have.sessionId.empty() &&
                !have.receivers.empty() &&
                have.receivers.size() == observed.lifecycle.receivers.size() &&
                have.receivers.size() == observed.receivers.size(),
                "Configured scan point has no matching Ready receiver session");
        std::map<std::string, const EccControlReceiverStatus *> lifecycleById;
        std::map<std::string, const EccControlReceiverProcess *> liveById;
        for (const auto &lifecycle : have.receivers)
            Require(lifecycleById.insert(std::make_pair(lifecycle.id, &lifecycle)).second,
                    "Configured ECC status has duplicate receiver process IDs");
        for (const auto &live : observed.receivers)
            Require(liveById.insert(std::make_pair(live.id, &live)).second,
                    "Configured receiver observation has duplicate process IDs");
        Require(lifecycleById.size() == liveById.size(),
                "Configured receiver process sets differ from live observation");
        for (const auto &entry : liveById) {
            const auto lifecycleIt = lifecycleById.find(entry.first);
            Require(lifecycleIt != lifecycleById.end(),
                    "Configured live receiver is missing lifecycle evidence");
            const auto &lifecycle = *lifecycleIt->second;
            const auto &live = *entry.second;
            Require(live.state == "running" && live.pid > 0 &&
                    lifecycle.pid == live.pid && !lifecycle.reaped &&
                    lifecycle.exitCode == -1 && !lifecycle.sealVerified &&
                    lifecycle.sessionId == have.sessionId,
                    "Configured scan receiver process differs from its live Ready session");
        }
    }
    return result;
}

}
