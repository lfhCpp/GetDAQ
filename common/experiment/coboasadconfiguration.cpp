// Adapted from CEA GET CoBoConfigParser, asad/utl and AsAdPulserMgrI.
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "coboasadconfiguration.h"
#include <algorithm>
#include <cmath>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
namespace GetDAQ {
namespace {
struct InspectionSignals {
    const char *device;
    const char *isp1[4];
    const char *isp2[8];
};
const InspectionSignals signals[] = {
    {"ADC", {"CKS", "SPI_CS", "MOSI", "CKR"}, {"SPI_CK", "CAS", "ACK", "RCK"}},
    {"DAC", {"CKS", "SPI_CS", "MOSI", "MISO"}, {"SPI_CK", "CAS", "ACS", "RESET_FAST"}},
    {"TCM", {"CKS", "SPI_CS", "MOSI", "MISO"}, {"SPI_CK", "CAS", "ACS", "CKO"}},
    {"MONITOR", {"CKS", "SPI_CS", "MOSI", "MISO"}, {"SPI_CK", "CAS", "ACS", "IDLE"}},
    {"ID", {"CKS", "SPI_CS", "MOSI", "MISO"}, {"SPI_CK", "CAS", "ACS", "IDLE"}},
    {"IM", {"CKS", "SPI_CS", "MOSI", "MISO"}, {"SPI_CK", "CAS", "ACS", "IDLE"}},
    {"LCM", {"CKS", "SPI_CS2", "MOSI2", "SPI_CK1"}, {"SPI_CK2", "CAS", "SPI_CS1", "MOSI1"}},
    {"SCA0", {"CKW", "SCW_AGET", "CKR", "SCR_AGET"}, {"WCK", "SCW", "RCK", "SCR"}},
    {"SCA1", {"CKW", "SCW_AGET", "CKR", "SCR_AGET"}, {"WCK", "SCW", "RCK", "SCR"}},
    {"SCA2", {"CKW", "SCW_AGET", "CKR", "SCR_AGET"}, {"WCK", "SCW", "RCK", "SCR"}},
    {"SCA3", {"CKW", "SCW_AGET", "CKR", "SCR_AGET"}, {"WCK", "SCW", "RCK", "SCR"}},
    {"AGET0", {"CKP", "SC_CS", "DIN", "DOUT"}, {"SC_CK", "CA0", "A0C", "LAST_INIT"}},
    {"AGET1", {"CKP", "SC_CS", "DIN", "DOUT"}, {"SC_CK", "CA1", "A1C", "LAST_INIT"}},
    {"AGET2", {"CKP", "SC_CS", "DIN", "DOUT"}, {"SC_CK", "CA2", "A2C", "LAST_INIT"}},
    {"AGET3", {"CKP", "SC_CS", "DIN", "DOUT"}, {"SC_CK", "CA3", "A3C", "LAST_INIT"}},
    {"Multiplicity", {"TRGint", "CKO", nullptr, nullptr}, {"CKO", "CKW", "SCW", "TRGext", "DISCT0", "DISCT1", "DISCT2", "DISCT3"}}
};
std::string Value(const std::map<std::string, std::string> &values, const char *name, const char *fallback)
{
    const auto found = values.find(name);
    if (found != values.end()) return found->second;
    if (fallback) return fallback;
    throw std::invalid_argument(std::string("Missing Generator/") + name);
}
bool Boolean(const std::map<std::string, std::string> &values, const char *name)
{
    const auto value = Value(values, name, "false");
    if (value == "true") return true;
    if (value == "false") return false;
    throw std::invalid_argument(std::string("Generator/") + name + ": expected true or false");
}
}
unsigned int CompileAsAdInspection(const std::string &device, const std::string &isp1, const std::string &isp2)
{
    for (unsigned int id = 0; id < 16; ++id) if (device == signals[id].device) {
        int first = -1, second = -1;
        for (unsigned int i = 0; i < (id == 15 ? 2U : 4U); ++i) if (isp1 == signals[id].isp1[i]) first = int(i);
        for (unsigned int i = 0; i < (id == 15 ? 8U : 4U); ++i) if (isp2 == signals[id].isp2[i]) second = int(i);
        if (first < 0 || second < 0) throw std::invalid_argument("Invalid InspectionLines ISP1/ISP2 for " + device);
        return (id << 4) | (unsigned(first) << (id == 15 ? 3 : 2)) | unsigned(second);
    }
    throw std::invalid_argument("Unknown InspectionLines device: " + device);
}
int ParseAsAdVoltage(const std::string &text, const std::string &name)
{
    if (text.empty() || text.size() > 64 || text.find_first_not_of("0123456789+-.eE") != std::string::npos)
        throw std::invalid_argument(name + ": expected finite decimal voltage in mV");
    std::istringstream input(text); input.imbue(std::locale::classic());
    double value = 0; input >> std::noskipws >> value;
    if (!input || input.peek() != std::char_traits<char>::eof() || !std::isfinite(value) || value < -1000 || value > 1000)
        throw std::invalid_argument(name + ": voltage must be finite and within -1000..1000 mV");
    return static_cast<int>(value); // 原远端接口使用整数 mV；不以四舍五入改变原实验。
}
unsigned int AsAdDacCode(int voltageMv)
{
    if (voltageMv < -1000 || voltageMv > 1000) throw std::invalid_argument("AsAd voltage exceeds -1000..1000 mV");
    // 保留原 AsAdPulserMgrI 的 float 运算和向下取整；14 位 DAC。
    const float factor = (31.6f / 32.0f) * float(0x2000);
    return static_cast<unsigned int>(factor * (1.0f - float(voltageMv) / 1102.0f));
}
CoBoAsAdImage CompileCoBoAsAd(const CoBoAsAdSettings &settings)
{
    if (!settings.asadMask || settings.asadMask > 15) throw std::invalid_argument("Active AsAd mask must be 1..15");
    const std::set<std::string> known = {"amplitudeStart", "defaultVoltage", "externalTriggerMode", "periodicModeEnabled",
        "period_ms", "doublePulseEnabled", "secondPulseAmplitude", "poissonModeEnabled"};
    for (const auto &item : settings.generator) if (!known.count(item.first)) throw std::invalid_argument("Unknown Generator parameter: " + item.first);
    CoBoAsAdImage image; image.asadMask = settings.asadMask;
    auto &pulser = image.pulser;
    pulser.voltageMv = ParseAsAdVoltage(Value(settings.generator, "amplitudeStart", nullptr), "Generator/amplitudeStart");
    pulser.defaultVoltageMv = ParseAsAdVoltage(Value(settings.generator, "defaultVoltage", "0"), "Generator/defaultVoltage");
    pulser.secondVoltageMv = ParseAsAdVoltage(Value(settings.generator, "secondPulseAmplitude", "0"), "Generator/secondPulseAmplitude");
    pulser.dac = AsAdDacCode(pulser.voltageMv); pulser.defaultDac = AsAdDacCode(pulser.defaultVoltageMv); pulser.secondDac = AsAdDacCode(pulser.secondVoltageMv);
    pulser.external = Boolean(settings.generator, "externalTriggerMode"); pulser.periodic = Boolean(settings.generator, "periodicModeEnabled");
    pulser.doublePulse = Boolean(settings.generator, "doublePulseEnabled"); pulser.poisson = Boolean(settings.generator, "poissonModeEnabled");
    pulser.periodMs = ParseCoBoUnsigned(Value(settings.generator, "period_ms", "1000"), "Generator/period_ms");
    if (!pulser.periodMs) throw std::invalid_argument("Generator/period_ms must be positive");
    unsigned int seen = 0;
    for (const auto &board : settings.boards) {
        if (board.asadId > 3 || !(settings.asadMask & (1U << board.asadId)) || (seen & (1U << board.asadId)))
            throw std::invalid_argument("Duplicate, inactive or invalid AsAd board");
        seen |= 1U << board.asadId;
        if (board.triggerDelay > 31) throw std::invalid_argument("AsAd Generator/triggerDelay exceeds 31 WCK periods");
        if (board.capacitor != "1pF" && board.capacitor != "11pF" && board.capacitor != "10pF")
            throw std::invalid_argument("AsAd Generator/injectValue must be 1pF or 11pF (legacy 10pF accepted)");
        CoBoAsAdBoardImage entry; entry.asadId = board.asadId; entry.triggerDelay = board.triggerDelay;
        entry.calibrationMode = board.calibrationMode; entry.capacitor11pF = board.capacitor != "1pF";
        entry.inspection = CompileAsAdInspection(board.inspectionDevice, board.isp1, board.isp2); image.boards.push_back(entry);
    }
    if (seen != settings.asadMask) throw std::invalid_argument("AsAd settings must cover every active board");
    std::sort(image.boards.begin(), image.boards.end(), [](const CoBoAsAdBoardImage &a, const CoBoAsAdBoardImage &b) { return a.asadId < b.asadId; });
    return image;
}
CoBoConfigureImage CompileCoBoConfigure(const CoBoConfigureSettings &settings)
{
    CoBoConfigureImage result; result.readout = CompileCoBoReadout(settings.readout); result.asad = CompileCoBoAsAd(settings.asad);
    if (result.readout.aget.asadMask != result.asad.asadMask) throw std::invalid_argument("AGET/readout and AsAd masks must match");
    return result;
}
}
