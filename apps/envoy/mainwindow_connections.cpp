#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "runcontrol.h"
#include "autorunwidget.h"
#include "plantransitionwindow.h"
#include "cobomaintenancewindow.h"
#include <QAction>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QThread>
#include <QTimer>

void MainWindow::InitializeRunControl()
{
    fRunControl->moveToThread(&fControlThread);
    connect(&fControlThread, &QThread::finished, fRunControl, &QObject::deleteLater);
    connect(this, &MainWindow::ConnectSignal, fRunControl, &RunControl::ConnectSlot);
    connect(this, &MainWindow::UpdateRunMetadataSignal, fRunControl, &RunControl::UpdateRunMetadataSlot);
    connect(this, &MainWindow::DisconnectSignal, fRunControl, &RunControl::DisconnectSlot);
    connect(this, &MainWindow::ProgressSignal, fRunControl, &RunControl::ProgressSlot);
    connect(this, &MainWindow::RegressSignal, fRunControl, &RunControl::RegressSlot);
    connect(this, &MainWindow::StartSignal, fRunControl, &RunControl::StartRunSlot);
    connect(this,&MainWindow::StartScanPointSignal,fRunControl,&RunControl::StartScanPointSlot);
    connect(this,&MainWindow::OfflineToolsStateSignal,fRunControl,&RunControl::SetOfflineToolsOpenSlot);
    connect(this, &MainWindow::StopSignal, fRunControl, &RunControl::StopRunSlot);
    connect(this, &MainWindow::FinalizeSignal, fRunControl, &RunControl::FinalizeSlot);
    connect(this, &MainWindow::RecoverSignal, fRunControl, &RunControl::RecoverSlot);
    connect(this, &MainWindow::ResumeCatalogReplacementSignal, fRunControl, &RunControl::ResumeCatalogReplacementSlot);
    connect(this, &MainWindow::ShutdownSignal, fRunControl, &RunControl::ShutdownSlot);
    connect(this,&MainWindow::StopProductionOnlySignal,fRunControl,&RunControl::StopProductionAfterFailureSlot);
    connect(this,&MainWindow::AbortFailedReceiversSignal,fRunControl,&RunControl::AbortFailedReceiversSlot);
    connect(this,&MainWindow::RetireFailedReceiversSignal,fRunControl,&RunControl::RetireFailedReceiversSlot);
    connect(this,&MainWindow::RetireFailedSourcesSignal,fRunControl,&RunControl::RetireFailedSourcesSlot);
    connect(this,&MainWindow::ReconcileAcquisitionWithSourcesSignal,fRunControl,&RunControl::ReconcileAcquisitionWithSourcesSlot);
    connect(this,&MainWindow::UnprepareAcquisitionAfterFailureSignal,fRunControl,&RunControl::UnprepareAcquisitionAfterFailureSlot);
    connect(this,&MainWindow::ReconcileDescriptionSignal,fRunControl,&RunControl::ReconcileDescriptionSlot);
    connect(this,&MainWindow::ReconcilePreparationSignal,fRunControl,&RunControl::ReconcilePreparationSlot);
    connect(this,&MainWindow::ReconcilePreparationWithAlarmSignal,fRunControl,&RunControl::ReconcilePreparationWithAlarmSlot);
    connect(this,&MainWindow::ReleaseStoppedAlarmSignal,fRunControl,&RunControl::ReleaseStoppedAlarmSlot);
    connect(this,&MainWindow::ReconcileAcquisitionSignal,fRunControl,&RunControl::ReconcileAcquisitionSlot);
    connect(this,&MainWindow::ReconcileAcquisitionWithAlarmSignal,fRunControl,&RunControl::ReconcileAcquisitionWithAlarmSlot);
    connect(this,&MainWindow::UnprepareAfterFailureSignal,fRunControl,&RunControl::UnprepareAfterFailureSlot);
    connect(this,&MainWindow::BreakupAfterFailureSignal,fRunControl,&RunControl::BreakupAfterFailureSlot);
    connect(this,&MainWindow::PauseSignal,fRunControl,&RunControl::PauseSlot); connect(this,&MainWindow::ResumeSignal,fRunControl,&RunControl::ResumeSlot);
    connect(this,&MainWindow::InspectConfigurationSignal,fRunControl,&RunControl::InspectConfigurationSlot);
    connect(this,&MainWindow::InspectOwnershipSignal,fRunControl,&RunControl::InspectOwnershipSlot);
    connect(this,&MainWindow::InspectConfigureReadbacksSignal,fRunControl,&RunControl::InspectConfigureReadbacksSlot);
    connect(this,&MainWindow::InspectCoBoMaintenanceTargetsSignal,fRunControl,&RunControl::InspectCoBoMaintenanceTargetsSlot);
    connect(this,&MainWindow::ReprepareCoBoSignal,fRunControl,&RunControl::ReprepareCoBoSlot);
    connect(this,&MainWindow::ConfigureCoBoSignal,fRunControl,&RunControl::ConfigureCoBoSlot);
    connect(this,&MainWindow::SetCoBoLedSignal,fRunControl,&RunControl::SetCoBoLedSlot);
    connect(this,&MainWindow::BlinkCoBoLedsSignal,fRunControl,&RunControl::BlinkCoBoLedsSlot);
    connect(this,&MainWindow::FlipCoBoLedSignal,fRunControl,&RunControl::FlipCoBoLedSlot);
    connect(this,&MainWindow::PulseCoBoLedSignal,fRunControl,&RunControl::PulseCoBoLedSlot);
    connect(this,&MainWindow::TriggerAsAdPulserSignal,fRunControl,&RunControl::TriggerAsAdPulserSlot);
    connect(fRunControl,&RunControl::CoBoMaintenanceStateSignal,this,&MainWindow::ShowCoBoMaintenanceStateSlot);
    connect(fRunControl,&RunControl::CoBoMaintenanceTargetsSignal,this,&MainWindow::ShowCoBoMaintenanceTargetsSlot);
    connect(fRunControl,&RunControl::CoBoMaintenanceResultSignal,this,[this](const QByteArray &body,const QString &error) {
        if(fCoBoMaintenance)fCoBoMaintenance->ShowResult(body,error);
    });
    connect(this,&MainWindow::RetireDataPlansSignal,fRunControl,&RunControl::RetireDataPlansSlot);
    connect(this,&MainWindow::InspectPlanRetirementReceiptSignal,fRunControl,&RunControl::InspectPlanRetirementReceiptSlot);
    connect(this,&MainWindow::CompletePlanRetirementSignal,fRunControl,&RunControl::CompletePlanRetirementSlot);
    connect(this,&MainWindow::InspectPlanRetirementCompletionSignal,fRunControl,&RunControl::InspectPlanRetirementCompletionSlot);
    connect(fRunControl,&RunControl::PlanCompletionStateSignal,this,[this](bool canComplete,bool canObserve,const QByteArray &original,const QByteArray &status,const QByteArray &completion,const QString &phase,const QString &reason) {
        if(!fConfig.nativeEcc)return;
        fPlanCanComplete=canComplete;fPlanCanObserveCompletion=canObserve;fPlanOriginalRequest=original;fPlanOriginalStatus=status;
        fPlanCompletionRequest=completion;fPlanCompletionPhase=phase;fPlanCompletionReason=reason;
        if(fPlanTransition)fPlanTransition->SetCompletionState(canComplete,canObserve,original,status,completion,phase,reason);
    });
    connect(fRunControl,&RunControl::PlanTransitionStateSignal,this,[this](bool canSubmit,bool canInspect,const QByteArray &status,const QByteArray &identity,const QString &reason) {
        if(!fConfig.nativeEcc)return;
        if(!identity.isEmpty() && identity!=fPlanIdentity){fPlanReceipt.clear();fPlanRequest.clear();}
        fPlanCanSubmit=canSubmit;fPlanCanInspect=canInspect;fPlanStatus=status;fPlanIdentity=identity;fPlanReason=reason;
        if(fPlanTransition)fPlanTransition->SetControlState(canSubmit,canInspect,status,identity,reason);
    });
    connect(fRunControl,&RunControl::PlanTransitionResultSignal,this,[this](const QByteArray &receipt,const QByteArray &request,const QString &error) {
        if(!fConfig.nativeEcc)return;
        fPlanReceipt=receipt;fPlanRequest=request;fPlanError=error;
        if(fPlanTransition)fPlanTransition->ShowResult(receipt,request,error);
    });
    connect(fRunControl,&RunControl::PlanRetirementHoldSignal,this,[this](bool held,const QString &snapshot) {
        if(!fConfig.nativeEcc || snapshot!=fConfig.nativeClient.snapshotSha256)return;
        fPlanRetired=held;ShowControlSlot(ui->Phase->text(),fConnected,fBusy,fActive);
    });
    connect(fRunControl, &RunControl::StatusSignal, this, &MainWindow::ShowStatusSlot);
    connect(fRunControl, &RunControl::ControlSignal, this, &MainWindow::ShowControlSlot);
    connect(fRunControl, &RunControl::CatalogRecoveryStateSignal, this, &MainWindow::ShowCatalogRecoveryStateSlot);
    connect(fRunControl,&RunControl::LogEntrySignal,this,&MainWindow::ShowLogEntry);
    connect(this,&MainWindow::ConfigureAutoRunSignal,fRunControl,&RunControl::ConfigureAutoRunSlot);
    connect(this,&MainWindow::SetAutoRunEnabledSignal,fRunControl,&RunControl::SetAutoRunEnabledSlot);
    connect(fRunControl,&RunControl::AutoRunEnabledSignal,this,[this](bool enabled){
        fConfig.autoRun.enabled=enabled;
        for(auto *controls:findChildren<AutoRunWidget *>())controls->SetEnabledState(enabled);
    });
    connect(this,&MainWindow::CancelAutomaticRunSignal,fRunControl,&RunControl::CancelAutomaticRunSlot);
    connect(ui->RunNumber,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{emit CancelAutomaticRunSignal(tr("Automatic Run canceled: Run number edited manually."));});
    connect(fRunControl,&RunControl::AutoRunStatusSignal,this,[this](const QString &text,bool armed){
        const bool changed=fAutomaticArmed!=armed;fAutomaticArmed=armed;
        for(auto *controls:findChildren<AutoRunWidget *>()) {controls->SetStatus(text,armed);controls->SetLocked(fActive || armed);}
        ui->AutoRunSettingsButton->setToolTip(text);
        if(changed)ShowControlSlot(ui->Phase->text(),fConnected,fBusy,fActive);
    });
    connect(fRunControl,&RunControl::ConfigurationSignal,this,&MainWindow::ShowConfigurationSlot);
    connect(fRunControl,&RunControl::OwnershipSignal,this,&MainWindow::ShowOwnershipSlot);
    connect(fRunControl,&RunControl::ConfigureReadbacksSignal,this,&MainWindow::ShowConfigureReadbacksSlot);
    connect(fRunControl, &RunControl::RunFinishedSignal, this, [this](int next) { fConfig.runNumber = next; const QSignalBlocker blocked(ui->RunNumber);ui->RunNumber->setValue(next); if(fExitAfterRun){fExitAfterRun=false;QTimer::singleShot(0,this,&MainWindow::ContinueManagedExit);} });
    connect(fRunControl, &RunControl::ShutdownFinishedSignal, &fControlThread, &QThread::quit, Qt::DirectConnection);
    connect(fRunControl, &RunControl::ShutdownFinishedSignal, this, [this]() {
        fControlThread.quit(); fControlThread.wait(); fCanClose = true; close();
    });
    fControlThread.start();
}
