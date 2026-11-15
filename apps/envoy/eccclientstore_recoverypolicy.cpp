#include "eccclientstore_recoverypolicy.h"
#include "systemexperimentconfiguration.h"
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
}

bool EccClientStoreRecoveryPolicy::CanResumeCatalogReplacement(
    const std::string &operationId,
    const QMap<QString, GetDAQ::EccControlRequest> &requests,
    const QMap<QString, GetDAQ::EccControlOperation> &observations,
    const QSet<QString> &heldOperations, const std::string &instance,
    const EccSystemClientOptions &options, bool noPendingOperations) {
    const auto id = QString::fromStdString(operationId);
    if (!requests.contains(id) || !observations.contains(id) || !noPendingOperations) return false;
    const auto &request = requests[id];
    const auto &observed = observations[id];
    if (request.command != "catalog" || !request.catalogReplaceExisting || observed.phase != "failed" ||
        observed.request.operationId != operationId || request.instanceId != instance ||
        request.clientId != options.clientId.toUtf8().toStdString() ||
        request.clientEpoch != options.clientEpoch.toStdString() ||
        request.snapshotSha256 != options.snapshotSha256.toStdString() || observed.resultJson.empty()) return false;
    for (const auto &held : heldOperations) if (held != id) return false;
    const auto status = GetDAQ::DecodeEccControlControllerStatus(observed.resultJson);
    return status.state == 3 && !status.transition && !status.error && !status.outcomeUnknown &&
        !status.needsReconciliation;
}

void EccClientStoreRecoveryPolicy::CheckRetirementContext(
    const GetDAQ::EccControlRequest &request, const std::string &retirementContextKey,
    const QByteArray &retirementContextBytes) {
    Require(request.command == "retire-data-plans" &&
        retirementContextKey == GetDAQ::EccControlRequestKey(request),
        "Retirement context belongs to another request");
    const auto status = GetDAQ::DecodeEccControlSystemStatus(retirementContextBytes.toStdString());
    Require(status.instanceId == request.instanceId && status.snapshotSha256 == request.snapshotSha256 &&
        (status.clientEpoch.empty() || status.clientEpoch == request.clientEpoch) && !status.operationActive &&
        !status.controlNeedsReconciliation && !status.dataNeedsReconciliation &&
        !status.controller.needsReconciliation && !status.controller.outcomeUnknown &&
        !status.controller.transition && !status.controller.error &&
        (status.controller.state == 1 || status.controller.state == 2) && !status.nodes.empty() &&
        status.nodes.size() == request.successorReceiverPlans.size(),
        "Retirement context is not the original healthy stopped cycle");
    for (const auto &node : status.nodes) {
        bool target = false;
        for (const auto &entry : request.successorReceiverPlans)
            if (entry.agentId == node.agentId && entry.planSha256 != node.planSha256) target = true;
        Require(target && node.state == "stopped" && !node.needsReconciliation && !node.receivers.empty(),
            "Retirement context node differs from reviewed targets");
        for (const auto &receiver : node.receivers)
            Require(receiver.reaped && receiver.sealVerified && receiver.exitCode == 0,
                "Retirement context lacks normal receiver seals");
    }
}

void EccClientStoreRecoveryPolicy::CheckCompletion(
    const GetDAQ::EccControlRequest &request,
    const QMap<QString, GetDAQ::EccControlRequest> &requests,
    const QMap<QString, GetDAQ::EccControlOperation> &observations,
    const QSet<QString> &heldOperations, const std::string &retirementContextKey,
    const QByteArray &retirementContextBytes, const QString &lastRetirementCompletion) {
    const auto parent = QString::fromStdString(request.failedOperationId);
    Require(request.command == "complete-data-handoff" && requests.contains(parent) && observations.contains(parent),
        "Completion lacks original retirement intent/result");
    const auto &original = requests[parent];
    const auto &failed = observations[parent];
    CheckRetirementContext(original, retirementContextKey, retirementContextBytes);
    Require(failed.phase == "failed" && request.instanceId == original.instanceId &&
        request.snapshotSha256 == original.snapshotSha256 && request.clientId == original.clientId &&
        request.clientEpoch == original.clientEpoch, "Completion original failure/ownership differs");
    for (const auto &held : heldOperations) {
        const auto &operation = observations[held];
        Require(held == parent || (operation.request.command == "complete-data-handoff" &&
            operation.request.failedOperationId == original.operationId && operation.phase == "failed"),
            "Independent client hold prevents retirement completion");
    }
    if (!lastRetirementCompletion.isEmpty())
        Require(observations.contains(lastRetirementCompletion) &&
            observations[lastRetirementCompletion].phase == "failed",
            "Observe the existing completion before any new attempt");
}
