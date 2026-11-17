#include "runrecorder.h"
#include "runrecorder_p.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QDateTime>
#include <QTemporaryDir>
#include <QUuid>

namespace {
QString Absolute(const QString &path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool SameRunIdentity(const RunConfig &saved, const RunConfig &current)
{
    if (saved.experiment != current.experiment || saved.runNumber != current.runNumber ||
        saved.simulation != current.simulation || saved.nativeEcc != current.nativeEcc ||
        Absolute(saved.configDir) != Absolute(current.configDir) ||
        Absolute(saved.outputDir) != Absolute(current.outputDir) ||
        saved.nodes.size() != current.nodes.size()) return false;

    for (int i = 0; i < saved.nodes.size(); ++i) {
        const NodeConfig &x = saved.nodes[i], &y = current.nodes[i];
        if (x.id != y.id || x.mutant != y.mutant || x.host != y.host || x.eccPort != y.eccPort ||
            x.sentryPort != y.sentryPort || x.routerPort != y.routerPort || x.exporterPort != y.exporterPort ||
            x.routerHost != y.routerHost || x.exporterHost != y.exporterHost || x.sentryHost != y.sentryHost ||
            x.describeId != y.describeId || x.prepareId != y.prepareId || x.configureId != y.configureId ||
            x.soapInlineXml != y.soapInlineXml) return false;
    }
    return true;
}

bool InspectCompleteRun(const QString &directory, const RunConfig &expected, QString *error)
{
    const QFileInfo root(directory);
    const QString runConfigPath = QDir(directory).filePath("run.yml");
    const QString journalPath = QDir(directory).filePath("journal.json");
    const QString csvPath = QDir(directory).filePath("run.csv");
    if (root.isSymLink() || !root.isDir() || QFileInfo(runConfigPath).isSymLink() || !QFileInfo(runConfigPath).isFile() ||
        QFileInfo(journalPath).isSymLink() || !QFileInfo(journalPath).isFile() ||
        QFileInfo(csvPath).isSymLink() || !QFileInfo(csvPath).isFile()) {
        *error = "Existing Run is not a complete regular directory; preserve it.";
        return false;
    }

    RunConfig saved;
    QString loadError;
    if (!saved.Load(runConfigPath, &loadError) || !SameRunIdentity(saved, expected)) {
        *error = "Existing Run configuration identity differs; preserve it.";
        return false;
    }
    QFile journalFile(journalPath);
    if (!journalFile.open(QIODevice::ReadOnly) || journalFile.size() > 1024 * 1024) {
        *error = "Existing Run journal cannot be safely inspected; preserve it.";
        return false;
    }
    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(journalFile.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject() ||
        document.object().value("phase").toString() != "Complete") {
        *error = "Existing Run is not verified Complete; preserve it.";
        return false;
    }
    return true;
}
}

bool RunRecorder::Reserve(const RunConfig &c, QString *error)
{
    return Reserve(c, false, error);
}

bool RunRecorder::Reserve(const RunConfig &c, bool replaceCompletedRun, QString *error)
{
    const QFileInfo path(c.RunDirectory());
    if (!QDir().mkpath(path.absolutePath())) {
        *error = "Run directory already exists or cannot be created: " + path.filePath(); return false;
    }
    const bool targetPresent = path.exists() || path.isSymLink();
    QString backupName;
    QString retainedReason;
    if (replaceCompletedRun && !targetPresent) {
        *error = "The Run selected for replacement no longer exists; cancel and review the target again.";
        return false;
    }
    if (targetPresent) {
        if (!replaceCompletedRun) {
            *error = "Run directory already exists: " + path.filePath(); return false;
        }
        if (!InspectCompleteRun(path.filePath(), c, error)) {
            // Legacy SOAP users can explicitly reuse an interrupted/warning Run.
            // Retain it intact; never treat it as a verified Complete backup to delete.
            if(c.nativeEcc || path.isSymLink() || !path.isDir())return false;
            retainedReason=*error;error->clear();
        }
        backupName = "." + path.fileName() + ".replaced-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (QFileInfo(QDir(path.absolutePath()).filePath(backupName)).exists()) {
            *error = "Cannot reserve a unique replacement backup path; preserve existing Run."; return false;
        }
    }
    // Publish a reservation only after both the run configuration and initial
    // journal are complete. The staging directory is a sibling so rename stays
    // on the same filesystem. An explicitly approved replacement first retains
    // the old Complete Run under a private sibling name.
    QTemporaryDir staging(QDir(path.absolutePath()).filePath("." + path.fileName() + ".pending-XXXXXX"));
    if (!staging.isValid()) {
        *error = "Cannot create private Run reservation staging directory: " + staging.errorString();
        return false;
    }
    if (!c.Save(QDir(staging.path()).filePath("run.yml"), error)) return false;
    QJsonObject journal;
    journal["phase"] = "Reserved";
    journal["started_utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    journal["catalog"] = QJsonObject();
    if (!backupName.isEmpty()) journal["replacement_backup"] = backupName;
    if(!retainedReason.isEmpty()) {
        journal["replacement_backup_policy"]="retain";
        journal["retained_previous_run"]=backupName;
        journal["previous_run_inspection_warning"]=retainedReason;
    }
    if (!RunRecorderInternal::WriteRunFile(QDir(staging.path()).filePath("journal.json"), QJsonDocument(journal).toJson(), error)) return false;

    QDir parent(path.absolutePath());
    bool movedOldRun = false;
    if (!backupName.isEmpty()) {
        if (!parent.rename(path.fileName(), backupName)) {
            *error = "Cannot retain the old Run before replacement; preserve it.";
            return false;
        }
        movedOldRun = true;
    }
    if (!parent.rename(QFileInfo(staging.path()).fileName(), path.fileName())) {
        if (movedOldRun && !parent.rename(backupName, path.fileName()))
            *error = "Cannot publish the new Run or restore the previous Run; both directories were preserved for inspection.";
        else
            *error = "Cannot publish reservation; the previous Run was restored.";
        return false;
    }
    return true;
}

bool RunRecorder::FinalizeReplacement(const RunConfig &c, QString *error)
{
    QJsonObject currentJournal;
    if (!ReadJournal(c, &currentJournal, error)) return false;
    if (currentJournal.value("phase").toString() != "Complete") {
        *error = "Cannot discard the previous Run before the replacement is Complete.";
        return false;
    }
    const QString backupName = currentJournal.value("replacement_backup").toString();
    if (backupName.isEmpty()) return true;
    const QFileInfo target(c.RunDirectory());
    if (QFileInfo(backupName).fileName() != backupName || backupName.contains('\\') ||
        !backupName.startsWith("." + target.fileName() + ".replaced-")) {
        *error = "Replacement backup path is invalid; preserve it.";
        return false;
    }
    if(currentJournal.value("replacement_backup_policy").toString()=="retain") {
        currentJournal["retained_previous_run"]=backupName;
        currentJournal["replacement_cleanup_skipped"]=true;
        return WriteJournal(c,currentJournal,error);
    }
    const QString backupPath = QDir(target.absolutePath()).filePath(backupName);
    const QFileInfo backupInfo(backupPath);
    if (!backupInfo.exists()) return true; // Idempotent after completed cleanup.
    if (!InspectCompleteRun(backupPath, c, error)) return false;
    QDir backup(backupPath);
    if (backupInfo.isSymLink() || !backup.removeRecursively()) {
        *error = "Replacement is Complete, but previous Run cleanup failed; inspect " + backupPath;
        return false;
    }
    currentJournal.remove("replacement_backup");
    if (!WriteJournal(c, currentJournal, error)) {
        *error = "Previous Run was removed, but cleanup completion could not be recorded: " + *error;
        return false;
    }
    return true;
}

