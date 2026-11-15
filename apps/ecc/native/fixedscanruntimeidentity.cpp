#include "fixedscanruntimeidentity.h"

#include "../../../common/experiment/configurationsha256.h"
#include "../../../common/experiment/systemexperimentconfiguration.h"

#include <algorithm>
#include <stdexcept>

namespace GetDAQ {
namespace {

void Require(bool value, const char *message)
{
    if (!value) throw std::invalid_argument(message);
}

}

ScanRuntimeIdentityExpectation BuildFixedScanRuntimeIdentityExpectation(
    const ScanExecutionSnapshot &snapshot,
    unsigned int pointIndex,
    const FixedEccConfiguration &deployment,
    const std::string &identityBytes,
    const std::string &statusBytes)
{
    Require(pointIndex < snapshot.Points().size() && pointIndex <= 255,
            "Fixed ECC runtime identity point is outside the frozen execution snapshot");
    ScanRuntimePointIdentityRequest request;
    request.executionSnapshotSha256 = ConfigurationSha256(snapshot.Encode());
    request.pointIndex = pointIndex;
    request.configurationSha256 = snapshot.Points()[pointIndex].configurationSha256;
    request.artifactSha256 = snapshot.Points()[pointIndex].artifactSha256;
    return BuildFixedScanRuntimeIdentityExpectation(
        request, deployment, identityBytes, statusBytes);
}

ScanRuntimeIdentityExpectation BuildFixedScanRuntimeIdentityExpectation(
    const ScanRuntimePointIdentityRequest &request,
    const FixedEccConfiguration &deployment,
    const std::string &identityBytes,
    const std::string &statusBytes)
{
    (void)EncodeScanRuntimePointIdentityRequest(request);
    deployment.Check();
    Require(deployment.ControlAccessValidated(),
            "Fixed ECC runtime identity requires validated control deployment inputs");
    const NativeEccOptions controller = deployment.ControllerOptions();
    const RemoteReceiverOptions receivers = deployment.ReceiverOptions();
    const std::string dataBinding = RemoteReceiverBindingSha256(receivers);
    const NativeEccPlan nativePlan = PlanNativeEccConfiguration(controller, dataBinding);
    const SystemExperimentArtifact artifact =
        DecodeSystemExperimentArtifact(controller.experimentArtifactBytes);
    Require(ConfigurationSha256(controller.experimentArtifactBytes) == request.artifactSha256 &&
            artifact.configurationSha256 == request.configurationSha256,
            "Fixed ECC artifact/configuration differs from the frozen scan point");

    const EccControlIdentity identity = DecodeEccControlIdentity(identityBytes);
    Require(identity.serviceId == deployment.ServiceId() &&
            identity.clientId == deployment.ClientId() &&
            identity.snapshotSha256 == nativePlan.snapshotSha256 &&
            identity.inputBindingSha256 == deployment.BindingSha256() &&
            identity.configurationSha256 == request.configurationSha256 &&
            identity.artifactSha256 == request.artifactSha256 &&
            identity.dataBindingSha256 == dataBinding,
            "ECC identity differs from the checked fixed point deployment");

    const EccControlSystemStatus status = DecodeEccControlSystemStatus(statusBytes);
    Require(status.instanceId == identity.instanceId &&
            status.snapshotSha256 == nativePlan.snapshotSha256 &&
            status.dataScopeSha256 == nativePlan.dataControllerScopeSha256,
            "ECC status differs from the fixed point snapshot or data scope");

    std::vector<RemoteReceiverEndpoint> expectedEndpoints = receivers.endpoints;
    std::vector<EccControlDataNodeStatus> observedNodes = status.nodes;
    std::sort(expectedEndpoints.begin(), expectedEndpoints.end(),
              [](const RemoteReceiverEndpoint &left, const RemoteReceiverEndpoint &right) {
                  return left.agentId < right.agentId;
              });
    std::sort(observedNodes.begin(), observedNodes.end(),
              [](const EccControlDataNodeStatus &left, const EccControlDataNodeStatus &right) {
                  return left.agentId < right.agentId;
              });
    Require(expectedEndpoints.size() == observedNodes.size(),
            "ECC receiver node set differs from the fixed deployment");

    ScanRuntimeIdentityDeployment runtime;
    runtime.serviceId = deployment.ServiceId();
    runtime.clientId = deployment.ClientId();
    runtime.instanceId = identity.instanceId;
    runtime.snapshotSha256 = nativePlan.snapshotSha256;
    runtime.inputBindingSha256 = deployment.BindingSha256();
    runtime.dataBindingSha256 = dataBinding;
    runtime.dataScopeSha256 = nativePlan.dataControllerScopeSha256;
    runtime.dataEpoch = status.dataEpoch;
    for (std::size_t i = 0; i < expectedEndpoints.size(); ++i) {
        Require(observedNodes[i].agentId == expectedEndpoints[i].agentId &&
                observedNodes[i].planSha256 == expectedEndpoints[i].planSha256,
                "ECC receiver agent/plan differs from the fixed deployment");
        runtime.dataNodes.push_back({expectedEndpoints[i].agentId,
                                     observedNodes[i].instanceId,
                                     expectedEndpoints[i].planSha256});
    }

    ScanRuntimeIdentityExpectation expected;
    expected.executionSnapshotSha256 = request.executionSnapshotSha256;
    expected.pointIndex = request.pointIndex;
    expected.serviceId = runtime.serviceId;
    expected.clientId = runtime.clientId;
    expected.instanceId = runtime.instanceId;
    expected.snapshotSha256 = runtime.snapshotSha256;
    expected.inputBindingSha256 = runtime.inputBindingSha256;
    expected.configurationSha256 = request.configurationSha256;
    expected.artifactSha256 = request.artifactSha256;
    expected.dataBindingSha256 = runtime.dataBindingSha256;
    expected.dataScopeSha256 = runtime.dataScopeSha256;
    expected.dataEpoch = runtime.dataEpoch;
    expected.dataNodes = runtime.dataNodes;
    if (request.mode == "adopt")
        ObserveScanRuntimeIdentity(identityBytes, statusBytes, expected);
    else if (request.mode == "revalidate")
        ObserveScanRuntimeIdentityForRevalidation(
            identityBytes, statusBytes, expected,
            request.expectedControllerState);
    else
        throw std::invalid_argument("Unsupported fixed scan runtime identity mode");
    return expected;
}

}
