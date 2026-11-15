#ifndef GETDAQ_SCANRUNTIMEIDENTITYCOORDINATOR_H
#define GETDAQ_SCANRUNTIMEIDENTITYCOORDINATOR_H

#include "scanexecutionjournalstore.h"
#include "scanoperationrunner.h"
#include "../../common/control/scanruntimeidentity.h"
#include "../../common/experiment/scanexecutionsnapshot.h"

namespace GetDAQ {

// Joins the read-only ECC point-identity request to the durable per-point
// journal boundary. It does not submit ECC commands or start acquisition.
class ScanRuntimeIdentityCoordinator {
public:
    ScanRuntimeIdentityCoordinator(ScanExecutionJournalStore &store,
                                   const ScanExecutionSnapshot &snapshot,
                                   const ScanRuntimeIdentityExpectation &expected,
                                   bool revalidateExistingBinding = false,
                                   bool adoptExistingInstance = false);

    // Call before the HTTP request. A point that has not durably completed
    // start-ecc, has an unresolved intent, or already has a binding is refused.
    ScanRuntimePointIdentityRequest PrepareQuery() const;

    // Recovery-only readback for an already-bound point. It permits a fresh
    // identity observation from ECC_STARTED through CONFIGURED and never
    // changes the persisted identity digest.
    ScanRuntimePointIdentityRequest PrepareRevalidationQuery() const;

    // Explicitly adopt an ECC process that was started outside this console.
    // The durable intent is written before the read-only identity request.
    ScanRuntimePointIdentityRequest PrepareAdoptionQuery(
        const std::string &operationId);

    // Call only with the complete response body returned by the ECC client.
    // The response is rechecked against the current journal before persistence.
    ScanExecutionJournal BindResponse(const std::string &responseBytes);

private:
    ScanExecutionJournalStore &fStore;
    ScanExecutionSnapshot fSnapshot;
    ScanRuntimeIdentityExpectation fExpected;
    bool fRevalidateExistingBinding;
    bool fAdoptExistingInstance;
    std::string fAdoptionOperationId;
};

}

#endif
