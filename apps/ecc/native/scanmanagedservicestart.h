#ifndef GETDAQ_SCANMANAGEDSERVICESTART_H
#define GETDAQ_SCANMANAGEDSERVICESTART_H

#include "scanexecutionjournalstore.h"
#include "eccserviceprocesssupervisor.h"

#include <string>

namespace GetDAQ {

// Persist the reviewed deployment intent before launching the ECC child.
// Any failure after that point quarantines the scan; this helper never retries.
EccServiceProcessStartEvidence StartManagedEccServiceForScanPoint(
    ScanExecutionJournalStore &store,
    EccServiceProcessSupervisor &supervisor,
    const std::string &executable, const std::string &configurationFile,
    const std::string &expectedExecutableSha256,
    const std::string &expectedConfigurationSha256,
    const std::string &operationId);

}

#endif
