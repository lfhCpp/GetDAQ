// Adapted from CEA GET CoBoNode monitoring/limits and CoBoConfigParser.
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "cobomonitoringconfiguration.h"
#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace GetDAQ {
namespace {
unsigned int Byte(double code, const char *name)
{
    if (!std::isfinite(code) || code < 0 || std::floor(code) > 255)
        throw std::invalid_argument(std::string(name) + " limit cannot be represented by an unsigned 8-bit code");
    return unsigned(std::floor(code));
}
void Range(double low, double high, const char *name)
{
    if (!std::isfinite(low) || !std::isfinite(high) || low < 0 || low > high)
        throw std::invalid_argument(std::string(name) + " limits must be finite, nonnegative and ordered");
}
int Temperature(std::uint64_t raw) { return raw >= 128 ? int(raw)-256 : int(raw); }
}
double ParseMonitoringReal(const std::string &text, const std::string &name)
{
    if (text.empty() || text.size() > 64) throw std::invalid_argument("Invalid monitoring decimal: " + name);
    for (char c : text) if ((c < '0' || c > '9') && c != '+' && c != '-' && c != '.' && c != 'e' && c != 'E')
        throw std::invalid_argument("Invalid monitoring decimal: " + name);
    std::istringstream stream(text); stream.imbue(std::locale::classic()); stream >> std::noskipws;
    double value = 0; stream >> value;
    if (!stream || !stream.eof() || !std::isfinite(value)) throw std::invalid_argument("Invalid finite monitoring decimal: " + name);
    return value;
}
unsigned int CompileAsAdMonitoringItem(const std::string &name)
{
    if (name == "VDD1") return 0;
    if (name == "Tint") return 1;
    if (name == "Text") return 2;
    if (name == "VDD") return 4;
    if (name == "IDD") return 5;
    throw std::invalid_argument("Unknown AsAd monitoring item: " + name);
}
unsigned int CompileAsAdVddLimit(double volts) { return Byte(512.0 * volts / 7.524,"VDD"); }
unsigned int CompileAsAdIddLimit(double amps) { return Byte(193.28 * amps / 2.28,"IDD"); }
CoBoMonitoringImage CompileCoBoMonitoring(const CoBoMonitoringSettings &s)
{
    if (s.ledPolicy != CoBoMonitoringSettings::NoLedManager && s.ledPolicy != CoBoMonitoringSettings::RequiredLedManager)
        throw std::invalid_argument("Unknown LED manager policy");
    if (s.ledPolicy == CoBoMonitoringSettings::RequiredLedManager && s.ledHardwareVersion != 2 && s.ledHardwareVersion != 5)
        throw std::invalid_argument("Received LED manager only supports hardware version 2 or 5");
    if (s.asadMask == 0 || s.asadMask > 15) throw std::invalid_argument("Monitoring AsAd mask must be 1..15");
    if (s.modeMapping != CoBoMonitoringSettings::DeclaredParameters && s.modeMapping != CoBoMonitoringSettings::LegacyCallArguments)
        throw std::invalid_argument("Unknown monitoring mode mapping");
    if (s.powerDelayMs > 60000 || s.settleAttempts == 0 || s.settleAttempts > 100 || s.settleDelayMs > 60000 || s.zeroReadRetries == 0 || s.zeroReadRetries > 10)
        throw std::invalid_argument("Invalid bounded power/monitoring timing");
    unsigned int seen = 0; CoBoMonitoringImage image; image.asadMask = s.asadMask; image.modeMapping = s.modeMapping;
    unsigned int count = 0;
    for (const auto &b : s.boards) {
        if (b.asadId > 3 || !(s.asadMask & (1U << b.asadId)) || (seen & (1U << b.asadId)))
            throw std::invalid_argument("Invalid, inactive or duplicate monitoring AsAd ID");
        seen |= 1U << b.asadId; ++count;
        Range(b.vddLower,b.vddUpper,"VDD"); Range(b.iddLower,b.iddUpper,"IDD");
        if (!std::isfinite(b.vddSettleLower) || b.vddSettleLower < 0)
            throw std::invalid_argument("VDD settle voltage must be finite and nonnegative");
        if (b.externalTemperatureLower < -128 || b.externalTemperatureUpper > 127 || b.externalTemperatureLower > b.externalTemperatureUpper)
            throw std::invalid_argument("Text limits must be ordered signed temperatures -128..127 Celsius");
        AsAdMonitoringBoardImage board; board.asadId = b.asadId; board.checkPowerSupply = b.checkPowerSupply;
        board.limits = {{unsigned(b.externalTemperatureLower < 0 ? b.externalTemperatureLower+256 : b.externalTemperatureLower),
            unsigned(b.externalTemperatureUpper < 0 ? b.externalTemperatureUpper+256 : b.externalTemperatureUpper),
            CompileAsAdVddLimit(b.vddLower),CompileAsAdVddLimit(b.vddUpper),CompileAsAdIddLimit(b.iddLower),CompileAsAdIddLimit(b.iddUpper)}};
        const double vddStep = (2.28/256.0)*(3.3/2.0);
        double required = std::ceil(b.vddSettleLower/vddStep);
        // 浮点临界点按实际读取公式修正，不能把刚好低于请求的一个码接受为已稳定。
        while (required <= 255 && required*vddStep < b.vddSettleLower) ++required;
        while (required > 0 && (required-1)*vddStep >= b.vddSettleLower) --required;
        if (required > 255) throw std::invalid_argument("VDD settle voltage cannot be reached by an 8-bit reading");
        board.vddSettleMinimum = unsigned(required);
        const unsigned int item = CompileAsAdMonitoringItem(b.singleChannelItem);
        if (s.modeMapping == CoBoMonitoringSettings::DeclaredParameters) {
            board.configuration2 = item | (unsigned(b.singleChannelModeEnabled)<<4) | (unsigned(b.averagingDisabled)<<5);
            board.configuration3 = unsigned(b.fastConversionEnabled);
        } else {
            // 旧调用(enableAlarms,fast,item,single,averaging)对应形参(enableAlarms,averaging,single,item,fast)。
            board.configuration2 = unsigned(b.singleChannelModeEnabled) | (unsigned(item != 0)<<4) | (unsigned(b.fastConversionEnabled)<<5);
            board.configuration3 = unsigned(b.averagingDisabled);
        }
        board.protectionMask = b.alarmsEnabled ? 0x23 : 0xff;
        if (b.alarmsEnabled) {
            const unsigned int bits[] = {2,3,4,6,7};
            for (unsigned int i = 0; i < 5; ++i) if (b.disableAlerts[i]) board.protectionMask |= 1U << bits[i];
        }
        board.protectionConfiguration1 = b.alarmsEnabled ? 0x0d : 0x2d;
        image.boards.push_back(board);
    }
    if (seen != s.asadMask) throw std::invalid_argument("Missing active monitoring AsAd");
    const std::uint64_t maximumDelay = std::uint64_t(s.powerDelayMs)*(4+count) + std::uint64_t(count)*s.settleAttempts*s.settleDelayMs;
    if (maximumDelay > 300000 || std::uint64_t(count)*s.settleAttempts*s.zeroReadRetries > 4096)
        throw std::invalid_argument("Monitoring timing exceeds 300 seconds or 4096 reads");
    std::sort(image.boards.begin(),image.boards.end(),[](const AsAdMonitoringBoardImage &a,const AsAdMonitoringBoardImage &b) { return a.asadId < b.asadId; });
    return image;
}
AsAdMeasurements DecodeAsAdMeasurements(const std::array<std::uint64_t,5> &raw)
{
    for (auto value : raw) if (value > 255) throw std::invalid_argument("AsAd monitoring sample exceeds 8 bits");
    AsAdMeasurements result; result.vddVolts = (2.28/256.0)*(3.3/2.0)*raw[0];
    result.vdd1Volts = raw[1] ? 0.06+7.0*raw[1]/256.0 : 0.0; // 原程序的零码表示读取未就绪，不假装已经读到 0.06V。
    result.iddAmps = (2.28/256.0)*(1/0.755)*raw[2];
    result.internalTemperatureC = Temperature(raw[3]); result.externalTemperatureC = Temperature(raw[4]);
    return result;
}
}
