#include "mutantconfiguration.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace GetDAQ;
namespace {
void Require(bool ok,const char *text) { if (!ok) throw std::runtime_error(text); }
template<class F> void Rejects(F call) { try { call(); } catch (const std::invalid_argument &) { return; } throw std::runtime_error("Expected MuTaNT rejection"); }
}
int main()
{
    try {
        MutantSettings s; auto image = CompileMutant(s);
        Require(image.globalConfiguration == 0xfd && image.level0GateDuration == 1 && image.level1bTimeout == 1,"Default MuTaNT reference");
        for (unsigned int raw = 0; raw <= 65535; ++raw) {
            const double us = raw/100.0;
            Require(MutantTenNsCode(us) == unsigned(std::floor(100.0f*static_cast<float>(us))),"10ns legacy float mismatch");
            Require(MutantL1bTimeoutCode(raw*2.56) == raw,"L1B representable code mismatch");
        }
        for (unsigned int flags = 0; flags < 512; ++flags) {
            s = MutantSettings(); s.level0Enabled = flags&1; s.level1Enabled = flags&2; s.level2Enabled = flags&4;
            s.hitPatternModified = flags&8; s.level1BeforeLevel0 = flags&16; s.twoProtonMode = flags&32; s.halfEventReadout = flags&64;
            s.masterPortAEnabled = flags&128; s.masterPortBEnabled = flags&256;
            image = CompileMutant(s);
            const unsigned expected = 1 | (unsigned(s.masterPortAEnabled)<<2) | (unsigned(s.masterPortBEnabled)<<3) |
                ((flags&7)<<4) | (unsigned(s.level2Enabled && s.hitPatternModified)<<7) | (unsigned(s.level1BeforeLevel0)<<8) |
                (unsigned(s.twoProtonMode)<<11) | (unsigned(s.twoProtonMode && s.halfEventReadout)<<12);
            Require(image.globalConfiguration == expected,"MuTaNT GLOBAL_CONF bit reference");
        }
        const char *selections[] = {"LOW","HIGH","OR","AND"};
        for (unsigned int i = 0; i < 4; ++i) { s = MutantSettings(); s.level1Selection = selections[i]; Require(((CompileMutant(s).globalConfiguration>>9)&3) == i,"v1.5 L1 mapping"); }
        const char *clocks[] = {"FireWire1","FireWire2","VHDCI","Optical","FireWireA","FireWireB"}; const unsigned codes[] = {0,1,2,3,0,1};
        for (unsigned int i = 0; i < 6; ++i) { s.externalClockSelection = clocks[i]; Require(CompileMutant(s).externalClockSource == codes[i],"Clock alias reference"); }
        s.externalClockSelection = "FireWire3"; Rejects([&] { CompileMutant(s); }); s.clockPolicy = MutantSettings::LegacyVhdciFallback;
        Require(CompileMutant(s).externalClockSourceFallback && CompileMutant(s).externalClockSource == 2,"Explicit legacy fallback missing");
        s = MutantSettings(); s.isSlave = true; Rejects([&] { CompileMutant(s); }); s.level0Enabled = s.level1Enabled = s.level2Enabled = false;
        for (unsigned int port = 0; port < 2; ++port) for (unsigned int id = 0; id < 2; ++id) {
            s.slavePortA = port; s.slaveId = id; s.moduleCount = 2; image = CompileMutant(s);
            Require(image.globalConfiguration == ((id<<1) | (port ? 4 : 8)) && image.wscaSource == (port ? 1U : 2U) && !image.promptWsca && image.clockPathValue == 5,"Slave control reference");
        }
        s.centrumScalerMode = MutantSettings::Centrum; Rejects([&] { CompileMutant(s); });
        s = MutantSettings(); s.timestamp10ns = 0x123456789abcULL; s.eventNumber = 0x76543210; s.gmcShelfMask = 0xabc; s.wscaShelfMask = 0x5f0;
        s.inspection = {{0x12,0x34,0x56,0x78}}; s.layout = MutantSettings::LegacyNamesV15; image = CompileMutant(s);
        Require(image.timestampWords == std::array<unsigned,3>{{0x9abc,0x5678,0x1234}} && image.eventWords == std::array<unsigned,2>{{0x3210,0x7654}},"Counter split precision/order");
        Require(image.shelfMask == (0xabc&0x5f0) && image.frontInspection == 0x3412 && image.rearInspection == 0x7856 && std::string(image.shelfRegister) == "MASTER_MASK","Shelf/inspection/layout reference");
        for (double bad : {-1.,655.350001,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) Rejects([&] { MutantTenNsCode(bad); });
        Rejects([] { MutantTenNsCode(0,true); }); Rejects([] { MutantL1bTimeoutCode(167769.600001); }); Rejects([] { MutantL1bTimeoutCode(-0.01); });
        s.timestamp10ns = 1ULL<<48; Rejects([&] { CompileMutant(s); }); s.timestamp10ns = 0; s.eventNumber = 1ULL<<32; Rejects([&] { CompileMutant(s); });
        s = MutantSettings(); s.layout = static_cast<MutantSettings::RegisterLayout>(99); Rejects([&] { CompileMutant(s); });
        s = MutantSettings(); s.bucketCounts[0] = 512; Rejects([&] { CompileMutant(s); });
        s = MutantSettings(); s.gmcShelfMask = 4096; Rejects([&] { CompileMutant(s); });
        s = MutantSettings(); s.frameType = 3; Rejects([&] { CompileMutant(s); });
        std::cout << "PASS MuTaNT core: 65536 timing codes, 512 global combinations, master/slave, clock aliases, masks/counters and rejected overflow\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
