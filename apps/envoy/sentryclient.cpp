#include "sentryclient.h"
#include "protocol.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QUrl>
#include <QJsonArray>

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
            if (error.isEmpty()) ParseObservation(body,catalog,&status,&error);
            (catalog ? fBusy : fStatusBusy) = false;
            if(catalog && error.isEmpty()){fStatusHttp->Cancel();fStatusBusy=false;}
            emit ReplySignal(fNode.id, catalog, status, error);
        });
    return true;
}

bool SentryClient::SendReplacementCatalog(const QString &operationId, const QString &filePolicy)
{
    if (operationId.isEmpty() || fBusy) return false;
    if (fConfig.simulation) return false;
    // Keep the original replacement request for legacy Sentry. New policy
    // fields are sent only to receivers that explicitly support them.
    const bool supported=!filePolicy.isEmpty() && SupportsFilePolicy(filePolicy);
    fBusy=true;
    QUrl url;
    url.setScheme("http"); url.setHost(fNode.sentryHost.isEmpty() ? fNode.host : fNode.sentryHost); url.setPort(fNode.sentryPort);
    url.setPath("/catalog");
    QJsonObject params;
    params["experiment"] = fConfig.experiment;
    params["run_number"] = fConfig.runNumber;
    params["replace_completed_run"] = true;
    params["operation_id"] = operationId;
    if(supported && filePolicy=="overwrite")params["overwrite_existing_run"]=true;
    if(supported && filePolicy=="append")params["append_existing_run"]=true;
    fHttp->Request(url, QJsonDocument(params).toJson(QJsonDocument::Compact), QByteArray("application/json"),
                   fConfig.catalogTimeoutMs, [this,filePolicy,supported](const QByteArray &body, const QString &failure) {
        QString error = SentryFailure(body,failure);
        if(error.isEmpty() && supported && QJsonDocument::fromJson(body).object()["replacement_policy"].toString()!=filePolicy)error="Sentry did not confirm the requested same-Run file policy. Upgrade the receiver's getdaq-sentry; inspect raw files before retrying archive.";
        NodeStatus status;
        if (error.isEmpty()) ParseObservation(body,true,&status,&error);
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

void SentryClient::ParseObservation(const QByteArray &body,bool catalog,NodeStatus *status,QString *error){
 auto object=QJsonDocument::fromJson(body).object();const auto aggregate=object;
 fRunFilePolicies.clear();for(const auto &value:aggregate["run_file_policies"].toArray())fRunFilePolicies<<value.toString();
 const bool perSource=!catalog && object.contains("per_source");
 if(perSource){const auto source=object["per_source"].toObject()[QString::number(fNode.boardIndex>=0?fNode.boardIndex:fNode.id)].toObject();object["data_written_bytes"]=source["data_written_bytes"].toString("0");object["data_written_gb"]=source["data_written_gb"].toDouble();object["data_path_files"]=source["data_path_files"].toInt();}
 if(!Protocol::ParseSentry(QJsonDocument(object).toJson(),status,error))return;
 status->projectConfigurationPending=aggregate["configuration_pending"].toBool();status->sentrySourceSpecific=perSource;status->projectFiles=aggregate["data_path_files"].toInt();bool ok=false;status->projectFileBytes=aggregate["data_written_bytes"].toString().toULongLong(&ok);status->projectFileBytesKnown=ok;
}
