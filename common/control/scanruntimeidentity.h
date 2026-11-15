#ifndef GETDAQ_SCANRUNTIMEIDENTITY_H
#define GETDAQ_SCANRUNTIMEIDENTITY_H

#include "eccprotocol.h"
#include "../experiment/scanexecutionsnapshot.h"

#include <string>
#include <vector>

namespace GetDAQ {

struct ScanRuntimeDataNodeExpectation {
    std::string agentId;
    std::string instanceId;
    std::string planSha256;
};

// Values captured from the reviewed point deployment before observing ECC.
// This is a comparison contract, not a source of truth by itself.
struct ScanRuntimeIdentityExpectation {
    // Binds this observation to the immutable scan input and exact point in
    // its journal. These differ from the ECC service's configuration snapshot.
    std::string executionSnapshotSha256;
    unsigned int pointIndex = 0;
    std::string serviceId;
    std::string clientId;
    std::string instanceId;
    std::string snapshotSha256;
    std::string inputBindingSha256;
    std::string configurationSha256;
    std::string artifactSha256;
    std::string dataBindingSha256;
    std::string dataScopeSha256;
    std::string dataEpoch;
    std::vector<ScanRuntimeDataNodeExpectation> dataNodes;
};

// Runtime deployment identity captured by the point launcher/receiver
// deployment adapter. Static scan-input hashes are intentionally omitted;
// BindScanRuntimeIdentityExpectation derives them from the validated snapshot.
struct ScanRuntimeIdentityDeployment {
    std::string serviceId;
    std::string clientId;
    std::string instanceId;
    std::string snapshotSha256;
    std::string inputBindingSha256;
    std::string dataBindingSha256;
    std::string dataScopeSha256;
    std::string dataEpoch;
    std::vector<ScanRuntimeDataNodeExpectation> dataNodes;
};

// Build a client-side complete expectation from the independently authenticated
// identity, status, and receiver observations. Fixed client anchors and the
// configured receiver-agent set are supplied locally; runtime epochs/instances
// are accepted only when all observations agree.
ScanRuntimeIdentityDeployment CaptureScanRuntimeIdentityDeployment(
    const std::string &fixedServiceId,
    const std::string &fixedClientId,
    const std::string &fixedSnapshotSha256,
    const std::string &fixedInputBindingSha256,
    const std::vector<std::string> &configuredAgentIds,
    const std::string &identityBytes,
    const std::string &statusBytes,
    const std::string &receiverObservationBytes,
    bool allowResolvedLifecycle = false);

ScanRuntimeIdentityExpectation BindScanRuntimeIdentityExpectation(
    const ScanExecutionSnapshot &snapshot,
    unsigned int pointIndex,
    const ScanRuntimeIdentityDeployment &deployment);

struct ScanRuntimeIdentityObservation {
    std::string canonicalBytes;
    std::string sha256;
};

// Authenticated service response to a point-identity request. The JSON fields
// are carried as bounded strings so their original protocol bytes remain
// available to the journal validator.
struct ScanRuntimePointIdentityResponse {
    std::string mode = "adopt";
    unsigned int observedControllerState = 1;
    std::string identityJson;
    std::string statusJson;
    std::string receiversJson;
    ScanRuntimeIdentityExpectation expectation;
};

// Small authenticated control-plane contract used to ask a fixed ECC service
// whether its active deployment matches one frozen scan point. It carries no
// configuration payload and cannot execute a hardware operation.
struct ScanRuntimePointIdentityRequest {
    // Adoption is a one-time check of a reviewed point while ECC is idle.
    // Revalidation checks the same frozen deployment after a lifecycle step.
    std::string mode = "adopt";
    unsigned int expectedControllerState = 1;
    std::string executionSnapshotSha256;
    unsigned int pointIndex = 0;
    std::string configurationSha256;
    std::string artifactSha256;
};

std::string EncodeScanRuntimePointIdentityRequest(
    const ScanRuntimePointIdentityRequest &request);
ScanRuntimePointIdentityRequest DecodeScanRuntimePointIdentityRequest(
    const std::string &bytes);

// The ECC service returns this fixed-deployment contract with its fresh
// identity/status observations. The caller must still compare the scan and
// configured service anchors before accepting or persisting the observation.
std::string EncodeScanRuntimeIdentityExpectation(
    const ScanRuntimeIdentityExpectation &expected);
ScanRuntimeIdentityExpectation DecodeScanRuntimeIdentityExpectation(
    const std::string &bytes);

std::string EncodeScanRuntimePointIdentityResponse(
    const ScanRuntimePointIdentityResponse &response);
ScanRuntimePointIdentityResponse DecodeScanRuntimePointIdentityResponse(
    const std::string &bytes);

// Validates the two read-only ECC observations against the caller's frozen
// per-point expectation and returns a stable digest input. This does not
// authenticate the service or prove physical hardware state.
ScanRuntimeIdentityObservation ObserveScanRuntimeIdentity(
    const std::string &identityBytes,
    const std::string &statusBytes,
    const ScanRuntimeIdentityExpectation &expected);

// Includes a just-fetched remote receiver-process observation. Every expected
// node must be available, idle, fresh, and have no receiver process/session.
ScanRuntimeIdentityObservation ObserveScanRuntimeIdentity(
    const std::string &identityBytes,
    const std::string &statusBytes,
    const std::string &receiverObservationBytes,
    const ScanRuntimeIdentityExpectation &expected);

// Read-only revalidation of the same deployment at the controller state
// recorded by the scan journal. ECC state values 1..4 are Idle, Described,
// Prepared and Ready. The returned deployment digest is stable across steps.
ScanRuntimeIdentityObservation ObserveScanRuntimeIdentityForRevalidation(
    const std::string &identityBytes,
    const std::string &statusBytes,
    const ScanRuntimeIdentityExpectation &expected,
    unsigned int controllerState);
ScanRuntimeIdentityObservation ObserveScanRuntimeIdentityForRevalidation(
    const std::string &identityBytes,
    const std::string &statusBytes,
    const std::string &receiverObservationBytes,
    const ScanRuntimeIdentityExpectation &expected,
    unsigned int controllerState);

}

#endif
