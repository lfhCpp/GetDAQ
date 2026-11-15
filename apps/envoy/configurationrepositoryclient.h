#ifndef GETDAQ_CONFIGURATIONREPOSITORYCLIENT_H
#define GETDAQ_CONFIGURATIONREPOSITORYCLIENT_H
#include "httpclient.h"
#include <QJsonObject>
#include <QMap>
struct ConfigurationRepositoryEndpoint {
    QString address,credentialFile,repositoryId;
    int port=24002,timeoutMs=60000;
};
class ConfigurationRepositoryClient : public QObject {
    Q_OBJECT
public:
    explicit ConfigurationRepositoryClient(const ConfigurationRepositoryEndpoint &endpoint,QObject *parent=nullptr);
    ~ConfigurationRepositoryClient() override;
    bool QueryIdentity();
    bool QueryVersions();
    bool QueryVersion(const QString &manifestSha);
    bool Upload(const QString &manifestFile);
    bool Resume(); // 先观察原版本/块；不创建另一个版本或静默重放未知提交。
    bool HasUploadPlan() const { return fHavePlan; }
    QByteArray SelectionBytes() const { return fSelectionBytes; }
    void Cancel();
signals:
    void IdentitySignal(QJsonObject identity,QString error);
    void VersionsSignal(QJsonObject versions,QString error);
    void ProgressSignal(QString message);
    void FinishedSignal(QJsonObject receipt,QString error);
private:
    struct Object {QString file;qint64 offset=0;int length=0;QByteArray bytes;};
    ConfigurationRepositoryEndpoint fEndpoint;
    HttpClient *fHttp;
    QByteArray fCredential;
    QByteArray fSelectionBytes;
    bool fCredentialFault=false,fBusy=false,fHavePlan=false;
    QString fPublicationSha;
    QJsonObject fExpected;
    QMap<QString,Object> fObjects;
    QStringList fOrder;
    QJsonObject fCommit;
    int fIndex=0;
    QMap<QByteArray,QByteArray> Headers();
    QUrl Url(const QString &path) const;
    QStringList AddBytes(const QByteArray &bytes,const QString &file=QString());
    QByteArray ObjectBytes(const QString &sha) const;
    void CheckIdentity(std::function<void()> next);
    void ObserveVersion();
    void NextObject();
    void Commit();
    void Fail(const QString &error);
    void Accept(const QByteArray &bytes);
};
#endif
