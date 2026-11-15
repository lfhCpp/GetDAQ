#include "sentryservice.h"
#include "sentrycatalog_p.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QLockFile>
#include <QSet>
#include <memory>
#include <stdexcept>
#ifdef Q_OS_UNIX
#include "../../common/storage/directorylease.h"
#endif

using namespace SentryCatalogInternal;

ServiceResult SentryFiles::FinalizeReplacement(const SentryConfig &c, const QString &experiment, int run,
                                                const QString &operationId)
{
    if (!QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_-]{0,79}$").match(experiment).hasMatch() || run < 0 ||
        !QRegularExpression("^[A-Za-z0-9_-]{1,80}$").match(operationId).hasMatch())
        return Failure(400, "Invalid replacement identity.");
    const QDir root(c.dataDir);
    QLockFile lock(root.filePath(".getdaq-catalog.lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) return Failure(409, "Another catalog operation owns this directory.");
#ifdef Q_OS_UNIX
    std::unique_ptr<GetDAQ::DirectoryLease> receiverLease;
    try { receiverLease.reset(new GetDAQ::DirectoryLease(QFile::encodeName(root.absolutePath()).toStdString())); }
    catch (const std::exception &e) { return Failure(409, QString::fromLocal8Bit(e.what())); }
#endif
    const QString runName = QString("run_%1").arg(run, 4, 10, QLatin1Char('0'));
    const QString experimentDirectory = root.filePath(experiment);
    if (QFileInfo(experimentDirectory).isSymLink() || !QFileInfo(experimentDirectory).isDir())
        return Failure(409, "Experiment catalog directory is missing or is not a regular directory.");
    const QString target = QDir(experimentDirectory).filePath(runName);
    const QString manifestPath = QDir(target).filePath("catalog.json");
    QFile manifestFile(manifestPath);
    if (QFileInfo(target).isSymLink() || !QFileInfo(target).isDir() || QFileInfo(manifestPath).isSymLink() ||
        !manifestFile.open(QIODevice::ReadOnly) || manifestFile.size() > 1024 * 1024)
        return Failure(409, "Replacement Run manifest cannot be safely inspected.");
    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll(), &parse);
    // Windows does not allow QSaveFile to atomically replace a file while a
    // QFile handle to the destination is still open. The parsed document owns
    // its data, so release the handle before any manifest update below.
    manifestFile.close();
    QJsonObject manifest = document.object();
    if (parse.error != QJsonParseError::NoError || !document.isObject() ||
        manifest.value("status").toString() != "complete" || manifest.value("experiment").toString() != experiment ||
        !manifest.value("run_number").isDouble() || manifest.value("run_number").toInt(-1) != run ||
        manifest.value("replacement_operation_id").toString() != operationId)
        return Failure(409, "New Run is not Complete or replacement operation identity differs.");
    const QString backupName = manifest.value("replacement_backup").toString();
    if (backupName.isEmpty()) {
        ServiceResult result; result.json["replacement_cleanup_complete"] = true;
        result.json["experiment"] = experiment; result.json["run_number"] = run;
        result.json["operation_id"] = operationId; return result;
    }
    if (QFileInfo(backupName).fileName() != backupName || backupName.contains('\\') ||
        !backupName.startsWith("." + runName + ".replaced-"))
        return Failure(409, "Replacement backup path is invalid; preserve it.");
    const QString backupPath = QDir(experimentDirectory).filePath(backupName);
    if (QFileInfo::exists(backupPath)) {
        const QStringList newNames = [&manifest]() {
            QStringList names;
            for (const QJsonValue &entry : manifest.value("planned").toArray()) names.append(entry.toString());
            return names;
        }();
        QSet<QString> newNameSet;
        for (const QString &name : newNames) newNameSet.insert(name);
        QSet<QString> transferred;
        for (const QJsonValue &entry : manifest.value("replacement_preserved").toArray()) {
            if (!entry.isString() || entry.toString().isEmpty() || QFileInfo(entry.toString()).fileName() != entry.toString() ||
                !entry.toString().endsWith(".graw") || transferred.contains(entry.toString()))
                return Failure(409, "Replacement progress contains an unsafe preserved filename; preserve both Runs.");
            transferred.insert(entry.toString());
        }
        QStringList oldNames;
        QFile oldManifestFile(QDir(backupPath).filePath("catalog.json"));
        if (QFileInfo(oldManifestFile.fileName()).isSymLink() || !oldManifestFile.open(QIODevice::ReadOnly) ||
            oldManifestFile.size() > 1024 * 1024)
            return Failure(409, "Previous Run manifest cannot be read safely; preserve both Runs.");
        QJsonParseError oldParse;
        const QJsonDocument oldDocument = QJsonDocument::fromJson(oldManifestFile.readAll(), &oldParse);
        oldManifestFile.close();
        if (oldParse.error != QJsonParseError::NoError || !oldDocument.isObject() ||
            !oldDocument.object().value("planned").isArray())
            return Failure(409, "Previous Run raw list is malformed; preserve both Runs.");
        for (const QJsonValue &entry : oldDocument.object().value("planned").toArray()) {
            if (!entry.isString() || entry.toString().isEmpty() || QFileInfo(entry.toString()).fileName() != entry.toString() ||
                !entry.toString().endsWith(".graw") || oldNames.contains(entry.toString()))
                return Failure(409, "Previous Run raw list contains an unsafe filename; preserve both Runs.");
            oldNames.append(entry.toString());
        }
        for (const QString &name : transferred)
            if (!oldNames.contains(name))
                return Failure(409, "Replacement progress does not match a unique old raw filename; preserve both Runs.");
        // A crash may occur after an atomic rename but before its progress receipt is saved.
        // Infer only a missing old, non-colliding file whose regular destination is present.
        for (const QString &name : oldNames) {
            if (newNameSet.contains(name) || transferred.contains(name)) continue;
            const QFileInfo sourceInfo(QDir(backupPath).filePath(name));
            const QFileInfo destinationInfo(QDir(target).filePath(name));
            if (!sourceInfo.exists() && !sourceInfo.isSymLink() && !destinationInfo.isSymLink() && destinationInfo.isFile())
                transferred.insert(name);
        }
        QString inspectError;
        if (!CompleteLegacyCatalog(backupPath, experiment, run, &inspectError, transferred)) return Failure(409, inspectError);
        QDir backup(backupPath);
        for (const QString &name : oldNames) {
            if (newNameSet.contains(name)) continue; // The new file wins an exact filename collision.
            const QString source = backup.filePath(name);
            const QString destination = QDir(target).filePath(name);
            const QFileInfo sourceInfo(source), destinationInfo(destination);
            if (transferred.contains(name)) {
                if (sourceInfo.exists() || destinationInfo.isSymLink() || !destinationInfo.isFile())
                    return Failure(409, "Replacement progress no longer matches preserved raw files; preserve both Runs.");
                continue;
            }
            if (!sourceInfo.exists()) {
                if (destinationInfo.isSymLink() || !destinationInfo.isFile())
                    return Failure(409, "Previous Run raw file disappeared before it could be preserved; inspect both Runs.");
                // Recover a crash after the same-volume rename but before the journal update.
            } else if (sourceInfo.isSymLink() || !sourceInfo.isFile() || destinationInfo.exists() || destinationInfo.isSymLink() ||
                       !QFile::rename(source, destination)) {
                return Failure(500, "Cannot retain a non-colliding raw file from the previous Run: " + name);
            }
            transferred.insert(name);
            QJsonArray progress;
            for (const QString &preserved : transferred) progress.append(preserved);
            manifest["replacement_preserved"] = progress;
            if (!SaveJson(manifestPath, manifest))
                return Failure(500, "Raw file was retained but replacement progress could not be saved; retry the same operation id.");
        }
        QJsonArray allNames;
        for (const QString &name : newNames) allNames.append(name);
        for (const QString &name : transferred) if (!newNameSet.contains(name)) allNames.append(name);
        manifest["planned"] = allNames;
        manifest["moved"] = allNames;
        manifest["replacement_cleanup_complete"] = false;
        if (!SaveJson(manifestPath, manifest))
            return Failure(500, "Preserved raw files are present, but the merged catalog manifest could not be saved; retry the same operation id.");
        if (!CompleteLegacyCatalog(target, experiment, run, &inspectError))
            return Failure(500, "Merged Run catalog validation failed: " + inspectError);
        if (QFileInfo(backupPath).isSymLink() || !backup.removeRecursively())
            return Failure(500, "Same-name raw files were replaced and other raw files retained, but old catalog metadata cleanup failed: " + backupPath);
    }
    manifest["replacement_cleanup_complete"] = true;
    if (!SaveJson(manifestPath, manifest))
        return Failure(500, "Previous Run cleanup completed, but its receipt could not be saved; retry this operation id.");
    ServiceResult result;
    result.json["replacement_cleanup_complete"] = true;
    result.json["experiment"] = experiment;
    result.json["run_number"] = run;
    result.json["operation_id"] = operationId;
    return result;
}
