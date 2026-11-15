#include "autorun.h"
#include <limits>
#include <QStringList>

bool AutoRunSettings::Validate(QString *error) const {
    if((condition!=Time && condition!=FileSize && condition!=Either && condition!=NoCondition) || (action!=Stop && action!=NextRun) ||
       (enabled && condition==NoCondition) || (UsesTime() && (durationMs<100 || durationMs>365LL*24*3600000)) || (UsesSize() && !fileBytes) || maximumRuns<0 || maximumRuns>1000000) {
        if(error)*error="Invalid automatic Run condition, threshold, action or loop count.";
        return false;
    }
    return true;
}
void AutoRunPolicy::Window(qint64 now) {
    fStarted=now;fByteObservation=now;windowBytes=0;fBaselineKnown=false;fBaseline.clear();fPrevious.clear();
}
void AutoRunPolicy::Baseline(const AutoRunObservation &view) {
    if(view.bytesKnown){fBaseline=view.fileBytes;fPrevious=view.fileBytes;fBaselineKnown=true;}
}
void AutoRunPolicy::Configure(const AutoRunSettings &value,qint64 now,const AutoRunObservation &view) {
    settings=value;Cancel();completedRuns=0;
    if(settings.enabled && view.active && view.healthy && (view.running || view.paused)) {
        Window(now);Baseline(view);stage=view.paused ? Paused : Monitoring;
    }
}
void AutoRunPolicy::ManualStart(qint64 now,const AutoRunObservation &view) {
    Cancel();completedRuns=0;
    if(settings.enabled){Window(now);Baseline(view);stage=AwaitRunning;}
}
void AutoRunPolicy::Cancel(const QString &message) {stage=Idle;reason=message;fDeadline=0;fNextStartNotBefore=0;}
void AutoRunPolicy::Completed(qint64 now) {
    if(stage!=Stopping)return;
    ++completedRuns;
    if(settings.action==AutoRunSettings::Stop || (settings.maximumRuns && completedRuns>=settings.maximumRuns)) {
        Cancel("Automatic Run complete");return;
    }
    stage=AwaitReady;fDeadline=0; // Tick establishes a bounded new-stage deadline.
    fNextStartNotBefore=now+MinimumRunGapMs;
}
AutoRunPolicy::Command AutoRunPolicy::Tick(qint64 now,const AutoRunObservation &v,qint64 timeout,qint64 observationTimeout) {
    if(stage==Idle)return None;
    if(!v.connected){Cancel("Automatic Run canceled: disconnected");return None;}
    // Transitional callbacks can temporarily have no fresh observation. Busy
    // stages get a bounded wait; stale/unknown idle control never starts a Run.
    if(stage==AwaitRunning || stage==Stopping || stage==AwaitReady || stage==Preparing) {
        if(!fDeadline)fDeadline=now+timeout+((stage==AwaitReady && now<fNextStartNotBefore) ? fNextStartNotBefore-now : 0);
        if(now>=fDeadline){Cancel("Automatic Run canceled: transition/finalization deadline exceeded");return None;}
    }
    if(stage==AwaitRunning) {
        if(!v.active && !v.busy){Cancel("Automatic Run canceled: Run start was rejected");return None;}
        if(v.active && v.running && v.healthy && !v.busy){fStarted=now;fByteObservation=now;stage=Monitoring;fDeadline=0;}
        return None;
    }
    if(stage==Stopping)return None; // only explicit successful Complete advances
    if(stage==AwaitReady || stage==Preparing) {
        if(v.busy)return None;
        if(!v.healthy || v.active){Cancel("Automatic Run canceled: next Run is not in a verified stopped state");return None;}
        if(now<fNextStartNotBefore || !v.canControl)return None;
        if(v.ready){Window(now);Baseline(v);stage=AwaitRunning;fDeadline=now+timeout;return StartNext;}
        if(stage==AwaitReady && v.prepared){stage=Preparing;fDeadline=now+timeout;return PrepareNext;}
        Cancel("Automatic Run canceled: unexpected state before next Run");return None;
    }
    if(!v.active || !v.healthy){Cancel("Automatic Run canceled: acquisition state or observation unavailable");return None;}
    if(v.paused){stage=Paused;return None;}
    if(stage==Paused && v.running){Window(now);Baseline(v);stage=Monitoring;}
    if(!v.running && !v.busy){Cancel("Automatic Run canceled: manual state change");return None;}
    const bool timeReached=settings.UsesTime() && now-fStarted>=settings.durationMs;
    if(timeReached) {
        if(!v.busy && v.canControl){stage=Stopping;fDeadline=now+timeout;return StopCurrent;}
        return None;
    }
    if(settings.UsesSize()) {
        if(!v.bytesKnown){
            if(now-fByteObservation>=observationTimeout)Cancel("Automatic Run canceled: raw-file-size observation unavailable or stale");
            return None;
        }
        fByteObservation=now;
        if(!fBaselineKnown){fBaseline=v.fileBytes;fPrevious=v.fileBytes;fBaselineKnown=true;}
        if(fBaseline.keys()!=v.fileBytes.keys()){Cancel("Automatic Run canceled: receiver session or data path changed");return None;}
        quint64 total=0;
        for(auto it=v.fileBytes.cbegin();it!=v.fileBytes.cend();++it) {
            if(it.value()<fPrevious.value(it.key())){Cancel("Automatic Run canceled: file byte counter decreased");return None;}
            const quint64 delta=it.value()-fBaseline.value(it.key());
            if(delta>std::numeric_limits<quint64>::max()-total){Cancel("Automatic Run canceled: byte total overflow");return None;}
            total+=delta;
        }
        fPrevious=v.fileBytes;windowBytes=total;
    }
    const bool reached=settings.UsesSize() && windowBytes>=settings.fileBytes;
    if(reached && !v.busy && v.canControl){stage=Stopping;fDeadline=now+timeout;return StopCurrent;}
    return None;
}
QString AutoRunPolicy::Description(qint64 now) const {
    if(!settings.enabled)return "Automatic Run off";
    if(stage==Idle)return reason.isEmpty() ? "Ready · starts with Run start" : reason;
    if(stage==Stopping)return "Stopping and finalizing current Run…";
    if(stage==AwaitReady && now<fNextStartNotBefore)return QString("Finalized · next Run in ≥ %1 s").arg(double(fNextStartNotBefore-now)/1000,0,'f',1);
    if(stage==AwaitReady || stage==Preparing)return "Waiting for next verified Ready session…";
    if(stage==AwaitRunning)return "Waiting for confirmed Running…";
    if(stage==Paused)return "Paused · counters restart on Resume";
    QStringList parts;
    if(settings.UsesTime())parts.append(QString("%1 / %2 s").arg(double(now-fStarted)/1000.0,0,'f',settings.durationMs<1000 ? 1 : 0).arg(double(settings.durationMs)/1000.0,0,'f',settings.durationMs<1000 ? 1 : 0));
    if(settings.UsesSize())parts.append(!fBaselineKnown ? QString("Waiting for file-size baseline…") : QString("%1 / %2 GB").arg(double(windowBytes)*1e-9,0,'f',3).arg(double(settings.fileBytes)*1e-9,0,'f',3));
    const QString progress=parts.join(" OR ");
    return QString("%1 · %2 Run(s) complete").arg(progress).arg(completedRuns);
}
