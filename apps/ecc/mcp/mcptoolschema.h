#pragma once

#include <QJsonObject>
#include <QString>

namespace GetDAQControlMcpToolSchema {
QJsonObject ReadTool(const QString &name, const QString &description);
QJsonObject OperationTool();
QJsonObject ControlTool();
QJsonObject CatalogTool();
QJsonObject ConfigureCoBoTool();
QJsonObject ReprepareCoBoTool();
QJsonObject CatalogRecoveryTool();
}
