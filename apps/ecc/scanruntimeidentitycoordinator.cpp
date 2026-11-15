#include "scanruntimeidentitycoordinator.h"

#include "scanruntimeidentitybinding.h"
#include "../../common/experiment/configurationsha256.h"

#include <stdexcept>

namespace GetDAQ {
namespace {

void Require(bool condition, const char *message)
{
    if (!condition) throw std::invalid_argument(message);
}

}

ScanRuntimeIdentityCoordinator::ScanRuntimeIdentityCoordinator(
    ScanExecutionJournalStore &store,
    const ScanExecutionSnapshot &snapshot,
    const ScanRuntimeIdentityExpectation &expected,
    bool revalidateExistingBinding,
    bool adoptExistingInstance)
    : fStore(store), fSnapshot(snapshot), fExpected(expected),
      fRevalidateExistingBinding(revalidateExistingBinding),
      fAdoptExistingInstance(adoptExistingInstance)
{
    // Reject a caller's stale/mismatched point contract before it can initiate
    // even a read-only network request.
    Require(!(fRevalidateExistingBinding && fAdoptExistingInstance),
            "Runtime identity revalidation and ECC adoption are mutually exclusive");
    if (fRevalidateExistingBinding) (void)PrepareRevalidationQuery();
    else if (fAdoptExistingInstance) {
        const ScanExecutionJournal current = fStore.Current();
        Require(current.State() == ScanPointState::Reviewed &&
                !current.HasPendingOperation() && !current.PendingOperationUnknown() &&
                current.RuntimeIdentitySha256().empty(),
                "ECC adoption requires a reviewed point with no pending operation or identity binding");
    } else (void)PrepareQuery();
}

namespace {

ScanRuntimePointIdentityRequest MakeRequest(
    const ScanExecutionJournal &current,
    const ScanExecutionSnapshot &snapshot,
    const ScanRuntimeIdentityExpectation &expected,
    const std::string &mode,
    unsigned int expectedControllerState)
{
    const std::string snapshotBytes = snapshot.Encode();
    Require(snapshot.ExecutionId() == current.ExecutionId() &&
            ConfigurationSha256(snapshotBytes) == current.SourceSnapshotSha256() &&
            snapshot.SourcePlanSha256() == current.SourcePlanSha256() &&
            snapshot.PointCount() == current.PointCount(),
            "ECC runtime identity snapshot differs from the journal's frozen execution input");
    Require(expected.executionSnapshotSha256 == current.SourceSnapshotSha256() &&
            expected.pointIndex == current.PointIndex() &&
            expected.pointIndex < snapshot.Points().size(),
            "ECC runtime identity expectation differs from the journal's current scan point");
    const ScanExecutionSnapshotPoint &point = snapshot.Points()[expected.pointIndex];
    Require(expected.configurationSha256 == point.configurationSha256 &&
            expected.artifactSha256 == point.artifactSha256,
            "ECC runtime identity expectation differs from the frozen scan point artifacts");

    ScanRuntimePointIdentityRequest request;
    request.mode = mode;
    request.expectedControllerState = expectedControllerState;
    request.executionSnapshotSha256 = current.SourceSnapshotSha256();
    request.pointIndex = expected.pointIndex;
    request.configurationSha256 = point.configurationSha256;
    request.artifactSha256 = point.artifactSha256;
    (void)EncodeScanRuntimePointIdentityRequest(request);
    (void)EncodeScanRuntimeIdentityExpectation(expected);
    return request;
}

}

ScanRuntimePointIdentityRequest ScanRuntimeIdentityCoordinator::PrepareQuery() const
{
    const ScanExecutionJournal current = fStore.Current();
    Require(current.State() == ScanPointState::EccStarted &&
            !current.HasPendingOperation() && !current.PendingOperationUnknown(),
            "ECC runtime identity query requires a durably completed start-ecc operation");
    Require(current.RuntimeIdentitySha256().empty(),
            "ECC runtime identity is already bound for this scan point");
    return MakeRequest(current, fSnapshot, fExpected, "adopt", 1U);
}

ScanRuntimePointIdentityRequest ScanRuntimeIdentityCoordinator::PrepareRevalidationQuery() const
{
    const ScanExecutionJournal current = fStore.Current();
    const bool eligibleState = current.State() == ScanPointState::EccStarted ||
        current.State() == ScanPointState::Described ||
        current.State() == ScanPointState::Prepared ||
        current.State() == ScanPointState::Configured;
    Require(eligibleState && !current.HasPendingOperation() &&
            !current.PendingOperationUnknown(),
            "Runtime identity revalidation requires a resolved point before Run start");
    Require(!current.RuntimeIdentitySha256().empty(),
            "Runtime identity revalidation requires an existing journal binding");
    unsigned int controllerState = 0;
    switch (current.State()) {
    case ScanPointState::EccStarted: controllerState = 1U; break;
    case ScanPointState::Described: controllerState = 2U; break;
    case ScanPointState::Prepared: controllerState = 3U; break;
    case ScanPointState::Configured: controllerState = 4U; break;
    default: throw std::invalid_argument("Journal state cannot be revalidated before Run start");
    }
    return MakeRequest(current, fSnapshot, fExpected, "revalidate", controllerState);
}

ScanRuntimePointIdentityRequest ScanRuntimeIdentityCoordinator::PrepareAdoptionQuery(
    const std::string &operationId)
{
    Require(fAdoptExistingInstance && fAdoptionOperationId.empty(),
            "ECC adoption query is not available or was already submitted");
    const ScanExecutionJournal current = fStore.Current();
    Require(current.State() == ScanPointState::Reviewed &&
            !current.HasPendingOperation() && !current.PendingOperationUnknown() &&
            current.RuntimeIdentitySha256().empty(),
            "ECC adoption requires a reviewed point with no pending operation or identity binding");
    const ScanRuntimePointIdentityRequest request =
        MakeRequest(current, fSnapshot, fExpected, "adopt", 1U);
    ScanOperationRunner runner(fStore);
    runner.PersistIntent(operationId, "adopt-ecc",
                         EncodeScanRuntimePointIdentityRequest(request));
    fAdoptionOperationId = operationId;
    return request;
}

ScanExecutionJournal ScanRuntimeIdentityCoordinator::BindResponse(
    const std::string &responseBytes)
{
    if (fRevalidateExistingBinding) {
        (void)PrepareRevalidationQuery();
        const ScanRuntimePointIdentityResponse response =
            DecodeScanRuntimePointIdentityResponse(responseBytes);
        Require(EncodeScanRuntimeIdentityExpectation(response.expectation) ==
                EncodeScanRuntimeIdentityExpectation(fExpected),
                "Runtime identity response belongs to a different deployment or scan point");
        Require(response.mode == "revalidate",
                "Runtime identity response does not match the revalidation request");
        const ScanExecutionJournal current = fStore.Current();
        const ScanRuntimePointIdentityRequest request = PrepareRevalidationQuery();
        Require(response.observedControllerState == request.expectedControllerState,
                "Runtime identity response observed a different journal lifecycle state");
        const ScanRuntimeIdentityObservation observed =
            ObserveScanRuntimeIdentityForRevalidation(
                response.identityJson, response.statusJson, response.receiversJson,
                fExpected, request.expectedControllerState);
        Require(observed.sha256 == current.RuntimeIdentitySha256(),
                "Fresh ECC runtime identity differs from the persisted scan binding");
        return current;
    }
    if (fAdoptExistingInstance) {
        Require(!fAdoptionOperationId.empty(),
                "ECC adoption response arrived without a persisted adoption intent");
        const ScanExecutionJournal current = fStore.Current();
        Require(current.State() == ScanPointState::Reviewed &&
                current.HasPendingOperation() &&
                current.PendingOperationId() == fAdoptionOperationId &&
                current.PendingOperation() == "adopt-ecc" &&
                !current.PendingOperationUnknown(),
                "ECC adoption response no longer matches the pending journal intent");
        const ScanRuntimePointIdentityResponse response =
            DecodeScanRuntimePointIdentityResponse(responseBytes);
        Require(response.mode == "adopt",
                "Runtime identity response does not match the adoption request");
        Require(EncodeScanRuntimeIdentityExpectation(response.expectation) ==
                EncodeScanRuntimeIdentityExpectation(fExpected),
                "Runtime identity response belongs to a different deployment or scan point");
        (void)ObserveScanRuntimeIdentity(response.identityJson, response.statusJson,
                                         response.receiversJson, fExpected);
        ScanOperationRunner runner(fStore);
        runner.Complete(fAdoptionOperationId, responseBytes);
        fAdoptionOperationId.clear();
        return PersistScanRuntimeIdentityBinding(fStore, fSnapshot, fExpected,
                                                 responseBytes);
    }
    (void)PrepareQuery();
    return PersistScanRuntimeIdentityBinding(fStore, fSnapshot, fExpected, responseBytes);
}

}
