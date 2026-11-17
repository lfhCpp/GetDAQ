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



QString RunRecorder::Finish(const RunConfig &c, double seconds)
{
    const QDir run(c.RunDirectory());
    QString error;
    if (!c.nativeEcc && !QFileInfo::exists(run.filePath("backup.complete"))) {
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


