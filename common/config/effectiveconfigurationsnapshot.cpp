#include "effectiveconfigurationsnapshot.h"
#include "configurationrange.h"
#include "../experiment/strictjson.h"
#include "../experiment/effectiveconfigurationrecord.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <stdexcept>

namespace {
const int MaxBytes=64*1024*1024;
void Require(bool condition,const QString &message) { if (!condition) throw std::invalid_argument(message.toStdString()); }
QString Hash(const QByteArray &bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()); }
QByteArray Value(const GetDAQ::ConfigurationValue &value) {
    return QByteArray::fromStdString(GetDAQ::NormalizeConfigurationValue(value));
}
QByteArray Range(const QString &text) {
    return QByteArray::fromStdString(GetDAQ::EncodeConfigurationRange(text.toStdString()));
}
GetDAQ::JsonValue Record(const QJsonObject &snapshot) {
    GetDAQ::JsonLimits limits; limits.stringBytes=1024*1024; limits.arrayItems=200000;
    return GetDAQ::ParseStrictJson(QJsonDocument(snapshot).toJson(QJsonDocument::Compact).toStdString(),limits);
}
QJsonObject Origin(const XcfgDocument &document,int index) {
    QJsonObject result;
    if (index>=0) { const auto &node=document.Nodes().at(index); result["file"]=node.file; result["path"]=node.path; result["line"]=node.line; }
    return result;
}
QPair<QString,QString> Digests(const QJsonObject &snapshot) {
    const auto result=GetDAQ::CheckEffectiveConfigurationRecord(Record(snapshot),false);
    return qMakePair(QString::fromStdString(result.effectiveSha256),QString::fromStdString(result.snapshotSha256));
}
void Check(const QJsonObject &snapshot) { GetDAQ::CheckEffectiveConfigurationRecord(Record(snapshot)); }
bool SaveNew(const QJsonObject &object,const QString &path,QString *error) {
    const auto bytes=QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (bytes.size()>MaxBytes) { *error="Snapshot/report exceeds 64 MiB"; return false; }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly|QIODevice::NewOnly)) { *error="Cannot create new snapshot/report: "+file.errorString(); return false; }
    if (file.write(bytes)!=bytes.size() || !file.flush()) {
        *error="Incomplete snapshot/report write: "+file.errorString(); file.remove(); return false;
    }
    file.close(); error->clear(); return true;
}
QMap<QString,QJsonObject> Indexed(const QJsonArray &array,const QString &key) {
    QMap<QString,QJsonObject> result; for (const auto &item:array) { const auto object=item.toObject(); result[object.value(key).toString()]=object; } return result;
}
}

bool EffectiveConfigurationSnapshot::Capture(const XcfgDocument &document,QJsonObject *snapshot,QString *error)
{
    return CaptureLoaded(document,true,snapshot,error);
}
bool EffectiveConfigurationSnapshot::CaptureLoaded(const XcfgDocument &document,bool verifySources,QJsonObject *snapshot,QString *error)
{
    try {
        QMap<QString,XcfgDocument::Node> values;
        if (verifySources ? !document.CaptureEffectiveTree(&values,error) : !document.EffectiveNodes(&values,error)) return false;
        QJsonObject result; result["schema_version"]=1; result["kind"]="getdaq.effective-configuration-snapshot"; result["root_file"]=document.RootFile();
        result["root_path"]=document.Nodes().at(document.Sources().value(document.RootFile()).root).path;
        QJsonArray sources,objects,scalars;
        for (auto it=document.Sources().constBegin();it!=document.Sources().constEnd();++it) {
            QJsonObject source; source["path"]=it.key(); source["sha256"]=Hash(it->bytes); source["bytes"]=QString::number(it->bytes.size()); sources.append(source);
        }
        for (auto it=values.constBegin();it!=values.constEnd();++it) {
            if (!it->scalar) { objects.append(it.key()); continue; }
            const auto &node=it.value(); XcfgDocument::Node resolved; XcfgDocument::ScalarProperties properties;
            if (!document.ResolveScalar(it.key(),&resolved,&properties,error)) return false;
            const auto parsed=GetDAQ::ParseConfigurationValue(node.value.toStdString(),node.attributes.value("type").toStdString());
            QJsonObject scalar; scalar["path"]=it.key(); scalar["value"]=node.value; scalar["declared_type"]=node.attributes.value("type");
            scalar["type"]=QString::fromStdString(GetDAQ::ConfigurationValueType(parsed)); scalar["normalized_value"]=QString::fromUtf8(Value(parsed));
            scalar["unit"]=properties.unit; scalar["range"]=properties.range;
            scalar["value_source"]=Origin(document,document.Lookup(it.key(),error)); scalar["unit_source"]=Origin(document,properties.unitSource); scalar["range_source"]=Origin(document,properties.rangeSource);
            scalars.append(scalar);
        }
        result["sources"]=sources; result["objects"]=objects; result["scalars"]=scalars; result["effective_sha256"]=""; result["snapshot_sha256"]="";
        const auto digests=Digests(result); result["effective_sha256"]=digests.first; result["snapshot_sha256"]=digests.second;
        if (QJsonDocument(result).toJson(QJsonDocument::Compact).size()>MaxBytes) { *error="Snapshot exceeds 64 MiB"; return false; }
        *snapshot=result; error->clear(); return true;
    } catch (const std::exception &e) { *error=QString::fromUtf8(e.what()); return false; }
}
bool EffectiveConfigurationSnapshot::Validate(const QJsonObject &snapshot,QString *error)
{
    try { Check(snapshot); error->clear(); return true; } catch (const std::exception &e) { *error=QString::fromUtf8(e.what()); return false; }
}
bool EffectiveConfigurationSnapshot::Load(const QString &path,QJsonObject *snapshot,QString *error)
{
    try {
        QFile file(path); Require(file.open(QIODevice::ReadOnly),"Cannot open snapshot: "+file.errorString());
        Require(file.size()<=MaxBytes,"Snapshot exceeds 64 MiB"); const auto bytes=file.read(MaxBytes+1);
        Require(file.error()==QFile::NoError,"Cannot read snapshot");
        GetDAQ::DecodeEffectiveConfigurationRecord(bytes.toStdString()); // 共享核心先核对语义、重复键和两个摘要。
        const auto json=QJsonDocument::fromJson(bytes); Require(json.isObject(),"Snapshot must be a JSON object");
        const auto result=json.object(); *snapshot=result; error->clear(); return true;
    } catch (const std::exception &e) { *error=QString::fromUtf8(e.what()); return false; }
}
bool EffectiveConfigurationSnapshot::Save(const QJsonObject &snapshot,const QString &path,QString *error)
{
    if (!Validate(snapshot,error)) return false;
    return SaveNew(snapshot,path,error);
}
bool EffectiveConfigurationSnapshot::Compare(const QJsonObject &before,const QJsonObject &after,QJsonObject *report,QString *error)
{
    if (!Validate(before,error) || !Validate(after,error)) return false;
    QJsonObject result; result["schema_version"]=1; result["kind"]="getdaq.effective-configuration-review";
    result["before_effective_sha256"]=before.value("effective_sha256"); result["after_effective_sha256"]=after.value("effective_sha256");
    result["before_snapshot_sha256"]=before.value("snapshot_sha256"); result["after_snapshot_sha256"]=after.value("snapshot_sha256");
    result["effective_changed"]=before.value("effective_sha256")!=after.value("effective_sha256"); result["snapshot_changed"]=before.value("snapshot_sha256")!=after.value("snapshot_sha256");
    result["root_changed"]=before.value("root_file")!=after.value("root_file") || before.value("root_path")!=after.value("root_path");
    QSet<QString> beforeObjects,afterObjects;
    for (const auto &object:before.value("objects").toArray()) beforeObjects.insert(object.toString());
    for (const auto &object:after.value("objects").toArray()) afterObjects.insert(object.toString());
    auto objectPaths=(beforeObjects|afterObjects).values(); objectPaths.sort(); QJsonArray objectChanges;
    for (const auto &path:objectPaths) if (beforeObjects.contains(path)!=afterObjects.contains(path)) {
        QJsonObject change; change["path"]=path; change["change"]=beforeObjects.contains(path) ? "removed" : "added"; objectChanges.append(change);
    }
    result["object_changes"]=objectChanges;
    const auto a=Indexed(before.value("scalars").toArray(),"path"),b=Indexed(after.value("scalars").toArray(),"path");
    QSet<QString> paths; for (auto it=a.constBegin();it!=a.constEnd();++it) paths.insert(it.key()); for (auto it=b.constBegin();it!=b.constEnd();++it) paths.insert(it.key());
    auto sorted=paths.values(); sorted.sort(); QJsonArray changes;
    for (const auto &path:sorted) {
        QJsonObject change; change["path"]=path;
        if (!a.contains(path) || !b.contains(path)) change["change"]=a.contains(path) ? "removed" : "added";
        else {
            QJsonArray fields;
            if (a[path].value("type")!=b[path].value("type") || a[path].value("normalized_value")!=b[path].value("normalized_value")) fields.append("value");
            if (a[path].value("value")!=b[path].value("value") || a[path].value("declared_type")!=b[path].value("declared_type")) fields.append("text_or_declared_type");
            if (a[path].value("unit")!=b[path].value("unit")) fields.append("unit");
            if (Range(a[path].value("range").toString())!=Range(b[path].value("range").toString())) fields.append("range");
            else if (a[path].value("range")!=b[path].value("range")) fields.append("range_text");
            for (const QString &key:QStringList{"value_source","unit_source","range_source"}) if (a[path].value(key)!=b[path].value(key)) fields.append(key);
            if (fields.isEmpty()) continue;
            change["change"]="changed"; change["fields"]=fields;
        }
        if (a.contains(path)) change["before"]=a[path];
        if (b.contains(path)) change["after"]=b[path];
        changes.append(change);
    }
    result["scalar_changes"]=changes;
    const auto sa=Indexed(before.value("sources").toArray(),"path"),sb=Indexed(after.value("sources").toArray(),"path");
    paths.clear(); for (auto it=sa.constBegin();it!=sa.constEnd();++it) paths.insert(it.key()); for (auto it=sb.constBegin();it!=sb.constEnd();++it) paths.insert(it.key());
    sorted=paths.values(); sorted.sort(); QJsonArray sourceChanges;
    for (const auto &path:sorted) {
        if (sa.contains(path) && sb.contains(path) && sa[path]==sb[path]) continue;
        QJsonObject change; change["path"]=path; change["change"]=!sa.contains(path) ? "added" : !sb.contains(path) ? "removed" : "changed";
        if (sa.contains(path)) change["before"]=sa[path];
        if (sb.contains(path)) change["after"]=sb[path];
        sourceChanges.append(change);
    }
    result["source_changes"]=sourceChanges;
    if (QJsonDocument(result).toJson(QJsonDocument::Compact).size()>MaxBytes) { *error="Configuration review exceeds 64 MiB"; return false; }
    *report=result; error->clear(); return true;
}
bool EffectiveConfigurationSnapshot::SaveReport(const QJsonObject &report,const QString &path,QString *error)
{
    return SaveNew(report,path,error);
}
