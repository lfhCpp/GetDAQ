#ifndef GETDAQ_EARLY_WARNING_RECOVERY_H
#define GETDAQ_EARLY_WARNING_RECOVERY_H
#include "runconfig.h"
#include <QObject>
#include <QJsonObject>
#include <QProcess>
#include <QElapsedTimer>
class EccSystemClient;
class HttpClient;
// Narrow cold recovery: a single CoBo failed before subscription/power-on, only
// on a readDataDelay=255 mismatch. No hardware writes or historical rewrites.
QJsonObject ValidateEarlyDelayWarningEvidence(const QString &directory,const QByteArray &status);
class EarlyWarningRecovery:public QObject {
    Q_OBJECT
public:
    EarlyWarningRecovery(const RunConfig &config,const QString &programs,QObject *parent=nullptr);
    void Start();
signals:
    void Log(QString message);
    void Verified(QJsonObject proof);
    void Failed(QString error);
private:
    RunConfig fConfig;
    QString fPrograms,fDirectory;
    EccSystemClient *fClient;
    HttpClient *fHttp;
    QProcess *fProcess;
    QJsonObject fProof,fBoard,fReads;
    int fReadIndex=0;
    bool fDone=false;
    void ReadNext();
    void Fail(const QString &message);
};
#endif
