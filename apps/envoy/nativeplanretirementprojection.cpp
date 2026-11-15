#include "nativeplanretirementprojection.h"

namespace GetDAQ {
NativePlanRetirementProjection ProjectNativePlanRetirement(
    const NativePlanRetirementProjectionInput &input) {
    NativePlanRetirementProjection view;
    const bool sameInstance=input.system.instanceId==input.request.instanceId;
    const bool operationIdle=!input.system.operationActive;
    const bool originalLive=input.connected && !input.shuttingDown && input.haveStatus &&
        input.clientAvailable && !input.activeRun && sameInstance && operationIdle;

    view.canSubmit=input.availableForControl && input.normalRetirementEvidence;
    view.canInspect=input.connected && !input.shuttingDown && input.haveStatus &&
        input.retirementComplete && input.clientAvailable && !input.clientBusy &&
        operationIdle && sameInstance;
    view.retirementHeld=input.request.command=="retire-data-plans";
    view.canComplete=originalLive && !input.busy && input.storeAvailable &&
        input.storeCanComplete && input.clientCanComplete;
    view.canObserveCompletion=originalLive && input.hasCompletion && !input.clientBusyForObservation;
    if(view.retirementHeld)
        view.originalRequest=EncodeEccControlRequest(input.request);
    view.originalStatus=input.originalStatus;
    view.completionRequest=input.completionRequest;
    view.completionPhase=input.completionPhase;
    view.reason=!originalLive ? "Completion requires the original live ECC instance" :
        !input.hasRetirementContext ? "Saved pre-retirement status is missing; observation only" :
        input.phase;
    return view;
}
}
