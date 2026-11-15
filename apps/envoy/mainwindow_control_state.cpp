#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "consolepresentation.h"
#include "autorunwidget.h"
#include "configurationreadbackwindow.h"
#include "plantransitionwindow.h"
#include "eccprotocol.h"
#include <QAction>
#include <QPushButton>
#include <QStyle>

void MainWindow::ShowControlSlot(QString phase, bool connected, bool busy, bool active)
{
    if (connected && !fConnected && !fLogStore.HasConnectionLog()) StartConnectionLog();
    else if (!connected && fConnected) EndConnectionLog(tr("Connection closed"));
    if(!connected || !fConfig.nativeEcc) {
        fPlanCanSubmit=fPlanCanInspect=false;fPlanReason="Original native ECC is not connected";
        fPlanCanComplete=fPlanCanObserveCompletion=false;
        if(fPlanTransition) {fPlanTransition->SetControlState(false,false,fPlanStatus,fPlanIdentity,fPlanReason);fPlanTransition->SetCompletionState(false,false,{},{},{},{},fPlanReason);}
    }
    if ((fConfig.nativeEcc ? active : phase=="Running") && !fDisplayClock.isValid()) {fDisplayClock.start();ui->Elapsed->setText(ConsolePresentation::ElapsedText(0));}
    if ((fConfig.nativeEcc ? !active : phase!="Running") && fDisplayClock.isValid()) fDisplayClock.invalidate();
    fConnected = connected; fBusy = busy; fActive = active;
    if(!connected)fNativeReady=false;
    for(auto *controls:findChildren<AutoRunWidget *>())controls->SetLocked(active || fAutomaticArmed);
    if(auto *maintenance=findChild<QAction *>("NativeCoBoMaintenance"))
        maintenance->setEnabled(fConfig.nativeEcc && connected && !busy && !active && fCoBoCanSubmit);
    ui->Phase->setText(phase);
    const bool phaseFailed = phase.contains(QStringLiteral("error"), Qt::CaseInsensitive) ||
        phase.contains(QStringLiteral("failed"), Qt::CaseInsensitive) ||
        phase.contains(QStringLiteral("inspection"), Qt::CaseInsensitive);
    if(ui->ConnectionStatusBadge->property("connected").toBool()!=connected) {
        ui->ConnectionStatusBadge->setProperty("connected",connected);
        ui->ConnectionStatusBadge->style()->unpolish(ui->ConnectionStatusBadge);
        ui->ConnectionStatusBadge->style()->polish(ui->ConnectionStatusBadge);
    }
    const QString phaseState = phaseFailed ? QStringLiteral("error") : busy ? QStringLiteral("busy") : active ? QStringLiteral("running") : QStringLiteral("idle");
    for (QWidget *widget : QList<QWidget *>{ui->ConnectionStatusBadge, ui->Phase}) {
        if (widget->property("state").toString() == phaseState) continue;
        widget->setProperty("state", phaseState);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
    ui->Mode->setText(fConfig.nativeEcc ? "HARDWARE" : fConfig.simulation ? "SIMULATION" : "HARDWARE / LEGACY SOAP");
    ui->ResumeCatalogReplacement->setEnabled(fConfig.nativeEcc && connected && !busy && active && fCatalogRecoveryAvailable);
    const bool edit = !connected && !busy && !active;
    const bool runMetadataEditable = !active;
    ui->Experiment->setEnabled(edit);
    for (QWidget *widget : QList<QWidget *>{ui->RunNumber, ui->Description, ui->Fields, ui->AddField, ui->RemoveField})
        widget->setEnabled(runMetadataEditable);
    for (QWidget *widget : QList<QWidget *>{ui->Simulation, ui->LoadConfig, ui->SaveConfig, ui->Nodes}) widget->setEnabled(edit);
    ui->Connect->setEnabled(edit);
    const bool stoppedPending=!fConfig.nativeEcc && (phase=="Stopped; finalization pending" || phase=="Stopped; catalog inspection required");
    ui->Disconnect->setEnabled(connected && !busy && (!active || stoppedPending) && !fRemoteRunning);
    ui->Disconnect->setToolTip(stoppedPending
        ? tr("Disconnect from stopped ECC and retain the unfinished Run journal. Background services keep running.")
        : tr("Disconnect from ECC without stopping background services."));
    for (QPushButton *button : {ui->Progress, ui->Regress, ui->NodeProgress, ui->NodeRegress, ui->Recover}) button->setEnabled(connected && !busy && !active);
    ui->StartRun->setEnabled(connected && !busy && !active && (!fConfig.nativeEcc || fNativeReady));
    ui->StopRun->setEnabled(connected && !busy && (active || fAutomaticArmed));
    ui->Finalize->setEnabled(connected && !busy && active);
    if (fConfig.nativeEcc) {
        ui->Simulation->setEnabled(false);
        if(!fSingleCoBoLifecycleTarget.isEmpty()) {
            const bool selected=SelectedSingleCoBoLifecycleTarget();
            ui->NodeProgress->setEnabled(connected && !busy && !active && selected);
            ui->NodeRegress->setEnabled(connected && !busy && !active && selected);
        } else {
            ui->NodeRegress->setVisible(true);ui->NodeRegress->setEnabled(false);
            ui->NodeProgress->setEnabled(false);
        }
    }
    fReceiverAbort->setEnabled(fConfig.nativeEcc && connected && !busy && fHardwareOutcomeUnknown);
    fFailureBreakup->setEnabled(fConfig.nativeEcc && connected && !busy && fHardwareOutcomeUnknown);
    fReceiverRetirement->setEnabled(fConfig.nativeEcc && connected && !busy && fHardwareOutcomeUnknown);
    fSourceRetirement->setEnabled(fConfig.nativeEcc && connected && !busy && fHardwareOutcomeUnknown);
    fSourceRecovery->setEnabled(fConfig.nativeEcc && connected && !busy && fHardwareOutcomeUnknown);
    fAcquisitionUnprepare->setEnabled(fConfig.nativeEcc && connected && !busy && fHardwareOutcomeUnknown);
    fAcquisitionRecovery->setEnabled(fConfig.nativeEcc && connected && !busy && fHardwareOutcomeUnknown);
    fAlarmAcquisitionRecovery->setEnabled(fConfig.nativeEcc && fConfig.nativeClient.alarmStatusEnabled && connected && !busy && fHardwareOutcomeUnknown);
    fDescriptionRecovery->setEnabled(fConfig.nativeEcc && connected && !busy && !active && fHardwareOutcomeUnknown);
    fPreparationRecovery->setEnabled(fConfig.nativeEcc && connected && !busy && !active && fHardwareOutcomeUnknown);
    fAlarmPreparationRecovery->setEnabled(fConfig.nativeEcc && fConfig.nativeClient.alarmStatusEnabled && connected && !busy && !active && fHardwareOutcomeUnknown);
    fFailureUnprepare->setEnabled(fConfig.nativeEcc && connected && !busy && !active && fHardwareOutcomeUnknown);
    fProductionStop->setEnabled(fConfig.nativeEcc && connected && !busy);
    fPause->setEnabled(connected && !busy && active && (fConfig.nativeEcc || phase=="Running"));
    fResume->setEnabled(connected && !busy && active && (fConfig.nativeEcc || phase=="Paused"));
    fStoppedAlarmRelease->setEnabled(fConfig.nativeEcc && fConfig.nativeClient.alarmStatusEnabled && connected && !busy && !fHardwareOutcomeUnknown);
    if(fConfig.nativeEcc && fPlanRetired) {
        for(QPushButton *button:{ui->Progress,ui->Regress,ui->StartRun,ui->StopRun,ui->Finalize,ui->Recover,ui->NodeProgress,ui->NodeRegress})button->setEnabled(false);
        for(QAction *action:{fPause,fResume,fProductionStop,fReceiverAbort,fFailureBreakup,fReceiverRetirement,fSourceRetirement,fSourceRecovery,fAcquisitionUnprepare,
            fAcquisitionRecovery,fAlarmAcquisitionRecovery,fDescriptionRecovery,fPreparationRecovery,fAlarmPreparationRecovery,fFailureUnprepare,fStoppedAlarmRelease})action->setEnabled(false);
    }
    fConfigurationInfo->setEnabled(fConfig.nativeEcc && connected);
    fOwnershipInfo->setEnabled(fConfig.nativeEcc && connected);
    if (fOwnershipConnected!=connected) {++fOwnershipGeneration;fOwnershipConnected=connected;}
    fReadbacksInfo->setEnabled(fConfig.nativeEcc && connected);
    if(fReadbacksView)fReadbacksView->Bind(fConfig.nativeClient.snapshotSha256,fOwnershipGeneration,fConfig.nativeEcc && connected);
    if (!connected) ShowConfigurationSlot({},{});
    if (!connected) {fOwnershipBody.clear();fOwnershipError=tr("Native ECC is disconnected");fOwnershipObservedAt=-1;RenderOwnership();}
    RefreshOfflineToolsGate();
}
