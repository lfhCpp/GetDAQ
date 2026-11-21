#include "runcontrol.h"
#include "diagnostics.h"

QVector<int> RunControl::Cobos() const { return fSentry.keys().toVector(); }
QVector<int> RunControl::All() const { return fEcc.keys().toVector(); }
bool RunControl::Fresh(int id) const
{
    const NodeStatus s = fStatus.value(id);
    return s.eccOnline && s.eccTime >= 0 && fClock.elapsed() - s.eccTime <= fConfig.timeoutMs + 2 * fConfig.pollMs;
}
bool RunControl::AllAt(const QVector<int> &ids, int state) const
{
    for (int id : ids) if (!Fresh(id) || fStatus.value(id).state != state || fStatus.value(id).transition != 0) return false;
    return !ids.isEmpty();
}
bool RunControl::Available()
{
    if (!fConnected || fBusy) { emit LogSignal("Controller is disconnected or busy."); return false; }
    return true;
}

void RunControl::Publish()
{
    for(auto it=fStatus.begin();it!=fStatus.end();++it)
        it->sentryCatalogPending=fAwaitCatalog.contains(it.key()) || fAwaitReplacementCleanup.contains(it.key());
    emit StatusSignal(fStatus.values().toVector());
    const QString phase=fCatalogResultUncertain && fActive && !fBusy
        ? (AllAt(All(),4) ? QString("Stopped; catalog inspection required") : QString("Catalog result unknown; inspect ECC state"))
        : fLocalFinalizationFailed && fActive && !fBusy
        ? (AllAt(All(),4) ? QString("Stopped; finalization pending") : QString("Finalization pending; inspect ECC state"))
        : fPhase;
    emit ControlSignal(phase, fConnected, fBusy, fActive);
    if(!fBusy && fDeferredAction){auto action=std::move(fDeferredAction);fDeferredAction={};const int sequence=fSequence;QTimer::singleShot(0,this,[this,action,sequence]{if(sequence==fSequence && fConnected)action();});}
}

void RunControl::PollSlot()
{
    if (!fConnected || fShuttingDown) return;
    const qint64 now = fClock.elapsed();
    for (auto it = fStatus.begin(); it != fStatus.end(); ++it) {
        if (it->eccTime >= 0 && now - it->eccTime > fConfig.timeoutMs + 2 * fConfig.pollMs) { it->eccOnline = false; it->error = "Stale ECC status"; }
        if (it->sentryTime >= 0 && now - it->sentryTime > fConfig.timeoutMs + 2 * fConfig.pollMs) { it->sentryOnline = false; it->rateMB = 0; it->sentryRateKnown=false; it->sentryError = "Stale sentry status"; }
    }
    for (auto ecc : fEcc) ecc->Send("GetState");
    for (auto sentry : fSentry) sentry->Send();
    Publish();
}

void RunControl::SentryReplySlot(int id, bool catalog, NodeStatus status, QString error)
{
    if (!fConnected || fShuttingDown || !fStatus.contains(id)) return;
    NodeStatus &s = fStatus[id];
    const qint64 now = fClock.elapsed();
    s.sentryOnline = error.isEmpty() && status.disk != "N/A";
    s.sentryError = error;
    s.rateMB = 0;
    s.sentryRateKnown=false;s.projectRateKnown=false;s.projectRateMB=0;
    if (error.isEmpty()) {
        if (!catalog && s.sentryTime >= 0 && status.dataPath == s.dataPath && now > s.sentryTime &&
            now-s.sentryTime<=fConfig.timeoutMs+2*fConfig.pollMs) {
            if(status.projectFileBytesKnown && s.projectFileBytesKnown && status.projectFileBytes>=s.projectFileBytes){s.projectRateMB=double(status.projectFileBytes-s.projectFileBytes)/(1000.0*double(now-s.sentryTime));s.projectRateKnown=true;}
            if(status.rawFileBytesKnown && s.rawFileBytesKnown && status.rawFileBytes>=s.rawFileBytes) {
                s.rateMB=double(status.rawFileBytes-s.rawFileBytes)/(1000.0*double(now-s.sentryTime));s.sentryRateKnown=true;
            } else if(!status.rawFileBytesKnown && !s.rawFileBytesKnown && status.writtenGB>=s.writtenGB) {
                s.rateMB=(status.writtenGB-s.writtenGB)*1000000.0/double(now-s.sentryTime);s.sentryRateKnown=true;
            }
        }
        s.disk = status.disk; s.process = status.process; s.dataPath = status.dataPath;
        s.writtenGB = status.writtenGB; s.files = status.files;
        s.projectConfigurationPending=status.projectConfigurationPending;s.sentrySourceSpecific=status.sentrySourceSpecific;s.projectFiles=status.projectFiles;s.projectFileBytes=status.projectFileBytes;s.projectFileBytesKnown=status.projectFileBytesKnown;
        s.rawFileBytes=status.rawFileBytes;s.rawFileBytesKnown=status.rawFileBytesKnown;
        s.availableGB = status.availableGB; s.totalGB = status.totalGB; s.sentryTime = now;
    }
    if (catalog && fAwaitCatalog.contains(id)) {
        QJsonObject results = fJournal.value("catalog").toObject();
        if (!error.isEmpty() || !s.sentryOnline || status.files != 0) {
            results[QString::number(id)] = "unknown";
            fJournal["catalog"] = results;
            fCatalogResultUncertain=true;
            auto errors=fJournal.value("catalog_errors").toObject();
            const auto detail=error.isEmpty()?QString("Sentry did not confirm an empty incoming directory (%1 files)").arg(status.files):error;
            errors[QString::number(id)]=detail;fJournal["catalog_errors"]=errors;
            ReportLog(QString("Node %1 archive warning: %2. Keeping the unknown result and raw files; no automatic retry.").arg(id).arg(detail),Diagnostics::Warning);
            if(!Journal("Cataloging with warnings")){Fail("Cannot preserve the failed archive result.");return;}
            fAwaitCatalog.remove(id);
            if(fAwaitCatalog.isEmpty()) {
                if(!AllAt(All(),4)){Fail("Archive warning recorded, but ECC stop state is not confirmed. Inspect states before closing the Run.");return;}
                StartFiles();
            }
        } else {
            results[QString::number(id)] = "complete";
            fJournal["catalog"] = results;
            if (!Journal("Cataloging")) { Fail("Cannot persist catalog acknowledgement."); return; }
            fAwaitCatalog.remove(id);
            if (fAwaitCatalog.isEmpty()) StartFiles();
        }
    }
    Publish();
}

void RunControl::ReplacementFinalizedSlot(int id, QString error)
{
    if (!fConnected || fShuttingDown || !fAwaitReplacementCleanup.contains(id)) return;
    QJsonObject results = fJournal.value("replacement_cleanup").toObject();
    if (!error.isEmpty()) {
        results[QString::number(id)] = "unknown";
        fJournal["replacement_cleanup"] = results;
        Journal("Replacement cleanup uncertain");
        Fail(QString("Node %1 has not confirmed cleanup of the old Run: %2. The new catalog remains in place; recover using the same operation ID.").arg(id).arg(error));
        return;
    }
    results[QString::number(id)] = "complete";
    fJournal["replacement_cleanup"] = results;
    if (!Journal("Replacement cleanup")) { Fail("Cannot persist Sentry replacement cleanup acknowledgement."); return; }
    fAwaitReplacementCleanup.remove(id);
    if (fAwaitReplacementCleanup.isEmpty()) StartReplacementCleanup();
}


QVector<int> RunControl::MonitorRepresentatives()const{
 QVector<int> result;QSet<QString> seen;for(const auto &node:fConfig.nodes)if(!node.mutant){const auto key=(node.sentryHost.isEmpty()?node.host:node.sentryHost).toLower()+':'+QString::number(node.sentryPort);if(!seen.contains(key)){seen.insert(key);result.append(node.id);}}return result;
}
