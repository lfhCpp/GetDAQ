#pragma once

#include <QJsonObject>

#include <string>

namespace GetDAQControlMcpStdio {

QJsonObject RpcError(const QJsonValue &id, int code, const QString &message);
void WriteMessage(const QJsonObject &message);
bool ReadLine(std::string *line, bool *tooLarge);

}
