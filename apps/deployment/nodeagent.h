#ifndef GETDAQ_NODE_AGENT_H
#define GETDAQ_NODE_AGENT_H
#include "deploymentcommon.h"
#include "httpserver.h"
#include <QLockFile>
#include <memory>
namespace Deployment {
QJsonObject InitializeNode(const QString &root,const QString &programs,const QString &address,int port,const QString &dataDirectory);
void RenewNodePairing(const QString &configuration);
class NodeAgent:public QObject {
    Q_OBJECT
public:
    explicit NodeAgent(const QString &configuration,QObject *parent=nullptr);
    ~NodeAgent() override;
    void Start();
private:
    QJsonObject fConfig;
    QString fRoot,fPrograms,fData,fNode;
    QByteArray fCredential;
    HttpServer fServer;
    std::unique_ptr<QLockFile> fLock;
    bool fBusy=false,fFault=false;
    int fReads=0;
    void Handle(QString method,QString path,QByteArray body,HttpConnection *connection);
    QJsonObject Pair(const QJsonObject &request);
    QJsonObject Inventory() const;
    QJsonObject InstanceStatus(const QString &instance) const;
    QJsonObject Execute(const QJsonObject &request,bool *effectStarted);
    void Progress(const QJsonObject &request,const QString &stage);
    QJsonObject Prepare(const QJsonObject &request);
    QJsonObject Activate(const QJsonObject &request,bool *effectStarted);
    QJsonObject StopInstance(const QJsonObject &request,bool repair,bool *effectStarted);
    QJsonObject ReleaseTerminated(const QJsonObject &request,bool *effectStarted);
};
}
#endif
