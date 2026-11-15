#include "coboclockconfiguration.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
void Require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
template<class F> void Rejects(F action) { try { action(); } catch (const std::invalid_argument &) { return; } throw std::runtime_error("Invalid clock configuration accepted"); }
GetDAQ::AsAdClockExpertFields Sampling() {
    return {{"FINDIV",18}, {"FBDIV",18}, {"OADIV",0}, {"OAMUX",4}, {"FBSEL",1},
        {"FBDLY",0}, {"xDlySel",0}, {"DlyGLA",0}, {"VCOSel",2}};
}
GetDAQ::AsAdClockExpertFields Reading() {
    return {{"FINDIV",4}, {"FBDIV",4}, {"OADIV",0}, {"OBDIV",0}, {"OAMUX",0}, {"OBMUX",2},
        {"FBSEL",1}, {"FBDLY",0}, {"xDlySel",0}, {"DlyGLA",0}, {"DlyGLB",0}};
}
unsigned int Decode(std::uint64_t word, unsigned int offset, unsigned int width) {
    unsigned int value = 0;
    for (unsigned int i = 0; i < width; ++i) value |= unsigned((word >> (offset + width - 1 - i)) & 1) << i;
    return value;
}
GetDAQ::CoBoClockSettings MakeClock(unsigned int mask, unsigned int version, float writing = 100) {
    GetDAQ::CoBoClockSettings s; s.asadMask = mask; s.hardwareVersion = version; s.writingMHz = writing;
    for (unsigned int id = 0; id < 4; ++id) if (mask & (1U << id)) { GetDAQ::AsAdBoardClockSettings b; b.asadId = id; s.boards.push_back(b); }
    return s;
}

}
int main()
{
    try {
        using namespace GetDAQ;
        const float frequencies[] = {100,50,25,12.5f,6.25f,3.125f};
        const std::uint64_t golden[] = {UINT64_C(0x48900001020000cf),UINT64_C(0x91200001020000d7),
            UINT64_C(0x20400001020000c7),UINT64_C(0x41420001020000c7),UINT64_C(0x81c30001020000c7),UINT64_C(0x01c38001020000c7)};
        for (unsigned int i = 0; i < 6; ++i) {
            const auto image = CompileAsAdSamplingClock(frequencies[i]);
            Require(image.value == golden[i] && image.nominalOutputKnown && image.nominalOutputMHz == frequencies[i], "LCM1 source reference differs");
            Require(CompileReducedCoBoWritingDivider(frequencies[i]) == i, "R-CoBo writing divider");
            const auto pll = CompileCoBoPll(frequencies[i]);
            Require(pll.packets.size() == 26 && pll.packets[0] == 0x20000 && !pll.nonStandardFrequency &&
                pll.nominalDefaultWritingMHz == frequencies[i], "PLL defaults or integer divider");
            for (unsigned int index = 4; index <= 6; ++index) Require(((pll.packets[index] >> 5) & 0x7ff) == unsigned(2000 / frequencies[i]) &&
                (pll.packets[index] & 31) == index - 1, "PLL output placement/address");
            Require(pll.packets[1] == 0xa00 && pll.packets[2] == 0xa01 && pll.packets[3] == 0xa02 &&
                pll.packets[14] == 0x1b12902d && pll.packets.back() == 0x1f001f, "PLL untouched CKR/register defaults");
        }
        for (float input : {50.0f,100.0f}) {
            const auto image = CompileAsAdSamplingClock(input, input);
            Require(image.nominalOutputMHz == 1 && Decode(image.value,45,5) == 24 && Decode(image.value,50,7) == 4, "Special 1MHz divider");
        }
        const auto generic = CompileAsAdSamplingClock(80);
        Require(Decode(generic.value,57,7) == 14 && Decode(generic.value,50,7) == 14 && generic.nominalOutputMHz == 80, "Generic LCM1 calculation");
        const auto unrepresentable = CompileAsAdSamplingClock(1);
        Require(!unrepresentable.nominalMatchesRequested && !unrepresentable.nominalWithinAgetRange && unrepresentable.nominalOutputMHz < 1,
            "Legacy LCM divider clamp hid an out-of-range nominal frequency");
        for (float input : {1.0f,2.0f,24.0f,40.0f,80.0f,150.0f,200.0f}) for (float ratio : {0.5f,1.0f,2.0f,4.0f}) {
            if (input / ratio < 1 || input / ratio > 200) continue;
            const auto image = CompileAsAdSamplingClock(input,ratio);
            Require(std::isfinite(image.nominalOutputMHz) && image.nominalOutputMHz > 0, "Generic frequency is invalid");
            Require((image.value & 0xe7) == 0xc7, "LCM1 fixed low bits changed");
        }
        for (float ckr : {25.0f,12.5f,6.25f}) for (int phase : {0,90,180,270}) for (int delay : {-5335,-535,0,535,5335}) {
            const auto image = CompileAsAdReadingClock(ckr,phase,delay);
            const unsigned int expectedMux = phase == 0 ? 2 : phase == 90 ? 1 : phase == 180 ? 0 : 3;
            Require(Decode(image.value,30,2) == expectedMux && image.nominalOutputMHz == 25 && image.nominalAdcMHz == 25 &&
                image.quantizedAdcDelayPs == delay, "LCM2 phase/delay/frequency reference");
        }
        Require(CompileAsAdReadingClock(25,0,0).value == UINT64_C(0x20400001620000c7), "LCM2 default reference");
        for (int delay = -5335; delay <= 5335; ++delay) {
            const auto image = CompileAsAdReadingClock(25,270,delay);
            Require(image.quantizedAdcDelayPs >= -5335 && image.quantizedAdcDelayPs <= 5335, "Quantized delay overflow");
            // 零附近保留旧整数截断；376ps 已编码为 535ps，不伪装成精确延迟。
            if (delay == 374 || delay == 375) Require(image.quantizedAdcDelayPs == 0, "Delay zero boundary");
            if (delay == 376) Require(image.quantizedAdcDelayPs == 535, "Legacy delay truncation");
        }
        auto sample = Sampling(); Require(CompileAsAdSamplingClock(100,1,sample).value == golden[0], "LCM1 full expert defaults");
        sample["OAMUX"] = 0; Require(CompileAsAdSamplingClock(100,2,sample).nominalOutputMHz == 100, "LCM1 clock bypass");
        sample["OAMUX"] = 1; Require(!CompileAsAdSamplingClock(100,1,sample).nominalOutputKnown, "Unknown expert output guessed");
        auto read = Reading(); Require(CompileAsAdReadingClock(25,0,0,read).value == UINT64_C(0x20400001620000c7), "LCM2 full expert defaults");
        struct Field { const char *name; unsigned int offset,width; };
        const Field sf[] = {{"FINDIV",57,7},{"FBDIV",50,7},{"OADIV",45,5},{"OAMUX",32,3},{"FBSEL",24,2},
            {"FBDLY",19,5},{"xDlySel",18,1},{"DlyGLA",13,5},{"VCOSel",3,2}};
        for (const auto &f : sf) for (unsigned int value = 0; value < (1U << f.width); ++value) {
            sample = Sampling(); sample[f.name] = value; const auto image = CompileAsAdSamplingClock(100,1,sample);
            Require(Decode(image.value,f.offset,f.width) == value, "LCM1 reversed field packing");
        }
        const Field rf[] = {{"FINDIV",57,7},{"FBDIV",50,7},{"OADIV",45,5},{"OBDIV",40,5},{"OAMUX",33,2},
            {"OBMUX",30,2},{"FBSEL",24,2},{"FBDLY",19,5},{"xDlySel",18,1},{"DlyGLA",13,5},{"DlyGLB",8,5}};
        for (const auto &f : rf) for (unsigned int value = 0; value < (1U << f.width); ++value) {
            read = Reading(); read[f.name] = value; const auto image = CompileAsAdReadingClock(25,0,0,read);
            Require(Decode(image.value,f.offset,f.width) == value, "LCM2 reversed field packing");
        }
        const auto sync = CompileCoBoPll(100,{{11,0x0401000b},{13,0x1b12906d}});
        Require(sync.packets.size() == 27 && sync.packets[12] == 0x0401000b && sync.packets[13] == 0x0400000b &&
            sync.packets[14] == 0x0b0c006c && sync.expertRegisters, "PLL EN_CLKin1 R11 second sync packet");
        const auto nonstandard = CompileCoBoPll(123);
        Require(nonstandard.nonStandardFrequency && nonstandard.divider == 16 && nonstandard.nominalDefaultWritingMHz == 125, "PLL truncation not exposed");
        for (unsigned int id = 0; id <= 31; ++id) {
            if (id >= 17 && id <= 23) { Rejects([&] { CompileCoBoPll(100,{{id,id}}); }); continue; }
            const auto image = CompileCoBoPll(100,{{id,0x80000000U | id}});
            bool present = false; for (auto word : image.packets) if (word == (0x80000000U | id)) present = true;
            Require(present, "PLL high-bit override lost");
        }
        for (const auto &f : sf) { sample = Sampling(); sample[f.name] = 1U << f.width; Rejects([&] { CompileAsAdSamplingClock(100,1,sample); }); }
        for (const auto &f : rf) { read = Reading(); read[f.name] = 1U << f.width; Rejects([&] { CompileAsAdReadingClock(25,0,0,read); }); }
        sample = Sampling(); sample.erase("FINDIV"); Rejects([&] { CompileAsAdSamplingClock(100,1,sample); });
        read = Reading(); read.erase("FBDIV"); read["typo"] = 4; Rejects([&] { CompileAsAdReadingClock(25,0,0,read); });
        for (float bad : {0.0f,-1.0f,201.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
            Rejects([&] { CompileAsAdSamplingClock(bad); }); Rejects([&] { CompileCoBoPll(bad); });
        }
        for (float bad : {0.0f,-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) Rejects([&] { CompileAsAdSamplingClock(100,bad); });
        Rejects([] { CompileAsAdSamplingClock(100,101); }); Rejects([] { CompileAsAdSamplingClock(100,0.1f); });
        Rejects([] { CompileAsAdReadingClock(100,0,0); }); Rejects([] { CompileAsAdReadingClock(25,45,0); });
        Rejects([] { CompileAsAdReadingClock(25,0,-5336); }); Rejects([] { CompileAsAdReadingClock(25,0,5336); });
        Rejects([] { CompileCoBoPll(100,{{32,32}}); }); Rejects([] { CompileCoBoPll(100,{{10,11}}); });
        Rejects([] { CompileReducedCoBoWritingDivider(123); });
        for (const char *text : {"100", "1e2", "+1.25e1", "200", "1"}) Require(ParseClockReal(text,"clock",1,200) >= 1,"Decimal real rejected");
        for (const char *text : {"", "200.000001", "0.999999999", "nan", "inf", "0x64", " 100", "100 ", "100MHz", "1e999", "1e-999", "1..0", "+", "--1"})
            Rejects([&] { ParseClockReal(text,"clock",1,200); });
        Require(AsAdSamplingClockParameters().size() == 9 && AsAdReadingClockParameters().size() == 11 && CoBoPllParameters().size() == 18,"Clock descriptor coverage");
        for (unsigned int mask = 1; mask <= 15; ++mask) for (unsigned int version : {1U,2U}) for (float writing : frequencies) {
            const auto s = MakeClock(mask,version,writing); const auto image = CompileCoBoClock(s);
            Require(image.asadMask == mask && image.nominalUpstreamKnown && image.nominalTargetsMatched && !image.nominalKnownDifference && image.nominalOutputsWithinRange,"Whole board clock model");
            Require(image.programPll == (version >= 2) && image.programReadingDivider == (version == 1) && image.programWritingDivider == (version == 1),"Hardware clock branch differs");
            if (version == 2) Require(image.pll.packets == CompileCoBoPll(writing).packets,"Normal named PLL defaults alter original packets");
            for (const auto &b : image.boards) Require(b.sampling.value == CompileAsAdSamplingClock(writing).value && b.effectiveNominalReadingMHz == 25 && b.effectiveNominalAdcMHz == 25,"Whole clock board packing/CKR");
        }
        auto whole = MakeClock(11,2,123); auto full = CompileCoBoClock(whole);
        Require(full.nominalCoBoWritingMHz == 125 && full.nominalKnownDifference && !full.nominalTargetsMatched && full.boards[0].effectiveNominalSamplingMHz == 125,"Whole PLL quantization hidden");
        whole = MakeClock(5,1); whole.readingMHz = 12.5f; full = CompileCoBoClock(whole);
        Require(full.readingDivider == 2 && full.nominalCoBoReadingMHz == 25 && full.boards[0].effectiveNominalReadingMHz == 50 && full.nominalKnownDifference,"R-CoBo legacy CKR mismatch hidden");
        whole = MakeClock(1,2); whole.skipPllConfig = true; full = CompileCoBoClock(whole);
        Require(!full.programPll && full.programWritingDivider && !full.nominalUpstreamKnown && !full.nominalTargetsMatched && !full.nominalKnownDifference,"skipPLL guessed upstream");
        whole = MakeClock(1,2); whole.pll["MODE"] = "6"; Require(!CompileCoBoClock(whole).nominalUpstreamKnown,"PLL expert mode guessed upstream");
        whole = MakeClock(1,2); whole.pll["EN_CLKin1"] = "true"; whole.pll["SYNC_POL_INV"] = "true";
        Require(CompileCoBoClock(whole).pll.packets == sync.packets,"Named PLL field placement differs");
        whole = MakeClock(1,2); whole.boards[0].samplingExpert = Sampling(); whole.boards[0].samplingExpert["OAMUX"] = 1;
        full = CompileCoBoClock(whole); Require(!full.nominalTargetsMatched && !full.nominalOutputsWithinRange && !full.nominalKnownDifference,"Unknown AsAd output guessed");
        whole = MakeClock(1,2,1); Require(!CompileCoBoClock(whole).nominalOutputsWithinRange,"Clamped legacy LCM output range hidden");
        for (unsigned int version : {0U, UINT32_MAX}) Rejects([&] { CompileCoBoClock(MakeClock(1,version)); });
        for (unsigned int mask : {0U,16U}) Rejects([&] { CompileCoBoClock(MakeClock(mask,2)); });
        whole = MakeClock(3,2); whole.boards.pop_back(); Rejects([&] { CompileCoBoClock(whole); });
        whole = MakeClock(1,2); whole.boards.push_back(whole.boards.front()); Rejects([&] { CompileCoBoClock(whole); });
        whole = MakeClock(1,2); whole.boards[0].asadId = 4; Rejects([&] { CompileCoBoClock(whole); });
        whole = MakeClock(1,1,123); Rejects([&] { CompileCoBoClock(whole); });
        whole = MakeClock(1,1); whole.readingMHz = 6.25f; Rejects([&] { CompileCoBoClock(whole); });
        whole = MakeClock(1,2); whole.verifyHardwareVersion = false; Rejects([&] { CompileCoBoClock(whole); });
        whole = MakeClock(1,2); whole.pll["CLKin_Select_MODE"] = "0"; Rejects([&] { CompileCoBoClock(whole); }); // alias resolved by xcfg adapter only
        for (const auto &p : CoBoPllParameters()) {
            whole = MakeClock(1,2); whole.pll[p.name] = p.boolean ? "1" : std::to_string(1U << p.width);
            Rejects([&] { CompileCoBoClock(whole); });
        }
        std::cout << "PASS LCM1/LCM2 clocks, reverse fields, delay quantization, PLL packets/sync, whole-board upstream model and bounds\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
