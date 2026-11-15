#include "nativeruncontrol.h"
#include "runrecorder.h"
#include "nativereceiverfinalizationpolicy.h"
#include "scanexecutionjournalstore.h"
#include "scanexecutionjournal.h"
#include "scanoperationrunner.h"
#include <QDir>
#include <QFileInfo>

void NativeRunControl::Progress(int id) {
    if (id>=0) { emit LogSignal("Native ECC controls the whole system; per-node progress is disabled."); return; }
    if(fActive || fDeferredProgressState)return;
    if(!Available()) {
        if(fConnected && !fBusy && fHaveStatus && fClient && fClient->CanDeferProgressUntilReadCompletes() && fStore &&
           fStore->Pending().empty() && !fStore->NeedsReconciliation() && fSystem.controller.state>=1 && fSystem.controller.state<=3 &&
           !fSystem.operationActive && !fSystem.controlNeedsReconciliation && !fSystem.controller.outcomeUnknown && !fSystem.controller.needsReconciliation && !fSystem.dataNeedsReconciliation) {
            fDeferredProgressState=fSystem.controller.state;fDeferredProgressOperation=fSystem.controller.operationId;fDeferredProgressClock.start();
            emit LogSignal("Manual Progress waiting for current metadata; the command will be submitted once if its original state remains valid");Publish();
        }
        return;
    }
    const auto state=fSystem.controller.state;
    if (state==1) Send("describe",2); else if (state==2) Send("prepare",3); else if (state==3) Send("configure",4);
}
void NativeRunControl::Regress(int id) {
    if (id>=0 || fActive || !Available()) return;
    const auto state=fSystem.controller.state;
    if (state==4) Send("breakup",3); else if (state==3 || state==2) Send("reset",int(state)-1);
}
void NativeRunControl::StartRun(bool replaceCompletedRun) {
    if (fActive || !Available() || fSystem.controller.state!=4) return;
    if (fConfig.nativeClient.configurationStatusEnabled && fConfigurationBody.isEmpty()) {
        emit LogSignal("Observe the bound ECC startup configuration before recording a new Run");fClient->QueryConfiguration();Publish();return;
    }
    for (const auto &node:fSystem.nodes) for (const auto &receiver:node.receivers)
        if (node.needsReconciliation || node.state!="ready" || receiver.pid<=0 || receiver.reaped || receiver.exitCode!=-1 || receiver.sealVerified) { emit LogSignal("Native Start requires all receivers in their verified Ready sessions"); return; }
    QString error; if (!RunRecorder::Reserve(fConfig,replaceCompletedRun,&error) || !RunRecorder::ReadJournal(fConfig,&fJournal,&error)) { emit LogSignal(error); return; }
    fJournal["replace_completed_run"]=replaceCompletedRun;
    fActive=true; fDuration=0; fJournal["native_identity"]=QString::fromUtf8(fIdentityBody); fJournal["receiver_ready"]=QString::fromUtf8(fStatusBody);
    if (!fConfigurationBody.isEmpty()) fJournal["native_configuration"]=QString::fromUtf8(fConfigurationBody);
    if (!Journal("Native Run reserved before Start")) { Fail("Cannot save native Run bindings"); return; } Send("start",5);
}
void NativeRunControl::StopRun() {
    if (!Available()) return;
    if (fSystem.controller.state==5 || fSystem.controller.state==6) Send("stop",4);
}
void NativeRunControl::Pause() { if (fActive && Available() && fSystem.controller.state==5) Send("pause",6); }
void NativeRunControl::Resume() { if (fActive && Available() && fSystem.controller.state==6) Send("resume",5); }
void NativeRunControl::Finalize() {
    if (!fActive || !Available()) return;
    if (fSystem.controller.state==4) Send("breakup",3); else if (fSystem.controller.state==3) FinishRun();
}
void NativeRunControl::FinishRun() {
    if (!fHaveStatus || fSystem.controller.state!=3 || !Available()) return;
    if(fScan.pointFinalizationPending) {
        if(!fScan.runStore || fScan.runStore->Current().State()!=GetDAQ::ScanPointState::CatalogVerified ||
           fScan.runStore->Current().HasPendingOperation()) {
            Fail("Scan point archive has not been durably verified in its execution journal");return;
        }
    }
    const QString evidenceError=GetDAQ::NativeReceiverFinalizationError(fSystem,fConfig,fConfig.nativeCatalog);
    if(!evidenceError.isEmpty()) { Fail(evidenceError); return; }
    const auto recordedConfiguration=fJournal.value("native_configuration").toString().toUtf8();
    QString error,cleanupWarning;
    if(!RunRecorder::CompleteNativeRun(fConfig,fStatusBody,fIdentityBody,recordedConfiguration,
                                       fConfig.nativeCatalog,fDuration,&fJournal,&error,&cleanupWarning)) {
        Fail(error.isEmpty() ? "Native final record was not committed" : error);return;
    }
    if(!cleanupWarning.isEmpty())
        emit LogSignal("New Native Run is complete; the previous local Run record remains preserved at its replacement backup: "+cleanupWarning);
    fActive=false; fBusy=false; fPhase="Complete";
    const QString snapshot=QDir(fConfig.RunDirectory()).filePath("run.yml");
    const bool openedSnapshot=!fConfig.filePath.isEmpty() &&
        QFileInfo(fConfig.filePath).absoluteFilePath()==QFileInfo(snapshot).absoluteFilePath();
    ++fConfig.runNumber;
    if(!openedSnapshot && !fConfig.filePath.isEmpty()) {
        QString saveError;
        if(!fConfig.Save(fConfig.filePath,&saveError))
            emit LogSignal("Run complete, but configuration autosave failed: "+saveError);
    }
    emit RunFinishedSignal(fConfig.runNumber);
    if (fScan.pointFinalizationPending) {
        fScan.pointFinalizationPending=false;
        emit ScanPointRunStatusSignal("Scan point Run, receiver seal and configured catalog are complete; bind the archived Run to its scan point before proceeding",false);
        fScan.runOperationId.clear();
        fScan.operationRunner.reset();
        fScan.runStore.reset();
    }
    emit LogSignal("Native Run references saved; raw datasets remain on data hosts at recorded paths."); Publish();
}

void NativeRunControl::HandleLifecycleOperationResult(const QString &phase,
    const GetDAQ::EccControlOperation &operation,const GetDAQ::EccControlControllerStatus &result) {
    if (phase!="complete" || result.outcomeUnknown || result.needsReconciliation || (fTarget>=0 && result.state!=unsigned(fTarget))) { Fail(QString::fromStdString(operation.message)+"; native state/unknown result requires inspection"); return; }
    const auto command=fCommand; fOperationId.clear(); fBusy=false;
    if (command=="start") { fRunClock.restart(); fPhase="Running"; if (fActive && !Journal(fPhase)) { Fail("Running, but local Run result could not be saved"); return; } }
    else if (command=="pause" || command=="resume") { fPhase=command=="pause" ? "Paused" : "Running"; if (fActive && !Journal(fPhase)) { Fail("Cannot save native pause/resume phase"); return; } }
    else if (command=="stop") {
        fDuration=fRunClock.isValid() ? fRunClock.elapsed()/1000.0 : fDuration; fRunClock.invalidate(); fJournal["duration_seconds"]=fDuration;
        fPhase="Native stopped: connection retained";
        if (fActive) { if (!Journal(fPhase)) { Fail("Stopped, but local Run phase could not be saved"); return; } fCommand="finish-after-status"; }
    } else if ((command=="breakup" || command=="catalog") && fActive) fCommand="seal-after-status";
    else fPhase="Native monitoring";
    // 完成记录不是实时状态；下一步由新GET状态驱动，不使用之前的缓存。
    QueryStatusAfterOperation(); Publish();
}

void NativeRunControl::ContinueRunFinalizationFromStatus() {
    if (fCommand=="finish-after-status" && fActive && fSystem.controller.state==4 && Available()) { fCommand.clear(); Send("breakup",3); return; }
    if (fCommand=="seal-after-status" && fActive && fSystem.controller.state==3 && Available()) {
        bool catalogued=!fSystem.nodes.empty(); for (const auto &node:fSystem.nodes) catalogued=catalogued && node.catalogVerified;
        if (!ConfirmScanFinalizationFromStatus(catalogued)) return;
        fCommand.clear();
        if (fConfig.nativeCatalog && !catalogued) Send("catalog",3); else FinishRun(); return;
    }
}
