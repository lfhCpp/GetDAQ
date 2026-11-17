#include "sentryclient.h"
#include "protocol.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QUrl>

namespace {
QString SentryFailure(const QByteArray &body,const QString &failure) {
    if(failure.isEmpty())return failure;
    const QString detail=QJsonDocument::fromJson(body).object().value("error").toString().trimmed();
    return detail.isEmpty() ? failure : failure+"; Sentry: "+detail.left(2048);
}
}

SentryClient::SentryClient(const NodeConfig &node, const RunConfig &config, QObject *parent)
    : QObject(parent), fNode(node), fConfig(config), fHttp(new HttpClient(this)),
      fStatusHttp(new HttpClient(this))
{}

bool SentryClient::Send(bool catalog)
{
    bool &busy=catalog ? fBusy : fStatusBusy;
    if (busy || fConfig.simulation) return false;
    busy = true;
    QUrl url;
    url.setScheme("http"); url.setHost(fNode.sentryHost.isEmpty() ? fNode.host : fNode.sentryHost); url.setPort(fNode.sentryPort);
    url.setPath(catalog ? "/catalog" : "/status");
    QJsonObject params;
    params["experiment"] = fConfig.experiment;
    params["run_number"] = fConfig.runNumber;
    (catalog ? fHttp : fStatusHttp)->Request(url, catalog ? QJsonDocument(params).toJson(QJsonDocument::Compact) : QByteArray(),
        catalog ? QByteArray("application/json") : QByteArray(), catalog ? fConfig.catalogTimeoutMs : fConfig.timeoutMs,
        [this, catalog](const QByteArray &body, const QString &failure) {
            QString error = SentryFailure(body,failure);
            NodeStatus status;
            if (error.isEmpty()) Protocol::ParseSentry(body, &status, &error);
            (catalog ? fBusy : fStatusBusy) = false;
            if(catalog && error.isEmpty()){fStatusHttp->Cancel();fStatusBusy=false;}
            emit ReplySignal(fNode.id, catalog, status, error);
        });
    return true;
}

bool SentryClient::SendReplacementCatalog(const QString &operationId)
{
    if (operationId.isEmpty() || fBusy) return false;
    if (fConfig.simulation) return false;
    fBusy = true;
    QUrl url;
    url.setScheme("http"); url.setHost(fNode.sentryHost.isEmpty() ? fNode.host : fNode.sentryHost); url.setPort(fNode.sentryPort);
    url.setPath("/catalog");
    QJsonObject params;
    params["experiment"] = fConfig.experiment;
    params["run_number"] = fConfig.runNumber;
    params["replace_completed_run"] = true;
    params["operation_id"] = operationId;
    fHttp->Request(url, QJsonDocument(params).toJson(QJsonDocument::Compact), QByteArray("application/json"),
                   fConfig.catalogTimeoutMs, [this](const QByteArray &body, const QString &failure) {
        QString error = SentryFailure(body,failure);
        NodeStatus status;
        if (error.isEmpty()) Protocol::ParseSentry(body, &status, &error);
        fBusy = false;
        if(error.isEmpty()){fStatusHttp->Cancel();fStatusBusy=false;}
        emit ReplySignal(fNode.id, true, status, error);
    });
    return true;
}

bool SentryClient::FinalizeReplacement(const QString &operationId)
{
    if (operationId.isEmpty() || fBusy) return false;
    if (fConfig.simulation) return false;
    fBusy = true;
    QUrl url;
    url.setScheme("http"); url.setHost(fNode.sentryHost.isEmpty() ? fNode.host : fNode.sentryHost); url.setPort(fNode.sentryPort);
    url.setPath("/catalog/finalize");
    QJsonObject params;
    params["experiment"] = fConfig.experiment;
    params["run_number"] = fConfig.runNumber;
    params["operation_id"] = operationId;
    fHttp->Request(url, QJsonDocument(params).toJson(QJsonDocument::Compact), QByteArray("application/json"),
                   fConfig.catalogTimeoutMs, [this](const QByteArray &body, const QString &failure) {
        QString error = SentryFailure(body,failure);
        if (error.isEmpty()) {
            const QJsonDocument document = QJsonDocument::fromJson(body);
            if (!document.isObject() || !document.object().value("replacement_cleanup_complete").toBool())
                error = "Invalid Sentry replacement cleanup receipt.";
        }
        fBusy = false;
        emit ReplacementFinalizedSignal(fNode.id, error);
    });
    return true;
}

void SentryClient::Cancel() { if (fHttp) fHttp->Cancel(); if(fStatusHttp)fStatusHttp->Cancel(); fBusy = fStatusBusy = false; }
