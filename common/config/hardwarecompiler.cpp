#include "hardwarecompiler.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <stdexcept>
#include <limits>
namespace {
class Compiler {
public:
    const XcfgDocument &document;
    QJsonArray warnings;
    QSet<QString> warned;
    explicit Compiler(const XcfgDocument &d) : document(d) {}
    const XcfgDocument::Node &Find(const QString &path) {
        QString error; const int index = document.Lookup(path, &error);
        if (index < 0) throw std::runtime_error(error.toStdString());
        return document.Nodes()[index];
    }
    QStringList Children(const QString &path, const QString &name) {
        QString error; const auto result = document.Children(path, name, &error);
        if (!error.isEmpty()) throw std::runtime_error(error.toStdString());
        return result;
    }
    QString Id(const QString &path) {
        // Lookup 可能返回默认声明；实际实例名称必须来自查询路径。
        const QString id = path.mid(path.lastIndexOf('[') + 1).chopped(1);
        if (id.isEmpty() || id == "*" || id.toUtf8().size() > 512 || id.contains(QChar(0)))
            throw std::runtime_error("Invalid concrete hardware ID");
        return id;
    }
    QString Scalar(const QString &path) {
        const auto &node = Find(path);
        if (!node.scalar) throw std::runtime_error(("Expected scalar: " + path).toStdString());
        return node.value;
    }
    quint64 Number(const QString &path, bool address = false) {
        const auto &node = Find(path);
        const QString value = Scalar(path); bool ok = false;
        quint64 number = 0;
        if (address) {
            QString hex = value;
            if (hex.startsWith("0x", Qt::CaseInsensitive)) hex = hex.mid(2);
            else if (node.attributes.value("type") != "hex")
                throw std::runtime_error(("Address requires explicit 0x or type=hex: " + path).toStdString());
            if (QRegularExpression("^[0-9A-Fa-f]+$").match(hex).hasMatch()) number = hex.toULongLong(&ok, 16);
        } else if (QRegularExpression("^[0-9]+$").match(value).hasMatch()) number = value.toULongLong(&ok, 10);
        if (!ok) throw std::runtime_error(("Invalid or overflowing integer: " + path).toStdString());
        return number;
    }
    void Warn(const QString &path, const QString &message) {
        const QString text = path + ": " + message;
        if (!warned.contains(text)) { warned.insert(text); warnings.append(text); }
    }
    bool ReadOnly(const QString &path) {
        QString error; const int index = document.Lookup(path + "/readOnly", &error);
        if (index < 0) {
            if (!error.startsWith("No declared instance/value for ")) throw std::runtime_error(error.toStdString());
            return false;
        }
        const QString value = document.Nodes()[index].value;
        if (value != "true" && value != "false") throw std::runtime_error(("Invalid readOnly boolean: " + path).toStdString());
        if (value == "true") Warn(path, "Legacy board DeviceImpl does not enforce/preserve readOnly; host policy remains necessary");
        return value == "true";
    }
    void LegacyWriteOnly(const QString &path) {
        QString error; const int index = document.Lookup(path + "/writeOnly", &error);
        if (index < 0 && !error.startsWith("No declared instance/value for ")) throw std::runtime_error(error.toStdString());
        if (index >= 0 && document.Nodes()[index].value != "false")
            Warn(path, "writeOnly has no field in the GET wire description; original declaration is preserved in source snapshot");
    }
    QJsonObject Run() {
        const int rootIndex = document.Sources().value(document.RootFile()).root;
        const auto &root = document.Nodes().at(rootIndex);
        const QString nodeId = root.attributes.value("id");
        if (nodeId.isEmpty() || nodeId == "*" || nodeId.toUtf8().size() > 512 || nodeId.contains(QChar(0)))
            throw std::runtime_error("Hardware root must have a concrete node ID");
        QJsonArray devices; int registerCount = 0, fieldCount = 0;
        for (const QString &devicePath : Children(root.path, "Device")) {
            QJsonObject device;
            device["name"] = Id(devicePath); device["access"] = Scalar(devicePath + "/registerAccess");
            if (device["access"].toString().isEmpty() || device["access"].toString().toUtf8().size() > 512)
                throw std::runtime_error("Invalid registerAccess");
            const quint64 bytes = Number(devicePath + "/registerWidth");
            if (bytes < 1 || bytes > 8) throw std::runtime_error("registerWidth must be 1..8 bytes");
            device["register_bytes"] = int(bytes);
            const quint64 baseAddress = Number(devicePath + "/baseAddress", true);
            device["base_address"] = QString::number(baseAddress);
            QJsonArray registers;
            for (const QString &regPath : Children(devicePath, "Register")) {
                if (++registerCount > 65536) throw std::runtime_error("Too many registers");
                QJsonObject reg; reg["name"] = Id(regPath);
                const quint64 offsetAddress = Number(regPath + "/offset", true);
                if (offsetAddress > std::numeric_limits<quint64>::max() - baseAddress)
                    throw std::runtime_error(("Register address overflow: " + regPath).toStdString());
                reg["offset"] = QString::number(offsetAddress);
                const bool readOnly = ReadOnly(regPath); LegacyWriteOnly(regPath);
                reg["read_only"] = readOnly;
                if (readOnly) Warn(regPath, "Register-level readOnly has no GET wire field");
                QJsonArray fields;
                for (const QString &fieldPath : Children(regPath, "BitField")) {
                    if (++fieldCount > 100000 || fields.size() >= 256) throw std::runtime_error("Too many bit fields");
                    const quint64 offset = Number(fieldPath + "/offset"), width = Number(fieldPath + "/width");
                    if (width < 1 || width > bytes * 8 || offset > bytes * 8 - width)
                        throw std::runtime_error(("Bit field exceeds register width: " + fieldPath).toStdString());
                    QJsonObject field; field["name"] = Id(fieldPath);
                    field["offset_bits"] = int(offset); field["width_bits"] = int(width);
                    field["read_only"] = ReadOnly(fieldPath) || readOnly; LegacyWriteOnly(fieldPath);
                    fields.append(field);
                }
                reg["fields"] = fields; registers.append(reg);
            }
            device["registers"] = registers; devices.append(device);
            if (devices.size() > 4096) throw std::runtime_error("Too many devices");
        }
        if (devices.isEmpty()) throw std::runtime_error("Hardware description contains no concrete devices");
        QJsonArray sources;
        for (auto it = document.Sources().constBegin(); it != document.Sources().constEnd(); ++it) {
            QJsonObject source; source["path"] = it.key();
            source["sha256"] = QString::fromLatin1(QCryptographicHash::hash(it->bytes, QCryptographicHash::Sha256).toHex());
            sources.append(source);
        }
        QJsonObject result; result["schema_version"] = 1; result["kind"] = "getdaq.hardware-description";
        result["node_id"] = nodeId; result["devices"] = devices; result["sources"] = sources;
        result["warnings"] = warnings;
        return result;
    }
};
}
bool HardwareCompiler::Compile(const XcfgDocument &document, QJsonObject *output, QString *error)
{
    try {
        if (document.RootFile().isEmpty()) throw std::runtime_error("No configuration loaded");
        Compiler compiler(document); const auto result = compiler.Run(); *output = result; error->clear(); return true;
    } catch (const std::exception &e) { *error = QString::fromUtf8(e.what()); return false; }
}
bool HardwareCompiler::Save(const XcfgDocument &document, const QString &path, QString *error)
{
    QJsonObject compiled;
    if (!Compile(document, &compiled, error)) return false;
    const QByteArray bytes = QJsonDocument(compiled).toJson();
    if (bytes.size() > 8 * 1024 * 1024) { *error = "Compiled hardware description exceeds 8 MiB"; return false; }
    QFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) { *error = "Cannot create a new artifact: " + output.errorString(); return false; }
    if (output.write(bytes) != bytes.size() || !output.flush()) {
        *error = output.errorString(); output.close(); QFile::remove(path); return false;
    }
    error->clear(); return true;
}
