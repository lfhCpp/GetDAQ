#ifndef PROTOCOL_H
#define PROTOCOL_H
#include "runconfig.h"
#include <QByteArray>

namespace Protocol {
QByteArray EccRequest(const NodeConfig &node, const QString &experiment, const QString &operation);
bool ParseEcc(const QByteArray &xml, bool status, int *state, int *transition, QString *error);
bool ParseSentry(const QByteArray &json, NodeStatus *status, QString *error);
}
#endif
