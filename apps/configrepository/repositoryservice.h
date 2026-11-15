#ifndef GETDAQ_REPOSITORYSERVICE_H
#define GETDAQ_REPOSITORYSERVICE_H
#include "configurationrepository.h"
#include "repositorycredential.h"
#include "httpserver.h"
class RepositoryService : public QObject {
    Q_OBJECT
public:
    RepositoryService(const std::string &root,const std::string &credential,QObject *parent=nullptr,const std::string &readCredential=std::string());
    void SetTls(const QString &certificate,const QString &key){fServer.SetTls(certificate,key);}
    void Start(const QString &address,quint16 port);
    quint16 Port() const {return fServer.serverPort();}
    ~RepositoryService() override;
private:
    void Handle(const QString &method,const QString &path,const QByteArray &body,HttpConnection *connection);
    GetDAQ::ConfigurationRepository fRepository;
    GetDAQ::RepositoryCredential fCredential;
    std::unique_ptr<GetDAQ::RepositoryCredential> fReadCredential;
    HttpServer fServer;
    std::string fRepositoryId;
    bool fBusy=false;
};
#endif
