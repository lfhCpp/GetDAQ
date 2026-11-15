#include "cobomonitoringconfiguration.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace GetDAQ;
namespace {
void Require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
template<class F> void Rejects(F call) {
    try { call(); } catch (const std::invalid_argument &) { return; }
    throw std::runtime_error("Expected invalid monitoring settings rejection");
}
CoBoMonitoringSettings Settings(unsigned int mask = 1) {
    CoBoMonitoringSettings s; s.asadMask = mask;
    for (unsigned int id = 0; id < 4; ++id) if (mask & (1U << id)) {
        AsAdMonitoringBoardSettings b; b.asadId = id; s.boards.push_back(b);
    }
    return s;
}
}
int main()
{
    try {
        const auto standard = CompileCoBoMonitoring(Settings()).boards.at(0);
        Require(standard.limits == std::array<unsigned int,6>{{17,50,204,238,0,169}}, "Legacy threshold reference");
        Require(standard.vddSettleMinimum == 205 && standard.protectionMask == 0x23 && standard.protectionConfiguration1 == 0x0d, "Settle ceil/protection polarity");
        const double gain = (2.28/256.0)*(3.3/2.0);
        for (unsigned int code = 0; code <= 255; ++code) {
            auto s = Settings(); s.boards[0].vddSettleLower = code*gain;
            Require(CompileCoBoMonitoring(s).boards[0].vddSettleMinimum == code, "Exact settle code lost");
            if (code < 255) {
                s.boards[0].vddSettleLower = std::nextafter(code*gain,std::numeric_limits<double>::infinity());
                Require(CompileCoBoMonitoring(s).boards[0].vddSettleMinimum == code+1, "Below-request code accepted");
            }
            const auto decoded = DecodeAsAdMeasurements({{code,code,code,code,code}});
            Require(decoded.vddVolts == gain*code && decoded.iddAmps == (2.28/256.0)*(1/0.755)*code, "Measurement gain differs");
            Require(decoded.vdd1Volts == (code ? 0.06+7.0*code/256.0 : 0.0), "Unready VDD1 zero gained offset");
            Require(decoded.externalTemperatureC == (code >= 128 ? int(code)-256 : int(code)), "Signed temperature decode");
        }
        for (unsigned int mask = 1; mask < 16; ++mask) {
            auto s = Settings(mask); std::reverse(s.boards.begin(),s.boards.end()); const auto image = CompileCoBoMonitoring(s);
            unsigned int seen = 0, previous = 0;
            for (const auto &b : image.boards) { Require(b.asadId >= previous, "Unsorted monitoring boards"); seen |= 1U << b.asadId; previous = b.asadId; }
            Require(seen == mask, "Active coverage differs");
        }
        const char *items[] = {"VDD1","Tint","Text","VDD","IDD"}; const unsigned int codes[] = {0,1,2,4,5};
        for (unsigned int i = 0; i < 5; ++i) for (unsigned int flags = 0; flags < 8; ++flags) {
            auto s = Settings(); auto &b = s.boards[0]; b.singleChannelItem = items[i];
            b.fastConversionEnabled = (flags & 1) != 0; b.singleChannelModeEnabled = (flags & 2) != 0; b.averagingDisabled = (flags & 4) != 0;
            const auto declared = CompileCoBoMonitoring(s).boards[0];
            Require(declared.configuration2 == (codes[i] | (b.singleChannelModeEnabled ? 16U : 0U) | (b.averagingDisabled ? 32U : 0U)) && declared.configuration3 == unsigned(b.fastConversionEnabled), "Declared mode mapping");
            s.modeMapping = CoBoMonitoringSettings::LegacyCallArguments; const auto legacy = CompileCoBoMonitoring(s).boards[0];
            Require(legacy.configuration2 == (unsigned(b.singleChannelModeEnabled) | (codes[i] ? 16U : 0U) | (b.fastConversionEnabled ? 32U : 0U)) && legacy.configuration3 == unsigned(b.averagingDisabled), "Legacy bool/item conversion");
        }
        const unsigned int bits[] = {2,3,4,6,7};
        for (unsigned int mask = 0; mask < 32; ++mask) {
            auto s = Settings(); unsigned int expected = 0x23;
            for (unsigned int i = 0; i < 5; ++i) if (mask & (1U << i)) { s.boards[0].disableAlerts[i] = true; expected |= 1U << bits[i]; }
            Require(CompileCoBoMonitoring(s).boards[0].protectionMask == expected, "Protection disabling bits");
            s.boards[0].alarmsEnabled = false; const auto off = CompileCoBoMonitoring(s).boards[0];
            Require(off.protectionMask == 0xff && off.protectionConfiguration1 == 0x2d, "Alarm disabling polarity");
        }
        auto signedSettings = Settings(); signedSettings.boards[0].externalTemperatureLower = -128; signedSettings.boards[0].externalTemperatureUpper = -1;
        Require(CompileCoBoMonitoring(signedSettings).boards[0].limits[0] == 128 && CompileCoBoMonitoring(signedSettings).boards[0].limits[1] == 255, "Negative threshold wire code");
        for (const char *text : {"0", "3.2", "+3.2", "-12", ".5", "1e-3"}) Require(std::isfinite(ParseMonitoringReal(text,"test")), "Finite decimal rejected");
        for (const char *text : {"", " ", "3.2V", "nan", "inf", "0x10", "1e999", "--1", ".", "1 ", " 1", "1,2"}) Rejects([text] { ParseMonitoringReal(text,"test"); });
        Rejects([] { CompileAsAdMonitoringItem("vdd"); });
        for (double bad : {-0.001,4.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
            Rejects([bad] { CompileAsAdVddLimit(bad); }); Rejects([bad] { CompileAsAdIddLimit(bad); });
        }
        for (unsigned int field = 0; field < 5; ++field) { std::array<std::uint64_t,5> raw{{0,0,0,0,0}}; raw[field] = 256; Rejects([&] { DecodeAsAdMeasurements(raw); }); }
        auto invalid = Settings(); invalid.asadMask = 0; Rejects([&] { CompileCoBoMonitoring(invalid); });
        invalid = Settings(); invalid.asadMask = 16; Rejects([&] { CompileCoBoMonitoring(invalid); });
        invalid = Settings(3); invalid.boards.pop_back(); Rejects([&] { CompileCoBoMonitoring(invalid); });
        invalid = Settings(); invalid.boards.push_back(invalid.boards[0]); Rejects([&] { CompileCoBoMonitoring(invalid); });
        invalid = Settings(); invalid.boards[0].asadId = 4; Rejects([&] { CompileCoBoMonitoring(invalid); });
        invalid = Settings(); invalid.boards[0].vddLower = 3.6; Rejects([&] { CompileCoBoMonitoring(invalid); });
        invalid = Settings(); invalid.boards[0].iddLower = 2.1; Rejects([&] { CompileCoBoMonitoring(invalid); });
        invalid = Settings(); invalid.boards[0].externalTemperatureUpper = 128; Rejects([&] { CompileCoBoMonitoring(invalid); });
        invalid = Settings(); invalid.boards[0].externalTemperatureLower = -129; Rejects([&] { CompileCoBoMonitoring(invalid); });
        invalid = Settings(); invalid.boards[0].vddSettleLower = 256*gain; Rejects([&] { CompileCoBoMonitoring(invalid); });
        invalid = Settings(); invalid.modeMapping = static_cast<CoBoMonitoringSettings::ModeMapping>(99); Rejects([&] { CompileCoBoMonitoring(invalid); });
        invalid = Settings(); invalid.ledPolicy = static_cast<CoBoMonitoringSettings::LedPolicy>(99); Rejects([&] { CompileCoBoMonitoring(invalid); });
        invalid = Settings(); invalid.ledPolicy = CoBoMonitoringSettings::RequiredLedManager; invalid.ledHardwareVersion = 3;
        Rejects([&] { CompileCoBoMonitoring(invalid); });
        for (unsigned int version : {2,5}) { invalid.ledHardwareVersion = version; CompileCoBoMonitoring(invalid); }
        invalid.ledPolicy = CoBoMonitoringSettings::NoLedManager; invalid.ledHardwareVersion = 99; CompileCoBoMonitoring(invalid);
        invalid = Settings(); invalid.settleAttempts = 0; Rejects([&] { CompileCoBoMonitoring(invalid); });
        invalid = Settings(); invalid.zeroReadRetries = 11; Rejects([&] { CompileCoBoMonitoring(invalid); });
        invalid = Settings(15); invalid.settleAttempts = 100; invalid.settleDelayMs = 1000; Rejects([&] { CompileCoBoMonitoring(invalid); });
        std::cout << "PASS AsAd monitoring: 15 masks, mode/alert combinations, threshold references, 256 signed/raw/settle boundaries and rejected overflow\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
