#include "nativeruncontrol.h"
#include "nativeplanretirementprojection.h"
#include "nativereceiverfinalizationpolicy.h"
#include <QUuid>
#include <stdexcept>

void NativeRunControl::RetireDataPlans(const QByteArray &successorBytes,const QByteArray &targetsBytes) {
    if(!Available() || fActive || !GetDAQ::HasNormalNativeRetirementEvidence(fSystem)) {emit PlanTransitionResultSignal({},{},"Plan retirement requires the original healthy Idle/Described after normal sealed acquisition");return;}
    try {GetDAQ::ReviewEccPlanTransition(successorBytes.toStdString(),targetsBytes.toStdString());}
    catch(const std::exception &e) {emit PlanTransitionResultSignal({},{},QString::fromUtf8(e.what()));return;}
    fCommand="retire-data-plans";fTarget=-1;fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-retire-plans";
    fBusy=true;fPhase="Retiring reviewed data plans";fDeadline.start(fConfig.stepTimeoutMs);Publish();
    if(!fClient->SubmitPlanRetirement(successorBytes,targetsBytes,fOperationId))Fail("Plan retirement was not confirmed submitted; preserve the original intent and inspect it");
}
void NativeRunControl::InspectPlanRetirementReceipt() {
    if(!fConnected || fShuttingDown || !fHaveStatus || !fPlanRetirement.complete || !fClient || fSystem.operationActive || fSystem.instanceId!=fPlanRetirement.request.instanceId) {
        emit PlanTransitionResultSignal({},{},"Receipt requires the original live completed retirement");return;
    }
    std::string originalStatus;
    if(fStore && fStore->PlanRetirementContext(&originalStatus))fPlanRetirement.receiptStatus=GetDAQ::DecodeEccControlSystemStatus(originalStatus);
    else fPlanRetirement.receiptStatus=fSystem;
    if(!fClient->QueryControlHandoff(fPlanRetirement.request,fPlanRetirement.receiptStatus))emit LogSignal("Receipt query is unavailable or already pending");
    Publish();
}

void NativeRunControl::CompletePlanRetirement(const QString &originalOperationId) {
    try {
        if(!fConnected || fShuttingDown || fActive || fBusy || !fHaveStatus || !fClient || !fStore ||
            originalOperationId.toStdString()!=fPlanRetirement.request.operationId || !fStore->CanCompletePlanRetirement() ||
            !fClient->CanSubmit("complete-data-handoff"))throw std::runtime_error("Explicit completion requires the saved failed retirement on its original live ECC");
        fCommand="complete-data-handoff";fTarget=-1;fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-complete-handoff";
        fBusy=true;fPhase="Completing the original partial retirement once; old control remains fenced";
        fDeadline.start(fConfig.stepTimeoutMs);Publish();
        if(!fClient->SubmitPlanRetirementCompletion(fPlanRetirement.request,fOperationId))Fail("Completion was not confirmed submitted; observe the saved original completion ID");
    }catch(const std::exception &e){emit PlanTransitionResultSignal({},{},QString::fromUtf8(e.what()));Publish();}
}
void NativeRunControl::InspectPlanRetirementCompletion() {
    try {
        GetDAQ::EccControlRequest completion;
        if(!fConnected || fShuttingDown || !fHaveStatus || !fClient || !fStore || !fStore->PlanRetirementCompletion(&completion) ||
            fSystem.instanceId!=completion.instanceId || fClient->Busy())throw std::runtime_error("No saved completion is available for observation on the original live ECC");
        if(!fClient->Observe(completion))emit LogSignal("Original completion observation is already pending");
        Publish();
    }catch(const std::exception &e){emit PlanTransitionResultSignal({},{},QString::fromUtf8(e.what()));}
}

bool NativeRunControl::HandlePlanRetirementResult(const QString &phase,const GetDAQ::EccControlOperation &operation) {
    if(fCommand=="complete-data-handoff") {
        fStore->PlanRetirement(&fPlanRetirement.request,&fPlanRetirement.complete);fPlanRetirement.receiptAfterStatus=fPlanRetirement.complete;
        fOperationId.clear();fBusy=false;fCommand.clear();
        fPhase=phase=="complete" ? "Partial retirement completed; checking original receipt; old control remains fenced" : "Partial completion failed; preserve original results and inspect the cause";
        if(phase!="complete")emit PlanTransitionResultSignal({},QByteArray::fromStdString(GetDAQ::EncodeEccControlRequest(fPlanRetirement.request)),QString::fromStdString(operation.message));
        QueryStatusAfterOperation();Publish();return true;
    }
    if(fCommand=="retire-data-plans" && phase=="failed") {
        emit PlanTransitionResultSignal({},QByteArray::fromStdString(GetDAQ::EncodeEccControlRequest(operation.request)),QString::fromStdString(operation.message));
        Fail("Original plan retirement failed/partial; preserve node results and do not retry");QueryStatusAfterOperation();Publish();return true;
    }
    if(fCommand=="retire-data-plans" && phase=="complete") {
        fPlanRetirement.request=operation.request;fPlanRetirement.complete=true;fPlanRetirement.receiptAfterStatus=true;
        fOperationId.clear();fBusy=false;fCommand.clear();fPhase="Original plans retired; checking original receipt";
        QueryStatusAfterOperation();Publish();return true;
    }
    return false;
}
