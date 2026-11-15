#include "calibrationcompiler.h"
#include "experimentreader.h"
#include <QFile>
#include <QJsonDocument>
#include <stdexcept>
namespace {
void Four(ExperimentReader &reader, const QString &path, const QString &name)
{
    const auto children = reader.Children(path,name);
    if (children.size() != 4) throw std::invalid_argument((path + ": must declare all four " + name + " IDs 0..3").toStdString());
    for (int id = 0; id < 4; ++id) if (!children.contains(path + '/' + name + QString("[%1]").arg(id)))
        throw std::invalid_argument((path + ": invalid " + name + " ID").toStdString());
}
bool Exists(const XcfgDocument &document, const QString &path)
{
    QString error; if (document.Lookup(path,&error) >= 0) return true;
    if (!error.startsWith("No declared instance/value for ")) throw std::runtime_error(error.toStdString());
    return false;
}
}
bool CalibrationCompiler::Compile(const XcfgDocument &document, const QString &coboId, const QMap<int,quint16> &versions,
                                  GetDAQ::CoBoCalibrationSettings::DelayLayout layout, QJsonObject *output, QString *error,
                                  GetDAQ::CoBoCalibrationSettings *settingsOut, bool calibrationAgetRegister)
{
    try {
        ExperimentReader reader(document); const auto cobo = reader.CoBoPath(coboId);
        GetDAQ::CoBoCalibrationSettings settings; settings.delayLayout = layout; settings.calibrationAgetRegister = calibrationAgetRegister;
        settings.computeAutomaticReadDelay = reader.Boolean(cobo + "/Module/calibrateAutoReadDataDelay","true");
        Four(reader,cobo,"AsAd"); QMap<unsigned int,QString> policySources; QJsonArray chips;
        for (unsigned int asadId = 0; asadId < 4; ++asadId) {
            const auto asad = cobo + QString("/AsAd[%1]").arg(asadId);
            if (!reader.Boolean(asad + "/Control/isActive")) continue;
            settings.asadMask |= 1U << asadId; Four(reader,asad,"Aget");
            GetDAQ::AsAdCalibrationSettings board; board.asadId = asadId;
            const auto requested = reader.Scalar(asad + "/Control/calibrationAgetId","0");
            if (requested.size() != 1 || requested[0] < '0' || requested[0] > '3') throw std::invalid_argument("calibrationAgetId must be 0..3; 0 means automatic first active chip");
            board.calibrationAgetId = requested[0].unicode()-'0';
            const auto localPolicy = asad + "/Control/enableAutoReadDataDelay", modulePolicy = cobo + "/Module/enableAutoReadDataDelay";
            const auto policy = Exists(document,localPolicy) ? localPolicy : modulePolicy;
            board.useAutomaticReadDelay = reader.Boolean(policy,"false"); policySources[asadId] = policy;
            for (unsigned int agetId = 0; agetId < 4; ++agetId) {
                if (!reader.Boolean(asad + QString("/Aget[%1]/Control/isActive").arg(agetId))) continue;
                const int id = int(4*asadId+agetId);
                if (!versions.contains(id)) throw std::invalid_argument(("Missing version of active calibration chip " + QString::number(id)).toStdString());
                settings.chipVersions[unsigned(id)] = versions.value(id);
                QJsonObject chip; chip["global_id"] = id; chip["asad_id"] = int(asadId); chip["aget_id"] = int(agetId);
                chip["expected_version"] = "0x" + QString::number(versions.value(id),16); chips.append(chip);
            }
            settings.boards.push_back(board);
        }
        if (settings.chipVersions.size() != std::size_t(versions.size())) throw std::invalid_argument("Version map contains an inactive or undeclared calibration chip");
        const auto image = GetDAQ::CompileCoBoCalibration(settings);
        QJsonObject result; result["schema_version"] = 1; result["kind"] = "getdaq.cobo-calibration-image";
        result["cobo_instance"] = coboId; result["asad_mask"] = int(image.asadMask); result["aget_mask"] = int(image.agetMask);
        result["calibration_asad_mask"] = int(image.calibrationAsAdMask); result["first_calibration_global_chip"] = int(image.firstCalibrationChip);
        result["automatic_read_delay_mask"] = int(image.automaticReadDelayMask);
        result["delay_layout"] = layout == GetDAQ::CoBoCalibrationSettings::GlobalDelayField ? "global" : "per-asad";
        result["delay_layout_source"] = "caller-specified; requires matching bound hardware description";
        result["calibration_aget_register"] = calibrationAgetRegister;
        result["compute_automatic_read_delay"] = settings.computeAutomaticReadDelay;
        if (layout == GetDAQ::CoBoCalibrationSettings::GlobalDelayField) result["global_automatic_read_delay"] = image.globalAutomaticReadDelay;
        QJsonArray boards;
        for (const auto &board : settings.boards) {
            QJsonObject object; object["asad_id"] = int(board.asadId); object["requested_calibration_aget_id"] = int(board.calibrationAgetId);
            object["requested_zero_means_auto"] = true;
            object["has_active_chips"] = bool(image.calibrationAsAdMask & (1U << board.asadId));
            object["calibration_aget_id"] = object["has_active_chips"].toBool() ? QJsonValue(int(image.calibrationAgetIds[board.asadId])) : QJsonValue(QJsonValue::Null);
            object["use_automatic_read_delay"] = board.useAutomaticReadDelay; object["delay_policy_path"] = policySources.value(board.asadId); boards.append(object);
        }
        result["boards"] = boards; result["chips"] = chips; result["parameters"] = reader.parameters; result["sources"] = reader.Sources();
        result["hardware_deployment_supported"] = false; result["complete_prepare"] = false; result["physical_delay_verified"] = false;
        result["chip_version_source"] = "caller-specified; native serial calibration compares low 16 bits after calibration";
        QJsonArray warnings;
        warnings.append("Serial calibration resets ADC0 and leaves an ADC test pattern; ADCInit and final input reconciliation are still required");
        warnings.append("Automatic delay computation overwrites AGET reg1/reg2 and transiently enables firmware; execute before final Configure, never during acquisition");
        warnings.append("Compute and use are independent: compute=false/use=true selects a previous result whose physical validity is not proven");
        warnings.append("Any lost write reply stops the plan without retry or automatic cleanup; hardware state may remain partially changed");
        warnings.append("This offline artifact does not deploy hardware, complete Prepared, subscribe alarms or validate electrical timing");
        if (layout == GetDAQ::CoBoCalibrationSettings::PerAsAdDelayFields)
            warnings.append("Per-AsAd fields correct the legacy hard-coded field 0; repository description only proves the global layout, per-AsAd requires actual firmware/description acceptance");
        if (!calibrationAgetRegister) warnings.append("Explicit legacy absence assumption: only calibration chip 0 is supported; no failed-write fallback");
        if (image.calibrationAsAdMask != image.asadMask) warnings.append("Some active AsAd boards have no active chips and are not reported as serially calibrated");
        result["warnings"] = warnings;
        if (settingsOut) *settingsOut = settings;
        *output = result; error->clear(); return true;
    } catch (const std::exception &e) { *error = QString::fromUtf8(e.what()); return false; }
}
bool CalibrationCompiler::Save(const XcfgDocument &document, const QString &coboId, const QMap<int,quint16> &versions,
                               GetDAQ::CoBoCalibrationSettings::DelayLayout layout, const QString &path, QString *error)
{
    QJsonObject compiled; if (!Compile(document,coboId,versions,layout,&compiled,error)) return false;
    const auto bytes = QJsonDocument(compiled).toJson();
    if (bytes.size() > 8*1024*1024) { *error = "Calibration artifact exceeds 8 MiB"; return false; }
    QFile file(path); if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) { *error = "Cannot create a new artifact: " + file.errorString(); return false; }
    if (file.write(bytes) != bytes.size() || !file.flush()) { *error = file.errorString(); file.close(); QFile::remove(path); return false; }
    error->clear(); return true;
}
