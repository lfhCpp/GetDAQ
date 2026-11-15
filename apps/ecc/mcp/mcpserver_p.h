#ifndef GETDAQ_CONTROL_MCP_SERVER_P_H
#define GETDAQ_CONTROL_MCP_SERVER_P_H

#include "mcpsettings.h"
#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace GetDAQControlMcpInternal {
class Server {
public:
    explicit Server(const Settings &settings) : fSettings(settings) {}
    QJsonObject Handle(const QJsonObject &request, bool *hasResponse);
private:
    QByteArray Http(const QByteArray &method, const QString &path, const QByteArray &body = QByteArray());
    QString Call(const QString &name, const QJsonObject &arguments);
    QString Submit(const QJsonObject &arguments);
    QJsonObject CoBoTargets();
    QString CoBoLifecycles(const QJsonObject &arguments);
    QString ConfigureCoBo(const QJsonObject &arguments);
    QString ReprepareCoBo(const QJsonObject &arguments);
    QString Catalog(const QJsonObject &arguments);
    QString ResumeCatalogReplacement(const QJsonObject &arguments);
    QString SubmitRequest(const QString &command,const QString &operationId,const QJsonObject &fields);
    static const int MaximumResponseBytes = 1024 * 1024;
    Settings fSettings;
    bool fInitializePending = false;
    bool fInitialized = false;
};
} // namespace GetDAQControlMcpInternal

#endif
