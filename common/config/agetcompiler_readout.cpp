#include "agetcompiler.h"
#include "agetcompiler_p.h"
#include "../experiment/coboagetconfiguration.h"
#include "../experiment/coboreadoutconfiguration.h"

#include <QJsonArray>
#include <stdexcept>

using namespace AgetCompilerInternal;

bool AgetCompiler::CompileReadout(const XcfgDocument &document, const QString &coboId, const QMap<int, quint16> &versions,
                                 QJsonObject *output, QString *error, GetDAQ::CoBoReadoutSettings *typedSettings)
{
    try {
        QJsonObject aget; GetDAQ::CoBoReadoutSettings settings;
        if (!CompileCoBo(document, coboId, versions, &aget, error, &settings.aget)) return false;
        Compiler reader(document);
        const auto &root = document.Nodes().at(document.Sources().value(document.RootFile()).root);
        const QString cobo = root.path + "/Node[CoBo]/Instance[" + coboId + "]";
        for (const auto &p : GetDAQ::CoBoModuleParameters()) {
            // 新名字优先；未使用的旧名字不求值，与旧后端行为一致。
            if ((p.name == "triggerPeriod" && settings.module.count("triggerPeriod_10ns"))
                || (p.name == "externalTriggerMode" && settings.module.count("secondaryTriggerMode"))
                || (p.name == "writeReadDelay" && settings.module.count("scwScrDelay"))) continue;
            const QString value = reader.Scalar(cobo + "/Module/" + QString::fromStdString(p.name),
                p.optional ? QString::fromStdString(p.defaultValue) : QString(), QString::fromStdString(p.unit), true);
            if (!p.defaultValue.empty() || !reader.parameters.last().toObject()["default_from_code"].toBool())
                settings.module[p.name] = value.toStdString();
        }
        for (const auto &p : GetDAQ::CoBoBufferParameters())
            settings.circularBuffer[p.name] = reader.Scalar(cobo + "/CircularBuffer/" + QString::fromStdString(p.name),
                p.optional ? QString::fromStdString(p.defaultValue) : QString(), QString::fromStdString(p.unit), true).toStdString();
        for (unsigned int id = 0; id < 4; ++id) {
            const QString path = cobo + QString("/AsAd[%1]/CircularBuffer/").arg(id);
            settings.buffers[id].start = reader.Scalar(path + "startOfMemory", QStringLiteral(""), QString(), true).toStdString();
            settings.buffers[id].end = reader.Scalar(path + "endOfMemory", QStringLiteral(""), QString(), true).toStdString();
        }
        for (const auto &chip : settings.aget.chips) {
            const QString path = cobo + QString("/AsAd[%1]/Aget[%2]").arg(chip.asadId).arg(chip.agetId);
            GetDAQ::CoBoChipReadout entry; entry.asadId = chip.asadId; entry.agetId = chip.agetId;
            const QString suppress = reader.Scalar(path + "/Global/suppressMultiplicity", "false");
            if (suppress != "true" && suppress != "false") throw std::runtime_error((path + "/Global/suppressMultiplicity: expected true or false").toStdString());
            entry.suppressMultiplicity = suppress == "true";
            for (unsigned int id = 0; id < 68; ++id) {
                const QString parameter = path + QString("/channel[%1]/zeroSuppressionThreshold").arg(id);
                entry.zeroSuppressionThresholds[id] = GetDAQ::ParseCoBoUnsigned(reader.Scalar(parameter, QString(), QString(), true).toStdString(), parameter.toStdString(), 4095);
            }
            settings.chips.push_back(entry);
        }
        const auto image = GetDAQ::CompileCoBoReadout(settings); QJsonArray writes;
        for (const auto &w : image.writes) {
            QJsonObject entry; entry["device"] = QString::fromStdString(w.device); entry["register"] = QString::fromStdString(w.reg);
            entry["field"] = QString::fromStdString(w.field); entry["value"] = QString::number(w.value); entry["hex"] = "0x" + QString::number(w.value, 16); writes.append(entry);
        }
        QJsonArray buffers;
        for (unsigned int id = 0; id < 4; ++id) {
            QJsonObject buffer; buffer["asad_id"] = int(id); buffer["start"] = QString::number(image.bufferStart[id]);
            buffer["end"] = QString::number(image.bufferEnd[id]); buffer["active"] = (image.aget.asadMask & (1U << id)) != 0; buffers.append(buffer);
        }
        QJsonObject result; result["schema_version"] = 1; result["kind"] = "getdaq.cobo-readout-image";
        result["scope"] = "aget-and-cobo-module-zero-suppression-circular-buffers"; result["hardware_deployment_supported"] = false;
        result["cobo_instance"] = coboId; result["sources"] = aget["sources"]; aget.remove("sources"); result["aget"] = aget;
        result["full_readout"] = image.fullReadout; result["suppress_multiplicity"] = int(image.suppressMultiplicity);
        result["trigger_mask"] = QString::number(image.triggerMask); result["use_external_time_reset"] = image.useExternalTimeReset;
        result["buffer_layout"] = "per-asad"; result["buffers"] = buffers; result["writes"] = writes; result["parameters"] = reader.parameters;
        QJsonArray warnings; warnings.append("Offline only; no complete Configure, real chip/version verification or hardware deployment");
        warnings.append("AsAd TCM/inspection/pulser, 2p initialization and ECC state/ownership constraints remain required");
        warnings.append("Buffer addresses are local CoBo RAM, not host storage; thresholds must also cover the maximum real frame size");
        result["warnings"] = warnings;
        if (typedSettings) *typedSettings = settings;
        *output = result; error->clear(); return true;
    } catch (const std::exception &e) { *error = QString::fromUtf8(e.what()); return false; }
}

bool AgetCompiler::SaveReadout(const XcfgDocument &document, const QString &coboId, const QMap<int, quint16> &versions,
                              const QString &path, QString *error)
{
    QJsonObject compiled; if (!CompileReadout(document, coboId, versions, &compiled, error)) return false;
    return SaveJson(compiled, path, error);
}
