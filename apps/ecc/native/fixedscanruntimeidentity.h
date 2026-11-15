#ifndef GETDAQ_FIXEDSCANRUNTIMEIDENTITY_H
#define GETDAQ_FIXEDSCANRUNTIMEIDENTITY_H

#include "fixedeccconfiguration.h"
#include "../../../common/control/scanruntimeidentity.h"

namespace GetDAQ {

// Derive the static point expectation from the checked fixed deployment and
// bind only runtime-generated instance/epoch values from a matching idle
// identity/status observation. The observation is validated before return.
ScanRuntimeIdentityExpectation BuildFixedScanRuntimeIdentityExpectation(
    const ScanExecutionSnapshot &snapshot,
    unsigned int pointIndex,
    const FixedEccConfiguration &deployment,
    const std::string &identityBytes,
    const std::string &statusBytes);

// Service-side overload for a bounded HTTP point request. The requested
// hashes are accepted only if they match the active fixed deployment.
ScanRuntimeIdentityExpectation BuildFixedScanRuntimeIdentityExpectation(
    const ScanRuntimePointIdentityRequest &request,
    const FixedEccConfiguration &deployment,
    const std::string &identityBytes,
    const std::string &statusBytes);

}

#endif
