#ifndef GETDAQ_DEPLOYMENT_PATHS_H
#define GETDAQ_DEPLOYMENT_PATHS_H
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
QString DeploymentPathSummary(const QJsonObject &group,const QJsonArray &hosts,const QString &source,const QString &records);
#endif
