#ifndef GETDAQ_DEPLOYMENT_CONTROLLER_H
#define GETDAQ_DEPLOYMENT_CONTROLLER_H
#include "distributedproject.h"
#include "runconfig.h"
#include <functional>
namespace Deployment {
using Logger=std::function<void(const QString &)>;
QJsonObject EnsureMaster(const QString &root,const QString &programs,const QString &address,int port);
void StopMaster(const QJsonObject &master);
QJsonObject EnsureLocalNode(const QString &root,const QString &programs,const QString &address,int port,const QString &dataDirectory);
QJsonObject PairNode(const QJsonObject &master,const QString &invitation);
Endpoint NodeEndpoint(const QJsonObject &node);
QJsonObject NodeInventory(const QJsonObject &node);
QJsonObject PrepareProject(const QJsonObject &master,const QJsonArray &nodes,const Bundle &bundle,const Logger &log={},const QString &selectedGroup=QString());
QJsonObject ActivateProject(const QJsonObject &master,const QJsonArray &nodes,const QJsonObject &preparation,const Logger &log={},const QString &selectedGroup=QString());
QJsonObject StopGroup(const QJsonObject &master,const QJsonArray &nodes,const QJsonObject &preparation,const QString &groupId,bool repair,const Logger &log={});
QJsonObject ReleaseTerminatedGroup(const QJsonObject &master,const QJsonArray &nodes,const QString &groupId,const Logger &log={});
QJsonObject ObserveDeployment(const QJsonObject &master,const QJsonArray &nodes);
RunConfig ConsoleConfiguration(const QJsonObject &master,const QJsonObject &preparation,const QJsonObject &activation,const QString &groupId,const RunConfig &base);
}
#endif
