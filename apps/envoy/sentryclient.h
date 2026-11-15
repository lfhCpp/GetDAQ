#ifndef SENTRYCLIENT_H
#define SENTRYCLIENT_H
#include "httpclient.h"
#include "runconfig.h"
#include <QElapsedTimer>
class SentryClient : public QObject
{
    Q_OBJECT
public:
    SentryClient(const NodeConfig &node, const RunConfig &config, QObject *parent = nullptr);
    bool Send(bool catalog = false);
    bool SendReplacementCatalog(const QString &operationId);
    bool FinalizeReplacement(const QString &operationId);
    void Cancel();
    void SetRunning(bool running) { fRunning = running; }
    void SetRunMetadata(const RunConfig &config) {
        fConfig.experiment = config.experiment;
        fConfig.runNumber = config.runNumber;
        fConfig.description = config.description;
        fConfig.fields = config.fields;
    }
signals:
    void ReplySignal(int id, bool catalog, NodeStatus status, QString error);
    void ReplacementFinalizedSignal(int id, QString error);
private:
    NodeConfig fNode;
    RunConfig fConfig;
    HttpClient *fHttp;
    HttpClient *fStatusHttp;
    bool fStatusBusy = false;
    bool fBusy = false;
    bool fRunning = false;
    int fGeneration = 0;
    NodeStatus fSim;
    QElapsedTimer fClock;
    qint64 fLastTime = 0;
};
#endif
