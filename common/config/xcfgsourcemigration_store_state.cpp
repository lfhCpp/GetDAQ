#include "xcfgsourcemigration.h"
#include "xcfgsourcemigration_p.h"
#include "xcfgsavejournal.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

using namespace XcfgSourceMigrationInternal;

namespace {
QString Marker(const QString &path)
{
    return QFileInfo(path).absolutePath() + "/.getdaq-workspace-save-" + Hash(path.toUtf8()) + ".json";
}

QByteArray MarkerBytes(const QString &path, const QString &directory, const QString &key)
{
    return QJsonDocument(QJsonObject{{"schema", 1}, {"kind", "getdaq.workspace-save-marker"},
        {"source", path}, {"directory", directory}, {"manifest_sha256", key}})
        .toJson(QJsonDocument::Compact);
}
} // namespace

namespace XcfgSourceMigrationInternal {
bool RecordCompleted(const Record &record)
{
    const auto path = record.directory + "/complete.json";
    if (!QFileInfo::exists(path)) return false;
    const auto value = ParseRecordObject(ReadRecordFile(path, 4096));
    RequireRecordFields(value, {"schema", "kind", "manifest_sha256", "outcome"});
    Require(value["schema"].toDouble() == 1 &&
            RecordText(value, "kind") == "getdaq.workspace-save-complete" &&
            RecordText(value, "manifest_sha256") == record.key,
            "Migration completion binding differs");
    const auto outcome = RecordText(value, "outcome");
    Require(outcome == "candidate" || outcome == "original",
            "Migration completion outcome differs");
    return true;
}

void CheckRecordMarker(const Record &record, const QString &path, bool required)
{
    const auto marker = Marker(path);
    if (!QFileInfo::exists(marker)) {
        Require(!required, "Migration source marker disappeared");
        return;
    }
    Require(ReadRecordFile(marker, 8192) == MarkerBytes(path, record.directory, record.key),
            "Migration source belongs to a different record");
}

void InstallRecordMarkers(const Record &record, bool initial)
{
    for (const auto &path : Paths(record.before, record.after)) {
        const auto marker = Marker(path);
        const auto expected = MarkerBytes(path, record.directory, record.key);
        if (QFileInfo::exists(marker) && ReadRecordFile(marker, 8192) != expected) {
            QString pending, error;
            Require(initial && XcfgSaveJournal::Pending(path, &pending, &error) && pending.isEmpty(),
                    "Migration source belongs to another pending save");
            Require(QFile::remove(marker), "Cannot retire completed source marker");
            SyncRecordDirectory(QFileInfo(path).absolutePath());
        }
        if (!QFileInfo::exists(marker)) WriteNewRecordFile(marker, expected);
        CheckRecordMarker(record, path, true);
    }
}

void CheckRecordState(const Record &record, const QMap<QString, QByteArray> &observed)
{
    GuardRecordDirectory(record.directory);
    Require(ReadRecordFile(record.directory + "/manifest.json", 2 * 1024 * 1024) == record.manifest &&
            !RecordCompleted(record), "Migration record changed or already completed");
    for (const auto &path : Paths(record.before, record.after))
        CheckRecordMarker(record, path, true);
    Require(ObserveRecord(record) == observed, "Migration disk state changed after review");
}

void FinishRecord(const Record &record, bool candidate)
{
    const auto &target = candidate ? record.after : record.before;
    CheckRecordState(record, target);
    const auto completion = QJsonDocument(QJsonObject{{"schema", 1},
        {"kind", "getdaq.workspace-save-complete"}, {"manifest_sha256", record.key},
        {"outcome", candidate ? "candidate" : "original"}}).toJson(QJsonDocument::Compact);
    WriteNewRecordFile(record.directory + "/complete.json", completion);
    for (const auto &path : Paths(record.before, record.after)) {
        CheckRecordMarker(record, path, true);
        Require(QFile::remove(Marker(path)), "Migration completed but source marker cleanup failed");
        SyncRecordDirectory(QFileInfo(path).absolutePath());
    }
}
} // namespace XcfgSourceMigrationInternal
