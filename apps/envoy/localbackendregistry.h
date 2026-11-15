#ifndef GETDAQ_LOCAL_BACKEND_REGISTRY_H
#define GETDAQ_LOCAL_BACKEND_REGISTRY_H
#include <QStringList>
void RememberLocalBackend(const QString &configuration);
QStringList KnownLocalBackends();
#endif
