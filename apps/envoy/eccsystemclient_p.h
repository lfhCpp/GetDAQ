#ifndef ECCSYSTEMCLIENT_P_H
#define ECCSYSTEMCLIENT_P_H
#include <QString>
#include <exception>
#include <string>

namespace EccSystemClientInternal {
inline std::string Utf8(const QString &text) { return text.toUtf8().toStdString(); }
inline QString Failure(const std::exception &e) { return QString::fromUtf8(e.what()).left(512); }
}
#endif
