#include "mcpsettings.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStringList>
#include <stdexcept>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace GetDAQControlMcpInternal {

void Require(bool condition, const QString &message)
{
    if (!condition) throw std::runtime_error(message.toStdString());
}

QString Text(const QJsonValue &value, const QString &field, int maximum)
{
    Require(value.isString(), field + " must be a string");
    const QString text = value.toString();
    Require(!text.isEmpty() && text.size() <= maximum, field + " length is invalid");
    return text;
}



Settings LoadSettings(const QString &path)
{
    const QFileInfo configInfo(path);
    Require(configInfo.exists() && !configInfo.isSymLink() && configInfo.isFile() && configInfo.size() <= 64 * 1024,
            "MCP config must be a bounded regular file, not a symlink");
    QFile config(path);
    Require(config.open(QIODevice::ReadOnly) && config.size() <= 64 * 1024,
            "Cannot read MCP config or config exceeds 64 KiB");
    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(config.readAll(), &parse);
    Require(parse.error == QJsonParseError::NoError && document.isObject(), "MCP config is not valid JSON");
    const QJsonObject object = document.object();
    const QStringList allowedKeys = QStringList() << "schema" << "base_url" << "credential_file"
        << "client_epoch_file" << "client_id" << "request_timeout_ms" << "allow_control";
    for (auto i = object.constBegin(); i != object.constEnd(); ++i)
        Require(allowedKeys.contains(i.key()), "Unknown MCP config field: " + i.key());
    Require(object.value("schema").toInt(-1) == 1, "Unsupported MCP config schema");
    Settings settings;
    settings.baseUrl = QUrl(Text(object.value("base_url"), "base_url"));
    Require((settings.baseUrl.scheme() == "http" || settings.baseUrl.scheme() == "https") &&
            !settings.baseUrl.host().isEmpty() && settings.baseUrl.userInfo().isEmpty() &&
            settings.baseUrl.query().isEmpty() && settings.baseUrl.fragment().isEmpty() &&
            (settings.baseUrl.path().isEmpty() || settings.baseUrl.path() == "/"),
            "base_url must be an HTTP(S) origin without credentials, query, fragment or path");
    settings.credentialFile = Text(object.value("credential_file"), "credential_file");
    settings.clientEpochFile = Text(object.value("client_epoch_file"), "client_epoch_file");
    settings.clientId = Text(object.value("client_id"), "client_id", 128);
    Require(QRegularExpression("^[A-Za-z0-9_-]+$").match(settings.clientId).hasMatch(),
            "client_id allows only ASCII letters, digits, underscore and hyphen");
    settings.allowControl = object.value("allow_control").toBool(false);
    if (object.contains("allow_control"))
        Require(object.value("allow_control").isBool(), "allow_control must be a JSON boolean");
    if (object.contains("request_timeout_ms")) {
        const int timeout = object.value("request_timeout_ms").toInt(-1);
        Require(timeout >= 500 && timeout <= 30000, "request_timeout_ms must be 500..30000");
        settings.timeoutMs = timeout;
    }
    return settings;
}

QString ReadCredential(const QString &path)
{
    const QFileInfo info(path);
    Require(info.exists() && !info.isSymLink() && info.isFile() && info.size() <= 4096,
            "Credential must be a bounded regular file, not a symlink");
#ifdef Q_OS_UNIX
    struct stat status;
    Require(::lstat(QFile::encodeName(path).constData(), &status) == 0 && S_ISREG(status.st_mode) &&
            status.st_uid == ::geteuid() && !(status.st_mode & 0077),
            "Credential must be owned by this user with no group/other permissions");
#endif
    QFile file(path);
    Require(file.open(QIODevice::ReadOnly) && file.size() <= 4096, "Cannot read credential file");
    QString token = QString::fromLatin1(file.readAll()).trimmed();
    Require(QRegularExpression("^[0-9a-f]{64}$").match(token).hasMatch(), "Credential must contain one 64-character lowercase hex token");
    return token;
}

QString LoadOrCreateEpoch(const QString &path, const QString &currentOwnerEpoch)
{
    const QRegularExpression digest("^[0-9a-f]{64}$");
    const auto readExisting = [&]() -> QString {
        QFileInfo info(path);
        Require(info.exists() && !info.isSymLink() && info.isFile() && info.size() <= 256,
                "Client epoch must be a bounded regular file, not a symlink");
#ifdef Q_OS_UNIX
        struct stat status;
        Require(::lstat(QFile::encodeName(path).constData(), &status) == 0 && S_ISREG(status.st_mode) &&
                status.st_uid == ::geteuid() && !(status.st_mode & 0077),
                "Client epoch must be owned by this user with no group/other permissions");
#endif
        QFile input(path);
        Require(input.open(QIODevice::ReadOnly), "Cannot read client epoch file");
        const QString value = QString::fromLatin1(input.readAll()).trimmed();
        Require(digest.match(value).hasMatch(), "Client epoch file is malformed");
        return value;
    };
    if (QFileInfo::exists(path)) {
        const QString existing = readExisting();
        Require(currentOwnerEpoch.isEmpty() || existing == currentOwnerEpoch,
                "Local client epoch differs from current ECC owner; no automatic takeover");
        return existing;
    }

    if (!currentOwnerEpoch.isEmpty()) {
        Require(digest.match(currentOwnerEpoch).hasMatch(), "ECC owner epoch is malformed");
        QFile output(path);
        if (output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            Require(output.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner),
                    "Cannot restrict client epoch file permissions");
            Require(output.write((currentOwnerEpoch + "\n").toLatin1()) == 65 && output.flush(),
                    "Cannot persist existing ECC owner epoch");
            output.close();
            return currentOwnerEpoch;
        }
        if (QFileInfo::exists(path)) {
            const QString existing = readExisting();
            Require(existing == currentOwnerEpoch, "Local client epoch differs from current ECC owner; no automatic takeover");
            return existing;
        }
        throw std::runtime_error("Cannot persist current ECC owner epoch");
    }

    QByteArray entropy;
    for (int i = 0; i < 4; ++i) {
        const quint64 value = QRandomGenerator::system()->generate64();
        entropy.append(reinterpret_cast<const char *>(&value), sizeof(value));
    }
    const QString epoch = QString::fromLatin1(QCryptographicHash::hash(entropy, QCryptographicHash::Sha256).toHex());
    QFile output(path);
    if (output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        Require(output.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner),
                "Cannot restrict client epoch file permissions");
        Require(output.write((epoch + "\n").toLatin1()) == 65 && output.flush(), "Cannot persist client epoch");
        output.close();
        return epoch;
    }
    if (QFileInfo::exists(path)) return readExisting(); // Another instance won exclusive creation.
    throw std::runtime_error("Cannot exclusively create client epoch file");
}


} // namespace GetDAQControlMcpInternal
