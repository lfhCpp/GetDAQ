#include "sentryservice.h"


SentryService::SentryService(const SentryConfig &config, QObject *parent) : QObject(parent), fConfig(config)
{
    connect(&fServer, &HttpServer::RequestSignal, this, &SentryService::Handle);
}
bool SentryService::Start(QString *error)
{
    if (!fConfig.Validate(error)) return false;
    if (!fServer.listen(QHostAddress(fConfig.bindAddress), quint16(fConfig.port))) { *error = fServer.errorString(); return false; }
    return true;
}
