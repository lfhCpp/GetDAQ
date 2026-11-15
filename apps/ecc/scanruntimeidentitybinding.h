#ifndef GETDAQ_SCANRUNTIMEIDENTITYBINDING_H
#define GETDAQ_SCANRUNTIMEIDENTITYBINDING_H

#include "scanexecutionjournalstore.h"
#include "../../common/control/scanruntimeidentity.h"
#include "../../common/experiment/scanexecutionsnapshot.h"

namespace GetDAQ {

// Validate read-only ECC identity/status and a fresh receiver observation
// against the frozen point contract, then durably append its identity digest.
ScanExecutionJournal PersistScanRuntimeIdentityBinding(
    ScanExecutionJournalStore &store,
    const ScanExecutionSnapshot &snapshot,
    const ScanRuntimeIdentityExpectation &expected,
    const std::string &responseBytes);

ScanExecutionJournal PersistScanRuntimeIdentityBinding(
    ScanExecutionJournalStore &store,
    const ScanExecutionSnapshot &snapshot,
    const ScanRuntimeIdentityExpectation &expected,
    const std::string &identityBytes,
    const std::string &statusBytes,
    const std::string &receiverObservationBytes);

}

#endif
