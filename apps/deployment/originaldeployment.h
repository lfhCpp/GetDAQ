#ifndef GETDAQ_ORIGINAL_DEPLOYMENT_H
#define GETDAQ_ORIGINAL_DEPLOYMENT_H
#include "deploymentcommon.h"
#include "runconfig.h"
namespace Deployment {
QJsonArray OriginalRepository(const QString &directory,const QString &group,QMap<QString,QByteArray> *files);
void ValidateOriginalFiles(const QJsonObject &group,const QMap<QString,QByteArray> &files);
RunConfig OriginalConsole(const QJsonObject &master,const QJsonObject &group,const QJsonObject &active,const RunConfig &base);
int OriginalEccState(const QString &address,int port,int *transition=nullptr);
}
#endif
