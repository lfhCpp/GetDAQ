#include "runcontrol.h"
#include "runrecorder.h"
#include "diagnostics.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QUuid>
#include <QStandardPaths>
#include <QtConcurrent>
#ifdef Q_OS_LINUX
#include "deploymentcontroller.h"
#endif

void RunControl::ProgressSlot(int id)
{
    if(!fAutomaticDispatch)CancelAutoRun("Automatic Run canceled: manual Progress");
    
    if (!Available() || fActive) return;
#ifdef Q_OS_LINUX
    if(!fConfig.deploymentProject.isEmpty() && !All().isEmpty() && !fProjectConfigurationChecked){
        fBusy=true;fPhase="Checking project configuration before Describe";Publish();const auto config=fConfig;const int sequence=fSequence;auto *watcher=new QFutureWatcher<QPair<RunConfig,QString>>(this);
        connect(watcher,&QFutureWatcher<QPair<RunConfig,QString>>::finished,this,[this,watcher,id,sequence]{const auto result=watcher->result();watcher->deleteLater();if(sequence!=fSequence || !fConnected)return;fBusy=false;if(fShuttingDown)return;if(!result.second.isEmpty()){fPhase="Configuration sync warning";ReportLog(result.second,Diagnostics::Warning);Publish();
            if(ConfirmWarnings("Progress",{result.second+" Continue the requested ECC operation with existing project files?"},[this,id]{fProjectConfigurationChecked=true;ProgressSlot(id);fProjectConfigurationChecked=false;})){fProjectConfigurationChecked=true;ProgressSlot(id);fProjectConfigurationChecked=false;}return;}
            fCandidateBackupWarning=result.first.configurationBackupWarning;fCandidateConfigurationSnapshot=result.first.configurationSnapshot;fProjectConfigurationChecked=true;ProgressSlot(id);fProjectConfigurationChecked=false;
        });watcher->setFuture(QtConcurrent::run([config]{try{Deployment::OperationScope scope({},30000);return qMakePair(Deployment::RefreshProjectRunConfiguration(config),QString());}catch(const std::exception &error){return qMakePair(config,QString::fromUtf8(error.what()));}}));return;
    }
#endif
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

void RunControl::StartRunSlot(bool replaceCompletedRun, bool acceptExistingIncoming)
{

    const auto view=AutoRunView();
    if(!fAutomaticDispatch && view.connected && !view.busy && !view.active && view.ready && view.canControl)
        fAutoRun.ManualStart(fAutoClock.elapsed(),view);
    
    if(!fConnected || All().isEmpty()){emit LogSignal("Connect a valid ECC target first.");return;}
    if(fBusy && fStep>=0){fDeferredAction=[this,replaceCompletedRun,acceptExistingIncoming]{StartRunSlot(replaceCompletedRun,acceptExistingIncoming);};emit LogSignal("Start queued behind the in-flight hardware command.");return;}
    for(int id:Cobos())if(fStatus[id].sentryOnline && fStatus[id].totalGB>0 && fStatus[id].availableGB<=0){ReportLog("Receiver reports a full raw-data disk; correct raw recording before Start.",Diagnostics::Error);return;}
    QStringList warnings;
    if(fActive || fBusy)warnings<<"Local Run bookkeeping is unfinished; retain it and continue using observed hardware state.";
    if(!AllAt(All(),4) && !AllAt(All(),5))warnings<<"Not every ECC is freshly Ready. Continue sends Start once; ECC decides whether it is allowed.";
    if(fOfflineToolsOpen)warnings<<"An offline tool is open. Viewing remains independent; avoid changing files being acquired.";
    for(int id:Cobos()){const auto &s=fStatus[id];if(s.projectConfigurationPending)warnings<<QString("Node %1 has a newer configuration; this does not change the hardware's last configured version.").arg(id);if(!s.sentryOnline || s.availableGB<=0)warnings<<QString("Node %1 receiver monitoring or disk information is unavailable. Verify raw recording before continuing.").arg(id);}
    if(!ConfirmWarnings("Start",warnings,[this,replaceCompletedRun,acceptExistingIncoming]{StartRunSlot(replaceCompletedRun,acceptExistingIncoming);}))return;
    if(AllAt(All(),5)){fActive=true;fBusy=false;fPhase="Running";fJournal=QJsonObject{{"phase","Attached to existing Running ECC"},{"untracked",true}};fRunClock.start();Publish();return;}
    if(fActive || fBusy){Journal("Previous local work retained before operator-approved next Start");++fSequence;fActive=false;fBusy=false;fStep=-1;fAwaitCatalog.clear();fAwaitReplacementCleanup.clear();for(auto *s:fSentry)s->Cancel();}
    QJsonArray retainedIncoming;
    for (int id : Cobos()) {
        const auto &status=fStatus[id];
        if((status.projectFiles>=0?status.projectFiles:status.files)!=0) {
            retainedIncoming.append(QJsonObject{{"node_id",id},{"data_path",status.dataPath},{"files",status.projectFiles>=0?status.projectFiles:status.files},{"data_written_gb",status.writtenGB}});
        }
    }
    if (!retainedIncoming.isEmpty() && !replaceCompletedRun && !acceptExistingIncoming) {
        CancelAutoRun("Automatic Run canceled: existing incoming files require manual confirmation.");
        ReportLog("Receiver directory contains unarchived files; this does not establish that the selected Run number exists. Confirm retaining these files before starting.", Diagnostics::Warning);
        if (!fAutomaticDispatch) {
            QStringList summary;
            for (const auto &value : retainedIncoming) {
                const auto entry=value.toObject();
                summary.append(QString("Node %1: %2 file(s) in %3").arg(entry["node_id"].toInt()).arg(entry["files"].toInt()).arg(entry["data_path"].toString()));
            }
            emit IncomingFilesConfirmationSignal(replaceCompletedRun, summary.join('\n'));
        }
        return;
    }
    QString error;
    if ((fFileTask && fFileTask->isRunning()) || !RunRecorder::Reserve(fConfig,replaceCompletedRun,&error)) {
        if(QFileInfo::exists(fConfig.RunDirectory()) && !replaceCompletedRun && !(fFileTask && fFileTask->isRunning())){ReportLog(error+" Explicit same-Run replacement confirmation is required.",Diagnostics::Warning);return;}
        if(!ConfirmWarnings("Local records",{error+" Use a separate fallback records directory; raw data is not moved."},[this,replaceCompletedRun,acceptExistingIncoming]{StartRunSlot(replaceCompletedRun,acceptExistingIncoming);}))return;
        fConfig.outputDir=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/degraded-runs/"+QUuid::createUuid().toString(QUuid::WithoutBraces);emit RecordingDirectorySignal(fConfig.outputDir);replaceCompletedRun=false;
        if(!RunRecorder::Reserve(fConfig,false,&error)){fJournal=QJsonObject{{"recording_degraded",true}};ReportLog("Warning: volatile Run metadata only: "+error,Diagnostics::Warning);}
    }
    if (!RunRecorder::ReadJournal(fConfig, &fJournal, &error)) { fJournal=QJsonObject{{"recording_degraded",true}};emit LogSignal("Warning: "+error); }
    if(!retainedIncoming.isEmpty()) {
        fJournal["existing_incoming_acknowledged"]=true;
        if (replaceCompletedRun) fJournal["same_run_existing_incoming_acknowledged"]=true;
        fJournal["existing_incoming_data"]=retainedIncoming;
        if (replaceCompletedRun) {
            fJournal["replacement_backup_policy"]="retain";
            fJournal["retained_previous_run"]=fJournal.value("replacement_backup");
        }
        ReportLog("Existing incoming files explicitly accepted. Raw files are left in place and may be archived with this acquisition.",Diagnostics::Warning);
    }
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
    
    if(!fConnected || All().isEmpty()){emit LogSignal("No connected ECC targets for Stop.");return;}
    if(fBusy && fStep>=0){fDeferredAction=[this]{StopRunSlot();};emit LogSignal("Stop queued with priority behind the accepted hardware command.");return;}
    if(fBusy){++fSequence;fBusy=false;fAwaitCatalog.clear();fAwaitReplacementCleanup.clear();for(auto *s:fSentry)s->Cancel();}
    bool running = false;
    for (int id : All()) if (fStatus[id].state == 5 || fStatus[id].state == 6) running = true;
    if(!fActive && !running && !AllAt(All(),4) && !ConfirmWarnings("Stop",{"No fresh Running state. Send Stop once and let ECC report its result?"},[this]{StopRunSlot();}))return;
    if(AllAt(All(),4) && !fActive){emit LogSignal("ECC already Ready; no duplicate Stop needed.");Publish();return;}
    QVector<Step> plan;
    if (fMutant >= 0) plan.append({{fMutant}, "Stop", 4});
    plan.append({Cobos(), "Stop", 4});
    Begin("Stop", plan);
}

void RunControl::RecoverSlot()
{
    CancelAutoRun("Automatic Run canceled: manual journal recovery");
    
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
    if (fJournal.value("phase").toString() == "CompleteWithWarnings" ||
        (fJournal.value("phase").toString() == "Complete" && !replacementPending)) {
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
