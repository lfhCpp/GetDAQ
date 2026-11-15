#include "agetcompiler.h"
#include "agetcompiler_p.h"
#include "../experiment/coboasadconfiguration.h"
#include "../experiment/coboagetconfiguration.h"
#include "../experiment/coboreadoutconfiguration.h"

#include <QJsonArray>
#include <map>
#include <stdexcept>

using namespace AgetCompilerInternal;

bool AgetCompiler::CompileConfigure(const XcfgDocument &document, const QString &coboId, const QMap<int, quint16> &versions,
                                   QJsonObject *output, QString *error, GetDAQ::CoBoConfigureSettings *typedSettings)
{
    try {
        GetDAQ::CoBoConfigureSettings settings; QJsonObject readout;
        if (!CompileReadout(document, coboId, versions, &readout, error, &settings.readout)) return false;
        settings.asad.asadMask = settings.readout.aget.asadMask; Compiler reader(document);
        const auto &root = document.Nodes().at(document.Sources().value(document.RootFile()).root);
        const QString cobo = root.path + "/Node[CoBo]/Instance[" + coboId + "]";
        QString amplitude = reader.Scalar(cobo + "/Generator/amplitudeStart", QStringLiteral(""), "mV", false, true);
        // 旧函数无显式 asadId 时取 AsAd0 的 Amplitude[0]，并非每板的独立输出幅度。
        if (reader.parameters.last().toObject()["default_from_code"].toBool())
            amplitude = reader.Scalar(cobo + "/AsAd[0]/Generator/Amplitude[0]", QString(), "mV", false, true);
        settings.asad.generator["amplitudeStart"] = amplitude.toStdString();
        for (const auto &p : std::map<std::string, std::string>{{"defaultVoltage", "0"}, {"secondPulseAmplitude", "0"},
                {"externalTriggerMode", "false"}, {"periodicModeEnabled", "false"}, {"doublePulseEnabled", "false"},
                {"poissonModeEnabled", "false"}, {"period_ms", "1000"}}) {
            const bool voltage = p.first == "defaultVoltage" || p.first == "secondPulseAmplitude";
            settings.asad.generator[p.first] = reader.Scalar(cobo + "/Generator/" + QString::fromStdString(p.first),
                QString::fromStdString(p.second), voltage ? "mV" : p.first == "period_ms" ? "ms" : QString(), p.first == "period_ms", voltage).toStdString();
        }
        for (unsigned int id = 0; id < 4; ++id) if (settings.asad.asadMask & (1U << id)) {
            const QString asad = cobo + QString("/AsAd[%1]").arg(id); GetDAQ::CoBoAsAdBoardSettings b; b.asadId = id;
            QString calibration = reader.Scalar(asad + "/Generator/isCalibMode", QStringLiteral(""));
            if (reader.parameters.last().toObject()["default_from_code"].toBool()) calibration = reader.Scalar(cobo + "/Generator/isCalibMode");
            if (calibration != "true" && calibration != "false") throw std::runtime_error("AsAd Generator/isCalibMode must be true or false");
            b.calibrationMode = calibration == "true";
            QString capacitor = reader.Scalar(asad + "/Generator/injectValue", QStringLiteral(""));
            if (reader.parameters.last().toObject()["default_from_code"].toBool()) capacitor = reader.Scalar(cobo + "/Generator/injectValue");
            b.capacitor = capacitor.toStdString();
            b.triggerDelay = GetDAQ::ParseCoBoUnsigned(reader.Scalar(asad + "/Generator/triggerDelay", "0", "WCK periods", true).toStdString(), "AsAd Generator/triggerDelay", 31);
            const QString inspection = asad + "/InspectionLines/";
            b.inspectionDevice = reader.Scalar(inspection + "deviceToInspect", "ADC").toStdString();
            // 名字先由原始映射验证，防止用不可信字符串拼装配置路径。
            if (b.inspectionDevice.empty() || b.inspectionDevice.find_first_of("/[]") != std::string::npos)
                throw std::runtime_error("Invalid InspectionLines device name");
            const QString selected = inspection + QString::fromStdString(b.inspectionDevice) + '/';
            const bool defaultDevice = reader.parameters.last().toObject()["default_from_code"].toBool();
            b.isp1 = reader.Scalar(selected + "ISP1", defaultDevice ? QString("SPI_CS") : QString()).toStdString();
            b.isp2 = reader.Scalar(selected + "ISP2", defaultDevice ? QString("CAS") : QString()).toStdString();
            settings.asad.boards.push_back(b);
        }
        const auto image = GetDAQ::CompileCoBoConfigure(settings); QJsonArray boards;
        for (const auto &b : image.asad.boards) {
            QJsonObject entry; entry["asad_id"] = int(b.asadId); entry["calibration_mode"] = b.calibrationMode;
            entry["capacitor_11pf"] = b.capacitor11pF; entry["trigger_delay_wck"] = int(b.triggerDelay);
            entry["inspection_value"] = QString::number(b.inspection); boards.append(entry);
        }
        const auto &p = image.asad.pulser; QJsonObject pulser;
        pulser["default_voltage_mv"] = QString::number(p.defaultVoltageMv); pulser["voltage_mv"] = QString::number(p.voltageMv);
        pulser["second_voltage_mv"] = QString::number(p.secondVoltageMv); pulser["default_dac"] = QString::number(p.defaultDac);
        pulser["dac"] = QString::number(p.dac); pulser["second_dac"] = QString::number(p.secondDac);
        pulser["external"] = p.external; pulser["periodic"] = p.periodic; pulser["double_pulse"] = p.doublePulse;
        pulser["poisson"] = p.poisson; pulser["period_ms"] = QString::number(p.periodMs);
        QJsonObject result; result["schema_version"] = 1; result["kind"] = "getdaq.cobo-configure-image";
        result["scope"] = "cobo-aget-asad-module-configure-parameters"; result["hardware_deployment_supported"] = false;
        result["cobo_instance"] = coboId; result["sources"] = readout["sources"]; readout.remove("sources"); result["readout"] = readout;
        result["asad_mask"] = int(image.asad.asadMask); result["asad_boards"] = boards; result["pulser"] = pulser;
        result["initialize_2p"] = true; result["initialize_2p_delay_ms"] = 20; result["parameters"] = reader.parameters;
        QJsonArray warnings; warnings.append("Configure parameters only; full Prepare, production ECC state/recovery/ownership and real hardware acceptance remain required");
        warnings.append("Voltages are offsets from Vref=1102mV, truncated towards zero to integer mV to match the historical board manager");
        warnings.append("Pulser actions require the board AsAdPulserMgr; compilation never starts a timer or acquisition");
        result["warnings"] = warnings;
        if (typedSettings) *typedSettings = settings;
        *output = result; error->clear(); return true;
    } catch (const std::exception &e) { *error = QString::fromUtf8(e.what()); return false; }
}

bool AgetCompiler::SaveConfigure(const XcfgDocument &document, const QString &coboId, const QMap<int, quint16> &versions,
                                const QString &path, QString *error)
{
    QJsonObject compiled; if (!CompileConfigure(document, coboId, versions, &compiled, error)) return false;
    return SaveJson(compiled, path, error);
}
