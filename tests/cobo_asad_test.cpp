#include "cobo_configure_fixture.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace GetDAQ;
namespace {
void Require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
template<class F> void Rejects(F f) { try { f(); } catch (const std::invalid_argument &) { return; } throw std::runtime_error("Expected invalid settings rejection"); }
}
int main()
{
    try {
        Require(CompileAsAdInspection("ADC", "SPI_CS", "CAS") == 0x05, "ADC inspection reference");
        Require(CompileAsAdInspection("LCM", "SPI_CK1", "MOSI1") == 0x6f, "LCM special signal names");
        Require(CompileAsAdInspection("AGET2", "DIN", "A2C") == 0xda, "AGET inspection reference");
        Require(CompileAsAdInspection("Multiplicity", "CKO", "DISCT3") == 0xff, "Multiplicity uses 1+3 bits");
        Require(CompileAsAdInspection("Multiplicity", "TRGint", "DISCT0") == 0xf4, "Multiplicity low ISP2 bits");
        const char *devices[] = {"ADC", "DAC", "TCM", "MONITOR", "ID", "IM", "LCM", "SCA0", "SCA1", "SCA2", "SCA3", "AGET0", "AGET1", "AGET2", "AGET3"};
        for (unsigned int id = 0; id < 15; ++id) {
            const char *first = id < 7 ? "CKS" : id < 11 ? "CKW" : "CKP";
            const char *second = id < 6 ? "SPI_CK" : id == 6 ? "SPI_CK2" : id < 11 ? "WCK" : "SC_CK";
            Require(CompileAsAdInspection(devices[id], first, second) == id * 16, "Inspection module index changed");
        }
        Rejects([] { CompileAsAdInspection("ADC", "spi_cs", "CAS"); });
        Rejects([] { CompileAsAdInspection("Multiplicity", "MOSI", "DISCT0"); });
        Rejects([] { CompileAsAdInspection("OTHER", "CKS", "CAS"); });
        Require(ParseAsAdVoltage("-12.9", "v") == -12 && ParseAsAdVoltage("+12.9", "v") == 12, "Voltage truncation changed");
        Require(ParseAsAdVoltage("1e3", "v") == 1000 && ParseAsAdVoltage("-.5", "v") == 0, "Finite decimal conversion");
        for (const char *s : {"nan", "inf", "-1000.1", "1000.1", "1 0", " 1", "1 ", "1mV", "0x10", "1e999", "--1", "", "."})
            Rejects([s] { ParseAsAdVoltage(s, "v"); });
        Require(AsAdDacCode(0) == 8089 && AsAdDacCode(-1000) == 15430 && AsAdDacCode(1000) == 748, "DAC legacy reference values");
        for (int v = -999; v <= 1000; ++v) Require(AsAdDacCode(v) < AsAdDacCode(v - 1), "DAC voltage ordering");
        Rejects([] { AsAdDacCode(1001); });
        for (unsigned int mask = 1; mask < 16; ++mask) {
            auto s = GetDAQTest::Configure(mask); std::reverse(s.asad.boards.begin(), s.asad.boards.end());
            auto image = CompileCoBoConfigure(s); unsigned int found = 0;
            for (const auto &b : image.asad.boards) { Require(!(found & (1U << b.asadId)), "Duplicate board"); found |= 1U << b.asadId; }
            Require(found == mask && image.asad.boards.front().asadId <= image.asad.boards.back().asadId, "Mask/sorting");
            Require(image.asad.pulser.periodMs == 1000 && !image.asad.pulser.periodic && image.asad.pulser.dac == 15430, "Pulser defaults");
        }
        auto s = GetDAQTest::Configure(11); s.asad.generator["externalTriggerMode"] = "true";
        s.asad.generator["periodicModeEnabled"] = "true"; s.asad.generator["doublePulseEnabled"] = "true";
        s.asad.generator["poissonModeEnabled"] = "true"; s.asad.generator["period_ms"] = "4294967295";
        auto image = CompileCoBoConfigure(s);
        Require(image.asad.pulser.periodMs == UINT32_MAX && image.asad.pulser.external && image.asad.pulser.doublePulse && image.asad.pulser.poisson, "Pulser flags/period width");
        const auto base = s;
        for (const auto &entry : std::map<std::string, std::string>{{"amplitudeStart", "1001"}, {"defaultVoltage", "-1001"}, {"secondPulseAmplitude", "1000.5"},
                {"period_ms", "0"}, {"externalTriggerMode", "yes"}, {"unknown", "1"}}) {
            s = base; s.asad.generator[entry.first] = entry.second; Rejects([&] { CompileCoBoConfigure(s); });
        }
        s = base; s.asad.boards.pop_back(); Rejects([&] { CompileCoBoConfigure(s); });
        s = base; s.asad.boards.push_back(s.asad.boards[0]); Rejects([&] { CompileCoBoConfigure(s); });
        s = base; s.asad.boards[0].triggerDelay = 32; Rejects([&] { CompileCoBoConfigure(s); });
        s = base; s.asad.boards[0].capacitor = "10nF"; Rejects([&] { CompileCoBoConfigure(s); });
        s = base; s.asad.boards[0].capacitor = "10pF"; Require(CompileCoBoConfigure(s).asad.boards[0].capacitor11pF, "Legacy capacitor alias");
        s = base; s.asad.asadMask = 15; CoBoAsAdBoardSettings b; b.asadId = 2; s.asad.boards.push_back(b); Rejects([&] { CompileCoBoConfigure(s); });
        std::cout << "PASS CoBo AsAd: original inspection/DAC references, all active masks, finite voltages, bounds and shared masks\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
