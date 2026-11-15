#ifndef NATIVEPLANRETIREMENTPROJECTION_H
#define NATIVEPLANRETIREMENTPROJECTION_H

#include "../../common/control/eccprotocol.h"
#include <string>

namespace GetDAQ {
struct NativePlanRetirementProjectionInput {
    EccControlRequest request;
    EccControlSystemStatus system;
    bool availableForControl=false;
    bool normalRetirementEvidence=false;
    bool connected=false;
    bool shuttingDown=false;
    bool haveStatus=false;
    bool retirementComplete=false;
    bool activeRun=false;
    bool busy=false;
    bool clientAvailable=false;
    bool clientBusy=false;
    bool clientBusyForObservation=false;
    bool storeAvailable=false;
    bool storeCanComplete=false;
    bool clientCanComplete=false;
    bool hasRetirementContext=false;
    bool hasCompletion=false;
    std::string originalStatus;
    std::string completionRequest;
    std::string completionPhase;
    std::string phase;
};

struct NativePlanRetirementProjection {
    bool canSubmit=false;
    bool canInspect=false;
    bool retirementHeld=false;
    bool canComplete=false;
    bool canObserveCompletion=false;
    std::string originalRequest;
    std::string originalStatus;
    std::string completionRequest;
    std::string completionPhase;
    std::string reason;
};

NativePlanRetirementProjection ProjectNativePlanRetirement(
    const NativePlanRetirementProjectionInput &input);
}

#endif
