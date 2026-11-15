#include "runcontrol.h"
#include "runrecorder.h"
#include "nativeruncontrol.h"
#include "diagnostics.h"
#include <QDir>
#include <QFileInfo>
#include <QUuid>

void RunControl::ProgressSlot(int id)
{
    if(!fAutomaticDispatch)CancelAutoRun("Automatic Run canceled: manual Progress");
    if (fNative) { fNative->Progress(id); return; }
    if (!Available() || fActive) return;
    QVector<Step> plan;
    if (id < 0) {
        if (AllAt(All(), 1)) plan.append({All(), "Describe", 2});
        else if (AllAt(All(), 2)) { if (fMutant >= 0) plan.append({{fMutant}, "Prepare", 3}); plan.append({Cobos(), "Prepare", 3}); }
        else if (AllAt(All(), 3)) { plan.append({Cobos(), "Configure", 4}); if (fMutant >= 0) plan.append({{fMutant}, "Configure", 4}); }
    } else if (fEcc.contains(id) && Fresh(id) && fStatus[id].transition == 0) {
        const int state = fStatus[id].state;
        if (state == 1) plan.append({{id}, "Describe", 2});
        if (state == 2 && (fMutant < 0 || id == fMutant || AllAt({fMutant}, 3) || AllAt({fMutant}, 4))) plan.append({{id}, "Prepare", 3});
        if (state == 3 && (id != fMutant || AllAt(Cobos(), 4))) plan.append({{id}, "Configure", 4});
    }
    if (plan.isEmpty()) { emit LogSignal("Progress requires fresh compatible states and MuTaNT/CoBo ordering."); return; }
    Begin("Configure", plan);
}

void RunControl::RegressSlot(int id)
{
    CancelAutoRun("Automatic Run canceled: manual Regress");
    if (fNative) { fNative->Regress(id); return; }
    if (!Available() || fActive) return;
    const QVector<int> ids = id < 0 ? All() : QVector<int>{id};
    if (ids.isEmpty() || !fEcc.contains(ids.first())) return;
    const int state = fStatus.value(ids.first()).state;
    if (!AllAt(ids, state) || state < 2 || state > 4) { emit LogSignal("Regress requires matching Described, Prepared or Ready states."); return; }
    Begin("Regress", {{ids, state == 4 ? "Breakup" : "Undo", state - 1}});
}
void RunControl::SetOfflineToolsOpenSlot(bool open) {
    fOfflineToolsOpen=open;
    if(open)CancelAutoRun("Automatic Run canceled: offline data tool opened.");
}

void RunControl::StartRunSlot(bool replaceCompletedRun)
{
    if(fOfflineToolsOpen){ReportLog("Control rejected Run start while an offline data tool is open. Close its window first.",Diagnostics::Warning);return;}
    const auto view=AutoRunView();
    if(!fAutomaticDispatch && view.connected && !view.busy && !view.active && view.ready && view.canControl)
        fAutoRun.ManualStart(fAutoClock.elapsed(),view);
    if (fNative) {
        fNative->StartRun(replaceCompletedRun);
        if(!fNative->AutomationObservation().active && fAutoRun.stage==AutoRunPolicy::AwaitRunning)CancelAutoRun("Automatic Run canceled: Run start was rejected.");
        return;
    }
    if (!Available() || fActive) return;
    if (!AllAt(All(), 4)) { emit LogSignal("Start requires every ECC in Ready with fresh status."); return; }
    for (int id : Cobos()) {
        if (!fStatus[id].sentryOnline || fStatus[id].files != 0 || fStatus[id].availableGB <= 0) {
            emit LogSignal("Start requires healthy sentries, available disk space, and empty incoming directories."); return;
        }
    }
    QString error;
    if (!RunRecorder::Reserve(fConfig, replaceCompletedRun, &error)) { CancelAutoRun("Automatic Run canceled: Run reservation failed."); ReportLog(error + " Use another run number, or Recover the existing journal.",Diagnostics::Error); return; }
    if (!RunRecorder::ReadJournal(fConfig, &fJournal, &error)) { emit LogSignal(error); return; }
    fReplacementOperationId.clear();
    if (replaceCompletedRun) {
        fReplacementOperationId = "gui-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        fJournal["replacement_operation_id"] = fReplacementOperationId;
        fJournal["replacement_cleanup"] = QJsonObject();
        if (!Journal("Reserved for explicit completed-Run replacement")) {
            emit LogSignal("Replacement intent could not be persisted; no hardware command was sent.");
            return;
        }
    }
    fActive = true;
    fDuration = 0;
    fRunClock.start();
    QVector<Step> plan;
    if (fMutant >= 0) { plan.append({{fMutant}, "Breakup", 3}); plan.append({{fMutant}, "Configure", 4}); }
    plan.append({Cobos(), "Start", 5});
    if (fMutant >= 0) plan.append({{fMutant}, "Start", 5});
    Begin("Start", plan);
}
void RunControl::StopRunSlot()
{
    if(!fAutomaticDispatch)CancelAutoRun("Automatic Run canceled: manual Run stop");
    if (fNative) { fNative->StopRun(); return; }
    if (!Available()) return;
    bool running = false;
    for (int id : All()) if (fStatus[id].state == 5 || fStatus[id].state == 6) running = true;
    if (!fActive && !running) { emit LogSignal("No active or observed running acquisition."); return; }
    QVector<Step> plan;
    if (fMutant >= 0) plan.append({{fMutant}, "Stop", 4});
    plan.append({Cobos(), "Stop", 4});
    Begin("Stop", plan);
}

void RunControl::RecoverSlot()
{
    CancelAutoRun("Automatic Run canceled: manual journal recovery");
    if (fNative) { fNative->Recover(); return; }
    if (!Available() || fActive) return;
    QString error;
    RunConfig saved;
    if (!saved.Load(QDir(fConfig.RunDirectory()).filePath("run.yml"), &error)) { emit LogSignal("Recovery: " + error); return; }
    // 要求先打开原快照并连接，避免向另一套硬件恢复命令。
    bool matches = saved.experiment == fConfig.experiment && saved.runNumber == fConfig.runNumber &&
        saved.nodes.size() == fConfig.nodes.size() && saved.simulation == fConfig.simulation &&
        QDir::cleanPath(QFileInfo(saved.configDir).absoluteFilePath()) == QDir::cleanPath(QFileInfo(fConfig.configDir).absoluteFilePath()) &&
        QDir::cleanPath(QFileInfo(saved.outputDir).absoluteFilePath()) == QDir::cleanPath(QFileInfo(fConfig.outputDir).absoluteFilePath());
    for (int i = 0; matches && i < saved.nodes.size(); ++i) {
        const NodeConfig &a = saved.nodes[i], &b = fConfig.nodes[i];
        matches = a.id == b.id && a.mutant == b.mutant && a.host == b.host && a.eccPort == b.eccPort &&
            a.sentryPort == b.sentryPort && a.sentryHost == b.sentryHost &&
            a.routerPort == b.routerPort && a.routerHost == b.routerHost &&
            a.exporterPort == b.exporterPort && a.exporterHost == b.exporterHost &&
            a.describeId == b.describeId && a.prepareId == b.prepareId && a.configureId == b.configureId &&
            a.soapInlineXml == b.soapInlineXml;
    }
    if (!matches) { emit LogSignal("Open the run's run.yml snapshot before recovering."); return; }
    if (!RunRecorder::ReadJournal(fConfig, &fJournal, &error)) { emit LogSignal(error); return; }
    fReplacementOperationId = fJournal.value("replacement_operation_id").toString();
    const bool replacementPending = !fReplacementOperationId.isEmpty() &&
        (!fJournal.value("replacement_cleanup_complete").toBool() || fJournal.contains("replacement_backup"));
    if (fJournal.value("phase").toString() == "Complete" && !replacementPending) {
        emit LogSignal("Run already complete; choose a new run number."); return;
    }
    fConfig.description = saved.description;
    fConfig.fields = saved.fields;
    fActive = true;
    fDuration = fJournal.value("duration_seconds").toDouble();
    fLocalFinalizationFailed=fJournal.value("local_finalization_failed").toBool() ||
        fJournal.value("error").toString().contains("local finalization failed",Qt::CaseInsensitive);
    if(fLocalFinalizationFailed)fJournal["local_finalization_failed"]=true;
    fCatalogResultUncertain=false;
    const auto catalogs=fJournal.value("catalog").toObject();
    for(auto it=catalogs.begin();it!=catalogs.end();++it)
        if(it.value().toString()=="unknown" || it.value().toString()=="sent")fCatalogResultUncertain=true;
    fPhase = "Recovered: inspect states, then Stop or Finalize";
    fRunClock.invalidate();
    emit LogSignal("Recovered incomplete journal. Elapsed time may be incomplete after process interruption.");
    Publish();
}
