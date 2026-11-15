#include "sentrycatalog_p.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QDebug>

namespace SentryCatalogInternal {
ServiceResult Failure(int code, const QString &error)
{ ServiceResult r; r.code = code; r.json["error"] = error; return r; }

bool SaveJson(const QString &path, const QJsonObject &object)
{
    QSaveFile file(path);
    const QByteArray data = QJsonDocument(object).toJson();
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot open JSON transaction file for replacement:" << path << file.errorString();
        return false;
    }
    if (file.write(data) != data.size()) {
        qWarning() << "Cannot write JSON transaction file:" << path << file.errorString();
        return false;
    }
    if (!file.commit()) {
        qWarning() << "Cannot commit JSON transaction file:" << path << file.errorString();
        return false;
    }
    return true;
}

bool CompleteLegacyCatalog(const QString &directory, const QString &experiment, int run, QString *error,
                           const QSet<QString> &transferredFiles)
{
    const QFileInfo root(directory);
    const QString manifestPath = QDir(directory).filePath("catalog.json");
    QFile manifestFile(manifestPath);
    if (root.isSymLink() || !root.isDir() || QFileInfo(manifestPath).isSymLink() ||
        !manifestFile.open(QIODevice::ReadOnly) || manifestFile.size() > 1024 * 1024) {
        *error = "Existing legacy catalog is not a bounded regular directory; preserve it.";
        return false;
    }
    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll(), &parse);
    manifestFile.close();
    const QJsonObject object = document.object();
    if (parse.error != QJsonParseError::NoError || !document.isObject() ||
        object.value("status").toString() != "complete" ||
        object.value("experiment").toString() != experiment ||
        !object.value("run_number").isDouble() || object.value("run_number").toInt(-1) != run ||
        !object.value("planned").isArray() || !object.value("moved").isArray()) {
        *error = "Existing legacy catalog identity or completion manifest differs; preserve it.";
        return false;
    }
    const QJsonArray planned = object.value("planned").toArray();
    const QJsonArray moved = object.value("moved").toArray();
    if (planned.size() != moved.size()) {
        *error = "Existing legacy catalog is incomplete; preserve it.";
        return false;
    }
    QSet<QString> expected;
    for (const QJsonValue &entry : planned) {
        if (!entry.isString()) { *error = "Existing legacy catalog has a malformed file list; preserve it."; return false; }
        const QString name = entry.toString();
        if (name.isEmpty() || QFileInfo(name).fileName() != name || !name.endsWith(".graw") || expected.contains(name)) {
            *error = "Existing legacy catalog has an unsafe file name; preserve it."; return false;
        }
        expected.insert(name);
    }
    QSet<QString> movedNames;
    for (const QJsonValue &entry : moved) {
        if (!entry.isString() || !expected.contains(entry.toString()) || movedNames.contains(entry.toString())) {
            *error = "Existing legacy catalog move list differs; preserve it."; return false;
        }
        movedNames.insert(entry.toString());
    }
    if (movedNames != expected) { *error = "Existing legacy catalog move list is incomplete; preserve it."; return false; }
    const QFileInfoList contents = QDir(directory).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : contents) {
        if (entry.isSymLink() || !entry.isFile() ||
            (entry.fileName() != "catalog.json" && entry.fileName() != "receiver-seal.json" && !expected.contains(entry.fileName()))) {
            *error = "Existing legacy catalog contains an unexpected or nonregular entry; preserve it.";
            return false;
        }
    }
    for (const QString &name : expected) {
        const QFileInfo file(QDir(directory).filePath(name));
        if (!file.exists() && transferredFiles.contains(name)) continue;
        if (file.isSymLink() || !file.isFile()) {
            *error = "Existing legacy catalog is missing a regular raw file; preserve it.";
            return false;
        }
    }
    const QString sealPath = QDir(directory).filePath("receiver-seal.json");
    if (QFileInfo::exists(sealPath) && (QFileInfo(sealPath).isSymLink() || !QFileInfo(sealPath).isFile())) {
        *error = "Existing legacy catalog seal is not a regular file; preserve it.";
        return false;
    }
    return true;
}
}
