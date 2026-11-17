#include "runrecorder_p.h"
#include "xcfgdocument.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QDateTime>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QTemporaryDir>

#include <QMap>

namespace {
bool WriteSnapshotFile(const QString &path, const QByteArray &bytes, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        *error = file.errorString(); return false;
    }
    return true;
}
QString SnapshotDigest(const QByteArray &bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
}

namespace RunRecorderInternal {

// 备份的是本机配置镜像，不声称这些字节已经下发或被 ECC 采用。
bool SnapshotConfiguration(const RunConfig &c, QString *error, bool *missingFile)
{
    if(missingFile)*missingFile=false;
    const QDir run(c.RunDirectory());
    const QString destination = run.filePath("configuration");
    if (!QFileInfo::exists(destination)) {
        const QDir source(c.configurationSnapshot.isEmpty()?c.configDir:c.configurationSnapshot);
        QMap<QString, QByteArray> contents;
        QJsonArray roots;
        qint64 total = 0;
        for (const NodeConfig &node : c.nodes) {
            for (const QString &kind : {QString("describe"), QString("prepare"), QString("configure")}) {
                const QString id = node.ConfigId(kind, c.experiment);
                // 与旧 ConfigFileManager 一致：先替换扩展名为 .xcfg，再尝试 .xml。
                // Legacy ECC uses Configure storage for both Prepare and Configure.
                const QString storageKind = kind == "prepare" ? QString("configure") : kind;
                const QFileInfo requested(source.filePath(storageKind + "-" + id));
                const QString stem = QDir(requested.path()).filePath(requested.completeBaseName());
                QString path = stem + ".xcfg";
                if (!QFileInfo::exists(path)) path = stem + ".xml";
                if(missingFile && !QFileInfo::exists(path))*missingFile=true;
                XcfgDocument document;
                if (!document.Load(path, error)) {
                    *error = QString("Configuration backup (%1, node %2, ID %3): %4. "
                                     "The GUI configuration directory must contain a readable local/NFS mirror of the ECC files; no files were fetched from ECC.")
                        .arg(kind).arg(node.id).arg(id, *error);
                    return false;
                }
                QJsonObject root; root["node_id"] = node.id; root["kind"] = kind;
                root["config_id"] = id; root["source"] = document.RootFile(); roots.append(root);
                for (auto it = document.Sources().constBegin(); it != document.Sources().constEnd(); ++it) {
                    if (contents.contains(it.key())) {
                        if (contents.value(it.key()) != it->bytes) { *error = "Configuration changed during snapshot: " + it.key(); return false; }
                    } else {
                        total += it->bytes.size();
                        if (contents.size() >= 256 || total > 64 * 1024 * 1024) { *error = "Configuration snapshot exceeds 256 files / 64 MiB."; return false; }
                        contents.insert(it.key(), it->bytes);
                    }
                }
            }
        }
        QTemporaryDir staging(run.filePath("configuration.pending-XXXXXX"));
        if (!staging.isValid()) { *error = "Cannot create configuration snapshot directory."; return false; }
        QJsonArray files;
        const QDir canonicalSource(QFileInfo(c.configDir).canonicalFilePath());
        for (auto it = contents.constBegin(); it != contents.constEnd(); ++it) {
            const QString relative = canonicalSource.relativeFilePath(it.key());
            const bool external = relative == ".." || relative.startsWith("../") || QDir::isAbsolutePath(relative);
            const QString stored = external
                ? "files/external/" + SnapshotDigest(it.key().toUtf8()) + "/" + QFileInfo(it.key()).fileName()
                : "files/config/" + relative;
            const QString target = QDir(staging.path()).filePath(stored);
            if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !WriteSnapshotFile(target, it.value(), error)) return false;
            QJsonObject entry; entry["source"] = it.key(); entry["stored"] = stored;
            entry["sha256"] = SnapshotDigest(it.value()); entry["bytes"] = QString::number(it.value().size());
            entry["external"] = external; files.append(entry);
        }
        // 防止收集期间文件被替换；这仍不是跨进程的全局配置锁。
        for (auto it = contents.constBegin(); it != contents.constEnd(); ++it) {
            QFile file(it.key());
            if (!file.open(QIODevice::ReadOnly) || file.size() != it.value().size() || file.read(it.value().size() + 1) != it.value()) {
                *error = "Configuration changed during snapshot: " + it.key(); return false;
            }
        }
        QJsonObject manifest; manifest["schema_version"] = 1;
        manifest["captured_utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        manifest["capture_phase"] = "finalize_local_mirror";
        manifest["config_directory"] = canonicalSource.path(); manifest["roots"] = roots; manifest["files"] = files;
        if (!WriteSnapshotFile(QDir(staging.path()).filePath("manifest.json"), QJsonDocument(manifest).toJson(), error)) return false;
        if (!QDir().rename(staging.path(), destination)) { *error = "Cannot publish configuration snapshot; preserve existing directory for inspection."; return false; }
        staging.setAutoRemove(false);
    }
    // 若目录发布后写完成标记失败，重试只校验已发布快照，不换成新的源文件。
    QFile manifestFile(QDir(destination).filePath("manifest.json"));
    if (!manifestFile.open(QIODevice::ReadOnly) || manifestFile.size() > 1024 * 1024) { *error = "Invalid snapshot manifest."; return false; }
    const QByteArray manifestBytes = manifestFile.readAll();
    const auto manifest = QJsonDocument::fromJson(manifestBytes).object();
    const auto files = manifest.value("files").toArray();
    if (manifest.value("schema_version").toInt() != 1 || files.isEmpty() || files.size() > 256) { *error = "Invalid snapshot manifest."; return false; }
    qint64 verifiedBytes = 0;
    for (const auto &value : files) {
        const auto entry = value.toObject();
        const QString stored = entry.value("stored").toString();
        if (!stored.startsWith("files/") || stored.split('/').contains("..") ||
            QDir::cleanPath(stored) != stored || stored.contains('\\')) { *error = "Invalid snapshot path."; return false; }
        const QString path = QDir(destination).filePath(stored);
        QFile file(path);
        verifiedBytes += QFileInfo(path).size();
        if (QFileInfo(path).isSymLink() || !file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024 || verifiedBytes > 64 * 1024 * 1024 ||
            QString::number(file.size()) != entry.value("bytes").toString() || SnapshotDigest(file.readAll()) != entry.value("sha256").toString()) {
            *error = "Configuration snapshot verification failed: " + path; return false;
        }
    }
    return WriteSnapshotFile(run.filePath("backup.complete"), ("configuration/manifest.json SHA256 " + SnapshotDigest(manifestBytes) + "\n").toUtf8(), error);
}


}
