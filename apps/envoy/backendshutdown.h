#ifndef GETDAQ_BACKEND_SHUTDOWN_H
#define GETDAQ_BACKEND_SHUTDOWN_H
#include "deploymentcontroller.h"
// Only authenticated node-agent instance records are eligible. External ECC
// endpoints and executable-name searches never confer ownership.
QJsonObject ManagedBackendExitPlan(const QString &projectFile,const QString &localNodeRoot);
void StopManagedBackends(const QJsonObject &plan,const QString &auditRoot,const Deployment::Logger &log={});
#endif
