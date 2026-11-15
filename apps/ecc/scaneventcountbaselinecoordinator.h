#ifndef GETDAQ_SCANEVENTCOUNTBASELINECOORDINATOR_H
#define GETDAQ_SCANEVENTCOUNTBASELINECOORDINATOR_H

#include "scanexecutionjournalstore.h"
#include "../../common/control/scanruntimeidentity.h"
#include "../../common/experiment/scanexecutionsnapshot.h"

namespace GetDAQ {

// Binds one authenticated ECC receiver-event observation to the configured
// point in the durable scan journal. This class performs no network or DAQ I/O.
class ScanEventCountBaselineCoordinator {
public:
    ScanEventCountBaselineCoordinator(ScanExecutionJournalStore &store,
                                      const ScanExecutionSnapshot &snapshot,
                                      const ScanRuntimeIdentityExpectation &expected);

    // Revalidate the current journal before allowing the caller to issue the
    // read-only event-count query for this point's selected CoBo source.
    std::string PrepareQuery() const;

    // Call only with the response from the authenticated ECC receiver-events
    // endpoint. The exact journal successor is persisted before it is returned.
    ScanExecutionJournal BindResponse(
        const std::string &responseBytes,
        const EccControlReceiverObservation &currentReceivers);

private:
    ScanExecutionJournalStore &fStore;
    ScanExecutionSnapshot fSnapshot;
    ScanRuntimeIdentityExpectation fExpected;
};

}

#endif
