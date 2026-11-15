#include "xcfgsourcemigration.h"
#include "xcfgsourcemigration_p.h"
#include "strictjson.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

using namespace XcfgSourceMigrationInternal;

QJsonObject XcfgSourceMigrationInternal::ParseRecordObject(const QByteArray &bytes)
{
    GetDAQ::ParseStrictJson(bytes.toStdString());
    const auto document = QJsonDocument::fromJson(bytes);
    Require(document.isObject(), "Migration metadata is not a JSON object");
    return document.object();
}

void XcfgSourceMigrationInternal::RequireRecordFields(const QJsonObject &value,
                                                       const QStringList &fields)
{
    Require(value.size() == fields.size(), "Migration metadata field count differs");
    for (const auto &field : fields)
        Require(value.contains(field), "Migration metadata field missing: " + field);
}

QString XcfgSourceMigrationInternal::RecordText(const QJsonObject &value, const QString &key)
{
    Require(value[key].isString(), "Migration metadata must contain a string: " + key);
    return value[key].toString();
}

namespace {
QStringList Roots(const QJsonValue &value)
{
    Require(value.isArray() && !value.toArray().isEmpty() && value.toArray().size() <= 16,
            "Migration root budget/type differs");
    QStringList result;
    for (const auto &row : value.toArray()) {
        Require(row.isString() && !result.contains(row.toString()),
                "Migration root type/uniqueness differs");
        result.append(row.toString());
    }
    return result;
}

void CheckMoves(const Record &record)
{
    Require(!record.moves.isEmpty() && record.moves.size() <= 256 &&
            record.before.size() == record.after.size(), "Migration mapping budget differs");
    QStringList destinations;
    for (auto it = record.moves.constBegin(); it != record.moves.constEnd(); ++it) {
        Require(record.before.contains(it.key()) && !record.after.contains(it.key()) &&
                !record.before.contains(it.value()) && record.after.contains(it.value()),
                "Migration must replace old paths with new absent-before paths");
        Require(!destinations.contains(it.value()), "Duplicate migration destination");
        destinations.append(it.value());
    }
    for (const auto &path : record.before.keys())
        Require(record.after.contains(record.moves.value(path, path)),
                "Migration source mapping differs");
    Require(record.beforeRoots.size() == record.afterRoots.size(), "Migration root count differs");
    for (int i = 0; i < record.beforeRoots.size(); ++i)
        Require(record.before.contains(record.beforeRoots[i]) &&
                record.afterRoots[i] == record.moves.value(record.beforeRoots[i], record.beforeRoots[i]),
                "Migration root mapping differs");
}
} // namespace

namespace XcfgSourceMigrationInternal {
Record LoadRecord(const QString &directory)
{
    GuardRecordDirectory(directory);
    GuardRecordDirectory(directory + "/original");
    GuardRecordDirectory(directory + "/candidate");

    Record record;
    record.directory = directory;
    record.manifest = ReadRecordFile(directory + "/manifest.json", 2 * 1024 * 1024);
    record.key = Hash(record.manifest);
    const auto manifest = ParseRecordObject(record.manifest);
    RequireRecordFields(manifest, {"schema", "kind", "roots_before", "roots_after", "moves", "files"});
    Require(manifest["schema"].toDouble() == 1 &&
            RecordText(manifest, "kind") == "getdaq.source-path-migration",
            "Unsupported path migration record");
    record.beforeRoots = Roots(manifest["roots_before"]);
    record.afterRoots = Roots(manifest["roots_after"]);
    Require(manifest["moves"].isArray() && manifest["moves"].toArray().size() <= 256 &&
            manifest["files"].isArray() && manifest["files"].toArray().size() <= 512,
            "Migration metadata array budget/type differs");

    for (const auto &item : manifest["moves"].toArray()) {
        Require(item.isObject(), "Migration mapping is not an object");
        const auto row = item.toObject();
        RequireRecordFields(row, {"from", "to"});
        const auto from = RecordText(row, "from");
        const auto to = RecordText(row, "to");
        Require(!record.moves.contains(from), "Duplicate migration source");
        record.moves[from] = to;
    }

    QStringList known;
    int index = 0;
    for (const auto &item : manifest["files"].toArray()) {
        Require(item.isObject(), "Migration source row is not an object");
        const auto row = item.toObject();
        RequireRecordFields(row, {"path", "before_exists", "after_exists", "before_sha256", "after_sha256"});
        const auto path = RecordText(row, "path");
        Require(Slot(path) == path && !known.contains(path),
                "Migration source path is not unique/canonical");
        known.append(path);
        Require(row["before_exists"].isBool() && row["after_exists"].isBool() &&
                (row["before_exists"].toBool() || row["after_exists"].toBool()),
                "Migration source existence differs");
        for (bool before : {true, false}) {
            const auto exists = before ? "before_exists" : "after_exists";
            const auto hash = before ? "before_sha256" : "after_sha256";
            const auto expected = RecordText(row, hash);
            if (row[exists].toBool()) {
                const auto payload = ReadRecordFile(directory +
                    (before ? "/original/" : "/candidate/") + QString::number(index) + ".xcfg");
                Require(Hash(payload) == expected, "Migration stored payload hash differs");
                (before ? record.before : record.after)[path] = payload;
            } else {
                Require(expected.isEmpty(), "Absent migration source cannot have a payload hash");
            }
        }
        ++index;
    }
    Budget(record.before);
    Budget(record.after);
    CheckMoves(record);
    return record;
}
} // namespace XcfgSourceMigrationInternal
