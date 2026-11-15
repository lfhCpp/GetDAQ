#include "scanruntimeidentitybinding.h"

#include <stdexcept>

#include "../../common/experiment/configurationsha256.h"

namespace GetDAQ {

ScanExecutionJournal PersistScanRuntimeIdentityBinding(
    ScanExecutionJournalStore &store,
    const ScanExecutionSnapshot &snapshot,
    const ScanRuntimeIdentityExpectation &expected,
    const std::string &responseBytes)
{
    const ScanRuntimePointIdentityResponse response =
        DecodeScanRuntimePointIdentityResponse(responseBytes);
    if (EncodeScanRuntimeIdentityExpectation(response.expectation) !=
        EncodeScanRuntimeIdentityExpectation(expected))
        throw std::invalid_argument(
            "Runtime identity response belongs to a different deployment or scan point");
    return PersistScanRuntimeIdentityBinding(store, snapshot, expected,
        response.identityJson, response.statusJson, response.receiversJson);
}

ScanExecutionJournal PersistScanRuntimeIdentityBinding(
    ScanExecutionJournalStore &store,
    const ScanExecutionSnapshot &snapshot,
    const ScanRuntimeIdentityExpectation &expected,
    const std::string &identityBytes,
    const std::string &statusBytes,
    const std::string &receiverObservationBytes)
{
    const ScanExecutionJournal current = store.Current();
    const std::string snapshotBytes = snapshot.Encode();
    if (snapshot.ExecutionId() != current.ExecutionId() ||
        ConfigurationSha256(snapshotBytes) != current.SourceSnapshotSha256() ||
        snapshot.SourcePlanSha256() != current.SourcePlanSha256() ||
        snapshot.PointCount() != current.PointCount())
        throw std::invalid_argument(
            "Runtime identity snapshot differs from the journal's frozen execution input");
    if (expected.executionSnapshotSha256 != current.SourceSnapshotSha256())
        throw std::invalid_argument(
            "Runtime identity expectation belongs to a different scan execution snapshot");
    if (expected.pointIndex != current.PointIndex() ||
        expected.pointIndex >= snapshot.Points().size())
        throw std::invalid_argument(
            "Runtime identity expectation belongs to a different scan point");
    const ScanExecutionSnapshotPoint &point = snapshot.Points()[expected.pointIndex];
    if (expected.configurationSha256 != point.configurationSha256 ||
        expected.artifactSha256 != point.artifactSha256)
        throw std::invalid_argument(
            "Runtime identity expectation configuration/artifact differs from the frozen scan point");
    const ScanRuntimeIdentityObservation observation = ObserveScanRuntimeIdentity(
        identityBytes, statusBytes, receiverObservationBytes, expected);
    const ScanExecutionJournal next =
        current.WithRuntimeIdentityBinding(observation.sha256);
    store.Persist(next);
    return store.Current();
}

}
