#include "coboprepareconfiguration.h"
#include <iostream>
#include <stdexcept>
namespace {
void Require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
template<class F> void Rejects(F action) {
    try { action(); } catch (const std::invalid_argument &) { return; }
    throw std::runtime_error("Invalid Prepare settings accepted");
}
std::uint64_t Value(const std::vector<GetDAQ::CoBoRegisterValue> &writes, const std::string &reg, const std::string &field = "") {
    for (const auto &w : writes) if (w.reg == reg && w.field == field) return w.value;
    throw std::runtime_error("Missing register " + reg + '.' + field);
}
}
int main()
{
    try {
        using namespace GetDAQ;
        const char *names[] = {"trigger_pulse_output", "busy_output", "pll_holdover_output", "pll_ld_output", "pll_clkin0_output",
            "pll_clkin1_output", "pll_sync_output", "external_trigger_input", "none", "pll_clkin0_input", "pll_clkin1_input",
            "pll_sync_input", "external_timestamp_reset_input"};
        const unsigned int codes[] = {0, 1, 2, 3, 4, 5, 6, 8, 9, 12, 13, 14, 15};
        for (unsigned int i = 0; i < 13; ++i) Require(CompileCoBoLemo(names[i]) == codes[i], "LEMO mapping differs from source");
        Rejects([] { CompileCoBoLemo("external_trigger"); }); Rejects([] { CompileCoBoLemo("None"); });
        CoBoPrepareFoundationSettings s;
        s.lemo = {{"trigger_pulse_output", "busy_output", "external_trigger_input", "external_timestamp_reset_input"}};
        const auto writes = CompileCoBoPrepareFoundation(s);
        Require(Value(writes, "lemoMux", "L2") == 8 && Value(writes, "lemoMux", "L3") == 15, "LEMO positions");
        Require(Value(writes, "triggerPeriod_10ns") == 500000000 && Value(writes, "triggerWindowEnd_2p") == 100000 &&
            Value(writes, "triggerTime_10ns_2p") == 1000 && Value(writes, "triggerMask") == 0 && Value(writes, "readDepth") == 512,
            "Legacy trigger defaults");
        for (unsigned int i = 0; i < 4; ++i) {
            Require(Value(writes, "readDataDelay", "AGET" + std::to_string(i)) == 14, "Initial manual delay");
            const auto suffix = "_asad" + std::to_string(i);
            Require(Value(writes, "memStart" + suffix) == 0x08000000U + i * 0x02000000U &&
                Value(writes, "memEnd" + suffix) == 0x0a000000U + i * 0x02000000U &&
                Value(writes, "readPtr" + suffix) == Value(writes, "memStart" + suffix), "Four initial 32MiB buffers");
        }
        Require(Value(writes, "readIfHitMask1_4") == 15 && Value(writes, "readIfHitMask5_36") == UINT32_MAX &&
            Value(writes, "readAlwaysMask37_68") == 0 && Value(writes, "aFullLevel") == 0x01000000 &&
            Value(writes, "aEmptyLevel") == 0x01000000, "Masks and buffer thresholds");
        s.hardwareVersion = 1; s.verifyHardwareVersion = false; s.bufferLayout = CoBoReadoutSettings::SharedBuffer;
        s.interruptRateRegister = false;
        const auto legacy = CompileCoBoPrepareFoundation(s);
        Require(Value(legacy, "memStart") == 0x08000000 && Value(legacy, "memEnd") == 0x10000000 &&
            Value(legacy, "readPtr") == 0x08000000, "Explicit shared Prepare buffer");
        for (const auto &w : legacy) Require(w.reg != "lemoMux" && w.reg != "interruptRate" && w.reg.find("_asad") == std::string::npos,
            "Legacy variant added unsupported registers");
        s.hardwareVersion = 2; Rejects([&] { CompileCoBoPrepareFoundation(s); });
        s.verifyHardwareVersion = true; s.hardwareVersion = 0; Rejects([&] { CompileCoBoPrepareFoundation(s); });
        s.hardwareVersion = UINT32_MAX; Rejects([&] { CompileCoBoPrepareFoundation(s); });
        s.hardwareVersion = 1; s.lemo[3] = "invalid"; Rejects([&] { CompileCoBoPrepareFoundation(s); });
        s.lemo[3] = "none"; s.bufferLayout = static_cast<CoBoReadoutSettings::BufferLayout>(99);
        Rejects([&] { CompileCoBoPrepareFoundation(s); });
        for (unsigned int mask = 1; mask <= 15; ++mask) {
            AsAdInputSettings input; input.asadMask = mask;
            for (int id = 3; id >= 0; --id) if (mask & (1U << id)) {
                AsAdInputBoardSettings b; b.asadId = unsigned(id);
                for (unsigned int pos = 0; pos < 4; ++pos) b.positions[pos].active = true;
                input.boards.push_back(b);
            }
            const auto image = CompileAsAdInputs(input);
            unsigned int previous = 0;
            for (unsigned int i = 0; i < image.boards.size(); ++i) {
                const auto &b = image.boards[i]; Require(b.adc0 == 0 && b.im0 == 15 && b.im1 == 255, "Active input defaults");
                Require(i == 0 || previous < b.asadId, "Input boards not sorted"); previous = b.asadId;
            }
            for (unsigned int id = 0; id < 4; ++id) Require(((image.agetMask >> (4 * id)) & 15) == ((mask & (1U << id)) ? 15U : 0U), "AGET activity mask");
        }
        // 遍历四位置及所有 active/enabled/ground/protection 组合，独立核对位与极性。
        for (unsigned int position = 0; position < 4; ++position) for (unsigned int flags = 0; flags < 16; ++flags) {
            AsAdInputSettings input; input.asadMask = 8; AsAdInputBoardSettings b; b.asadId = 3;
            for (auto &p : b.positions) p.active = true;
            auto &p = b.positions[position]; p.active = flags & 1; p.enabled = flags & 2;
            p.connectGroundToZAP = flags & 4; p.antiSparklingEnabled = flags & 8; input.boards.push_back(b);
            const auto image = CompileAsAdInputs(input); const auto &value = image.boards[0];
            Require(((value.adc0 >> (1 + position)) & 1) == !(p.active && p.enabled), "ADC power-down polarity");
            Require(((value.im1 >> position) & 1) == (p.active && p.enabled) &&
                ((value.im1 >> (4 + position)) & 1) == (p.active && p.enabled), "Sampling/readout enable bits");
            Require(((value.im0 >> position) & 1) == p.antiSparklingEnabled &&
                ((value.im0 >> (4 + position)) & 1) == p.connectGroundToZAP, "Protection/ZAP bits");
            Require(((image.agetMask >> (12 + position)) & 1) == p.active, "Slot enable incorrectly changed logical AGET activity");
        }
        AsAdInputSettings missing; missing.asadMask = 1; Rejects([&] { CompileAsAdInputs(missing); });
        AsAdInputBoardSettings b; missing.boards.push_back(b); missing.boards.push_back(b); Rejects([&] { CompileAsAdInputs(missing); });
        missing.boards.pop_back(); missing.boards[0].asadId = 1; Rejects([&] { CompileAsAdInputs(missing); });
        missing.boards[0].asadId = 4; Rejects([&] { CompileAsAdInputs(missing); });
        missing.asadMask = 0; Rejects([&] { CompileAsAdInputs(missing); });
        missing.asadMask = 16; Rejects([&] { CompileAsAdInputs(missing); });
        CoBoPrepareSettings prepare;
        prepare.inputs.asadMask = prepare.clock.asadMask = prepare.monitoring.asadMask = prepare.calibration.asadMask = 9;
        for (unsigned int id : {0,3}) {
            AsAdInputBoardSettings input; input.asadId = id; input.positions[2].active = true; prepare.inputs.boards.push_back(input);
            AsAdBoardClockSettings clock; clock.asadId = id; prepare.clock.boards.push_back(clock);
            AsAdMonitoringBoardSettings monitor; monitor.asadId = id; prepare.monitoring.boards.push_back(monitor);
            AsAdCalibrationSettings calibration; calibration.asadId = id; prepare.calibration.boards.push_back(calibration);
            prepare.calibration.chipVersions[4*id+2] = 0x202;
        }
        ValidateCoBoPrepareSettings(prepare);
        auto different = prepare; different.clock.asadMask = 1; different.clock.boards.pop_back(); Rejects([&] { ValidateCoBoPrepareSettings(different); });
        different = prepare; different.inputs.boards[1].positions[1].active = true; Rejects([&] { ValidateCoBoPrepareSettings(different); });
        different = prepare; different.clock.hardwareVersion = 5; Rejects([&] { ValidateCoBoPrepareSettings(different); });
        different = prepare; different.monitoring.ledPolicy = CoBoMonitoringSettings::RequiredLedManager; different.monitoring.ledHardwareVersion = 5;
        Rejects([&] { ValidateCoBoPrepareSettings(different); });
        different = prepare; different.foundation.verifyHardwareVersion = false; Rejects([&] { ValidateCoBoPrepareSettings(different); });
        different = prepare; different.manualReadDataDelay = 256; Rejects([&] { ValidateCoBoPrepareSettings(different); });
        for (unsigned int delay : {0,255}) { prepare.manualReadDataDelay = delay; ValidateCoBoPrepareSettings(prepare); }
        prepare.inputs.boards[0].positions[2].enabled = false; ValidateCoBoPrepareSettings(prepare); // 逻辑活动与物理槽位使能是不同策略。
        std::cout << "PASS Prepare foundation/input mapping and combined activity/version/manual-delay coherence\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
