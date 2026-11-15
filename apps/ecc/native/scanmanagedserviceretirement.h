#ifndef GETDAQ_SCANMANAGEDSERVICERETIREMENT_H
#define GETDAQ_SCANMANAGEDSERVICERETIREMENT_H

#include "scanexecutionjournalstore.h"
#include "eccserviceprocesssupervisor.h"

#include <string>

namespace GetDAQ {

// Synchronous, Qt-free bridge for a worker thread. It persists the exact
// shutdown intent before the HTTP side effect and quarantines every ambiguous
// result instead of retrying or advancing the scan.
EccServiceProcessExitEvidence RetireManagedEccServiceForScanPoint(
    ScanExecutionJournalStore &store,
    EccServiceProcessSupervisor &supervisor,
    const std::string &address, unsigned int port,
    const std::string &credentialFile,
    const std::string &expectedServiceConfigurationSha256,
    const std::string &instanceId, const std::string &serviceSnapshotSha256,
    const std::string &handoffOperationId,
    const std::string &retirementOperationId,
    unsigned int timeoutMs);

}

#endif
