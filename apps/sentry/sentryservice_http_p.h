#ifndef GETDAQ_SENTRY_SERVICE_HTTP_P_H
#define GETDAQ_SENTRY_SERVICE_HTTP_P_H

#include <QByteArray>
#include <QString>

namespace SentryHttpInternal {
enum class Route { Status, Catalog, FinalizeReplacement };

struct MutationRequest {
    QString experiment;
    QString operationId;
    int run = 0;
    bool replaceCompletedRun = false;
};

bool ResolveRoute(const QString &method, const QString &path, Route *route,
                  int *errorCode, QString *error);
bool ParseMutation(Route route, const QByteArray &body, MutationRequest *request,
                   int *errorCode, QString *error);
}

#endif
