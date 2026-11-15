#ifndef GETDAQ_SUPERVISORSERVICE_H
#define GETDAQ_SUPERVISORSERVICE_H
#include "httpserver.h"
#include "controljobs.h"
#include "credential.h"
#include "transferjobs.h"
namespace GetDAQ {
struct SupervisorServiceConfig {
    SupervisorControlOptions control;
    std::string bindAddress,credentialFile;
    unsigned int port=0;
    bool transfersEnabled=false;
    TransferJobOptions transfers;
    static SupervisorServiceConfig Load(const std::string &path);
    void Validate() const;
};
// Qt仅运行低频HTTP入口；后台原生核心控制独立receiver，不接原始帧。
class SupervisorService : public QObject {
    Q_OBJECT
public:
    explicit SupervisorService(const SupervisorServiceConfig &config,QObject *parent=nullptr);
    SupervisorService(const SupervisorServiceConfig &config,const std::string &predecessorProof,QObject *parent=nullptr);
    ~SupervisorService() override;
    void Start();
    unsigned int Port() const { return fServer.serverPort(); }
    std::string InstanceId() const { return fControl->InstanceId(); }
private:
    void Handle(const QString &method,const QString &path,const QByteArray &body,HttpConnection *connection);
    SupervisorServiceConfig fConfig;
    SupervisorCredential fCredential;
    std::shared_ptr<SupervisorPlanPredecessor> fPredecessor;
    std::unique_ptr<SupervisorControl> fControl;
    std::unique_ptr<TransferJobs> fTransfers;
    HttpServer fServer;
    unsigned int fSubmissions=0;
    unsigned int fMetadataQueries=0;
};
}
#endif
