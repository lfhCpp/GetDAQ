#ifndef GETDAQ_CONTROL_MCP_SETTINGS_H
#define GETDAQ_CONTROL_MCP_SETTINGS_H

#include <QJsonValue>
#include <QString>
#include <QUrl>

namespace GetDAQControlMcpInternal {

struct Settings {
    QUrl baseUrl;
    QString credentialFile;
    QString clientEpochFile;
    QString clientId;
    int timeoutMs = 5000;
    bool allowControl = false;
};

void Require(bool condition, const QString &message);
QString Text(const QJsonValue &value, const QString &field, int maximum = 4096);
Settings LoadSettings(const QString &path);
QString ReadCredential(const QString &path);
QString LoadOrCreateEpoch(const QString &path, const QString &currentOwnerEpoch);

} // namespace GetDAQControlMcpInternal

#endif
