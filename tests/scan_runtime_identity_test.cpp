#include "scanruntimeidentity.h"

#include "systemexperimentconfiguration.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace GetDAQ;
void Require(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}
template<class F> void Reject(F fn)
{
    bool rejected = false;
    try { fn(); } catch (const std::exception &) { rejected = true; }
    Require(rejected, "Unsafe ECC runtime observation was accepted");
}
std::string ReplaceOnce(std::string bytes, const std::string &from, const std::string &to)
{
    const std::size_t position = bytes.find(from);
    Require(position != std::string::npos, "Runtime identity test field not found");
    bytes.replace(position, from.size(), to);
    return bytes;
}
const std::string h(char c) { return std::string(64, c); }
}

int main()
{
    try {
        ScanRuntimeIdentityExpectation expected;
        expected.executionSnapshotSha256 = h('8');
        expected.pointIndex = 0;
        expected.serviceId = "scan-point-0";
        expected.clientId = "console";
        expected.instanceId = h('a');
        expected.snapshotSha256 = h('b');
        expected.inputBindingSha256 = h('c');
        expected.configurationSha256 = h('d');
        expected.artifactSha256 = h('e');
        expected.dataBindingSha256 = h('f');
        expected.dataScopeSha256 = h('1');
        expected.dataEpoch = h('2');
        expected.dataNodes.push_back({"receiver-0", h('3'), h('4')});
        expected.dataNodes.push_back({"receiver-1", h('5'), h('6')});
        const std::string expectationBytes = EncodeScanRuntimeIdentityExpectation(expected);
        const auto reopenedExpectation = DecodeScanRuntimeIdentityExpectation(expectationBytes);
        Require(EncodeScanRuntimeIdentityExpectation(reopenedExpectation) == expectationBytes,
                "Runtime identity expectation did not round-trip canonically");
        ScanRuntimePointIdentityRequest encodedPointRequest;
        encodedPointRequest.executionSnapshotSha256 = h('8');
        encodedPointRequest.configurationSha256 = h('d');
        encodedPointRequest.artifactSha256 = h('e');
        const auto pointRequest = DecodeScanRuntimePointIdentityRequest(
            EncodeScanRuntimePointIdentityRequest(encodedPointRequest));
        Require(pointRequest.executionSnapshotSha256 == h('8') &&
                pointRequest.pointIndex == 0 && pointRequest.configurationSha256 == h('d') &&
                pointRequest.artifactSha256 == h('e'),
                "Runtime point identity request did not round-trip");
        Reject([&] { DecodeScanRuntimeIdentityExpectation(
            expectationBytes.substr(0, expectationBytes.size() - 2) +
            ",\"unexpected\":true}\n"); });
        encodedPointRequest.pointIndex = 256;
        Reject([&] { EncodeScanRuntimePointIdentityRequest(encodedPointRequest); });
        const std::string identity =
            "{\"schema\":1,\"service\":\"getdaq-ecc-server\",\"service_id\":\"scan-point-0\",\"client_id\":\"console\",\"instance_id\":\""+h('a')+
            "\",\"snapshot_sha256\":\""+h('b')+"\",\"input_binding_sha256\":\""+h('c')+
            "\",\"configuration_sha256\":\""+h('d')+"\",\"artifact_sha256\":\""+h('e')+
            "\",\"data_binding_sha256\":\""+h('f')+"\"}";
        const std::string node =
            "{\"agent_id\":\"receiver-0\",\"instance_id\":\""+h('3')+"\",\"plan_sha256\":\""+h('4')+
            "\",\"lifecycle\":{\"schema\":1,\"state\":\"fresh\",\"needs_reconciliation\":false,\"plan_sha256\":\""+h('4')+
            "\",\"session_id\":\"\",\"message\":\"\",\"receivers\":[]}}";
        const std::string secondNode =
            "{\"agent_id\":\"receiver-1\",\"instance_id\":\""+h('5')+"\",\"plan_sha256\":\""+h('6')+
            "\",\"lifecycle\":{\"schema\":1,\"state\":\"fresh\",\"needs_reconciliation\":false,\"plan_sha256\":\""+h('6')+
            "\",\"session_id\":\"\",\"message\":\"\",\"receivers\":[]}}";
        const std::string status =
            "{\"schema\":1,\"service\":\"getdaq-ecc-server\",\"instance_id\":\""+h('a')+
            "\",\"snapshot_sha256\":\""+h('b')+"\",\"operation_active\":false,\"operation_id\":\"\",\"client_epoch\":\"\",\"control_needs_reconciliation\":false,\"message\":\"\","
            "\"controller\":{\"state\":1,\"transition\":0,\"error\":0,\"outcome_unknown\":false,\"needs_reconciliation\":false,\"operation_id\":\"\",\"message\":\"\"},"
            "\"data\":{\"needs_reconciliation\":false,\"controller_scope_sha256\":\""+h('1')+"\",\"controller_epoch\":\""+h('2')+
            "\",\"message\":\"\",\"nodes\":["+secondNode+","+node+"]}}";

        const std::string observedNode0 =
            "{\"agent_id\":\"receiver-0\",\"plan_sha256\":\""+h('4')+"\",\"instance_id\":\""+h('3')+
            "\",\"available\":true,\"message\":\"\",\"observation\":{\"schema\":1,\"busy\":false,\"observed_at_unix_ms\":\"1000\",\"lifecycle\":"+
            "{\"schema\":1,\"state\":\"fresh\",\"needs_reconciliation\":false,\"plan_sha256\":\""+h('4')+
            "\",\"session_id\":\"\",\"message\":\"\",\"receivers\":[]},\"receivers\":[]}}";
        const std::string observedNode1 =
            "{\"agent_id\":\"receiver-1\",\"plan_sha256\":\""+h('6')+"\",\"instance_id\":\""+h('5')+
            "\",\"available\":true,\"message\":\"\",\"observation\":{\"schema\":1,\"busy\":false,\"observed_at_unix_ms\":\"1001\",\"lifecycle\":"+
            "{\"schema\":1,\"state\":\"fresh\",\"needs_reconciliation\":false,\"plan_sha256\":\""+h('6')+
            "\",\"session_id\":\"\",\"message\":\"\",\"receivers\":[]},\"receivers\":[]}}";
        const std::string receivers =
            "{\"schema\":1,\"kind\":\"getdaq.ecc-receivers\",\"instance_id\":\""+h('a')+
            "\",\"snapshot_sha256\":\""+h('b')+"\",\"data_binding_sha256\":\""+h('f')+
            "\",\"nodes\":["+observedNode1+","+observedNode0+"]}";

        const auto capturedDeployment = CaptureScanRuntimeIdentityDeployment(
            "scan-point-0", "console", h('b'), h('c'),
            {"receiver-0", "receiver-1"}, identity, status, receivers);
        Require(capturedDeployment.serviceId == "scan-point-0" &&
                capturedDeployment.clientId == "console" &&
                capturedDeployment.instanceId == h('a') &&
                capturedDeployment.snapshotSha256 == h('b') &&
                capturedDeployment.inputBindingSha256 == h('c') &&
                capturedDeployment.dataBindingSha256 == h('f') &&
                capturedDeployment.dataScopeSha256 == h('1') &&
                capturedDeployment.dataEpoch == h('2') &&
                capturedDeployment.dataNodes.size() == 2 &&
                capturedDeployment.dataNodes[0].agentId == "receiver-0" &&
                capturedDeployment.dataNodes[1].agentId == "receiver-1",
                "Client deployment capture did not bind fixed anchors and ordered receiver observations");
        Reject([&] { CaptureScanRuntimeIdentityDeployment("other-service", "console",
            h('b'), h('c'), {"receiver-0", "receiver-1"}, identity, status, receivers); });
        Reject([&] { CaptureScanRuntimeIdentityDeployment("scan-point-0", "console",
            h('b'), h('c'), {"receiver-0"}, identity, status, receivers); });
        Reject([&] { CaptureScanRuntimeIdentityDeployment("scan-point-0", "console",
            h('b'), h('c'), {"receiver-0", "receiver-1"}, identity, status,
            ReplaceOnce(receivers, "\"available\":true", "\"available\":false")); });

        const auto freshObservation = ObserveScanRuntimeIdentity(identity, status, receivers, expected);
        Require(freshObservation.sha256.size() == 64 &&
                freshObservation.sha256 == ObserveScanRuntimeIdentity(identity, status, expected).sha256 &&
                freshObservation.sha256 == ObserveScanRuntimeIdentity(identity, status,
                    ReplaceOnce(receivers, "1000", "2000"), expected).sha256,
                "Runtime deployment digest changed with a read-only observation timestamp");
        const std::string describedStatus = ReplaceOnce(status, "\"state\":1", "\"state\":2");
        const auto describedObservation = ObserveScanRuntimeIdentityForRevalidation(
            identity, describedStatus, receivers, expected, 2U);
        Require(describedObservation.sha256 == freshObservation.sha256,
                "Described-state identity revalidation changed the static deployment digest");
        Reject([&] { ObserveScanRuntimeIdentityForRevalidation(
            identity, describedStatus, receivers, expected, 3U); });
        ScanRuntimePointIdentityRequest revalidationRequest;
        revalidationRequest.mode = "revalidate";
        revalidationRequest.expectedControllerState = 2U;
        revalidationRequest.executionSnapshotSha256 = expected.executionSnapshotSha256;
        revalidationRequest.pointIndex = expected.pointIndex;
        revalidationRequest.configurationSha256 = expected.configurationSha256;
        revalidationRequest.artifactSha256 = expected.artifactSha256;
        const auto decodedRevalidationRequest = DecodeScanRuntimePointIdentityRequest(
            EncodeScanRuntimePointIdentityRequest(revalidationRequest));
        Require(decodedRevalidationRequest.mode == "revalidate" &&
                decodedRevalidationRequest.expectedControllerState == 2U,
                "Revalidation state was lost from its read-only request");
        ScanRuntimePointIdentityResponse response;
        response.identityJson = identity; response.statusJson = status;
        response.receiversJson = receivers; response.expectation = expected;
        const std::string responseBytes = EncodeScanRuntimePointIdentityResponse(response);
        const auto reopenedResponse = DecodeScanRuntimePointIdentityResponse(responseBytes);
        Require(EncodeScanRuntimeIdentityExpectation(reopenedResponse.expectation) == expectationBytes &&
                reopenedResponse.receiversJson == receivers,
                "Runtime identity response did not round-trip observations and contract");
        Reject([&] { DecodeScanRuntimePointIdentityResponse(
            ReplaceOnce(responseBytes, "\"schema_version\":2", "\"schema_version\":3")); });
        Reject([&] { ObserveScanRuntimeIdentity(identity, status,
            ReplaceOnce(receivers, "\"available\":true", "\"available\":false"), expected); });
        Reject([&] { ObserveScanRuntimeIdentity(identity, status,
            ReplaceOnce(receivers, "\"busy\":false", "\"busy\":true"), expected); });
        Reject([&] { ObserveScanRuntimeIdentity(identity, status,
            ReplaceOnce(receivers, "\"state\":\"fresh\"", "\"state\":\"ready\""), expected); });
        Reject([&] { ObserveScanRuntimeIdentity(identity, status,
            ReplaceOnce(receivers, h('3'), h('7')), expected); });

        const auto observation = ObserveScanRuntimeIdentity(identity, status, expected);
        Require(observation.sha256.size() == 64 && !observation.canonicalBytes.empty(),
                "Validated identity observation did not produce a digest");
        Require(ObserveScanRuntimeIdentity(identity, status, expected).sha256 == observation.sha256,
                "Canonical observation digest is not stable");
        auto otherPoint = expected; otherPoint.pointIndex = 1;
        Require(ObserveScanRuntimeIdentity(identity, status, otherPoint).sha256 != observation.sha256,
                "Canonical observation digest is not scoped to its scan point");
        auto otherExecution = expected; otherExecution.executionSnapshotSha256 = h('9');
        Require(ObserveScanRuntimeIdentity(identity, status, otherExecution).sha256 != observation.sha256,
                "Canonical observation digest is not scoped to its execution snapshot");

        auto mismatch = expected; mismatch.artifactSha256 = h('5');
        Reject([&] { ObserveScanRuntimeIdentity(identity, status, mismatch); });
        mismatch = expected; mismatch.dataNodes[0].planSha256 = h('6');
        Reject([&] { ObserveScanRuntimeIdentity(identity, status, mismatch); });
        mismatch = expected; mismatch.clientId = "other-console";
        Reject([&] { ObserveScanRuntimeIdentity(identity, status, mismatch); });
        mismatch = expected; mismatch.executionSnapshotSha256.clear();
        Reject([&] { ObserveScanRuntimeIdentity(identity, status, mismatch); });
        mismatch = expected; mismatch.pointIndex = 256;
        Reject([&] { ObserveScanRuntimeIdentity(identity, status, mismatch); });
        const auto wrongInstance = ReplaceOnce(status, h('a'), h('7'));
        Reject([&] { ObserveScanRuntimeIdentity(identity, wrongInstance, expected); });
        const auto busy = ReplaceOnce(status, "\"operation_active\":false", "\"operation_active\":true");
        Reject([&] { ObserveScanRuntimeIdentity(identity, busy, expected); });
        const auto staleNode = ReplaceOnce(status, "\"state\":\"fresh\"", "\"state\":\"ready\"");
        Reject([&] { ObserveScanRuntimeIdentity(identity, staleNode, expected); });

        std::reverse(expected.dataNodes.begin(), expected.dataNodes.end());
        Require(ObserveScanRuntimeIdentity(identity, status, expected).sha256 == observation.sha256,
                "Data node expectation order changed the canonical identity");
        std::cout << "PASS validated ECC runtime identity, fresh live receiver nodes, idle state, data scope/epoch and exact plans\n";
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
