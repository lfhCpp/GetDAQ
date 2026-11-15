#include "clockcompiler.h"
#include "experimentreader.h"
#include "../experiment/coboreadoutconfiguration.h"
#include <QFile>
#include <QJsonDocument>
#include <stdexcept>
namespace {
int Signed(const QString &text, int minimum, int maximum, const QString &path)
{
    if (text.isEmpty() || text.size() > 16) throw std::invalid_argument(("Invalid decimal integer: " + path).toStdString());
    int index = text[0] == '-' ? 1 : 0;
    if (index == text.size()) throw std::invalid_argument(("Invalid decimal integer: " + path).toStdString());
    for (; index < text.size(); ++index) if (text[index] < '0' || text[index] > '9')
        throw std::invalid_argument(("Invalid decimal integer: " + path).toStdString());
    bool ok = false; const int value = text.toInt(&ok,10);
    if (!ok || value < minimum || value > maximum) throw std::invalid_argument(("Clock integer out of range: " + path).toStdString());
    return value;
}
void Expert(ExperimentReader &reader, const QString &path, const std::vector<GetDAQ::AsAdClockParameter> &fields,
            GetDAQ::AsAdClockExpertFields &values)
{
    if (!reader.Boolean(path + "/debugModeEnabled", "false")) return;
    for (const auto &p : fields) {
        const QString query = path + '/' + QString::fromStdString(p.name);
        values[p.name] = p.boolean ? unsigned(reader.Boolean(query)) :
            GetDAQ::ParseCoBoUnsigned(reader.Scalar(query,QString(),QString(),true).toStdString(),query.toStdString(),(1U << p.width)-1);
    }
}
QJsonObject ClockWord(const GetDAQ::AsAdClockImage &clock)
{
    QJsonObject result; result["value"] = QString::number(clock.value); result["hex"] = "0x" + QString::number(clock.value,16);
    result["width_bits"] = 64; result["nominal_output_known"] = clock.nominalOutputKnown;
    result["nominal_output_mhz"] = clock.nominalOutputKnown ? QJsonValue(clock.nominalOutputMHz) : QJsonValue(QJsonValue::Null);
    result["nominal_matches_requested"] = clock.nominalMatchesRequested; result["nominal_within_aget_range"] = clock.nominalWithinAgetRange;
    return result;
}
}
bool ClockCompiler::Compile(const XcfgDocument &document, const QString &coboId, quint32 hardwareVersion,
                            QJsonObject *output, QString *error, GetDAQ::CoBoClockSettings *settingsOut)
{
    try {
        ExperimentReader reader(document); const QString cobo = reader.CoBoPath(coboId);
        GetDAQ::CoBoClockSettings settings; settings.hardwareVersion = hardwareVersion;
        const QString module = cobo + "/Module";
        settings.writingMHz = GetDAQ::ParseClockReal(reader.Scalar(module + "/writingClockFrequency",QString(),"MHz",false,true).toStdString(),"CKW",1,200);
        settings.readingMHz = GetDAQ::ParseClockReal(reader.Scalar(module + "/readingClockFrequency",QString(),"MHz",false,true).toStdString(),"CKR",1,200);
        settings.skipPllConfig = reader.Boolean(module + "/skipPLLConfig","false");
        if (hardwareVersion >= 2 && !settings.skipPllConfig) for (const auto &p : GetDAQ::CoBoPllParameters()) {
            QString query = module + "/PLL/" + QString::fromStdString(p.name);
            if (!p.legacyName.empty()) {
                QString lookupError;
                if (document.Lookup(query,&lookupError) < 0) {
                    if (!lookupError.startsWith("No declared instance/value for ")) throw std::runtime_error(lookupError.toStdString());
                    const QString legacy = module + "/PLL/" + QString::fromStdString(p.legacyName);
                    if (document.Lookup(legacy,&lookupError) >= 0) query = legacy;
                    else if (!lookupError.startsWith("No declared instance/value for ")) throw std::runtime_error(lookupError.toStdString());
                }
            }
            settings.pll[p.name] = reader.Scalar(query,QString::fromStdString(p.defaultValue),QString(),!p.boolean).toStdString();
        }
        const auto children = reader.Children(cobo,"AsAd");
        if (children.size() != 4) throw std::invalid_argument("CoBo clock configuration must declare exactly four AsAd IDs 0..3");
        unsigned int seen = 0;
        for (const auto &child : children) {
            const auto id = child.mid(child.lastIndexOf('[')+1).chopped(1);
            if (id.size() != 1 || id[0] < '0' || id[0] > '3') throw std::invalid_argument("AsAd clock ID must be 0..3");
            const unsigned int number = id[0].unicode()-'0';
            if (seen & (1U << number)) throw std::invalid_argument("Duplicate AsAd clock ID");
            seen |= 1U << number;
            if (!reader.Boolean(child + "/Control/isActive")) continue;
            settings.asadMask |= 1U << number;
            GetDAQ::AsAdBoardClockSettings board; board.asadId = number;
            const QString clock = child + "/Clocking";
            board.samplingDivider = GetDAQ::ParseClockReal(reader.Scalar(clock + "/samplingClockDivider","1",QString(),false,true).toStdString(),"samplingClockDivider",0,200);
            board.phaseDegrees = Signed(reader.Scalar(clock + "/phaseShiftAdcClock",QString(),"degrees"),0,270,clock + "/phaseShiftAdcClock");
            board.delayPs = Signed(reader.Scalar(clock + "/delayAdcClock",QString(),"ps"),-5335,5335,clock + "/delayAdcClock");
            Expert(reader,clock + "/Debug/LCM1",GetDAQ::AsAdSamplingClockParameters(),board.samplingExpert);
            Expert(reader,clock + "/Debug/LCM2",GetDAQ::AsAdReadingClockParameters(),board.readingExpert);
            settings.boards.push_back(board);
        }
        const auto image = GetDAQ::CompileCoBoClock(settings);
        QJsonObject result; result["schema_version"] = 1; result["kind"] = "getdaq.cobo-clock-image";
        result["hardware_deployment_supported"] = false; result["complete_prepare"] = false;
        result["cobo_instance"] = coboId; result["hardware_version"] = QString::number(hardwareVersion);
        result["hardware_version_source"] = "caller-specified; not hardware-verified";
        result["writing_mhz_requested"] = settings.writingMHz; result["reading_mhz_requested"] = settings.readingMHz;
        result["asad_mask"] = int(image.asadMask); result["program_pll"] = image.programPll;
        result["program_writing_divider"] = image.programWritingDivider; result["writing_divider"] = int(image.writingDivider);
        result["program_reading_divider"] = image.programReadingDivider; result["reading_divider"] = int(image.readingDivider);
        result["nominal_upstream_known"] = image.nominalUpstreamKnown;
        result["nominal_cobo_writing_mhz"] = image.nominalUpstreamKnown ? QJsonValue(image.nominalCoBoWritingMHz) : QJsonValue(QJsonValue::Null);
        result["nominal_cobo_reading_mhz"] = image.nominalUpstreamKnown ? QJsonValue(image.nominalCoBoReadingMHz) : QJsonValue(QJsonValue::Null);
        result["nominal_targets_matched"] = image.nominalTargetsMatched;
        result["nominal_known_difference"] = image.nominalKnownDifference;
        result["nominal_outputs_within_range"] = image.nominalUpstreamKnown ? QJsonValue(image.nominalOutputsWithinRange) : QJsonValue(QJsonValue::Null);
        QJsonArray packets;
        for (auto value : image.pll.packets) { QJsonObject packet; packet["value"] = QString::number(value); packet["hex"] = "0x" + QString::number(value,16); packets.append(packet); }
        result["pll_packets"] = packets;
        QJsonArray boards;
        for (const auto &b : image.boards) {
            QJsonObject board; board["asad_id"] = int(b.asadId); board["lcm1"] = ClockWord(b.sampling);
            auto lcm2 = ClockWord(b.reading); lcm2["nominal_adc_mhz"] = b.reading.nominalAdcMHz;
            lcm2["quantized_adc_delay_ps"] = b.reading.quantizedAdcDelayPs; board["lcm2"] = lcm2;
            const bool known = image.nominalUpstreamKnown && b.sampling.nominalOutputKnown;
            board["effective_nominal_sampling_mhz"] = known ? QJsonValue(b.effectiveNominalSamplingMHz) : QJsonValue(QJsonValue::Null);
            board["effective_nominal_reading_mhz"] = image.nominalUpstreamKnown ? QJsonValue(b.effectiveNominalReadingMHz) : QJsonValue(QJsonValue::Null);
            board["effective_nominal_adc_mhz"] = image.nominalUpstreamKnown ? QJsonValue(b.effectiveNominalAdcMHz) : QJsonValue(QJsonValue::Null);
            boards.append(board);
        }
        result["boards"] = boards; result["parameters"] = reader.parameters; result["sources"] = reader.Sources();
        QJsonArray warnings;
        warnings.append("Nominal frequencies assume the legacy clock model; no PLL lock, frequency or phase measurement was performed");
        warnings.append("Offline artifact has no hardware deployment entry; native library subflow does not establish Prepared");
        if (!image.nominalUpstreamKnown) warnings.append("Upstream sampling frequency cannot be inferred with this skipPLL or expert mode");
        if (!image.nominalTargetsMatched) warnings.append("Requested clocks differ from, or cannot be confirmed by, the nominal upstream/downstream model");
        if (!image.nominalOutputsWithinRange) warnings.append("At least one nominal AsAd output is unknown or outside 1..200MHz");
        for (const auto &b : settings.boards) if (b.samplingDivider != 1) {
            warnings.append("AsAd sampling division can change SCW pulse timing; verify serial calibration and synchronization on hardware"); break;
        }
        result["warnings"] = warnings;
        *output = result; if (settingsOut) *settingsOut = settings; error->clear(); return true;
    } catch (const std::exception &e) { *error = QString::fromUtf8(e.what()); return false; }
}
bool ClockCompiler::Save(const XcfgDocument &document, const QString &coboId, quint32 hardwareVersion,
                         const QString &path, QString *error)
{
    QJsonObject compiled; if (!Compile(document,coboId,hardwareVersion,&compiled,error)) return false;
    const auto bytes = QJsonDocument(compiled).toJson();
    if (bytes.size() > 8 * 1024 * 1024) { *error = "Compiled clock image exceeds 8 MiB"; return false; }
    QFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) { *error = "Cannot create a new artifact: " + output.errorString(); return false; }
    if (output.write(bytes) != bytes.size() || !output.flush()) { *error = output.errorString(); output.close(); QFile::remove(path); return false; }
    error->clear(); return true;
}
