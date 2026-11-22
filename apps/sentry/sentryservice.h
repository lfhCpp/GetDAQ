#ifndef SENTRYSERVICE_H
#define SENTRYSERVICE_H
#include "httpserver.h"
#include <QJsonObject>
#include <QFutureWatcher>

struct SentryConfig
{
    QString dataDir;
    QString processName;
    QString processRegistry;
    QString diskName;
    QString bindAddress = "127.0.0.1";
    int port = 8080;
    int settleMs = 30000;
    bool checkProcess = true;
    static bool Load(const QString &path, SentryConfig *config, QString *error);
    bool Validate(QString *error) const;
};
struct ServiceResult
{
    int code = 200;
    QJsonObject json;
};
namespace SentryFiles {
ServiceResult Status(const SentryConfig &config);
ServiceResult Catalog(const SentryConfig &config, const QString &experiment, int run,
                      bool replaceCompletedRun = false, const QString &operationId = QString(), bool overwriteExisting = false, bool appendExisting = false);
ServiceResult FinalizeReplacement(const SentryConfig &config, const QString &experiment, int run,
                                  const QString &operationId);
}

class SentryService : public QObject
{
    Q_OBJECT
public:
    explicit SentryService(const SentryConfig &config, QObject *parent = nullptr);
    bool Start(QString *error);
    quint16 Port() const { return fServer.serverPort(); }
private:
    void Handle(QString method, QString path, QByteArray body, HttpConnection *connection);
    SentryConfig fConfig;
    HttpServer fServer;
    bool fCatalogBusy = false;
    int fJobs = 0;
};
#endif
