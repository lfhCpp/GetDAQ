#ifndef GETDAQ_SCANEVENTCOUNTOBSERVATION_H
#define GETDAQ_SCANEVENTCOUNTOBSERVATION_H

#include "eccprotocol.h"
#include "scanruntimeidentity.h"
#include "../experiment/scaneventcountmonitor.h"

#include <string>

namespace GetDAQ {

// Bind a receiver event sample to the exact frozen scan deployment and to
// independently refreshed whole-system and receiver-process observations.
// This is a pure validation step: it performs no I/O and submits no commands.
ScanEventCountBaseline BindScanEventCountObservation(
    const std::string &senderId,
    const std::string &responseBytes,
    bool systemStatusFresh,
    const EccControlSystemStatus &systemStatus,
    bool receiverStatusFresh,
    const EccControlReceiverObservation &receiverStatus,
    const ScanRuntimeIdentityExpectation &expected,
    const std::string &targetCoBo);

}

#endif
