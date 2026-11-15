#include "scanmanagedservicestart.h"

#include "configurationsha256.h"

#include <exception>
#include <stdexcept>

namespace GetDAQ {

EccServiceProcessStartEvidence StartManagedEccServiceForScanPoint(
    ScanExecutionJournalStore &store,
    EccServiceProcessSupervisor &supervisor,
    const std::string &executable, const std::string &configurationFile,
    const std::string &expectedExecutableSha256,
    const std::string &expectedConfigurationSha256,
    const std::string &operationId)
{
    const ScanExecutionJournal current = store.Current();
    const ScanExecutionJournal intent = current.WithManagedServiceStartIntent(
        operationId, expectedExecutableSha256, expectedConfigurationSha256);
    store.Persist(intent);

    try {
        const EccServiceProcessStartEvidence evidence = supervisor.StartForScanPoint(
            executable, configurationFile, expectedExecutableSha256,
            expectedConfigurationSha256, current.ExecutionId(),
            current.SourceSnapshotSha256(), current.PointIndex(), operationId);
        const ScanExecutionJournal started = store.Current().WithManagedServiceStarted(evidence);
        store.Persist(started);
        return evidence;
    } catch (const std::exception &failure) {
        try {
            const ScanExecutionJournal unknown = store.Current().WithManagedServiceStartUnknown(
                ConfigurationSha256(failure.what()));
            store.Persist(unknown);
        } catch (const std::exception &journalFailure) {
            throw std::runtime_error(std::string("Managed ECC startup outcome is unknown and its journal quarantine could not be persisted: ") +
                                     journalFailure.what());
        }
        throw std::runtime_error(std::string("Managed ECC startup outcome is unknown; scan point was quarantined: ") +
                                 failure.what());
    } catch (...) {
        try {
            const ScanExecutionJournal unknown = store.Current().WithManagedServiceStartUnknown(
                ConfigurationSha256("non-standard-exception"));
            store.Persist(unknown);
        } catch (...) {
            throw std::runtime_error("Managed ECC startup outcome is unknown; its journal quarantine also failed");
        }
        throw std::runtime_error("Managed ECC startup outcome is unknown; scan point was quarantined");
    }
}

}
