#include "agetconfiguration.h"
#include "coboagetconfiguration.h"
#include "aget_fixture.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void Check(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}
using GetDAQTest::Neutral;
template<class F> void Reject(F f, const char *expected)
{
    try { f(); } catch (const std::invalid_argument &e) {
        Check(std::string(e.what()).find(expected) != std::string::npos, "wrong rejection reason"); return;
    }
    throw std::runtime_error("invalid input was accepted");
}
void Globals()
{
    const auto neutral = Neutral(); const auto image = GetDAQ::CompileAget(neutral);
    const char *names[13] = {"reg1", "reg2", "reg3", "reg4", "reg6", "reg7", "reg8", "reg8msb", "reg9", "reg9msb", "reg10", "reg11", "reg12"};
    const unsigned int widths[13] = {32, 32, 34, 34, 64, 64, 64, 64, 64, 64, 64, 64, 9};
    Check(image.registers.size() == 13 && image.moduleReadoutDepth == 512 && image.registerStageDepth == 512, "neutral image");
    for (unsigned int i = 0; i < 13; ++i) Check(image.registers[i].name == names[i] && image.registers[i].widthBits == widths[i] && !image.registers[i].value, "neutral register layout/value");
    struct Vector { const char *key, *value; unsigned int reg; std::uint64_t expected; };
    const Vector vectors[] = {
        {"Reg1/isIcsax2", "true", 0, 0x1}, {"Reg1/TestModeRange", "240fF", 0, 0x2}, {"Reg1/TestModeRange", "1pF", 0, 0x4}, {"Reg1/TestModeRange", "10 pC", 0, 0x6},
        {"Reg1/TestModeSelection", "functionality", 0, 0x180}, {"Reg1/IntegrationMode", "fixed_500ns", 0, 0x200},
        {"Reg1/SCA_Pointer", "anySCAPointer", 0, 0xc00}, {"Reg1/is32channels", "true", 0, 0x2000},
        {"Reg1/isAllChannelRead", "true", 0, 0x4000}, {"Reg1/isFPNRead", "true", 0, 0x8000}, {"Reg1/isPositivePolarity", "true", 0, 0x10000},
        {"Reg1/Vicm", "1.65V", 0, 0x60000}, {"Reg1/GlobalThresholdValue", "7", 0, 0x380000},
        {"Reg1/isThresholdSignedPositive", "true", 0, 0x400000}, {"Reg1/TriggerVetoOption", "undefined", 0, 0x1800000},
        {"Reg1/isckwriteSynchro", "true", 0, 0x2000000}, {"Reg1/isTOTActive", "true", 0, 0x4000000},
        {"Reg1/isTriggerWidth200ns", "true", 0, 0x8000000}, {"Reg1/TriggerWidthRange", "width3", 0, 0x30000000},
        {"Reg1/ExternalLink", "CSAStandby", 0, 0xc0000000}, {"Reg2/DebugSelection", "none", 1, 0x7},
        {"Reg2/isReadFromColumn0", "true", 1, 0x8}, {"Reg2/isDigitalOutputTest", "true", 1, 0x10},
        {"Reg2/isResetLevelUndefined", "true", 1, 0x20}, {"Reg2/isMarkerEnable", "true", 1, 0x40},
        {"Reg2/isResetLevelVdd", "true", 1, 0x80}, {"Reg2/isBoostPower", "true", 1, 0x100},
        {"Reg2/isOutputscCkSynchro", "true", 1, 0x200}, {"Reg2/isSynchroEdgeFalling", "true", 1, 0x400},
        {"Reg2/isSCoutputBufferInTriState", "true", 1, 0x800}, {"Reg2/currentBias", "current3", 1, 0x3000},
        {"Reg2/bufferCurrent", "4.870mA", 1, 0xc000}, {"Reg2/isPWDonWrite", "true", 1, 0x10000},
        {"Reg2/isPWDonRead", "true", 1, 0x20000}, {"Reg2/isAlternatePower", "true", 1, 0x40000},
        {"Reg2/is1bitShortResetReadSeq", "true", 1, 0x80000}, {"Reg2/isTriggerOutputDisable", "true", 1, 0x100000},
        {"Reg2/isRisingedgeWriteAutoresetBank", "true", 1, 0x200000}, {"Reg2/islvdsTriggerOutput", "true", 1, 0x400000},
        {"Reg2/inputDynamicRange_5pct", "true", 1, 0x1000000}
    };
    for (const auto &v : vectors) {
        auto s = neutral; s.globals[v.key] = v.value; const auto result = GetDAQ::CompileAget(s);
        Check(result.registers[v.reg].value == v.expected, v.key);
        for (unsigned int i = 0; i < 13; ++i) if (i != v.reg) Check(!result.registers[i].value, "isolated global changed another register");
    }
    const unsigned int times[2][16] = {{70, 117, 232, 280, 334, 383, 502, 541, 568, 632, 721, 760, 831, 870, 976, 1014},
                                     {68, 116, 180, 225, 343, 442, 541, 641, 739, 836, 943, 1039, 2034, 3985, 5770, 7700}};
    for (unsigned int kind = 0; kind < 2; ++kind) for (unsigned int i = 0; i < 16; ++i) {
        auto s = neutral; s.chipVersion = kind ? 0x203 : 0x201; s.globals["Reg1/peackingTime"] = std::to_string(times[kind][i]);
        Check(GetDAQ::CompileAget(s).registers[0].value == 8 * i, "chip-specific peaking time");
    }
    auto astre = neutral; astre.chipVersion = 0x203; astre.globals["Reg1/peackingTime"] = "68";
    astre.globals["Reg2/triggerLvdsThreshold"] = "8"; astre.globals["Reg2/multiplicityLevel"] = "4";
    Check(GetDAQ::CompileAget(astre).registers[1].value == 0x16000000, "ASTRE multiplicity fields");
    auto split = neutral; split.twoPulseMode = true; split.globals["Reg1/SCA_Splitting"] = "true";
    split.globals["ReadoutPointerOffset"] = "255"; split.readoutDepth = 1;
    const auto two = GetDAQ::CompileAget(split);
    Check(two.registers[0].value == 0x1000 && two.registers[12].value == 255 && two.registerStageDepth == 1 && two.moduleReadoutDepth == 1, "2p offset/depth");
    auto offset = neutral; offset.globals["ReadoutPointerOffset"] = "511"; offset.readoutDepth = 1;
    Check(GetDAQ::CompileAget(offset).registers[12].value == 511, "9-bit readout offset");
}
void Channels()
{
    const int physical[68] = {0,1,2,3,4,5,6,7,8,9,10,-1,11,12,13,14,15,16,17,18,19,20,-1,21,22,23,24,25,26,27,28,29,30,31,
                             32,33,34,35,36,37,38,39,40,41,42,-1,43,44,45,46,47,48,49,50,51,52,-1,53,54,55,56,57,58,59,60,61,62,63};
    for (unsigned int id = 0; id < 68; ++id) {
        Check(GetDAQ::AgetPhysicalChannel(id) == physical[id], "physical channel/FPN mapping");
        auto s = Neutral(); for (auto &c : s.channels) c.active = false;
        auto &c = s.channels[id]; c.active = true; c.selectedForTest = true; c.gain = 3; c.threshold = 15; c.inhibition = 2; c.reading = GetDAQ::AgetChannel::Always;
        const auto image = GetDAQ::CompileAget(s);
        Check(image.registers[id < 34 ? 2 : 3].value == (UINT64_C(1) << (id < 34 ? 33 - id : id - 34)), "test selection including FPN");
        // Golden shifts from the old channel-number tables, independently of AgetPhysicalChannel.
        if (physical[id] >= 0) {
            unsigned int shift = 0;
            if (id < 34) shift = 62 - 2 * id + (id > 11 ? 2 : 0) + (id > 22 ? 2 : 0);
            else shift = 2 * (id - 34) - (id > 45 ? 2 : 0) - (id > 56 ? 2 : 0);
            Check(image.registers[id < 34 ? 4 : 5].value == (UINT64_C(3) << shift), "gain bit order at each non-FPN channel");
            Check(image.registers[id < 34 ? 10 : 11].value == (UINT64_C(2) << shift), "inhibition bit order");
            unsigned int reg = 0, thresholdShift = 0;
            if (id <= 16) { reg = 7; thresholdShift = 60 - 4 * id + (id > 11 ? 4 : 0); }
            else if (id <= 33) { reg = 6; thresholdShift = 60 - 4 * (id - 17) + (id > 22 ? 4 : 0); }
            else if (id <= 50) { reg = 8; thresholdShift = 4 * (id - 34) - (id > 45 ? 4 : 0); }
            else { reg = 9; thresholdShift = 4 * (id - 51) - (id > 56 ? 4 : 0); }
            for (unsigned int r = 6; r <= 9; ++r) Check(image.registers[r].value == (r == reg ? UINT64_C(15) << thresholdShift : 0), "threshold nibble ordering");
        } else {
            for (unsigned int r = 4; r <= 11; ++r) Check(!image.registers[r].value, "FPN must not set physical gain/threshold/inhibition bits");
        }
        const unsigned int group = id < 4 ? 0 : id < 36 ? 1 : 2;
        const unsigned int last = group == 0 ? 3 : group == 1 ? 35 : 67;
        for (unsigned int g = 0; g < 3; ++g) Check(image.readAlways[g] == (g == group ? UINT32_C(1) << (last - id) : 0) && !image.readIfHit[g], "readout groups and bit order");
        c.active = false; const auto disabled = GetDAQ::CompileAget(s);
        for (const auto &reg : disabled.registers) Check(!reg.value, "inactive channel changes register");
        for (unsigned int g = 0; g < 3; ++g) Check(!disabled.readAlways[g] && !disabled.readIfHit[g], "inactive channel changes readout masks");
    }
    auto all = Neutral(); for (auto &c : all.channels) { c.selectedForTest = true; c.gain = 3; c.threshold = 15; c.inhibition = 1; c.reading = GetDAQ::AgetChannel::OnlyIfHit; }
    const auto image = GetDAQ::CompileAget(all);
    Check(image.registers[2].value == UINT64_C(0x3ffffffff) && image.registers[3].value == UINT64_C(0x3ffffffff), "34-bit selection");
    for (unsigned int i = 4; i <= 9; ++i) Check(image.registers[i].value == UINT64_MAX, "full 64-bit fields");
    Check(image.registers[10].value == UINT64_C(0x5555555555555555) && image.registers[11].value == UINT64_C(0x5555555555555555), "all-channel inhibition");
    Check(image.readIfHit[0] == 15 && image.readIfHit[1] == UINT32_MAX && image.readIfHit[2] == UINT32_MAX, "full read-if-hit masks");
    std::reverse(all.channels.begin(), all.channels.end());
    const auto reversed = GetDAQ::CompileAget(all); for (unsigned int i = 0; i < 13; ++i) Check(reversed.registers[i].value == image.registers[i].value, "channel order dependence");
}
void Invalid()
{
    const auto base = Neutral();
    const std::pair<const char *, const char *> invalid[] = {{"Reg1/GlobalThresholdValue", "8"}, {"ReadoutPointerOffset", "512"},
        {"Reg1/isIcsax2", "1"}, {"Reg1/peackingTime", "999"}, {"Reg1/TestModeRange", "120fFgarbage"}, {"Reg1/TestModeSelection", "bad"},
        {"Reg1/TriggerVetoOption", "Undefined"}, {"Reg1/GlobalThresholdValue", "-1"}, {"Reg1/GlobalThresholdValue", "0x7"},
        {"Reg2/currentBias", "current4"}, {"Reg2/bufferCurrent", "9mA"}, {"Reg2/triggerLvdsThreshold", "3"}, {"Reg2/multiplicityLevel", "0"}};
    for (const auto &p : invalid) { auto s = base; s.globals[p.first] = p.second; Reject([&]() { GetDAQ::CompileAget(s); }, p.first); }
    auto s = base; s.globals.erase("Reg1/isIcsax2"); Reject([&]() { GetDAQ::CompileAget(s); }, "missing");
    s = base; s.globals["misspelled"] = "0"; Reject([&]() { GetDAQ::CompileAget(s); }, "unknown");
    s = base; s.chipVersion = 0; Reject([&]() { GetDAQ::CompileAget(s); }, "chipVersion");
    s = base; s.chipVersion = 0x203; Reject([&]() { GetDAQ::CompileAget(s); }, "peackingTime");
    s = base; s.chipVersion = 0x201; s.globals["Reg2/inputDynamicRange_5pct"] = "true"; Reject([&]() { GetDAQ::CompileAget(s); }, "2.1");
    s = base; s.globals["Reg2/triggerLvdsThreshold"] = "2"; Reject([&]() { GetDAQ::CompileAget(s); }, "ASTRE");
    s = base; s.twoPulseMode = true; Reject([&]() { GetDAQ::CompileAget(s); }, "SCA_Splitting");
    s = base; s.globals["ReadoutPointerOffset"] = "1"; Reject([&]() { GetDAQ::CompileAget(s); }, "readoutDepth");
    s = base; s.readoutDepth = 0; Reject([&]() { GetDAQ::CompileAget(s); }, "readoutDepth");
    s = base; s.readoutDepth = -2; Reject([&]() { GetDAQ::CompileAget(s); }, "readoutDepth");
    s = base; s.channels.pop_back(); Reject([&]() { GetDAQ::CompileAget(s); }, "68 channels");
    s = base; s.channels[67].id = 66; Reject([&]() { GetDAQ::CompileAget(s); }, "duplicate");
    s = base; s.channels[67].id = 68; Reject([&]() { GetDAQ::CompileAget(s); }, "out-of-range");
    s = base; s.channels[0].threshold = 16; Reject([&]() { GetDAQ::CompileAget(s); }, "channel[0]");
    Reject([]() { GetDAQ::AgetPhysicalChannel(68); }, "0..67");
    std::map<std::string, std::string> p = {{"isActive", "true"}, {"isSelectedforTestMode", "true"}, {"Gain", "10pC"},
        {"LSBThresholdValue", "15"}, {"TriggerInhibition", "inhibit_trigger_function"}, {"Reading", "always"}};
    auto c = GetDAQ::ParseAgetChannel(11, p); Check(c.gain == 3 && c.threshold == 15 && c.inhibition == 1 && c.reading == GetDAQ::AgetChannel::Always, "channel legacy alias");
    p["TriggerInhibition"] = "inhibit_trigger_data"; Check(GetDAQ::ParseAgetChannel(0, p).inhibition == 2, "second inhibition alias");
    p["LSBThresholdValue"] = "16"; Reject([&]() { GetDAQ::ParseAgetChannel(0, p); }, "LSBThresholdValue");
    p["isActive"] = "false"; Check(!GetDAQ::ParseAgetChannel(0, p).active, "inactive channel should not evaluate disabled parameters");
    p["typo"] = "bad"; Reject([&]() { GetDAQ::ParseAgetChannel(0, p); }, "unknown");
    Reject([]() { GetDAQ::ParseAgetChannel(0, {{"isActive", "true"}}); }, "missing");
}
void Board()
{
    GetDAQ::CoBoAgetSettings board; board.asadMask = 9;
    GetDAQ::CoBoAgetChipSettings chip; chip.asadId = 3; chip.agetId = 3; chip.settings = Neutral(); board.chips.push_back(chip);
    chip.asadId = 0; chip.agetId = 0; chip.settings.chipVersion = 0x203; chip.settings.globals["Reg1/peackingTime"] = "68";
    chip.settings.globals["Reg1/isFPNRead"] = "true"; board.chips.push_back(chip);
    const auto image = GetDAQ::CompileCoBoAget(board);
    Check(image.asadMask == 9 && image.agetMask == 0x8001 && image.fpnRead && image.readoutDepth == 512, "board masks and FPN OR");
    Check(image.chips.front().asadId == 0 && image.chips.back().agetId == 3 && image.chips.front().image.chipVersion == 0x203, "board order and mixed chip versions");
    auto bad = board; bad.chips[0].settings.globals["ReadoutPointerOffset"] = "1"; bad.chips[0].settings.readoutDepth = 511;
    Reject([&]() { GetDAQ::CompileCoBoAget(bad); }, "must share");
    bad = board; bad.chips[0].settings.readoutDepth = 500; Reject([&]() { GetDAQ::CompileCoBoAget(bad); }, "must share");
    bad = board; bad.chips[0].settings.twoPulseMode = true; bad.chips[0].settings.globals["Reg1/SCA_Splitting"] = "true";
    Reject([&]() { GetDAQ::CompileCoBoAget(bad); }, "must share");
    bad = board; bad.chips.push_back(bad.chips[0]); Reject([&]() { GetDAQ::CompileCoBoAget(bad); }, "Duplicate");
    bad = board; bad.asadMask = 1; Reject([&]() { GetDAQ::CompileCoBoAget(bad); }, "inactive");
    bad = board; bad.chips[0].asadId = 4; Reject([&]() { GetDAQ::CompileCoBoAget(bad); }, "0..3");
    bad = board; bad.chips.clear(); Reject([&]() { GetDAQ::CompileCoBoAget(bad); }, "1..16");
    bad = board; bad.asadMask = 0; Reject([&]() { GetDAQ::CompileCoBoAget(bad); }, "1..15");
    board.chips.clear(); board.asadMask = 15;
    for (unsigned int id = 0; id < 16; ++id) { chip.asadId = id / 4; chip.agetId = id % 4; chip.settings = Neutral(); board.chips.push_back(chip); }
    Check(GetDAQ::CompileCoBoAget(board).agetMask == 65535, "full board active mask");
}
}
int main()
{
    try { Globals(); Channels(); Invalid(); Board(); std::cout << "AGET configuration tests passed\n"; return 0; }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
