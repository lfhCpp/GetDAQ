#include "nativeruncontrol.h"
#include "nativealarmrecoverypolicy.h"
#include <QUuid>
#include <stdexcept>

void NativeRunControl::StopProductionAfterFailure() {
    if (!fClient || fBusy || !fStore || !fStore->Pending().empty()) return;
    fCommand="stop-production-after-failure"; fTarget=-1; fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-production-stop";
    fBusy=true; fPhase="Native explicit production stop"; fDeadline.start(fConfig.stepTimeoutMs);
    if (!fClient->Submit(fCommand,fOperationId)) Fail("Explicit production stop rejected; gate preserved");
    Publish();
}
void NativeRunControl::BreakupAfterFailure() {
    if(!fClient || fBusy || !fStore || !fStore->Pending().empty())return;
    if(!fHaveStatus || !fSystem.controller.outcomeUnknown || !fClient->CanSubmit("breakup-after-failure")) {
        emit LogSignal("Failure Breakup requires fresh original UNKNOWN and confirmed original production stop");return;
    }
    fCommand="breakup-after-failure";fTarget=-1;
    fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-failure-breakup";
    fBusy=true;fPhase="Closing original failed acquisition connections";fDeadline.start(fConfig.stepTimeoutMs);
    if(!fClient->Submit(fCommand,fOperationId)) Fail("Failure Breakup rejected; original gate preserved");
    Publish();
}
void NativeRunControl::UnprepareAfterFailure() {
    if(!fClient || fBusy || !fStore || !fStore->Pending().empty())return;
    if(fActive || !fHaveStatus || fSystem.controller.state!=2 || !fSystem.controller.outcomeUnknown || !fClient->CanSubmit("unprepare-after-failure")) {
        emit LogSignal("Failure unprepare requires fresh original failed preparation, stopped production proof and no Run");return;
    }
    fCommand="unprepare-after-failure";fTarget=-1;fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-failure-unprepare";
    fBusy=true;fPhase="Unpreparing original failed preparation";fDeadline.start(fConfig.stepTimeoutMs);
    if(!fClient->Submit(fCommand,fOperationId)) Fail("Failure unprepare rejected; original reconciliation gate preserved");
    Publish();
}
void NativeRunControl::ReconcileDescription() {
    if(!fClient || fBusy || !fStore || !fStore->Pending().empty())return;
    if(fActive || !fHaveStatus || fSystem.controller.state!=1 || !fSystem.controller.outcomeUnknown ||
       fSystem.controller.operationId.empty() || !fClient->CanSubmit("reconcile-description")) {
        emit LogSignal("Description reconciliation requires fresh original failed Describe and no active Run; no request sent");return;
    }
    fCommand="reconcile-description";fTarget=2;
    fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-description-reconcile";
    fBusy=true;fPhase="Verifying original failed description";fDeadline.start(fConfig.stepTimeoutMs);
    if(!fClient->Submit(fCommand,fOperationId,QString(),0,QString::fromStdString(fSystem.controller.operationId)))
        Fail("Description reconciliation rejected; original gate preserved");
    Publish();
}
void NativeRunControl::ReconcilePreparation() {
    if(!fClient || fBusy || !fStore || !fStore->Pending().empty())return;
    if(fActive || !fHaveStatus || fSystem.controller.state!=2 || !fSystem.controller.outcomeUnknown ||
       fSystem.controller.operationId.empty() || !fClient->CanSubmit("reconcile-preparation")) {
        emit LogSignal("Preparation reconciliation requires fresh original failed Prepare with confirmed power-off cleanup and no active Run; no request sent");return;
    }
    fCommand="reconcile-preparation";fTarget=2;
    fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-preparation-reconcile";
    fBusy=true;fPhase="Verifying original preparation cleanup";fDeadline.start(fConfig.stepTimeoutMs);
    if(!fClient->Submit(fCommand,fOperationId,QString(),0,QString::fromStdString(fSystem.controller.operationId)))
        Fail("Preparation reconciliation rejected; original gate preserved");
    Publish();
}
void NativeRunControl::ReconcileAcquisition() {
    if(!fClient || fBusy || !fStore || !fStore->Pending().empty())return;
    if(!fHaveStatus || !fSystem.controller.outcomeUnknown || !fClient->CanSubmit("reconcile-acquisition")) {
        emit LogSignal("Acquisition recovery requires fresh original UNKNOWN and confirmed data/connection/power-off cleanup");return;
    }
    fCommand="reconcile-acquisition";fTarget=2;
    fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-acquisition-reconcile";
    fBusy=true;fPhase="Verifying original stopped acquisition cleanup";fDeadline.start(fConfig.stepTimeoutMs);
    if(!fClient->Submit(fCommand,fOperationId,QString(),0,QString::fromStdString(fSystem.controller.operationId)))
        Fail("Acquisition recovery rejected; original hold and interrupted Run retained");
    Publish();
}
void NativeRunControl::ReconcileAcquisitionWithAlarm() {
    if(!fClient || fBusy || !fStore || !fStore->Pending().empty())return;
    if(!fHaveStatus || !fSystem.controller.outcomeUnknown || !fClient->CanSubmit("reconcile-acquisition-with-alarm")) {
        emit LogSignal("Alarm acknowledgement requires fresh original UNKNOWN and all original cleanup proofs");return;
    }
    try {
        if(fObservations.AlarmBody().isEmpty() || !fObservations.AlarmsFresh(fConfig.nativeClient.statusMaxAgeMs))
            throw std::runtime_error("Observe the current original alarm stop before acknowledging it");
        const auto &alarm=fObservations.Alarms();
        if(!GetDAQ::MatchesNativeAlarmRecovery(alarm,fSystem,GetDAQ::NativeAlarmRecoveryMode::AcknowledgeUnknownStop))
            throw std::runtime_error("Alarm acknowledgement does not match the same completed original UNKNOWN acquisition stop");
        fCommand="reconcile-acquisition-with-alarm";fTarget=2;
        fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-alarm-acquisition-reconcile";
        fBusy=true;fPhase="Acknowledging original alarm and verifying powered-off acquisition";fDeadline.start(fConfig.stepTimeoutMs);
        if(!fClient->Submit(fCommand,fOperationId,QString(),0,QString::fromStdString(fSystem.controller.operationId),QString::fromStdString(alarm.operationId)))
            Fail("Original alarm acknowledgement rejected; interrupted Run and fault holds retained");
    } catch(const std::exception &e){if(fBusy)Fail(QString::fromUtf8(e.what()));else emit LogSignal(QString::fromUtf8(e.what()));}
    Publish();
}
void NativeRunControl::ReconcilePreparationWithAlarm() {
    if(!fClient || fBusy || !fStore || !fStore->Pending().empty())return;
    if(fActive || !fHaveStatus || fSystem.controller.state!=2 || !fSystem.controller.outcomeUnknown || !fClient->CanSubmit("reconcile-preparation-with-alarm")) {
        emit LogSignal("Alarm acknowledgement requires fresh original UNKNOWN and all original cleanup proofs");return;
    }
    try {
        if(fObservations.AlarmBody().isEmpty() || !fObservations.AlarmsFresh(fConfig.nativeClient.statusMaxAgeMs))
            throw std::runtime_error("Observe the current original alarm stop before acknowledging it");
        const auto &alarm=fObservations.Alarms();
        if(!GetDAQ::MatchesNativeAlarmRecovery(alarm,fSystem,GetDAQ::NativeAlarmRecoveryMode::AcknowledgeUnknownStop))
            throw std::runtime_error("Alarm acknowledgement does not match the same completed original UNKNOWN preparation stop");
        fCommand="reconcile-preparation-with-alarm";fTarget=2;
        fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-alarm-preparation-reconcile";
        fBusy=true;fPhase="Acknowledging original alarm and verifying powered-off preparation";fDeadline.start(fConfig.stepTimeoutMs);
        if(!fClient->Submit(fCommand,fOperationId,QString(),0,QString::fromStdString(fSystem.controller.operationId),QString::fromStdString(alarm.operationId)))
            Fail("Original alarm acknowledgement rejected; preparation failure and fault holds retained");
    } catch(const std::exception &e){if(fBusy)Fail(QString::fromUtf8(e.what()));else emit LogSignal(QString::fromUtf8(e.what()));}
    Publish();
}
void NativeRunControl::ReleaseStoppedAlarm() {
    if(!fClient || fBusy || !fStore || !fStore->Pending().empty())return;
    if(!fHaveStatus || fSystem.controller.state!=4 || fSystem.controller.outcomeUnknown || fSystem.controller.needsReconciliation ||
       fSystem.dataNeedsReconciliation || !fClient->CanSubmit("release-stopped-alarm")) {
        emit LogSignal("Alarm release requires the original known stopped Ready state; UNKNOWN needs its own cleanup");return;
    }
    try {
        if(fObservations.AlarmBody().isEmpty() || !fObservations.AlarmsFresh(fConfig.nativeClient.statusMaxAgeMs))
            throw std::runtime_error("Observe the current original alarm before releasing it");
        const auto &alarm=fObservations.Alarms();
        if(!GetDAQ::MatchesNativeAlarmRecovery(alarm,fSystem,GetDAQ::NativeAlarmRecoveryMode::ReleaseKnownStoppedRun))
            throw std::runtime_error("Alarm release does not match the same original known Stop/Ready observation");
        fCommand="release-stopped-alarm";fTarget=2;
        fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-stopped-alarm-release";
        fBusy=true;fPhase="Closing original stopped alarm session and verifying power-off";fDeadline.start(fConfig.stepTimeoutMs);
        if(!fClient->Submit(fCommand,fOperationId,QString(),0,QString::fromStdString(fSystem.controller.operationId),QString::fromStdString(alarm.operationId)))
            Fail("Stopped alarm release rejected; original Run remains interrupted");
    } catch(const std::exception &e){if(fBusy)Fail(QString::fromUtf8(e.what()));else emit LogSignal(QString::fromUtf8(e.what()));}
    Publish();
}
void NativeRunControl::RetireFailedSources() {
    if(!fClient || fBusy || !fStore || !fStore->Pending().empty())return;
    if(!fHaveStatus || !fSystem.controller.outcomeUnknown || !fClient->CanSubmit("retire-failed-sources")) {
        emit LogSignal("Source retirement requires fresh original UNKNOWN and confirmed source producer stop");return;
    }
    fCommand="retire-failed-sources";fTarget=-1;fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-source-retirement";
    fBusy=true;fPhase="Retiring original stopped source connections without End";fDeadline.start(fConfig.stepTimeoutMs);
    if(!fClient->Submit(fCommand,fOperationId)) Fail("Original source retirement rejected; UNKNOWN retained");
    Publish();
}
void NativeRunControl::ReconcileAcquisitionWithSources() {
    if(!fClient || fBusy || !fStore || !fStore->Pending().empty())return;
    if(!fHaveStatus || !fSystem.controller.outcomeUnknown || !fClient->CanSubmit("reconcile-acquisition-with-sources")) {
        emit LogSignal("Source recovery requires original source retirement and complete acquisition/data/power-off proofs");return;
    }
    fCommand="reconcile-acquisition-with-sources";fTarget=2;fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-source-acquisition-reconcile";
    fBusy=true;fPhase="Verifying original retired sources and powered-off acquisition";fDeadline.start(fConfig.stepTimeoutMs);
    if(!fClient->Submit(fCommand,fOperationId,QString(),0,QString::fromStdString(fSystem.controller.operationId))) Fail("Source recovery rejected; original interrupted Run retained");
    Publish();
}
void NativeRunControl::AbortFailedReceivers() {
    if(!fClient || fBusy || !fStore || !fStore->Pending().empty()) return;
    if(!fHaveStatus || !fSystem.controller.outcomeUnknown || !fClient->CanSubmit("abort-failed-receivers")) {
        emit LogSignal("Receiver cleanup requires fresh original UNKNOWN status; no request sent");return;
    }
    fCommand="abort-failed-receivers";fTarget=-1;fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-receiver-abort";
    fBusy=true;fPhase="Native explicit failed receiver cleanup";fDeadline.start(fConfig.stepTimeoutMs);
    if(!fClient->Submit(fCommand,fOperationId)) Fail("Failed receiver cleanup rejected; original gate preserved");
    Publish();
}
void NativeRunControl::RetireFailedReceivers() {
    if(!fClient || fBusy || !fStore || !fStore->Pending().empty())return;
    if(!fHaveStatus || !fSystem.controller.outcomeUnknown || !fClient->CanSubmit("retire-failed-receivers")) {
        emit LogSignal("Data retirement requires fresh original UNKNOWN, confirmed connection cleanup and receiver Abort");return;
    }
    fCommand="retire-failed-receivers";fTarget=-1;
    fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-receiver-retirement";
    fBusy=true;fPhase="Retiring original failed data sessions";fDeadline.start(fConfig.stepTimeoutMs);
    if(!fClient->Submit(fCommand,fOperationId))Fail("Data retirement rejected; parent reconciliation remains held");
    Publish();
}
void NativeRunControl::UnprepareAcquisitionAfterFailure() {
    if(!fClient || fBusy || !fStore || !fStore->Pending().empty())return;
    if(!fHaveStatus || !fSystem.controller.outcomeUnknown || !fClient->CanSubmit("unprepare-acquisition-after-failure")) {
        emit LogSignal("Acquisition unprepare requires fresh original UNKNOWN and completed data retirement");return;
    }
    fCommand="unprepare-acquisition-after-failure";fTarget=-1;
    fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-acquisition-unprepare";
    fBusy=true;fPhase="Powering off original failed acquisition";fDeadline.start(fConfig.stepTimeoutMs);
    if(!fClient->Submit(fCommand,fOperationId)) Fail("Acquisition unprepare rejected; original gate retained");
    Publish();
}

