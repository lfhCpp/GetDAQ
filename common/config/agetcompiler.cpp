#include "agetcompiler.h"
#include "agetcompiler_p.h"

#include "../experiment/agetconfiguration.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <stdexcept>

namespace AgetCompilerInternal {
Compiler::Compiler(const XcfgDocument &document) : ExperimentReader(document) {}

    QJsonObject Compiler::Run(const QString &coboId, unsigned int asadId, unsigned int agetId, quint16 chipVersion) {
        if (coboId.isEmpty() || coboId == "*" || coboId.toUtf8().size() > 128 || coboId.contains(QChar(0))
            || coboId.contains('[') || coboId.contains(']') || coboId.contains('/'))
            throw std::runtime_error("CoBo instance ID must be a concrete path-safe ID of 1..128 UTF-8 bytes");
        if (asadId > 3 || agetId > 3) throw std::runtime_error("AsAd and AGET IDs must be 0..3");
        if (!document.Sources().contains(document.RootFile())) throw std::runtime_error("No experiment configuration loaded");
        const auto &root = document.Nodes().at(document.Sources().value(document.RootFile()).root);
        if (root.name != "Setup") throw std::runtime_error("Expected GET experiment Setup root");
        const QString cobo = root.path + "/Node[CoBo]/Instance[" + coboId + "]";
        const QString asad = cobo + QString("/AsAd[%1]").arg(asadId);
        const QString aget = asad + QString("/Aget[%1]").arg(agetId);
        if (!Boolean(cobo + "/isActive") || !Boolean(asad + "/Control/isActive") || !Boolean(aget + "/Control/isActive"))
            throw std::runtime_error("Selected CoBo, AsAd and AGET must all be active");
        GetDAQ::AgetSettings settings; settings.chipVersion = chipVersion;
        settings.twoPulseMode = Boolean(cobo + "/Module/enableMem2pMode");
        const QString depth = Scalar(cobo + "/Module/readoutDepth", settings.twoPulseMode ? "256" : "512", "buckets");
        bool depthOk = !depth.isEmpty(); for (QChar c : depth) if (c < '0' || c > '9') depthOk = false;
        bool conversionOk = false; const uint depthNumber = depth.toUInt(&conversionOk, 10);
        if (!depthOk || !conversionOk || depthNumber > 512) throw std::runtime_error("Module/readoutDepth must be a decimal integer <= 512");
        settings.readoutDepth = int(depthNumber);
        for (const auto &p : GetDAQ::AgetParameters()) {
            const QString name = QString::fromStdString(p.name);
            const QString fallback = p.defaultValue.empty() ? QString() : QString::fromStdString(p.defaultValue);
            const QString unit = name == "Reg1/peackingTime" ? "ns" : name == "ReadoutPointerOffset" ? "buckets" : QString();
            settings.globals[p.name] = Scalar(aget + "/Global/" + name, fallback, unit).toStdString();
        }
        const auto children = Children(aget, "channel");
        if (children.size() != 68) throw std::runtime_error("AGET must declare exactly the 68 concrete channel IDs 0..67");
        for (const auto &child : children) {
            const QString id = child.mid(child.lastIndexOf('[') + 1).chopped(1);
            bool ok = false; const uint number = id.toUInt(&ok, 10);
            if (!ok || number > 67 || QString::number(number) != id) throw std::runtime_error(("Noncanonical or out-of-range channel ID: " + id).toStdString());
        }
        const QStringList channelNames = {"isSelectedforTestMode", "Gain", "LSBThresholdValue", "TriggerInhibition", "Reading"};
        for (unsigned int id = 0; id < 68; ++id) {
            const QString channel = aget + QString("/channel[%1]").arg(id);
            std::map<std::string, std::string> values; values["isActive"] = Scalar(channel + "/isActive").toStdString();
            if (values["isActive"] != "false") for (const auto &name : channelNames) values[name.toStdString()] = Scalar(channel + '/' + name).toStdString();
            try { settings.channels.push_back(GetDAQ::ParseAgetChannel(id, values)); }
            catch (const std::exception &e) { throw std::runtime_error((channel + ": " + QString::fromUtf8(e.what())).toStdString()); }
        }
        const auto image = GetDAQ::CompileAget(settings);
        lastSettings = settings;
        QJsonArray registers;
        for (const auto &reg : image.registers) {
            QJsonObject entry; entry["name"] = QString::fromStdString(reg.name); entry["width_bits"] = int(reg.widthBits);
            // JSON double 无法精确表示 64 位值，十进制和十六进制均使用字符串。
            entry["value"] = QString::number(reg.value); entry["hex"] = "0x" + QString::number(reg.value, 16); registers.append(entry);
        }
        QJsonArray masks;
        const char *suffixes[3] = {"1_4", "5_36", "37_68"};
        for (int i = 0; i < 3; ++i) {
            QJsonObject mask; mask["suffix"] = suffixes[i]; mask["read_always"] = QString::number(image.readAlways[i]);
            mask["read_if_hit"] = QString::number(image.readIfHit[i]); masks.append(mask);
        }
        QJsonArray sources;
        for (auto it = document.Sources().constBegin(); it != document.Sources().constEnd(); ++it) {
            QJsonObject source; source["path"] = it.key();
            source["sha256"] = QString::fromLatin1(QCryptographicHash::hash(it->bytes, QCryptographicHash::Sha256).toHex()); sources.append(source);
        }
        QJsonObject result; result["schema_version"] = 1; result["kind"] = "getdaq.aget-register-image";
        result["scope"] = "single-aget-registers-and-readout-masks"; result["hardware_deployment_supported"] = false;
        result["cobo_instance"] = coboId; result["asad_id"] = int(asadId); result["aget_id"] = int(agetId);
        result["chip_version"] = "0x" + QString::number(chipVersion, 16); result["chip_version_source"] = "caller-specified; not hardware-verified";
        result["two_pulse_mode"] = image.twoPulseMode; result["register_stage_depth"] = int(image.registerStageDepth);
        result["module_readout_depth"] = int(image.moduleReadoutDepth);
        result["registers"] = registers; result["readout_masks"] = masks; result["parameters"] = parameters; result["sources"] = sources;
        QJsonArray warnings; warnings.append("Offline compilation does not verify actual chip version or hardware state");
        warnings.append("This artifact is not a complete experiment configuration or an executable hardware plan");
        if (image.registerStageDepth != image.moduleReadoutDepth) warnings.append("Module.readoutDepth differs from the temporary AGET-register-stage depth; the legacy Configure overwrites readDepth later");
        result["warnings"] = warnings; return result;
    }

bool SaveJson(const QJsonObject &compiled, const QString &path, QString *error)
{
    const QByteArray bytes = QJsonDocument(compiled).toJson();
    if (bytes.size() > 8 * 1024 * 1024) { *error = "Compiled AGET image exceeds 8 MiB"; return false; }
    QFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) { *error = "Cannot create a new artifact: " + output.errorString(); return false; }
    if (output.write(bytes) != bytes.size() || !output.flush()) { *error = output.errorString(); output.close(); QFile::remove(path); return false; }
    error->clear(); return true;
}
}

using namespace AgetCompilerInternal;

bool AgetCompiler::Compile(const XcfgDocument &document, const QString &coboId, unsigned int asadId,
                           unsigned int agetId, quint16 chipVersion, QJsonObject *output, QString *error)
{
    try { Compiler compiler(document); const auto result = compiler.Run(coboId, asadId, agetId, chipVersion); *output = result; error->clear(); return true; }
    catch (const std::exception &e) { *error = QString::fromUtf8(e.what()); return false; }
}

bool AgetCompiler::Save(const XcfgDocument &document, const QString &coboId, unsigned int asadId,
                        unsigned int agetId, quint16 chipVersion, const QString &path, QString *error)
{
    QJsonObject compiled; if (!Compile(document, coboId, asadId, agetId, chipVersion, &compiled, error)) return false;
    return SaveJson(compiled, path, error);
}
