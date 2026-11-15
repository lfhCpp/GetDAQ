#ifndef GETDAQ_DEPLOYMENT_COMMON_H
#define GETDAQ_DEPLOYMENT_COMMON_H
#include <QJsonObject>
#include <QByteArray>
#include <QStringList>
#include <QMap>
#include <QElapsedTimer>
#include <atomic>
#include <memory>
namespace Deployment {
class OperationScope {
public:
    explicit OperationScope(std::shared_ptr<std::atomic_bool> canceled,int timeoutMs=180000);
    ~OperationScope();
    void Check() const;
private:
    std::shared_ptr<std::atomic_bool> fCanceled;
    QElapsedTimer fClock;
    int fTimeoutMs;
    OperationScope *fPrevious;
};
void CheckOperation();
void Require(bool value,const QString &message);
QString Id(const QString &value);
QString Sha(const QByteArray &bytes);
QString Digest(const QString &value);
QByteArray Json(const QJsonObject &object);
QJsonObject Parse(const QByteArray &bytes);
QByteArray Read(const QString &path,qint64 maximum=2*1024*1024);
QByteArray Secret(const QString &path);
void PrivateDirectory(const QString &path);
void Write(const QString &path,const QByteArray &bytes,bool immutable=false);
QByteArray Token();
QString Address(const QString &address,bool local=false);
int Port(int port,bool allowZero=false);
QByteArray Run(const QString &program,const QStringList &arguments,int timeout=30000);
void CreateCertificate(const QString &directory,const QString &name);
QString CertificateFingerprint(const QString &path);
struct Endpoint {QString url,certificateSha256;QByteArray credential;};
struct Response {int code=0;QByteArray body;};
Response Request(const Endpoint &endpoint,const QString &method,const QString &path,const QByteArray &body=QByteArray(),const QByteArray &contentType="application/json",const QMap<QByteArray,QByteArray> &headers={});
QJsonObject Get(const Endpoint &endpoint,const QString &path);
QJsonObject Post(const Endpoint &endpoint,const QString &path,const QJsonObject &body,const QMap<QByteArray,QByteArray> &headers={});
// A reviewed release permits deployment replacement, not a claim of normal hardware shutdown.
inline bool DeploymentInstanceReleased(const QJsonObject &instance) {
    if(instance["phase"]=="stopped" && instance["stopped"].toBool())return true;
    const auto receipt=instance["inspection_release"].toObject();
    return instance["phase"]=="released-after-inspection" && receipt["operator_confirmed_stopped"].toBool() &&
        receipt["data_preserved"].toBool() && receipt["no_matching_processes"].toBool() &&
        receipt["release_id"]==instance["release_id"] && receipt["instance_id"]==instance["instance_id"];
}
}
#endif
