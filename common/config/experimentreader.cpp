#include "experimentreader.h"
#include "configurationrange.h"
#include <QCryptographicHash>
#include <stdexcept>
ExperimentReader::ExperimentReader(const XcfgDocument &d) : document(d) {
    // The editor's document model intentionally preserves unknown attributes,
    // but the legacy CompoundConfig XML reader rejects them before exposing a
    // configuration. Keep that strict boundary in every hardware compiler,
    // including attributes on branches that a particular compiler doesn't
    // otherwise query.
    static const char *const scalarAttributes[] = {"id", "type", "unit", "range"};
    static const char *const objectAttributes[] = {"id", "include"};
    for (const auto &node : document.Nodes()) {
        const char *const *allowed = node.scalar ? scalarAttributes : objectAttributes;
        const int allowedCount = node.scalar ? 4 : 2;
        for (auto it = node.attributes.constBegin(); it != node.attributes.constEnd(); ++it) {
            bool supported = false;
            for (int i = 0; i < allowedCount; ++i)
                if (it.key() == QLatin1String(allowed[i])) { supported = true; break; }
            if (!supported) {
                const QString kind = node.scalar ? "scalar" : "object";
                throw std::runtime_error(("Unsupported CompoundConfig " + kind + " attribute '" + it.key() + "' at " + node.path
                                          + " (" + node.file + ":" + QString::number(node.line) + ")").toStdString());
            }
        }
    }
}
QString ExperimentReader::Scalar(const QString &path, const QString &fallback, const QString &unit, bool allowHex, bool allowReal) {
    QString error; const int index = document.Lookup(path, &error);
    QJsonObject entry; entry["query_path"] = path;
    if (index < 0) {
        if (fallback.isNull() || !error.startsWith("No declared instance/value for ")) throw std::runtime_error(error.toStdString());
        entry["value"] = fallback; entry["default_from_code"] = true; parameters.append(entry); return fallback;
    }
    XcfgDocument::Node node; XcfgDocument::ScalarProperties properties;
    if (!document.ResolveScalar(path,&node,&properties,&error)) throw std::runtime_error(error.toStdString());
    const QString declaredUnit = node.attributes.value("unit");
    if (!declaredUnit.isEmpty() && declaredUnit != unit) throw std::runtime_error(("Unsupported unit at " + path + ": " + declaredUnit).toStdString());
    const QString type = node.attributes.value("type");
    if (!type.isEmpty() && type != "string" && type != "int" && type != "bool" && !(allowHex && type == "hex") && !(allowReal && type == "real"))
        throw std::runtime_error(("Unsupported scalar type at " + path + ": " + type).toStdString());
    QString valueError;
    if (!XcfgDocument::CheckValue(node, node.value, &valueError))
        throw std::runtime_error((path + ": " + valueError).toStdString());
    entry["value"] = node.value; entry["default_from_code"] = false;
    entry["source_path"] = node.path; entry["source_file"] = node.file; entry["line"] = node.line;
    QJsonObject attributes; for (auto it = node.attributes.constBegin(); it != node.attributes.constEnd(); ++it) attributes[it.key()] = it.value();
    entry["attributes"] = attributes;
    if (node.attributes!=document.Nodes().at(index).attributes || (properties.unitSource>=0 && properties.unitSource!=index) || (properties.rangeSource>=0 && properties.rangeSource!=index)) {
        QJsonObject shared; shared["class_path"]=properties.classPath;
        const auto origin=[&](int source) -> QJsonObject {
            QJsonObject result;
            if (source>=0) { const auto &declaration=document.Nodes().at(source); result["source_file"]=declaration.file; result["source_path"]=declaration.path; result["line"]=declaration.line; }
            return result;
        };
        shared["unit"]=origin(properties.unitSource); shared["range"]=origin(properties.rangeSource);
        entry["shared_metadata"]=shared;
    }
    QString compiled = allowHex && type == "hex" && !node.value.startsWith("0x", Qt::CaseInsensitive) ? "0x" + node.value : node.value;
    const auto scalar=GetDAQ::ParseConfigurationValue(node.value.toStdString(),type.toStdString());
    if (scalar.type==GetDAQ::ConfigurationValue::Boolean) compiled=scalar.boolean ? "true" : "false";
    if (compiled != node.value) entry["compiled_value"] = compiled;
    parameters.append(entry); return compiled;
}
bool ExperimentReader::Boolean(const QString &path, const QString &fallback) {
    const QString value = Scalar(path, fallback);
    if (value == "true") return true;
    if (value == "false") return false;
    throw std::runtime_error(("Expected true or false: " + path).toStdString());
}
QStringList ExperimentReader::Children(const QString &path, const QString &name) {
    QString error; const auto children = document.Children(path, name, &error);
    if (!error.isEmpty()) throw std::runtime_error(error.toStdString());
    return children;
}
QString ExperimentReader::CoBoPath(const QString &id)
{
    if (id.isEmpty() || id == "*" || id.toUtf8().size() > 128 || id.contains(QChar(0))
        || id.contains('[') || id.contains(']') || id.contains('/'))
        throw std::runtime_error("CoBo instance ID must be a concrete path-safe ID of 1..128 UTF-8 bytes");
    if (!document.Sources().contains(document.RootFile())) throw std::runtime_error("No experiment configuration loaded");
    const auto &root = document.Nodes().at(document.Sources().value(document.RootFile()).root);
    if (root.name != "Setup") throw std::runtime_error("Expected GET experiment Setup root");
    const QString path = root.path + "/Node[CoBo]/Instance[" + id + "]";
    if (!Boolean(path + "/isActive")) throw std::runtime_error("Selected CoBo must be active");
    return path;
}
QJsonArray ExperimentReader::Sources() const
{
    QJsonArray sources;
    for (auto it = document.Sources().constBegin(); it != document.Sources().constEnd(); ++it) {
        QJsonObject source; source["path"] = it.key();
        source["sha256"] = QString::fromLatin1(QCryptographicHash::hash(it->bytes, QCryptographicHash::Sha256).toHex()); sources.append(source);
    }
    return sources;
}
QString ExperimentReader::MutantPath(const QString &id)
{
    if (id.isEmpty() || id == "*" || id.toUtf8().size() > 128 || id.contains(QChar(0))
        || id.contains('[') || id.contains(']') || id.contains('/'))
        throw std::runtime_error("MuTaNT instance ID must be a concrete path-safe ID of 1..128 UTF-8 bytes");
    if (!document.Sources().contains(document.RootFile())) throw std::runtime_error("No experiment configuration loaded");
    const auto &root = document.Nodes().at(document.Sources().value(document.RootFile()).root);
    if (root.name != "Setup") throw std::runtime_error("Expected GET experiment Setup root");
    const QString node = root.path + "/Node[Mutant]", path = node + "/Instance[" + id + "]";
    // MuTaNT没有CoBo的isActive字段；默认声明本身也不能凭空生成实体。
    if (!Children(root.path,"Node").contains(node) || !Children(node,"Instance").contains(path))
        throw std::runtime_error("Selected MuTaNT instance must be explicitly declared");
    return path;
}
