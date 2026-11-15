#include "monitoringcompiler.h"
#include "experimentreader.h"
#include <QFile>
#include <QJsonDocument>
#include <stdexcept>
namespace {
bool Exists(const XcfgDocument &document, const QString &path)
{
    QString error; if (document.Lookup(path,&error) >= 0) return true;
    if (!error.startsWith("No declared instance/value for ")) throw std::runtime_error(error.toStdString());
    return false;
}
QStringList LimitPair(ExperimentReader &reader, const QString &monitor, const QString &modern,
                      const QString &legacyLow, const QString &legacyHigh, bool *usesModern)
{
    const QString low = monitor + '/' + modern + "/lowerValue", high = monitor + '/' + modern + "/upperValue";
    const bool hasLow = Exists(reader.document,low), hasHigh = Exists(reader.document,high);
    if (hasLow != hasHigh) throw std::invalid_argument(("Partial monitoring limit pair: " + monitor + '/' + modern).toStdString());
    *usesModern = hasLow;
    return hasLow ? QStringList{low,high} : QStringList{monitor + '/' + legacyLow,monitor + '/' + legacyHigh};
}
double Real(ExperimentReader &reader, const QString &path, const QString &unit)
{
    return GetDAQ::ParseMonitoringReal(reader.Scalar(path,QString(),unit,false,true).toStdString(),path.toStdString());
}
int Temperature(ExperimentReader &reader, const QString &path)
{
    const auto text = reader.Scalar(path,QString(),"C");
    if (text.isEmpty() || text.size() > 16) throw std::invalid_argument("Invalid temperature integer");
    int index = text[0] == '-' ? 1 : 0;
    if (index == text.size()) throw std::invalid_argument("Invalid temperature integer");
    for (; index < text.size(); ++index) if (text[index] < '0' || text[index] > '9') throw std::invalid_argument("Invalid temperature integer");
    bool ok = false; const int value = text.toInt(&ok,10);
    if (!ok || value < -128 || value > 127) throw std::invalid_argument("Temperature must be signed Celsius -128..127");
    return value;
}
QJsonObject Limit(double low, double high, unsigned int lowCode, unsigned int highCode, const QString &unit)
{
    QJsonObject object; object["requested_lower"] = low; object["requested_upper"] = high; object["unit"] = unit;
    object["lower_code"] = QString::number(lowCode); object["upper_code"] = QString::number(highCode); return object;
}
}
bool MonitoringCompiler::Compile(const XcfgDocument &document, const QString &coboId,
                                 GetDAQ::CoBoMonitoringSettings::ModeMapping mapping,
                                 QJsonObject *output, QString *error, GetDAQ::CoBoMonitoringSettings *settingsOut)
{
    try {
        ExperimentReader reader(document); const auto cobo = reader.CoBoPath(coboId);
        GetDAQ::CoBoMonitoringSettings settings; settings.modeMapping = mapping;
        settings.checkAsAdAlarms = reader.Boolean(cobo + "/Alarms/checkAsAd","true");
        settings.subscribeAlarms = reader.Boolean(cobo + "/Alarms/subscribe","true");
        const auto children = reader.Children(cobo,"AsAd");
        if (children.size() != 4) throw std::invalid_argument("Monitoring configuration must declare exactly four AsAd IDs 0..3");
        unsigned int seen = 0; QMap<unsigned int,QJsonObject> layouts;
        for (const auto &child : children) {
            const auto id = child.mid(child.lastIndexOf('[')+1).chopped(1);
            if (id.size() != 1 || id[0] < '0' || id[0] > '3') throw std::invalid_argument("Monitoring AsAd ID must be 0..3");
            const unsigned int number = id[0].unicode()-'0';
            if (seen & (1U << number)) throw std::invalid_argument("Duplicate monitoring AsAd ID");
            seen |= 1U << number;
            if (!reader.Boolean(child + "/Control/isActive")) continue;
            settings.asadMask |= 1U << number;
            GetDAQ::AsAdMonitoringBoardSettings board; board.asadId = number;
            board.checkPowerSupply = reader.Boolean(child + "/Control/checkPowerSupply","true");
            const auto monitor = child + "/Monitoring";
            board.alarmsEnabled = reader.Boolean(monitor + "/isAlarmSet");
            board.fastConversionEnabled = reader.Boolean(monitor + "/fastConversionEnabled","false");
            board.singleChannelModeEnabled = reader.Boolean(monitor + "/singleChannelModeEnabled","false");
            board.averagingDisabled = reader.Boolean(monitor + "/averagingDisabled","false");
            board.singleChannelItem = reader.Scalar(monitor + "/singleChannelItem").toStdString();
            QJsonObject layout; bool modern = false;
            auto pair = LimitPair(reader,monitor,"LimitsVDD","VddLowLimit","VddHighLimit",&modern);
            board.vddLower = Real(reader,pair[0],"V"); board.vddUpper = Real(reader,pair[1],"V");
            board.vddSettleLower = modern ? board.vddLower : 3.2;
            layout["vdd"] = modern ? "LimitsVDD" : "VddLowLimit/VddHighLimit";
            layout["vdd_settle"] = modern ? "LimitsVDD/lowerValue" : "legacy wait function code default: 3.2 V";
            pair = LimitPair(reader,monitor,"LimitsIDD","IddLowLimit","IddHighLimit",&modern);
            board.iddLower = Real(reader,pair[0],"A"); board.iddUpper = Real(reader,pair[1],"A");
            layout["idd"] = modern ? "LimitsIDD" : "IddLowLimit/IddHighLimit";
            pair = LimitPair(reader,monitor,"LimitsText","ExternalTempLowLimit","ExternalTempHighLimit",&modern);
            board.externalTemperatureLower = Temperature(reader,pair[0]); board.externalTemperatureUpper = Temperature(reader,pair[1]);
            layout["text"] = modern ? "LimitsText" : "ExternalTempLowLimit/ExternalTempHighLimit";
            if (board.alarmsEnabled) {
                const char *flags[] = {"LimitsText/disableAlertsTooHigh","LimitsText/disableAlertsTooLow","LimitsText/disableAlertsOutOfOrder","LimitsVDD/disableAlerts","LimitsIDD/disableAlerts"};
                for (unsigned int i = 0; i < 5; ++i) board.disableAlerts[i] = reader.Boolean(monitor + '/' + flags[i],"false");
            }
            layouts[number] = layout; settings.boards.push_back(board);
        }
        const auto image = GetDAQ::CompileCoBoMonitoring(settings);
        QJsonObject result; result["schema_version"] = 1; result["kind"] = "getdaq.cobo-monitoring-image";
        result["cobo_instance"] = coboId; result["asad_mask"] = int(image.asadMask);
        result["mode_mapping"] = mapping == GetDAQ::CoBoMonitoringSettings::DeclaredParameters ? "declared" : "legacy";
        result["hardware_deployment_supported"] = false; result["complete_prepare"] = false;
        QJsonObject alarmPolicy; alarmPolicy["check_asad"] = settings.checkAsAdAlarms; alarmPolicy["subscribe"] = settings.subscribeAlarms;
        result["alarm_policy"] = alarmPolicy;
        result["native_led_manager_available"] = true; result["native_alarm_subscription_available"] = true;
        result["host_alarm_subscription_supported"] = false; result["led_manager_supported"] = false;
        QJsonObject timing; timing["power_delay_ms"] = int(settings.powerDelayMs); timing["settle_attempts"] = int(settings.settleAttempts);
        timing["settle_delay_ms"] = int(settings.settleDelayMs); timing["zero_read_retries"] = int(settings.zeroReadRetries); result["timing"] = timing;
        QJsonArray boards, warnings;
        warnings.append("Offline artifact cannot be deployed through CLI/GUI; subflow completion does not establish Prepared or electrical safety");
        warnings.append("Monitoring reset/command writes have no implicit readback; native LED/alarm APIs require separate typed integration, ownership and callback endpoints");
        warnings.append("Tint/VDD1 limit writes and interruptMask2 were not enabled by the received legacy Prepare and are not added here");
        warnings.append(mapping == GetDAQ::CoBoMonitoringSettings::DeclaredParameters ?
            "Declared mapping corrects the legacy argument order; non-default monitoring behavior can change" :
            "Legacy mapping preserves the old reordered arguments, including item-to-bool conversion; verify selected monitoring behavior");
        for (const auto &b : image.boards) {
            const GetDAQ::AsAdMonitoringBoardSettings *request = nullptr;
            for (const auto &candidate : settings.boards) if (candidate.asadId == b.asadId) { request = &candidate; break; }
            QJsonObject board; board["asad_id"] = int(b.asadId); board["check_power_supply"] = b.checkPowerSupply;
            board["alarms_enabled"] = request->alarmsEnabled; board["limit_layout"] = layouts[b.asadId];
            QJsonObject limits;
            limits["Text"] = Limit(request->externalTemperatureLower,request->externalTemperatureUpper,b.limits[0],b.limits[1],"C");
            limits["VDD"] = Limit(request->vddLower,request->vddUpper,b.limits[2],b.limits[3],"V");
            limits["IDD"] = Limit(request->iddLower,request->iddUpper,b.limits[4],b.limits[5],"A"); board["limits"] = limits;
            board["configuration2"] = QString::number(b.configuration2); board["configuration3"] = QString::number(b.configuration3);
            board["protection_mask1"] = QString::number(b.protectionMask); board["protection_configuration1"] = QString::number(b.protectionConfiguration1);
            board["settling_configuration1"] = "45"; board["settling_mask1"] = "255";
            board["vdd_settle_lower_volts"] = request->vddSettleLower; board["vdd_settle_minimum_raw"] = QString::number(b.vddSettleMinimum);
            boards.append(board);
            if (!b.checkPowerSupply) warnings.append(QString("AsAd %1 explicitly permits a before-power supply check difference; after-power alarms remain errors").arg(b.asadId));
            if (!request->alarmsEnabled || b.protectionMask != 0x23) warnings.append(QString("AsAd %1 disables at least one hardware protection criterion").arg(b.asadId));
            if (request->vddSettleLower > request->vddUpper) warnings.append(QString("AsAd %1 settling minimum exceeds its requested protection upper limit").arg(b.asadId));
        }
        result["boards"] = boards; result["parameters"] = reader.parameters; result["sources"] = reader.Sources(); result["warnings"] = warnings;
        *output = result; if (settingsOut) *settingsOut = settings; error->clear(); return true;
    } catch (const std::exception &e) { *error = QString::fromUtf8(e.what()); return false; }
}
bool MonitoringCompiler::Save(const XcfgDocument &document, const QString &coboId,
                             GetDAQ::CoBoMonitoringSettings::ModeMapping mapping, const QString &path, QString *error)
{
    QJsonObject compiled; if (!Compile(document,coboId,mapping,&compiled,error)) return false;
    const auto bytes = QJsonDocument(compiled).toJson();
    if (bytes.size() > 8*1024*1024) { *error = "Compiled monitoring image exceeds 8 MiB"; return false; }
    QFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) { *error = "Cannot create a new artifact: " + output.errorString(); return false; }
    if (output.write(bytes) != bytes.size() || !output.flush()) { *error = output.errorString(); output.close(); QFile::remove(path); return false; }
    error->clear(); return true;
}
