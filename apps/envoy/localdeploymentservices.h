#ifndef GETDAQ_LOCAL_DEPLOYMENT_SERVICES_H
#define GETDAQ_LOCAL_DEPLOYMENT_SERVICES_H
#include "runconfig.h"
#include "localserviceprocesses.h"
#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QTimer>
#include <QElapsedTimer>
class EccSystemClient;
class LocalDeploymentServices:public QObject {
    Q_OBJECT
public:
    explicit LocalDeploymentServices(const RunConfig &config,const QString &programs,QObject *parent=nullptr);
    static bool IsLocalDeployment(const RunConfig &config);
    void ValidateStartupConfiguration(); // Read-only checks, including other local instances.
    void EnsureStarted();
    void StopSafely();
    void StopExplicitly(); // Repair path, with caller's explicit confirmation.
    void ForceRemaining();
    void CancelForce();
    bool Busy() const {return fBusy;}
signals:
    void Log(QString message);
    void Ready();
    void Stopped();
    void Failed(QString error);
    void ForceRequired(QString processes);
private:
    RunConfig fConfig;
    QString fPrograms,fDirectory,fPhase;
    QJsonObject fEcc,fSupervisor;
    QProcess *fCheck;
    EccSystemClient *fClient=nullptr;
    QTimer fTimeout;
    QElapsedTimer fClock;
    bool fBusy=false;
    int fIndex=0;
    QVector<LocalServiceProcess> fStopping;
    void Load();
    void CheckLocalConflicts();
    void NextService();
    void Check(const QString &name,const QStringList &arguments,const QString &phase);
    void CheckFinished(int code,QProcess::ExitStatus status);
    void VerifyIdentity();
    void VerifyEcc();
    void SignalProcesses();
    void WaitForExit(bool forced=false);
    void Fail(const QString &error);
};
#endif
