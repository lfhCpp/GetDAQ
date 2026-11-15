#ifndef GETDAQ_ECCSERVICE_H
#define GETDAQ_ECCSERVICE_H
#include "ecccontroljobs.h"
#include "credential.h"
#include "httpserver.h"
namespace GetDAQ {
// Qt只处理控制HTTP和短状态；原生工作线程执行ECC，原始数据不进入本进程。
class EccService : public QObject {
    Q_OBJECT
public:
    explicit EccService(const std::string &configurationFile,QObject *parent=nullptr);
    EccService(const std::string &configurationFile,const std::string &successorPlanFile,
               const std::string &handoffReceiptFile,QObject *parent=nullptr);
    ~EccService() override;
    void Start();
    unsigned int Port() const { return fServer.serverPort(); }
signals:
    void ShutdownRequestedSignal();
private:
    void Handle(const QString &method,const QString &path,const QByteArray &body,HttpConnection *connection);
    void HandleGet(const QString &path,HttpConnection *connection);
    void HandlePost(const QString &path,const QByteArray &body,HttpConnection *connection);
    FixedEccConfiguration fConfiguration;
    SupervisorCredential fCredential;
    std::shared_ptr<EccSuccessorPlan> fSuccessor;
    std::string fHandoffReceipt;
    std::unique_ptr<EccControlJobs> fJobs;
    HttpServer fServer;
    unsigned int fSubmissions=0;
    unsigned int fObservations=0; // 全服务一个即时查询，不积压远端网络任务。
    unsigned int fSourceObservations=0;
    unsigned int fScanIdentityObservations=0;
    bool fShutdownRequested=false;
};
}
#endif
