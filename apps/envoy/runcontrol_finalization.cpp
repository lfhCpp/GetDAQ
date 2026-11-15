#include "runcontrol.h"
#include "nativeruncontrol.h"
#include "runrecorder.h"
#include "sentryclient.h"
#include "diagnostics.h"
#include <QFile>
#include <QJsonDocument>
#include <QDir>
#include <QFileInfo>
#include <QtConcurrent>

void RunControl::StartCatalog()
{
    if (!AllAt(All(), 4)) { Fail("Archive requires every ECC confirmed Ready."); return; }
    fPhase = "Cataloging"; fBusy = true;
    QJsonObject results = fJournal.value("catalog").toObject();
    fAwaitCatalog.clear();
    for (int id : Cobos()) {
        const QString result = results.value(QString::number(id)).toString();
        if (result == "complete") continue;
        if ((result == "sent" || result == "unknown") && fReplacementOperationId.isEmpty()) {
            Fail("Uncertain catalog result in journal. Inspect the server and reconcile journal before retrying."); return;
        }
        fAwaitCatalog.insert(id);
    }
    if (fAwaitCatalog.isEmpty()) { StartFiles(); return; }
    // 发送前持久化，崩溃恢复时不会把归档命令当成从未发送过。
    for (int id : fAwaitCatalog) results[QString::number(id)] = "sent";
    fJournal["catalog"] = results;
    if (!Journal(fPhase)) { Fail("Cannot journal catalog requests."); return; }
    fDeadline->start(fConfig.catalogTimeoutMs + 1000);
    for (int id : fAwaitCatalog) {
        fSentry[id]->Cancel();
        const bool sent = fReplacementOperationId.isEmpty()
            ? fSentry[id]->Send(true)
            : fSentry[id]->SendReplacementCatalog(fReplacementOperationId);
        if (!sent) { Fail(QString("Node %1 catalog request could not be started.").arg(id)); return; }
    }
    Publish();
}

void RunControl::StartFiles()
{
    fDeadline->stop();
    fPhase = "Backup and run log"; fBusy = true;
    if (!Journal(fPhase)) { Fail("Cannot persist backup phase."); return; }
    fFileTask->setFuture(QtConcurrent::run(RunRecorder::Finish, fConfig, fDuration));
    Publish();
}

void RunControl::FileFinishedSlot()
{
    if (fShuttingDown) { emit ShutdownFinishedSignal(); return; }
    const QString error = fFileTask->result();
    if (!error.isEmpty()) {
        fLocalFinalizationFailed=true;fJournal["local_finalization_failed"]=true;
        Fail("Acquisition was stopped; local finalization failed: " + error + ". Run records remain incomplete. Retry finalization after fixing the path or permissions, or disconnect while every ECC is confirmed Ready; the journal will be retained."); return;
    }
    fLocalFinalizationFailed=false;fJournal.remove("local_finalization_failed");
    QFile warningFile(QDir(fConfig.RunDirectory()).filePath("configuration-backup-warning.json"));
    if(warningFile.exists()) {
        if(warningFile.size()>65536 || !warningFile.open(QIODevice::ReadOnly)) {Fail("Cannot read the configuration backup warning receipt.");return;}
        const auto warning=QJsonDocument::fromJson(warningFile.readAll()).object();
        if(warning["status"].toString()!="missing" || warning["message"].toString().isEmpty()) {Fail("Invalid configuration backup warning receipt.");return;}
        fJournal["configuration_backup_status"]="missing";
        fJournal["configuration_backup_warning"]=warning["message"];
        ReportLog("Run stopped; configuration backup warning: "+warning["message"].toString()+" Run will complete without a configuration backup.",Diagnostics::Warning);
    } else {
        fJournal["configuration_backup_status"]=fConfig.simulation ? "not_required" : "complete";
        fJournal.remove("configuration_backup_warning");
    }
    if (!Journal("Complete")) { Fail("Files saved but completion journal failed."); return; }
    if (!fReplacementOperationId.isEmpty()) { StartReplacementCleanup(); return; }
    CompleteRun();
}

void RunControl::StartReplacementCleanup()
{
    if (fReplacementOperationId.isEmpty()) { CompleteRun(); return; }
    QJsonObject results = fJournal.value("replacement_cleanup").toObject();
    fAwaitReplacementCleanup.clear();
    for (int id : Cobos()) if (results.value(QString::number(id)).toString() != "complete") fAwaitReplacementCleanup.insert(id);
    if (fAwaitReplacementCleanup.isEmpty()) {
        if (fDeadline) fDeadline->stop();
        if (fJournal.value("phase").toString() != "Complete" && !Journal("Complete")) {
            Fail("All Sentry hosts confirmed old-Run cleanup, but Complete could not be journaled."); return;
        }
        QString error;
        if (!RunRecorder::FinalizeReplacement(fConfig, &error)) {
            Fail("All Sentry hosts confirmed old-Run cleanup, but local old-Run cleanup failed: " + error); return;
        }
        fJournal.remove("replacement_backup");
        fJournal["replacement_cleanup_complete"] = true;
        if (!Journal("Complete")) { Fail("Replacement cleanup completed but its Run journal receipt failed."); return; }
        fReplacementOperationId.clear();
        CompleteRun();
        return;
    }
    for (int id : fAwaitReplacementCleanup) results[QString::number(id)] = "sent";
    fJournal["replacement_cleanup"] = results;
    if (!Journal("Replacement cleanup")) { Fail("Cannot persist replacement cleanup intents."); return; }
    fBusy = true; fPhase = "Removing old Run data after all new catalogs completed";
    fDeadline->start(fConfig.catalogTimeoutMs + 1000);
    for (int id : fAwaitReplacementCleanup) {
        fSentry[id]->Cancel();
        if (!fSentry[id]->FinalizeReplacement(fReplacementOperationId)) {
            Fail(QString("Node %1 replacement cleanup request could not be started.").arg(id)); return;
        }
    }
    Publish();
}

void RunControl::CompleteRun()
{
    fActive = false; fBusy = false;
    fPhase = fJournal.value("configuration_backup_status").toString()=="missing" ? "Complete (configuration backup warning)" : "Complete";
    emit LogSignal("Run saved: " + fConfig.RunDirectory());
    const QString snapshot = QDir(fConfig.RunDirectory()).filePath("run.yml");
    const bool openedSnapshot = QFileInfo(fConfig.filePath).absoluteFilePath() == QFileInfo(snapshot).absoluteFilePath();
    ++fConfig.runNumber;
    // Sentry 的后续 Catalog 参数必须随 Run 编号更新。
    for (auto sentry : fSentry) { sentry->Cancel(); sentry->deleteLater(); }
    fSentry.clear();
    for (const NodeConfig &n : fConfig.nodes) if (!n.mutant) {
        SentryClient *sentry = new SentryClient(n, fConfig, this);
        fSentry.insert(n.id, sentry);
        connect(sentry, &SentryClient::ReplySignal, this, &RunControl::SentryReplySlot);
        connect(sentry, &SentryClient::ReplacementFinalizedSignal, this, &RunControl::ReplacementFinalizedSlot);
    }
    QString saveError;
    if (!openedSnapshot && !fConfig.filePath.isEmpty() && !fConfig.Save(fConfig.filePath, &saveError)) emit LogSignal("Run complete, but configuration autosave failed: " + saveError);
    fAutoRun.Completed(fAutoClock.elapsed());
    emit RunFinishedSignal(fConfig.runNumber);
    Publish();
}

void RunControl::FinalizeSlot()
{
    if (fNative) { fNative->Finalize(); return; }
    if (!Available() || !fActive) return;
    if (!AllAt(All(), 4)) { emit LogSignal("Stop every node before finalizing."); return; }
    StartCatalog();
}

