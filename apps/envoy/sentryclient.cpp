#include "sentryclient.h"
#include "protocol.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QUrl>

namespace {
QString SentryFailure(const QByteArray &body,const QString &failure) {
    if(failure.isEmpty())return failure;
    const QString detail=QJsonDocument::fromJson(body).object().value("error").toString().trimmed();
    return detail.isEmpty() ? failure : failure+"; Sentry: "+detail.left(2048);
}
}

SentryClient::SentryClient(const NodeConfig &node, const RunConfig &config, QObject *parent)
    : QObject(parent), fNode(node), fConfig(config), fHttp(config.simulation ? nullptr : new HttpClient(this)),
      fStatusHttp(config.simulation ? nullptr : new HttpClient(this))
{
    fClock.start();
    fSim.disk = "Simulated disk"; fSim.process = "dataRouter (simulation)";
    fSim.dataPath = "simulation/buffer"; fSim.totalGB = 1000; fSim.availableGB = 800;
}

bool SentryClient::Send(bool catalog)
{
    bool &busy=catalog ? fBusy : fStatusBusy;
    if (busy) return false;
    busy = true;
    if (fConfig.simulation) {
        const int generation = fGeneration;
        QTimer::singleShot(40, this, [this, catalog, generation]() {
            if (generation != fGeneration) return;
            const qint64 now = fClock.elapsed();
            if (catalog) { fSim.writtenGB = 0; fSim.files = 0; }
            else if (fRunning) { fSim.writtenGB += (now - fLastTime) * (0.00002 + fNode.id * 0.000001); fSim.files = 1; }
            fLastTime = now;
            (catalog ? fBusy : fStatusBusy) = false;
            fSim.rawFileBytes=quint64(fSim.writtenGB*1e9);fSim.rawFileBytesKnown=true;
            emit ReplySignal(fNode.id, catalog, fSim, QString());
        });
    } else {
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
    }
    return true;
}

bool SentryClient::SendReplacementCatalog(const QString &operationId)
{
    if (operationId.isEmpty() || fBusy) return false;
    if (fConfig.simulation) {
        fBusy = true;
        const int generation = fGeneration;
        QTimer::singleShot(40, this, [this, generation]() {
            if (generation != fGeneration) return;
            fSim.writtenGB = 0; fSim.files = 0; fBusy = false;
            fSim.rawFileBytes=0;fSim.rawFileBytesKnown=true;
            emit ReplySignal(fNode.id, true, fSim, QString());
        });
        return true;
    }
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
    if (fConfig.simulation) {
        fBusy = true;
        const int generation = fGeneration;
        QTimer::singleShot(40, this, [this, generation]() {
            if (generation != fGeneration) return;
            fBusy = false;
            emit ReplacementFinalizedSignal(fNode.id, QString());
        });
        return true;
    }
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

void SentryClient::Cancel() { ++fGeneration; if (fHttp) fHttp->Cancel(); if(fStatusHttp)fStatusHttp->Cancel(); fBusy = fStatusBusy = false; }
