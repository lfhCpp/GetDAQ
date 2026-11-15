#ifndef CONFIGURATIONREPOSITORYCLIENT_P_H
#define CONFIGURATIONREPOSITORYCLIENT_P_H
#include "strictjson.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <stdexcept>

namespace ConfigurationRepositoryInternal {
inline QString Sha(const QByteArray &bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
}
inline void Digest(const QString &value) {
    if (!QRegularExpression("^[0-9a-f]{64}$").match(value).hasMatch())
        throw std::invalid_argument("Explicit repository SHA-256 identity required");
}
inline QJsonObject Json(const QByteArray &bytes) {
    GetDAQ::ParseStrictJson(bytes.toStdString());
    const auto doc=QJsonDocument::fromJson(bytes);
    if (!doc.isObject()) throw std::invalid_argument("Repository reply must be JSON object");
    return doc.object();
}
inline QByteArray Read(const QString &path,qint64 maximum) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size()>maximum)
        throw std::runtime_error("Cannot read bounded local publication file");
    const auto bytes=file.read(maximum+1);
    if (file.error()!=QFile::NoError || bytes.size()>maximum)
        throw std::runtime_error("Local publication file exceeds budget");
    return bytes;
}
}
#endif
