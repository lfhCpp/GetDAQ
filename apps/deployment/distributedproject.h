#ifndef GETDAQ_DISTRIBUTED_PROJECT_H
#define GETDAQ_DISTRIBUTED_PROJECT_H
#include "runconfig.h"
#include "deploymentcommon.h"
#include <QJsonArray>
namespace Deployment {
struct Bundle {QJsonObject manifest;QMap<QString,QByteArray> objects;};
Bundle BuildBundle(const QString &projectId,const QJsonArray &hosts,const QJsonArray &groups,const QMap<QString,QByteArray> &files);
void ValidateRelease(const QJsonObject &manifest);
QString GroupDigest(QJsonObject group);
void ValidateGroupFiles(const QJsonObject &group,const QMap<QString,QByteArray> &files);
QByteArray Assemble(const QJsonObject &file,const QMap<QString,QByteArray> &objects);
QMap<QString,QByteArray> ReadBundleFiles(const Bundle &bundle);
QString UploadBundle(const Endpoint &repository,const QString &repositoryId,const Bundle &bundle);
}
#endif
