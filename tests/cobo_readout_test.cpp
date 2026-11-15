#include "cobo_readout_fixture.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
void Require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
template<class Action> void Rejects(Action action, const std::string &message) {
    try { action(); } catch (const std::invalid_argument &e) { Require(std::string(e.what()).find(message) != std::string::npos, e.what()); return; }
    throw std::runtime_error("Expected rejection: " + message);
}
std::uint64_t Value(const GetDAQ::CoBoReadoutImage &image, const std::string &reg, const std::string &field = "") {
    for (auto it = image.writes.rbegin(); it != image.writes.rend(); ++it) if (it->reg == reg && it->field == field) return it->value;
    throw std::runtime_error("Missing write: " + reg);
}
void Triggers()
{
    const char *modes[] = {"external", "onRequest", "periodically", "onMultiplicity", "onSlowControl"};
    for (unsigned int id = 0; id < 5; ++id) for (unsigned int output = 0; output < 5; ++output) {
        auto s = GetDAQTest::Readout(); s.module["triggerMode"] = modes[id]; s.module["secondaryTriggerMode"] = modes[output];
        Require(GetDAQ::CompileCoBoReadout(s).triggerMask == ((1U << id) | (1U << (output + 16))), "Primary/secondary trigger bits wrong");
        s.module["triggerMode"] += "_2p"; s.module["enableMem2pMode"] = "true";
        s.aget.chips[0].settings.twoPulseMode = true; s.aget.chips[0].settings.globals["Reg1/SCA_Splitting"] = "true";
        Require(GetDAQ::CompileCoBoReadout(s).triggerMask == ((1U << id) | (1U << (id + 8)) | (1U << (output + 16))), "2p trigger bits wrong");
    }
    auto s = GetDAQTest::Readout(); s.module["triggerMode"] = "noTrigger";
    Require(GetDAQ::CompileCoBoReadout(s).triggerMask == 0, "noTrigger enabled a trigger");
    s.module["triggerMode"] = "external_2p";
    Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "2p triggerMode");
    s.module["triggerMode"] = "External"; Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "triggerMode");
    s.module["triggerMode"] = "external"; s.module["secondaryTriggerMode"] = "external_2p";
    Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "secondaryTriggerMode");
    s.module.erase("secondaryTriggerMode"); s.module["externalTriggerMode"] = "onRequest";
    Require(GetDAQ::CompileCoBoReadout(s).triggerMask == 0x20001, "Legacy secondary alias lost");
    s.module.erase("externalTriggerMode"); Require(GetDAQ::CompileCoBoReadout(s).triggerMask == 0x10001, "Secondary fallback lost");
    s.module["triggerMode"] = "onRequest_2p"; s.module["enableMem2pMode"] = "true";
    s.aget.chips[0].settings.twoPulseMode = true; s.aget.chips[0].settings.globals["Reg1/SCA_Splitting"] = "true";
    Require(GetDAQ::CompileCoBoReadout(s).triggerMask == 0x202, "Inherited 2p secondary should stay disabled");
}
void Buffers()
{
    for (unsigned int mask = 1; mask < 16; ++mask) {
        auto s = GetDAQTest::Readout(mask); const auto image = GetDAQ::CompileCoBoReadout(s);
        unsigned int count = 0; for (unsigned int id = 0; id < 4; ++id) if (mask & (1U << id)) ++count;
        const auto size = 0x8000000U / (count == 3 ? 4 : count); unsigned int cursor = 0x8000000;
        for (unsigned int id = 0; id < 4; ++id) {
            Require(image.bufferStart[id] == cursor, "Sparse buffer start wrong");
            cursor += (mask & (1U << id)) ? size : 0;
            Require(image.bufferEnd[id] == cursor && Value(image, "readPtr_asad" + std::to_string(id)) == image.bufferStart[id], "Buffer end/read pointer wrong");
        }
        Require(Value(image, "interruptRate") == 1, "Interrupt default wrong");
    }
    auto s = GetDAQTest::Readout(3);
    for (unsigned int id = 0; id < 4; ++id) { s.buffers[id].start = std::to_string(0x8000000U + id * 0x2000000); s.buffers[id].end = std::to_string(0x8000000U + (id+1) * 0x2000000); }
    Require(GetDAQ::CompileCoBoReadout(s).bufferEnd[0] == 0xa000000, "Explicit buffers lost");
    s.buffers[1].start = "0x8000000"; Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "overlap");
    s.buffers[1].start = "0xa000000"; s.buffers[3].end.clear(); Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "both start");
    s.buffers[3].start.clear(); Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "all four");
    s = GetDAQTest::Readout(); s.circularBuffer["endOfMemory"] = "0x7ffffff"; Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "less than");
    s = GetDAQTest::Readout(); s.circularBuffer["endOfMemory"] = "0x100000000"; Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "out-of-range");
    s = GetDAQTest::Readout(); s.circularBuffer["levelAlmostEmptyMemory"] = "1"; Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "levels");
    s = GetDAQTest::Readout(); s.circularBuffer["interruptRate"] = "0"; Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "positive");
    s = GetDAQTest::Readout(15); s.bufferLayout = GetDAQ::CoBoReadoutSettings::SharedBuffer;
    Require(Value(GetDAQ::CompileCoBoReadout(s), "readPtr") == 0x8000000, "Shared layout lost");
    s.bufferLayout = static_cast<GetDAQ::CoBoReadoutSettings::BufferLayout>(99); Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "layout");
}
void ChannelsAndLimits()
{
    auto s = GetDAQTest::Readout(11); const auto image = GetDAQ::CompileCoBoReadout(s);
    Require(image.suppressMultiplicity == 14 && !image.fullReadout, "Position suppression/full readout wrong");
    std::size_t zeros = 0;
    for (const auto &w : image.writes) if (w.field == "threshold") {
        const unsigned int board = zeros / 68 == 0 ? 0 : zeros / 68 == 1 ? 1 : 3;
        const auto id = zeros % 68;
        Require(w.reg == "aget0ch" + std::to_string(id) && w.value == board*68+id, "Zero threshold/channel mapping wrong");
        Require(w.device == (board ? "zeroSuppress_AsAd" + std::to_string(board) : "zeroSuppress"), "Zero device name wrong"); ++zeros;
    }
    Require(zeros == 204, "Missing FPN/inactive channel thresholds");
    s.chips[0].suppressMultiplicity = true; Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "suppressMultiplicity");
    s = GetDAQTest::Readout(3); s.aget.chips[0].settings.globals["Reg1/isAllChannelRead"] = "true";
    Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "isAllChannelRead");
    s.aget.chips[1].settings.globals["Reg1/isAllChannelRead"] = "true";
    Require(Value(GetDAQ::CompileCoBoReadout(s), "pipeCtrl", "partialReadOut") == 0, "Full readout pipe bit wrong");
    s = GetDAQTest::Readout(); s.chips[0].zeroSuppressionThresholds[67] = 4096; Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "zeroSuppressionThreshold");
    s = GetDAQTest::Readout(); s.chips.clear(); Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "exactly");
    s = GetDAQTest::Readout(3); s.chips[1] = s.chips[0]; Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "Duplicate");
    for (const auto &invalid : std::vector<std::pair<std::string,std::string>>{{"coboId","256"},{"dataSource","-1"},{"multSubtractDevs","8"},
            {"triggerPeriod","42950"},{"triggerDelay","4294967296"},{"isAcqTestMode","1"},{"triggerDelay","3.5"},{"triggerDelay"," 3"}}) {
        s = GetDAQTest::Readout(); s.module[invalid.first] = invalid.second;
        Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, invalid.first);
    }
    s = GetDAQTest::Readout(); s.module["triggerPeriod_10ns"] = "4294967295"; s.module["triggerPeriod"] = "invalid ignored alias";
    s.module["scwScrDelay"] = "123"; s.module["writeReadDelay"] = "invalid ignored alias";
    Require(Value(GetDAQ::CompileCoBoReadout(s), "triggerPeriod_10ns") == UINT32_MAX && Value(GetDAQ::CompileCoBoReadout(s), "ScwScrDelay") == 123, "Canonical aliases lost priority");
    s.module["unknown"] = "1"; Rejects([&]() { GetDAQ::CompileCoBoReadout(s); }, "Unknown");
}
}
int main() {
    try { Triggers(); Buffers(); ChannelsAndLimits(); std::cout << "CoBo readout configuration tests passed\n"; return 0; }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
