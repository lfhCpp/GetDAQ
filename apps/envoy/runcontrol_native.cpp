#include "runcontrol.h"
#include "nativeruncontrol.h"
#include "diagnostics.h"

void RunControl::ResumeCatalogReplacementSlot(QString failedOperationId,QString operationId)
{
    if(fNative)fNative->ResumeCatalogReplacement(failedOperationId,operationId);
}
void RunControl::StopProductionAfterFailureSlot()
{
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");
    if (fNative) fNative->StopProductionAfterFailure();
    else emit LogSignal("Explicit production-only stop is available in native ECC mode.");
}
void RunControl::PauseSlot()
{
    if (fNative) { fNative->Pause(); return; }
    if (!Available() || !fActive) return;
    if (!AllAt(All(), 5)) { emit LogSignal("Pause requires every ECC in fresh Running state."); return; }
    Begin("Pause", {{All(), "Pause", 6}});
}
void RunControl::AbortFailedReceiversSlot() {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");if(fNative) fNative->AbortFailedReceivers();}
void RunControl::RetireFailedReceiversSlot() {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");if(fNative) fNative->RetireFailedReceivers();}
void RunControl::RetireFailedSourcesSlot() {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");if(fNative) fNative->RetireFailedSources();}
void RunControl::ReconcileAcquisitionWithSourcesSlot() {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");if(fNative) fNative->ReconcileAcquisitionWithSources();}
void RunControl::UnprepareAcquisitionAfterFailureSlot() {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");if(fNative) fNative->UnprepareAcquisitionAfterFailure();}
void RunControl::ReconcileDescriptionSlot() {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");if(fNative) fNative->ReconcileDescription();}
void RunControl::ReconcilePreparationSlot() {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");if(fNative) fNative->ReconcilePreparation();}
void RunControl::ReconcilePreparationWithAlarmSlot() {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");if(fNative) fNative->ReconcilePreparationWithAlarm();}
void RunControl::ReleaseStoppedAlarmSlot() {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");if(fNative) fNative->ReleaseStoppedAlarm();}
void RunControl::ReconcileAcquisitionSlot() {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");if(fNative) fNative->ReconcileAcquisition();}
void RunControl::ReconcileAcquisitionWithAlarmSlot() {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");if(fNative) fNative->ReconcileAcquisitionWithAlarm();}
void RunControl::UnprepareAfterFailureSlot() {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");if(fNative) fNative->UnprepareAfterFailure();}
void RunControl::BreakupAfterFailureSlot() {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");if(fNative) fNative->BreakupAfterFailure();}
void RunControl::ResumeSlot()
{
    if (fNative) { fNative->Resume(); return; }
    if (!Available() || !fActive) return;
    if (!AllAt(All(), 6)) { emit LogSignal("Resume requires every ECC in fresh Paused state."); return; }
    Begin("Resume", {{All(), "Resume", 5}});
}
void RunControl::InspectConfigurationSlot() {
    if (fNative) fNative->InspectConfiguration();else emit ConfigurationSignal({},"Configuration observation requires a connected native ECC");
}
void RunControl::InspectConfigureReadbacksSlot(quint64 generation) {
    if(fNative)fNative->InspectConfigureReadbacks(generation);
    else emit ConfigureReadbacksSignal({},"Readback observation requires a connected native ECC",generation);
}
void RunControl::InspectOwnershipSlot(quint64 generation) {
    if (fNative) fNative->InspectOwnership(generation);else emit OwnershipSignal({},"Ownership observation requires a connected native ECC",generation);
}
void RunControl::QueryScanRuntimeIdentitySlot(QString journalPath,QString planDirectory,
                                              QByteArray snapshotBytes,unsigned int pointIndex) {
    if(fNative)fNative->QueryScanRuntimeIdentity(journalPath,planDirectory,snapshotBytes,pointIndex);
    else emit ScanRuntimeIdentityBoundSignal({}, {}, "Scan runtime identity requires a connected native ECC");
}
void RunControl::QueryScanEventCountBaselineSlot(QString journalPath) {
    if(fNative)fNative->QueryScanEventCountBaseline(journalPath);
    else emit ScanEventCountBaselineBoundSignal({}, {}, "Scan event baseline requires a connected native ECC");
}
void RunControl::ExecuteScanPointLifecycleOperationSlot(QString journalPath,QString command) {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");
    if(fNative)fNative->ExecuteScanPointLifecycleOperation(journalPath,command);
    else emit ScanPointRunStatusSignal("Scan lifecycle action requires a connected Native ECC",false);
}
void RunControl::ObservePendingScanLifecycleOperationSlot(QString journalPath,QString operationId,
                                                           QString expectedPlanSha256,
                                                           QByteArray snapshotBytes) {
    if(fNative)fNative->ObservePendingScanLifecycleOperation(journalPath,operationId,
                                                             expectedPlanSha256,snapshotBytes);
    else emit ScanPointRunStatusSignal("Saved scan operation observation requires a connected Native ECC",false);
}
void RunControl::StartScanPointSlot(QString journalPath) {
    if(fOfflineToolsOpen){
        const QString error="Control rejected scan Run start while an offline data tool is open. Close its window first.";
        ReportLog(error,Diagnostics::Warning);emit ScanPointRunStatusSignal(error,false);return;
    }
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");
    if(fNative)fNative->StartScanPoint(journalPath);
    else emit LogSignal("Scan point Run requires a connected Native ECC");
}
void RunControl::RetireDataPlansSlot(QByteArray successorBytes,QByteArray targetsBytes) {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");
    if(fNative)fNative->RetireDataPlans(successorBytes,targetsBytes);
    else emit PlanTransitionResultSignal({},{},"Plan retirement requires a connected native ECC");
}
void RunControl::CompletePlanRetirementSlot(QString originalOperationId) {
    if(fNative)fNative->CompletePlanRetirement(originalOperationId);
    else emit PlanTransitionResultSignal({},{},"Completion requires the original live native ECC");
}
void RunControl::InspectPlanRetirementCompletionSlot() {
    if(fNative)fNative->InspectPlanRetirementCompletion();
    else emit PlanTransitionResultSignal({},{},"Completion observation requires the original live native ECC");
}
void RunControl::InspectPlanRetirementReceiptSlot() {
    if(fNative)fNative->InspectPlanRetirementReceipt();
    else emit PlanTransitionResultSignal({},{},"Receipt observation requires the original native ECC");
}

void RunControl::InspectCoBoMaintenanceTargetsSlot() {
    if(fNative)fNative->InspectCoBoMaintenanceTargets();
    else emit CoBoMaintenanceTargetsSignal({},"Selected CoBo maintenance requires a connected native ECC");
}
void RunControl::ReprepareCoBoSlot(QString node,QString instance,QString snapshot) {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");
    if(fNative)fNative->ReprepareCoBo(node,instance,snapshot);
    else emit CoBoMaintenanceResultSignal({},"Selected CoBo maintenance requires a connected native ECC");
}
void RunControl::ConfigureCoBoSlot(QString node,QString instance,QString snapshot) {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");
    if(fNative)fNative->ConfigureCoBo(node,instance,snapshot);
    else emit CoBoMaintenanceResultSignal({},"Selected CoBo Configure requires a connected native ECC");
}
void RunControl::SetCoBoLedSlot(QString node,unsigned int asad,unsigned int type,unsigned int state,QString instance,QString snapshot) {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");
    if(fNative)fNative->SetCoBoLed(node,asad,type,state,instance,snapshot);
    else emit CoBoMaintenanceResultSignal({},"Selected CoBo LED control requires a connected native ECC");
}
void RunControl::BlinkCoBoLedsSlot(QString node,QString instance,QString snapshot) {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");
    if(fNative)fNative->BlinkCoBoLeds(node,instance,snapshot);
    else emit CoBoMaintenanceResultSignal({},"Selected CoBo LED control requires a connected native ECC");
}
void RunControl::FlipCoBoLedSlot(QString node,unsigned int ledId,QString instance,QString snapshot) {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");
    if(fNative)fNative->FlipCoBoLed(node,ledId,instance,snapshot);
    else emit CoBoMaintenanceResultSignal({},"Selected CoBo LED control requires a connected native ECC");
}
void RunControl::PulseCoBoLedSlot(QString node,unsigned int ledId,unsigned int periodMs,QString instance,QString snapshot) {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");
    if(fNative)fNative->PulseCoBoLed(node,ledId,periodMs,instance,snapshot);
    else emit CoBoMaintenanceResultSignal({},"Selected CoBo LED pulse requires a connected native ECC");
}
void RunControl::TriggerAsAdPulserSlot(QString node,int voltage,QString instance,QString snapshot) {
    CancelAutoRun("Automatic Run canceled: manual maintenance/recovery operation");
    if(fNative)fNative->TriggerAsAdPulser(node,voltage,instance,snapshot);
    else emit CoBoMaintenanceResultSignal({},"Selected CoBo pulser control requires a connected native ECC");
}
