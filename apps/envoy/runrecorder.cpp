#include "runrecorder.h"
#include "runrecorder_p.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QDateTime>

namespace {
QString Csv(QString value) { value.replace('"', "\"\""); return '"' + value + '"'; }
}

bool RunRecorder::WriteJournal(const RunConfig &c, const QJsonObject &journal, QString *error)
{
    return RunRecorderInternal::WriteRunFile(QDir(c.RunDirectory()).filePath("journal.json"), QJsonDocument(journal).toJson(), error);
}

bool RunRecorder::ReadJournal(const RunConfig &c, QJsonObject *journal, QString *error)
{
    QFile file(QDir(c.RunDirectory()).filePath("journal.json"));
    if (!file.open(QIODevice::ReadOnly)) { *error = file.errorString(); return false; }
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (!doc.isObject() || parse.error != QJsonParseError::NoError) { *error = "Invalid run journal."; return false; }
    *journal = doc.object();
    return true;
}

bool RunRecorder::WriteNativeReferences(const RunConfig &c,
                                       const QByteArray &receiverStatus,
                                       const QByteArray &eccIdentity,
                                       const QByteArray &eccConfiguration,
                                       bool includeCatalogReference,
                                       QString *error)
{
    const QDir run(c.RunDirectory());
    if (!RunRecorderInternal::WriteRunFile(run.filePath("receiver-status.json"), receiverStatus, error) ||
        !RunRecorderInternal::WriteRunFile(run.filePath("ecc-identity.json"), eccIdentity, error)) {
        *error = "Cannot save native seal/session references: " + *error;
        return false;
    }
    if (!eccConfiguration.isEmpty() &&
        !RunRecorderInternal::WriteRunFile(run.filePath("ecc-configuration.json"), eccConfiguration, error)) {
        *error = "Cannot save native startup configuration reference: " + *error;
        return false;
    }
    if (includeCatalogReference &&
        !RunRecorderInternal::WriteRunFile(run.filePath("data-catalogs.json"), receiverStatus, error)) {
        *error = "Cannot save native data-host catalog references: " + *error;
        return false;
    }
    return true;
}

QString RunRecorder::Finish(const RunConfig &c, double seconds)
{
    const QDir run(c.RunDirectory());
    QString error;
    // 模拟运行不会访问或复制真实 GET 配置。
    if (!c.nativeEcc && !c.simulation && !QFileInfo::exists(run.filePath("backup.complete"))) {
        bool missingFile=false;
        if (!RunRecorderInternal::SnapshotConfiguration(c, &error, &missingFile)) {
            if(!missingFile)return error;
            const QJsonObject warning{{"status","missing"},{"message",error},{"configuration_directory",c.configDir}};
            if(!RunRecorderInternal::WriteRunFile(run.filePath("configuration-backup-warning.json"),QJsonDocument(warning).toJson(),&error))return error;
        } else if(QFileInfo::exists(run.filePath("configuration-backup-warning.json")) &&
                  !QFile::remove(run.filePath("configuration-backup-warning.json"))) {
            return "Cannot remove the previous configuration backup warning after successful backup.";
        }
    }
    QStringList header = {"Run", "Note", "Duration"};
    QStringList row = {QString::number(c.runNumber), c.description, QString::number(seconds, 'f', 3)};
    for (auto it = c.fields.constBegin(); it != c.fields.constEnd(); ++it) { header.append(it.key()); row.append(it.value()); }
    for (QString &v : header) v = Csv(v);
    for (QString &v : row) v = Csv(v);
    // 每个运行独立 CSV，不会因新增实验字段破坏历史表头。
    if (!RunRecorderInternal::WriteRunFile(run.filePath("run.csv"), (header.join(',') + "\n" + row.join(',') + "\n").toUtf8(), &error)) return error;
    return QString();
}

bool RunRecorder::CompleteNativeRun(const RunConfig &c,
                                    const QByteArray &receiverStatus,
                                    const QByteArray &eccIdentity,
                                    const QByteArray &eccConfiguration,
                                    bool includeCatalogReference,
                                    double seconds,
                                    QJsonObject *journal,
                                    QString *error,
                                    QString *replacementCleanupWarning)
{
    if (!journal || !error || !replacementCleanupWarning) return false;
    replacementCleanupWarning->clear();
    if (!WriteNativeReferences(c, receiverStatus, eccIdentity, eccConfiguration,
                               includeCatalogReference, error)) return false;
    *error = Finish(c, seconds);
    if (!error->isEmpty()) return false;
    (*journal)["phase"] = "Complete";
    (*journal)["updated_utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if (!WriteJournal(c, *journal, error)) return false;
    if (journal->value("replace_completed_run").toBool() &&
        !FinalizeReplacement(c, replacementCleanupWarning)) {
        // Complete is already durable. Preserve the backup and report cleanup
        // as a warning rather than turning a finished Run into a failed one.
        return true;
    }
    return true;
}
