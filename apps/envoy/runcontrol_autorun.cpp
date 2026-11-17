#include "runcontrol.h"
#include "diagnostics.h"
#include <QFileInfo>
#include <QScopedValueRollback>
#include <limits>

void RunControl::ReportLog(const QString &message,int severity) {
    const QScopedValueRollback<int> level(fLogSeverity,severity);emit LogSignal(message);
}
void RunControl::CancelAutoRun(const QString &reason) {
    const bool armed=fAutoRun.stage!=AutoRunPolicy::Idle;
    fAutoRun.Cancel(reason);
    if(armed && !reason.isEmpty())ReportLog(reason.startsWith("Automatic Run") ? reason : "Automatic Run canceled: "+reason,Diagnostics::Warning);
    emit AutoRunStatusSignal(fAutoRun.Description(fAutoClock.elapsed()),false);
}
void RunControl::CancelAutomaticRunSlot(QString reason) {CancelAutoRun(reason);}
void RunControl::ConfigureAutoRunSlot(AutoRunSettings settings) {
    if(AutoRunView().active || fAutoRun.stage!=AutoRunPolicy::Idle) {
        ReportLog("Automatic Run settings are locked until the Run or automatic sequence finishes. Use Run stop to cancel.",Diagnostics::Warning);return;
    }
    QString error;if(!settings.Validate(&error)){ReportLog(error,Diagnostics::Error);return;}

    fConfig.autoRun=settings;
    emit AutoRunEnabledSignal(settings.enabled);
    
    fAutoRun.Configure(settings,fAutoClock.elapsed(),AutoRunView());
    if(!fAutoTimer->isActive())fAutoTimer->start();
    ReportLog(settings.enabled ? "Automatic Run settings applied. Counters start with the next confirmed Run start; conditions stay locked until the sequence ends." : "Automatic Run disabled; pending automatic continuation canceled.",Diagnostics::Information);
    emit AutoRunStatusSignal(fAutoRun.Description(fAutoClock.elapsed()),fAutoRun.stage!=AutoRunPolicy::Idle);
}
AutoRunObservation RunControl::AutoRunView() const {
    
    AutoRunObservation v;v.connected=fConnected;v.busy=fBusy;v.active=fActive;
    v.canControl=fConnected && !fBusy && !fShuttingDown;
    v.running=AllAt(All(),5);v.paused=AllAt(All(),6);v.ready=AllAt(All(),4);v.prepared=AllAt(All(),3);
    v.healthy=fConnected && !fShuttingDown && !fStatus.isEmpty();v.bytesKnown=true;
    const qint64 now=fClock.isValid() ? fClock.elapsed() : 0;
    for(const auto &n:fConfig.nodes) {
        const auto it=fStatus.constFind(n.id);
        if(it==fStatus.cend() || !Fresh(n.id) || !it->error.isEmpty()){v.healthy=false;v.bytesKnown=false;continue;}
        if(n.mutant)continue;
        if(!it->sentryOnline || now-it->sentryTime>fConfig.timeoutMs+2*fConfig.pollMs || !it->sentryError.isEmpty()) {v.healthy=false;v.bytesKnown=false;continue;}
        if(!it->rawFileBytesKnown || it->dataPath.isEmpty()){v.bytesKnown=false;continue;}
        const QString key=(n.sentryHost.isEmpty() ? n.host : n.sentryHost).toLower()+":"+QString::number(n.sentryPort)+":"+it->dataPath;
        if(v.fileBytes.contains(key) && v.fileBytes.value(key)!=it->rawFileBytes){v.bytesKnown=false;continue;}
        v.fileBytes.insert(key,it->rawFileBytes);
    }
    v.bytesKnown=v.bytesKnown && !v.fileBytes.isEmpty();
    return v;
}
void RunControl::AutomaticTick() {
    if(fShuttingDown)return;
    const auto before=fAutoRun.stage;
    const auto view=AutoRunView();
    const qint64 timeout=qint64(fConfig.stepTimeoutMs)*3+fConfig.catalogTimeoutMs+fConfig.settleMs+fConfig.timeoutMs;
    const auto command=fAutoRun.Tick(fAutoClock.elapsed(),view,timeout,fConfig.timeoutMs+2*fConfig.pollMs);
    if(before!=AutoRunPolicy::Idle && fAutoRun.stage==AutoRunPolicy::Idle && !fAutoRun.reason.isEmpty())
        ReportLog(fAutoRun.reason,Diagnostics::Error);
    const QScopedValueRollback<bool> dispatch(fAutomaticDispatch,true);
    if(command==AutoRunPolicy::StopCurrent) {
        ReportLog("Automatic Run threshold reached; stopping and waiting for complete finalization.",Diagnostics::Information);
        StopRunSlot();
    } else if(command==AutoRunPolicy::PrepareNext || command==AutoRunPolicy::StartNext) {
        // An automatic continuation never grants permission to replace an old Run.
        const QFileInfo target(fConfig.RunDirectory());
        if(target.exists() || target.isSymLink()) {
            CancelAutoRun("Automatic Run canceled: next Run number already exists; choose an unused number.");
        } else if(command==AutoRunPolicy::PrepareNext) {
            ReportLog("Automatic Run: preparing the next verified receiver session.",Diagnostics::Information);ProgressSlot();
        } else {
            ReportLog(QString("Automatic Run: starting Run %1.").arg(fConfig.runNumber),Diagnostics::Information);
            StartRunSlot(false);
            if(!AutoRunView().active)CancelAutoRun("Automatic Run canceled: next Run start was rejected.");
        }
    }
    emit AutoRunStatusSignal(fAutoRun.Description(fAutoClock.elapsed()),fAutoRun.stage!=AutoRunPolicy::Idle);
}

void RunControl::SetAutoRunEnabledSlot(bool enabled) {
    auto settings=fAutoRun.settings;settings.enabled=enabled;
    QString error;const auto view=AutoRunView();
    if(!settings.Validate(&error)) {
        ReportLog(error,Diagnostics::Error);
        emit AutoRunEnabledSignal(fAutoRun.settings.enabled);return;
    }
    if(enabled && view.active && (!view.healthy || (!view.running && !view.paused))) {
        ReportLog("Enable automatic Run after confirmed Running or Paused observations are available.",Diagnostics::Warning);
        emit AutoRunEnabledSignal(fAutoRun.settings.enabled);return;
    }
    if(settings.enabled==fAutoRun.settings.enabled){emit AutoRunEnabledSignal(enabled);return;}
    fConfig.autoRun=settings;
    
    if(enabled) {
        fAutoRun.Configure(settings,fAutoClock.elapsed(),view);
        ReportLog(view.active ? "Automatic Run enabled with the saved conditions; counting window restarted. Current acquisition continues." : "Automatic Run enabled with the saved conditions; starts with the next Run start.",Diagnostics::Information);
    } else {
        fAutoRun.settings=settings;CancelAutoRun("Automatic Run disabled; current acquisition or finalization continues, with no automatic continuation.");
    }
    if(!fAutoTimer->isActive())fAutoTimer->start();
    emit AutoRunEnabledSignal(enabled);
    emit AutoRunStatusSignal(fAutoRun.Description(fAutoClock.elapsed()),fAutoRun.stage!=AutoRunPolicy::Idle);
}
