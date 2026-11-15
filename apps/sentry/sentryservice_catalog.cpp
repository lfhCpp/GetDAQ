#include "sentryservice.h"
#include "sentrycatalog_p.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QLockFile>
#include <QDateTime>
#include <QUuid>
#include <memory>
#include <stdexcept>
#ifdef Q_OS_UNIX
#include "../../common/storage/directorylease.h"
#endif

using namespace SentryCatalogInternal;

ServiceResult SentryFiles::Catalog(const SentryConfig &c, const QString &experiment, int run,
                                   bool replaceCompletedRun, const QString &operationId)
{
    if (!QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_-]{0,79}$").match(experiment).hasMatch() || run < 0)
        return Failure(400, "Invalid experiment or run_number.");
    if (replaceCompletedRun && !QRegularExpression("^[A-Za-z0-9_-]{1,80}$").match(operationId).hasMatch())
        return Failure(400, "Replacement requires a bounded operation_id.");
    if (!replaceCompletedRun && !operationId.isEmpty())
        return Failure(400, "operation_id is only valid for an explicit replacement.");
    const QDir root(c.dataDir);
    QLockFile lock(root.filePath(".getdaq-catalog.lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) return Failure(409, "Another catalog operation owns this directory.");
    // 与原生接收器用同一个内核锁。进程崩溃会自动释放锁，但没有封存凭据仍禁止归档。
#ifdef Q_OS_UNIX
    std::unique_ptr<GetDAQ::DirectoryLease> receiverLease;
    try { receiverLease.reset(new GetDAQ::DirectoryLease(QFile::encodeName(root.absolutePath()).toStdString())); }
    catch (const std::exception &e) { return Failure(409, QString::fromLocal8Bit(e.what())); }
#endif
    const bool nativeReceiver = QFileInfo::exists(root.filePath(".getdaq-native-receiver"));
    if (replaceCompletedRun && nativeReceiver)
        return Failure(409, "Explicit legacy replacement is unavailable for Native receiver sessions; use the Native catalog transaction.");
    if (nativeReceiver) {
#ifndef Q_OS_UNIX
        return Failure(409, "Native receiver archives must be performed by the Linux/macOS agent on the receiving host.");
#else
        const QString sealError=NativeReceiverSealError(root.absolutePath());
        if(!sealError.isEmpty())return Failure(409,sealError);
#endif
    }
    const QString sealPath = root.filePath(".getdaq-receiver-seal.json");
    const ServiceResult before = Status(c);
    if (before.code != 200) return before;
    const QString experimentDir = root.filePath(experiment);
    if (QFileInfo(experimentDir).isSymLink()) return Failure(400, "Experiment directory cannot be a symlink.");
    const QString relative = QString("%1/run_%2").arg(experiment).arg(run, 4, 10, QLatin1Char('0'));
    const QString target = root.filePath(relative);
    QString backupName;
    bool movedOld = false;
    if (QFileInfo::exists(target) || QFileInfo(target).isSymLink()) {
        if (!replaceCompletedRun) return Failure(409, "Run already exists; files have not been moved.");
        QString completedError;
        if (CompleteLegacyCatalog(target, experiment, run, &completedError)) {
            QFile existingManifest(QDir(target).filePath("catalog.json"));
            QJsonParseError existingParse;
            const QJsonDocument existingDocument = existingManifest.open(QIODevice::ReadOnly)
                ? QJsonDocument::fromJson(existingManifest.readAll(), &existingParse) : QJsonDocument();
            const QJsonObject existing = existingDocument.object();
            if (existingParse.error == QJsonParseError::NoError && existingDocument.isObject() &&
                existing.value("replacement_operation_id").toString() == operationId) {
                // Same explicit operation may have completed even if its HTTP reply was lost.
                return Status(c);
            }
        }
        QString inspectError;
        if (!CompleteLegacyCatalog(target, experiment, run, &inspectError)) return Failure(409, inspectError);
        backupName = "." + QFileInfo(target).fileName() + ".replaced-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (QFileInfo::exists(QDir(experimentDir).filePath(backupName))) return Failure(409, "Replacement backup path collision; preserve old Run.");
        if (!QDir(experimentDir).rename(QFileInfo(target).fileName(), backupName))
            return Failure(500, "Cannot retain old completed Run; preserve it.");
        movedOld = true;
    }
    if (!QDir().mkpath(experimentDir) || !QDir(experimentDir).mkdir(QFileInfo(target).fileName())) {
        if (movedOld && !QDir(experimentDir).rename(backupName, QFileInfo(target).fileName()))
            return Failure(500, "Cannot create replacement Run or restore old Run; both paths were preserved for inspection.");
        return Failure(500, "Cannot reserve run directory.");
    }
    QJsonObject manifest;
    manifest["experiment"] = experiment; manifest["run_number"] = run;
    manifest["started_utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    manifest["status"] = "moving";
    if (replaceCompletedRun) {
        manifest["replacement_operation_id"] = operationId;
        manifest["replacement_backup"] = backupName;
    }
    QJsonArray planned, moved;
    const QFileInfoList files = root.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
    for (const QFileInfo &file : files) if (file.suffix() == "graw") planned.append(file.fileName());
    manifest["planned"] = planned; manifest["moved"] = moved;
    const QString journal = QDir(target).filePath("catalog.json");
    if (!SaveJson(journal, manifest)) return Failure(500, "Cannot journal catalog operation; reserved run directory retained.");
    for (const QJsonValue &name : planned) {
        const QString source = root.filePath(name.toString());
        const QString destination = QDir(target).filePath(name.toString());
        // 同一数据根目录内重命名，无读入/复制整个原始数据文件，也不覆盖已有目标。
        if (QFileInfo(source).isSymLink() || !QFile::rename(source, destination)) {
            manifest["status"] = "partial"; manifest["moved"] = moved; SaveJson(journal, manifest);
            return Failure(500, "Partial catalog; inspect " + journal);
        }
        moved.append(name);
        manifest["moved"] = moved;
        if (!SaveJson(journal, manifest)) return Failure(500, "Catalog journal failed after a move; inspect source and target.");
    }
    ServiceResult after = Status(c);
    if (after.code != 200 || after.json.value("data_path_files").toInt() != 0) {
        manifest["status"] = "incomplete"; SaveJson(journal, manifest);
        return Failure(409, "Incoming data remains or status check failed; inspect " + journal);
    }
    manifest["status"] = "complete";
    manifest["receiver_seal_verified"] = nativeReceiver;
    if (nativeReceiver && !QFile::rename(sealPath, QDir(target).filePath("receiver-seal.json"))) {
        manifest["status"] = "incomplete"; SaveJson(journal, manifest);
        return Failure(500, "Data moved but receiver seal could not be moved; inspect catalog journal.");
    }
    manifest["finished_utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if (!SaveJson(journal, manifest)) return Failure(500, "Files moved but completion manifest failed.");
    return after;
}
