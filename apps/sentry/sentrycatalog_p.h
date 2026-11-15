#ifndef GETDAQ_SENTRYCATALOG_P_H
#define GETDAQ_SENTRYCATALOG_P_H

#include "sentryservice.h"
#include <QSet>

namespace SentryCatalogInternal {
ServiceResult Failure(int code, const QString &error);
bool SaveJson(const QString &path, const QJsonObject &object);
QString NativeReceiverSealError(const QString &dataDirectory);
bool CompleteLegacyCatalog(const QString &directory, const QString &experiment, int run,
                            QString *error, const QSet<QString> &transferredFiles = QSet<QString>());
}

#endif
