#include "nativeruncontrol.h"
#include "nativecobomaintenancepolicy.h"
#include "nativestatusprojection.h"
#include "nativereceiverfinalizationpolicy.h"
#include "nativeplanretirementprojection.h"
#include "runrecorder.h"
#include "scanexecutionsnapshot.h"
#include <QDateTime>
#include <QSet>
#include <QUuid>
#include <QScopedValueRollback>
#include "diagnostics.h"
#include <stdexcept>
NativeRunControl::NativeRunControl(QObject *parent) : QObject(parent), fPoll(this), fDeadline(this) {
    fDeadline.setSingleShot(true);
    connect(&fPoll,&QTimer::timeout,this,&NativeRunControl::Poll);
    connect(&fDeadline,&QTimer::timeout,this,[this] {
        HandleScanOperationDeadline();
        Fail("Native operation deadline expired; original intent remains queryable, no replay.");
    });
}
NativeRunControl::~NativeRunControl() = default;
void NativeRunControl::PlanRetirementState::Reset() {
    request=GetDAQ::EccControlRequest();receiptStatus=GetDAQ::EccControlSystemStatus();
    complete=false;receiptAfterStatus=false;
}
void NativeRunControl::Shutdown() {
    fDeferredProgressState=0;fShuttingDown=true; fPoll.stop(); fDeadline.stop();
    if (fActive) Journal("Interrupted");
    fScan.ReleaseResources();
    delete fClient; fClient=nullptr; fStore.reset(); emit ShutdownFinishedSignal();
}
bool NativeRunControl::Available() const {
    return fConnected && !fBusy && fHaveStatus && fClient && fClient->CanSubmit("start") && fStore && fStore->Pending().empty() && !fStore->NeedsReconciliation() &&
        !fSystem.operationActive && !fSystem.controlNeedsReconciliation && !fSystem.controller.outcomeUnknown && !fSystem.controller.needsReconciliation && !fSystem.dataNeedsReconciliation;
}
bool NativeRunControl::Journal(const QString &phase) {
    fJournal["phase"]=phase; fJournal["updated_utc"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QString error; if (!RunRecorder::WriteJournal(fConfig,fJournal,&error)) { emit LogSignal(error); return false; } return true;
}
bool NativeRunControl::Send(const QString &command,int target) {
    if (!Available()) { emit LogSignal("Native control requires fresh status and no unknown/pending/other-owner operation."); return false; }
    fCommand=command; fTarget=target;
    fOperationId=command=="catalog" && fScan.pointFinalizationPending && !fScan.runOperationId.isEmpty()
        ? fScan.runOperationId
        : "gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-"+command;
    fAutoOwnedOperation=fOperationId.toStdString();
    fBusy=true; fPhase="Native: "+command; fDeadline.start(fConfig.stepTimeoutMs); Publish();
    if (!fClient->Submit(command,fOperationId,command=="catalog" ? fConfig.experiment : QString(),command=="catalog" ? unsigned(fConfig.runNumber) : 0,QString(),QString(),
        command=="catalog" && fJournal.value("replace_completed_run").toBool())) { Fail("Native command was not submitted; inspect saved intent before any new action."); return false; }
    return true;
}
void NativeRunControl::Operation(const QString &id,const QString &phase,const QByteArray &body,const QString &error) {
    if (fShuttingDown || !fStore) return;
    if (!body.isEmpty()) { QString failure; if (!fStore->Observe(body.toStdString(),&failure)) { Fail(failure); return; } }
    if (id!=fOperationId) {
        if (!error.isEmpty()) emit LogSignal(error);
        if((phase=="complete" || phase=="failed") && !body.isEmpty()) {
            const auto observed=GetDAQ::DecodeEccControlOperation(body.toStdString());
            if(GetDAQ::IsNativeCoBoMaintenanceCommand(observed.request.command)) {
                emit CoBoMaintenanceResultSignal(body,phase=="complete" ? QString() : QString::fromStdString(observed.message));fClient->QueryStatus();Publish();
            }
            if(observed.request.command=="retire-data-plans" || observed.request.command=="complete-data-handoff") {
                fStore->PlanRetirement(&fPlanRetirement.request,&fPlanRetirement.complete);
                fPlanRetirement.receiptAfterStatus=fPlanRetirement.complete;fClient->RetainPlanRetirement(fPlanRetirement.request);
                fClient->QueryStatus();Publish();
            }
        }
        return;
    } // 启动后只读观察旧意图，不重建命令计划。
    if (phase=="failed") RecordScanFinalizationUnknown(
        "Native reported finalization failure; preserve the original operation for reconciliation");
    if (!error.isEmpty() || phase=="not-sent" || phase=="unknown") {
        RecordUnresolvedScanOperation(id,error);
        if(GetDAQ::IsNativeCoBoMaintenanceCommand(fCommand.toStdString()))emit CoBoMaintenanceResultSignal({},error.isEmpty() ? "Original maintenance outcome is unknown; only observe the saved operation" : error);
        if(fCommand=="retire-data-plans" || fCommand=="complete-data-handoff")emit PlanTransitionResultSignal({},{},error.isEmpty() ? "Original retirement outcome is unknown; inspect the saved operation" : error);
        Fail(error.isEmpty() ? "Native outcome unknown" : error); return;
    }
    if (phase=="accepted" || phase=="running") return;
    fDeadline.stop();
    const auto operation=GetDAQ::DecodeEccControlOperation(body.toStdString()); const auto result=GetDAQ::DecodeEccControlControllerStatus(operation.resultJson);
    // Each workflow consumes only its own result. Shared intent observation,
    // operation correlation and unresolved outcomes are handled above once.
    if (HandleCatalogReplacementResult(id,phase,operation,result) ||
        HandleScanOperationResult(id,phase,body,operation,result) ||
        HandleCoBoMaintenanceResult(phase,body,operation,result) ||
        HandlePlanRetirementResult(phase,operation) ||
        HandleRecoveryOperationResult(phase,body,operation,result)) return;
    HandleLifecycleOperationResult(phase,operation,result);
}
void NativeRunControl::Fail(const QString &message) {
    const QScopedValueRollback<int> level(fLogSeverity,Diagnostics::Error);
    emit FailureSignal(message);
    fDeadline.stop(); fDeferredProgressState=0;fRecovery.awaitingMetadata=false;fBusy=fAwaitingStatus=fDiscardStatus=false; fPhase="Native requires inspection"; if (fActive) { fJournal["error"]=message; Journal(fPhase); } emit LogSignal(message); Publish();
}
void NativeRunControl::Publish() {
    const bool pending=fStore && !fStore->Pending().empty();
    if(fRecovery.awaitingMetadata && (Available() || (fStore && fStore->NeedsReconciliation()) || (fHaveStatus &&
       (fSystem.controlNeedsReconciliation || fSystem.controller.outcomeUnknown || fSystem.controller.needsReconciliation || fSystem.dataNeedsReconciliation))))
        fRecovery.awaitingMetadata=false;
    emit ControlSignal(fPhase,fConnected,fBusy || pending || fAwaitingStatus || fRecovery.awaitingMetadata || fDeferredProgressState || (fClient && fClient->Busy()) || (fHaveStatus && fSystem.operationActive),fActive);
    GetDAQ::NativePlanRetirementProjectionInput projectionInput;
    projectionInput.request=fPlanRetirement.request;projectionInput.system=fSystem;
    projectionInput.availableForControl=Available();
    projectionInput.normalRetirementEvidence=projectionInput.availableForControl && !fActive &&
        GetDAQ::HasNormalNativeRetirementEvidence(fSystem);
    projectionInput.connected=fConnected;projectionInput.shuttingDown=fShuttingDown;
    projectionInput.haveStatus=fHaveStatus;projectionInput.retirementComplete=fPlanRetirement.complete;
    projectionInput.activeRun=fActive;projectionInput.busy=fBusy;projectionInput.clientAvailable=fClient!=nullptr;
    if(fConnected && !fShuttingDown && fHaveStatus && fPlanRetirement.complete && fClient)
        projectionInput.clientBusy=fClient->Busy();
    const auto transitionProjection=GetDAQ::ProjectNativePlanRetirement(projectionInput);
    emit PlanTransitionStateSignal(transitionProjection.canSubmit,transitionProjection.canInspect,fStatusBody,fIdentityBody,fPhase);
    emit PlanRetirementHoldSignal(transitionProjection.retirementHeld,fConfig.nativeClient.snapshotSha256);
    try {
        GetDAQ::EccControlRequest completion;std::string originalStatus,completionPhase;
        projectionInput.hasRetirementContext=fStore && fStore->PlanRetirementContext(&originalStatus);
        projectionInput.hasCompletion=fStore && fStore->PlanRetirementCompletion(&completion,&completionPhase);
        projectionInput.originalStatus=originalStatus;
        if(projectionInput.hasCompletion) {
            projectionInput.completionRequest=GetDAQ::EncodeEccControlRequest(completion);
            projectionInput.completionPhase=completionPhase;
        }
        const bool originalLive=fConnected && !fShuttingDown && fHaveStatus && fClient && !fActive &&
            fSystem.instanceId==fPlanRetirement.request.instanceId && !fSystem.operationActive;
        projectionInput.storeAvailable=fStore!=nullptr;
        if(originalLive && !fBusy && fStore) {
            projectionInput.storeCanComplete=fStore->CanCompletePlanRetirement();
            if(projectionInput.storeCanComplete)
                projectionInput.clientCanComplete=fClient->CanSubmit("complete-data-handoff");
        }
        if(originalLive && projectionInput.hasCompletion)
            projectionInput.clientBusyForObservation=fClient->Busy();
        const auto completionProjection=GetDAQ::ProjectNativePlanRetirement(projectionInput);
        emit PlanCompletionStateSignal(completionProjection.canComplete,completionProjection.canObserveCompletion,
            QByteArray::fromStdString(completionProjection.originalRequest),QByteArray::fromStdString(completionProjection.originalStatus),
            QByteArray::fromStdString(completionProjection.completionRequest),QString::fromStdString(completionProjection.completionPhase),
            QString::fromStdString(completionProjection.reason));
    } catch(const std::exception &e) {emit PlanCompletionStateSignal(false,false,{},{},{},{},QString::fromUtf8(e.what()));}
    emit CoBoMaintenanceStateSignal(Available() && !fActive && fSystem.controller.state==3 && !fSystem.controller.error,fStatusBody,fIdentityBody,fPhase);
    QString failedCatalog=fJournal.value("native_catalog_failed_operation_id").toString();
    bool canResume=false;QString recoveryReason="No retained failed replacement catalog operation";
    if(!failedCatalog.isEmpty()) {
        recoveryReason="Waiting for exact replacement recovery prerequisites";
        canResume=fConnected && !fShuttingDown && !fBusy && fActive && fHaveStatus && fIdentityMatched && fClient && fStore &&
            fStatusClock.isValid() && fStatusClock.elapsed()<=fConfig.nativeClient.statusMaxAgeMs && !fSystem.operationActive &&
            fSystem.controller.state==3 && !fSystem.controller.transition && !fSystem.controller.error &&
            !fSystem.controller.outcomeUnknown && !fSystem.controller.needsReconciliation && !fSystem.controlNeedsReconciliation &&
            fStore->CanResumeCatalogReplacement(failedCatalog.toStdString()) && fClient->CanSubmit("resume-catalog-replacement");
        if(canResume)recoveryReason="The original failed replacement is eligible for one explicit live resume";
    }
    emit CatalogRecoveryStateSignal(canResume,failedCatalog,recoveryReason);
}
