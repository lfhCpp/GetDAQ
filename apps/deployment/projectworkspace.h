#ifndef GETDAQ_PROJECT_WORKSPACE_H
#define GETDAQ_PROJECT_WORKSPACE_H
#include "deploymentcommon.h"
namespace Deployment {
QString FormatFingerprint(const QString &project);
QString ProjectPath(const QJsonObject &group,const QString &node,const QString &fallback);
QJsonObject SyncWorkspace(const QString &project,const QJsonObject &group,const QMap<QString,QByteArray> &files,const QString &release,bool enabled,bool canApply);
QJsonObject ReadServiceLog(const QString &path,qint64 offset,const QString &generation);
}
#endif
