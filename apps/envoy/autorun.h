#ifndef GETDAQ_AUTORUN_H
#define GETDAQ_AUTORUN_H
#include <QMap>
#include <QString>
#include <QMetaType>

struct AutoRunSettings {
    enum Condition { Time, FileSize, Either, NoCondition };
    enum Action { Stop, NextRun };
    bool enabled=false;
    Condition condition=Time;
    Action action=Stop;
    qint64 durationMs=600000;
    quint64 fileBytes=20000000000ULL;
    int maximumRuns=0; // zero = unlimited
    bool UsesTime() const {return condition==Time || condition==Either;}
    bool UsesSize() const {return condition==FileSize || condition==Either;}
    bool Validate(QString *error) const;
};
Q_DECLARE_METATYPE(AutoRunSettings)
struct AutoRunObservation {
    bool connected=false,busy=false,active=false,healthy=false,canControl=false;
    bool running=false,paused=false,ready=false,prepared=false;
    bool bytesKnown=false;
    QMap<QString,quint64> fileBytes;
};
// All calls are serialized by RunControl's control thread. No command is
// queued by this policy; an action is returned once and must be dispatched there.
class AutoRunPolicy {
public:
    enum Stage { Idle, AwaitRunning, Monitoring, Paused, Stopping, AwaitReady, Preparing };
    enum Command { None, StopCurrent, PrepareNext, StartNext };
    static const qint64 MinimumRunGapMs=3000; // starts after successful finalization
    void Configure(const AutoRunSettings &settings,qint64 now,const AutoRunObservation &view);
    void ManualStart(qint64 now,const AutoRunObservation &view);
    void Cancel(const QString &reason=QString());
    void Completed(qint64 now);
    Command Tick(qint64 now,const AutoRunObservation &view,qint64 operationTimeoutMs,qint64 observationTimeoutMs=10000);
    QString Description(qint64 now) const;
    Stage stage=Idle;
    QString reason;
    AutoRunSettings settings;
    int completedRuns=0;
    quint64 windowBytes=0;
private:
    void Window(qint64 now);
    void Baseline(const AutoRunObservation &view);
    qint64 fStarted=0,fDeadline=0,fByteObservation=0,fNextStartNotBefore=0;
    bool fBaselineKnown=false;
    QMap<QString,quint64> fBaseline,fPrevious;
};
#endif
