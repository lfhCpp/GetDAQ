#ifndef ECCCLIENT_H
#define ECCCLIENT_H
#include "httpclient.h"
#include "runconfig.h"
class EccClient : public QObject
{
    Q_OBJECT
public:
    EccClient(const NodeConfig &node, const RunConfig &config, QObject *parent = nullptr);
    bool Send(const QString &operation);
    void Cancel();
signals:
    void ReplySignal(int id, QString operation, int state, int transition, QString error);
private:
    NodeConfig fNode;
    RunConfig fConfig;
    HttpClient *fHttp;
    bool fBusy = false;
    int fSimState = 1;
    int fGeneration = 0;
};
#endif
