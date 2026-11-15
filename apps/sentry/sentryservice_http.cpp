#include "sentryservice.h"
#include "sentryservice_http_p.h"
#include <QJsonDocument>
#include <QPointer>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

using namespace SentryHttpInternal;

void SentryService::Handle(QString method, QString path, QByteArray body, HttpConnection *connection)
{
    auto reject = [connection](int code, const QString &message) {
        QJsonObject json; json["error"] = message;
        connection->Reply(code, QJsonDocument(json).toJson(QJsonDocument::Compact));
    };
    Route route;
    int errorCode = 400;
    QString error;
    if (!ResolveRoute(method, path, &route, &errorCode, &error)) { reject(errorCode, error); return; }
    const bool catalog = route == Route::Catalog;
    const bool finalizeReplacement = route == Route::FinalizeReplacement;
    const bool mutation = route != Route::Status;
    if (fJobs >= 8 || (mutation && fCatalogBusy)) { reject(409, "Service busy."); return; }
    MutationRequest request;
    if (mutation) {
        if (!ParseMutation(route, body, &request, &errorCode, &error)) { reject(errorCode, error); return; }
        fCatalogBusy = true;
    }
    ++fJobs;
    const QPointer<HttpConnection> reply(connection);
    // 30 秒等待使用事件定时器，/status 仍可响应，不占用睡眠线程。
    QTimer::singleShot(catalog ? fConfig.settleMs : 0, this, [this, reply, catalog, finalizeReplacement, request]() {
        if (!reply) { --fJobs; if (catalog || finalizeReplacement) fCatalogBusy = false; return; }
        QFutureWatcher<ServiceResult> *watcher = new QFutureWatcher<ServiceResult>(this);
        connect(watcher, &QFutureWatcher<ServiceResult>::finished, this, [this, watcher, reply, catalog, finalizeReplacement]() {
            const ServiceResult result = watcher->result();
            --fJobs; if (catalog || finalizeReplacement) fCatalogBusy = false;
            if (reply) reply->Reply(result.code, QJsonDocument(result.json).toJson(QJsonDocument::Compact));
            if (result.code != 200) qWarning().noquote() << result.json.value("error").toString();
            watcher->deleteLater();
        });
        const SentryConfig config = fConfig;
        watcher->setFuture(QtConcurrent::run([config, catalog, finalizeReplacement, request]() {
            if (catalog) return SentryFiles::Catalog(config, request.experiment, request.run, request.replaceCompletedRun, request.operationId);
            if (finalizeReplacement) return SentryFiles::FinalizeReplacement(config, request.experiment, request.run, request.operationId);
            return SentryFiles::Status(config);
        }));
    });
}
