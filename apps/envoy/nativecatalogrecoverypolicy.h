#ifndef NATIVECATALOGRECOVERYPOLICY_H
#define NATIVECATALOGRECOVERYPOLICY_H

#include "../../common/control/eccprotocol.h"
#include <string>

namespace GetDAQ {
bool CanResumeNativeCatalogReplacement(const EccControlSystemStatus &status,
                                       const std::string &journalFailedOperationId,
                                       const std::string &requestedFailedOperationId,
                                       const std::string &operationId,
                                       bool replacesCompletedRun,
                                       bool recoveryContextReady,
                                       bool statusFresh,
                                       bool replacementEligible,
                                       bool commandAvailable);
}

#endif
