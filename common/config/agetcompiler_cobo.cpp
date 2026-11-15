#include "agetcompiler.h"
#include "agetcompiler_p.h"
#include "../experiment/coboagetconfiguration.h"

#include <QJsonArray>
#include <stdexcept>

using namespace AgetCompilerInternal;

bool AgetCompiler::CompileCoBo(const XcfgDocument &document, const QString &coboId, const QMap<int, quint16> &versions,
                              QJsonObject *output, QString *error, GetDAQ::CoBoAgetSettings *typedSettings)
{
    try {
        if (coboId.isEmpty() || coboId == "*" || coboId.toUtf8().size() > 128 || coboId.contains(QChar(0))
            || coboId.contains('[') || coboId.contains(']') || coboId.contains('/')) throw std::runtime_error("Invalid concrete CoBo instance ID");
        if (!document.Sources().contains(document.RootFile())) throw std::runtime_error("No experiment configuration loaded");
        const auto &root = document.Nodes().at(document.Sources().value(document.RootFile()).root);
        if (root.name != "Setup") throw std::runtime_error("Expected GET experiment Setup root");
        if (versions.isEmpty() || versions.size() > 16) throw std::runtime_error("Specify versions of all 1..16 active chips");
        for (auto it = versions.constBegin(); it != versions.constEnd(); ++it)
            if (it.key() < 0 || it.key() > 15 || (it.value() != 0x201 && it.value() != 0x202 && it.value() != 0x203))
                throw std::runtime_error("Invalid chip ID or version in CoBo version map");
        Compiler finder(document); const QString cobo = root.path + "/Node[CoBo]/Instance[" + coboId + "]";
        if (!finder.Boolean(cobo + "/isActive")) throw std::runtime_error("Selected CoBo must be active");
        const auto checkFour = [&finder](const QString &path, const QString &name) {
            const auto children = finder.Children(path, name);
            if (children.size() != 4) throw std::runtime_error((path + ": must declare all four " + name + " IDs 0..3").toStdString());
            for (int id = 0; id < 4; ++id) if (!children.contains(path + '/' + name + QString("[%1]").arg(id)))
                throw std::runtime_error((path + ": invalid " + name + " ID").toStdString());
        };
        checkFour(cobo, "AsAd"); GetDAQ::CoBoAgetSettings settings; QJsonArray chips; QJsonArray sources;
        int used = 0;
        for (unsigned int asadId = 0; asadId < 4; ++asadId) {
            const QString asad = cobo + QString("/AsAd[%1]").arg(asadId);
            if (!finder.Boolean(asad + "/Control/isActive")) continue;
            settings.asadMask |= 1U << asadId; checkFour(asad, "Aget");
            for (unsigned int agetId = 0; agetId < 4; ++agetId) {
                const QString aget = asad + QString("/Aget[%1]").arg(agetId);
                if (!finder.Boolean(aget + "/Control/isActive")) continue;
                const int id = int(4 * asadId + agetId);
                if (!versions.contains(id)) throw std::runtime_error(("Missing version of active chip " + QString::number(id)).toStdString());
                Compiler compiler(document); QJsonObject entry = compiler.Run(coboId, asadId, agetId, versions.value(id));
                if (sources.isEmpty()) sources = entry["sources"].toArray();
                entry.remove("sources");
                GetDAQ::CoBoAgetChipSettings chip; chip.asadId = asadId; chip.agetId = agetId; chip.settings = compiler.lastSettings;
                settings.chips.push_back(chip); chips.append(entry); ++used;
            }
        }
        if (used != versions.size()) throw std::runtime_error("Version map contains an inactive or undeclared chip");
        const auto board = GetDAQ::CompileCoBoAget(settings);
        QJsonObject result; result["schema_version"] = 1; result["kind"] = "getdaq.cobo-aget-register-image";
        result["scope"] = "active-cobo-aget-registers-and-readout-masks"; result["hardware_deployment_supported"] = false;
        result["cobo_instance"] = coboId; result["asad_mask"] = int(board.asadMask); result["aget_mask"] = int(board.agetMask);
        result["two_pulse_mode"] = board.twoPulseMode; result["fpn_read"] = board.fpnRead;
        result["readout_offset"] = int(board.readoutOffset); result["readout_depth"] = int(board.readoutDepth);
        result["chips"] = chips; result["selection_parameters"] = finder.parameters; result["sources"] = sources;
        QJsonArray warnings; warnings.append("CoBo AGET portion only; no complete experiment Configure or hardware deployment");
        warnings.append("Version map is caller-specified, not a hardware reg5 observation"); result["warnings"] = warnings;
        if (typedSettings) *typedSettings = settings;
        *output = result; error->clear(); return true;
    } catch (const std::exception &e) { *error = QString::fromUtf8(e.what()); return false; }
}

bool AgetCompiler::SaveCoBo(const XcfgDocument &document, const QString &coboId, const QMap<int, quint16> &versions,
                           const QString &path, QString *error)
{
    QJsonObject compiled; if (!CompileCoBo(document, coboId, versions, &compiled, error)) return false;
    return SaveJson(compiled, path, error);
}
