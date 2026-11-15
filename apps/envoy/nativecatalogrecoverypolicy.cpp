#include "nativecatalogrecoverypolicy.h"

namespace GetDAQ {
bool CanResumeNativeCatalogReplacement(const EccControlSystemStatus &status,
                                       const std::string &journalFailedOperationId,
                                       const std::string &requestedFailedOperationId,
                                       const std::string &operationId,
                                       bool replacesCompletedRun,
                                       bool recoveryContextReady,
                                       bool statusFresh,
                                       bool replacementEligible,
                                       bool commandAvailable) {
    return recoveryContextReady &&
        !journalFailedOperationId.empty() &&
        requestedFailedOperationId==journalFailedOperationId &&
        !operationId.empty() && operationId!=journalFailedOperationId &&
        replacesCompletedRun && statusFresh &&
        !status.operationActive && status.controller.state==3 &&
        !status.controller.transition && !status.controller.error &&
        !status.controller.outcomeUnknown && !status.controller.needsReconciliation &&
        !status.controlNeedsReconciliation && replacementEligible && commandAvailable;
}
}
