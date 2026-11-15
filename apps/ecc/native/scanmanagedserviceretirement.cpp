#include "scanmanagedserviceretirement.h"

#include "configurationsha256.h"

#include <stdexcept>

namespace GetDAQ {

EccServiceProcessExitEvidence RetireManagedEccServiceForScanPoint(
    ScanExecutionJournalStore &store,
    EccServiceProcessSupervisor &supervisor,
    const std::string &address, unsigned int port,
    const std::string &credentialFile,
    const std::string &expectedServiceConfigurationSha256,
    const std::string &instanceId, const std::string &serviceSnapshotSha256,
    const std::string &handoffOperationId,
    const std::string &retirementOperationId,
    unsigned int timeoutMs)
{
    const ScanExecutionJournal current = store.Current();
    if (!supervisor.Running())
        throw std::runtime_error("Managed ECC service is not running; retirement requires its live parent process");
    if (supervisor.ServiceConfigurationSha256() != expectedServiceConfigurationSha256)
        throw std::runtime_error("Managed ECC process configuration differs from the reviewed deployment");

    const ScanExecutionJournal intent = current.WithManagedServiceShutdownIntent(
        retirementOperationId, supervisor.ProcessId(), expectedServiceConfigurationSha256,
        instanceId, serviceSnapshotSha256, handoffOperationId);
    store.Persist(intent);

    try {
        const EccServiceProcessExitEvidence evidence = supervisor.ShutdownAndWait(
            address, port, credentialFile, instanceId, serviceSnapshotSha256,
            handoffOperationId, retirementOperationId, timeoutMs,
            current.ExecutionId(), current.SourceSnapshotSha256(), current.PointIndex(),
            current.RuntimeIdentitySha256());
        const ScanExecutionJournal retired = store.Current().WithManagedServiceRetirement(evidence);
        store.Persist(retired);
        (void)store.Current();
        return evidence;
    } catch (const std::exception &failure) {
        try {
            const ScanExecutionJournal unknown = store.Current().WithManagedServiceShutdownUnknown(
                ConfigurationSha256(failure.what()));
            store.Persist(unknown);
        } catch (const std::exception &journalFailure) {
            throw std::runtime_error(std::string("ECC shutdown outcome is unknown and its journal quarantine could not be persisted: ") +
                                     journalFailure.what());
        }
        throw std::runtime_error(std::string("ECC shutdown outcome is unknown; scan point was quarantined: ") +
                                 failure.what());
    } catch (...) {
        try {
            const ScanExecutionJournal unknown = store.Current().WithManagedServiceShutdownUnknown(
                ConfigurationSha256("non-standard-exception"));
            store.Persist(unknown);
        } catch (...) {
            throw std::runtime_error("ECC shutdown outcome is unknown; its journal quarantine also failed");
        }
        throw std::runtime_error("ECC shutdown outcome is unknown; scan point was quarantined");
    }
}

}
