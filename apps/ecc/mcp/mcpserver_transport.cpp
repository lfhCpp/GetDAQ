#include "mcpserver_p.h"
#include <QEventLoop>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <stdexcept>

namespace GetDAQControlMcpInternal {
QByteArray Server::Http(const QByteArray &method, const QString &path, const QByteArray &body)
{
    const QString token = ReadCredential(fSettings.credentialFile);
    QNetworkRequest request(fSettings.baseUrl.resolved(QUrl(path)));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + token.toLatin1());
    request.setRawHeader("Accept", "application/json");
    if (!body.isEmpty()) request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkAccessManager manager;
    QNetworkReply *reply = method == "GET" ? manager.get(request) : manager.post(request, body);
    QEventLoop loop; QTimer timer; timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QByteArray response;
    QObject::connect(reply, &QIODevice::readyRead, &loop, [&]() {
        response += reply->readAll();
        if (response.size() > MaximumResponseBytes) reply->abort();
    });
    QObject::connect(&timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    if (!reply->isFinished()) { timer.start(fSettings.timeoutMs); loop.exec(); }
    timer.stop();
    response += reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString networkError = reply->errorString();
    const bool okay = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
    reply->deleteLater();
    Require(response.size() <= MaximumResponseBytes, "ECC response exceeds 1 MiB");
    if (!okay) {
        QString detail = networkError;
        if (status == 409) {
            QJsonParseError parse;
            const QJsonDocument errorDocument = QJsonDocument::fromJson(response, &parse);
            if (parse.error == QJsonParseError::NoError && errorDocument.isObject()) {
                const QString serverError = errorDocument.object().value("error").toString();
                if (!serverError.isEmpty() && serverError.size() <= 512)
                    detail = serverError;
            }
        }
        throw std::runtime_error(QString("ECC HTTP request failed (status %1): %2")
                                 .arg(status).arg(detail).toStdString());
    }
    return response;
}

QString Server::SubmitRequest(const QString &command,const QString &operationId,const QJsonObject &fields)
{
    QJsonParseError parse;
    const QJsonDocument identityDocument = QJsonDocument::fromJson(Http("GET", "/v1/identity"), &parse);
    Require(parse.error == QJsonParseError::NoError && identityDocument.isObject(), "ECC identity response is invalid JSON");
    const QJsonObject identity = identityDocument.object();
    const QString serviceClientId = Text(identity.value("client_id"), "identity.client_id", 128);
    Require(serviceClientId == fSettings.clientId,
            "MCP client_id does not match the fixed ECC service policy");
    const QString instance = Text(identity.value("instance_id"), "identity.instance_id", 64);
    const QString snapshot = Text(identity.value("snapshot_sha256"), "identity.snapshot_sha256", 64);
    const QRegularExpression digest("^[0-9a-f]{64}$");
    Require(digest.match(instance).hasMatch() && digest.match(snapshot).hasMatch(),
            "ECC identity digests are malformed");
    const QJsonDocument statusDocument = QJsonDocument::fromJson(Http("GET", "/v1/status"), &parse);
    Require(parse.error == QJsonParseError::NoError && statusDocument.isObject(), "ECC status response is invalid JSON");
    const QString ownerEpoch = statusDocument.object().value("client_epoch").toString();
    Require(ownerEpoch.isEmpty() || digest.match(ownerEpoch).hasMatch(), "ECC status owner epoch is malformed");
    const QString epoch = LoadOrCreateEpoch(fSettings.clientEpochFile, ownerEpoch);
    QJsonObject request;
    request.insert("schema", 1); request.insert("operation_id", operationId); request.insert("command", command);
    request.insert("instance_id", instance); request.insert("snapshot_sha256", snapshot);
    request.insert("client_id", fSettings.clientId); request.insert("client_epoch", epoch);
    for(auto i=fields.constBegin();i!=fields.constEnd();++i) request.insert(i.key(),i.value());
    return QString::fromUtf8(Http("POST", "/v1/operations", QJsonDocument(request).toJson(QJsonDocument::Compact)));
}

} // namespace GetDAQControlMcpInternal
