#include "sentryservice.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <cmath>
#include <limits>

bool SentryConfig::Validate(QString *error) const
{
    if (!QFileInfo(dataDir).isDir() || QFileInfo(dataDir).isSymLink()) *error = "data_dir must be an existing directory (not a symlink).";
    else if (checkProcess && processName.trimmed().isEmpty()) *error = "process_name is required when check_process is enabled.";
    else if (QHostAddress(bindAddress).isNull() || port < 0 || port > 65535) *error = "Invalid bind IP or port.";
    else if (settleMs < 0 || settleMs > 120000) *error = "settle_ms must be between 0 and 120000.";
    else { error->clear(); return true; }
    return false;
}
bool SentryConfig::Load(const QString &path, SentryConfig *config, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { *error = file.errorString(); return false; }
    if (file.size() > 65536) { *error = "Configuration too large."; return false; }
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (!doc.isObject() || parse.error != QJsonParseError::NoError) { *error = "Invalid JSON configuration."; return false; }
    const auto o = doc.object();
    SentryConfig c;
    c.dataDir = o.value("data_dir").toString();
    if (c.dataDir.isEmpty()) { *error = "data_dir is required."; return false; }
    if (QDir::isRelativePath(c.dataDir)) c.dataDir = QDir(QFileInfo(path).absolutePath()).absoluteFilePath(c.dataDir);
    if (o.contains("process_name") && !o.value("process_name").isString()) {
        *error = "process_name must be a string."; return false;
    }
    if (o.contains("disk_name") && !o.value("disk_name").isString()) {
        *error = "disk_name must be a string."; return false;
    }
    c.processRegistry=o.value("process_registry").toString();
    c.processName = o.value("process_name").toString(); c.diskName = o.value("disk_name").toString();
    if (o.contains("bind")) {
        if (!o.value("bind").isString()) { *error = "bind must be a string."; return false; }
        c.bindAddress = o.value("bind").toString();
    }
    const auto readInteger = [&o, error](const QString &key, int fallback, int *result) {
        if (!o.contains(key)) { *result = fallback; return true; }
        const QJsonValue value = o.value(key);
        if (!value.isDouble()) { *error = key + " must be an integer."; return false; }
        const double number = value.toDouble();
        if (!std::isfinite(number) || std::floor(number) != number
            || number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) {
            *error = key + " must be an integer in range."; return false;
        }
        *result = static_cast<int>(number);
        return true;
    };
    if (!readInteger("port", c.port, &c.port) || !readInteger("settle_ms", c.settleMs, &c.settleMs)) return false;
    if (o.contains("check_process")) {
        if (!o.value("check_process").isBool()) { *error = "check_process must be boolean."; return false; }
        c.checkProcess = o.value("check_process").toBool();
    }
    if (!c.Validate(error)) return false;
    *config = c; return true;
}





