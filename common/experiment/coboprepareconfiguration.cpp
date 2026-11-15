// Adapted from CEA GET CoBoNode Prepare helpers and CoBoConfigParser LEMO mapping.
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "coboprepareconfiguration.h"
#include <algorithm>
#include <stdexcept>
namespace GetDAQ {
unsigned int CompileCoBoLemo(const std::string &function)
{
    static const std::map<std::string, unsigned int> modes = {
        {"trigger_pulse_output", 0}, {"busy_output", 1}, {"pll_holdover_output", 2},
        {"pll_ld_output", 3}, {"pll_clkin0_output", 4}, {"pll_clkin1_output", 5}, {"pll_sync_output", 6},
        {"external_trigger_input", 8}, {"none", 9}, {"pll_clkin0_input", 12}, {"pll_clkin1_input", 13},
        {"pll_sync_input", 14}, {"external_timestamp_reset_input", 15}
    };
    const auto found = modes.find(function);
    if (found == modes.end()) throw std::invalid_argument("Unknown CoBo LEMO function: " + function);
    return found->second;
}
std::vector<CoBoRegisterValue> CompileCoBoPrepareFoundation(const CoBoPrepareFoundationSettings &s)
{
    if (s.hardwareVersion == 0 || s.hardwareVersion == UINT32_MAX)
        throw std::invalid_argument("Invalid CoBo hardware version");
    if (!s.verifyHardwareVersion && s.hardwareVersion != 1)
        throw std::invalid_argument("Only explicitly selected legacy R-CoBo may omit hardwareVersion");
    if (s.bufferLayout != CoBoReadoutSettings::PerAsAdBuffers && s.bufferLayout != CoBoReadoutSettings::SharedBuffer)
        throw std::invalid_argument("Invalid Prepare buffer layout");
    std::array<unsigned int, 4> lemo{};
    // 即使 R-CoBo 不使用 LEMO，也不能静默接受无效配置。
    for (unsigned int i = 0; i < 4; ++i) lemo[i] = CompileCoBoLemo(s.lemo[i]);
    std::vector<CoBoRegisterValue> writes;
    const auto write = [&writes](const std::string &reg, const std::string &field, std::uint64_t value) {
        CoBoRegisterValue w; w.device = "ctrl"; w.reg = reg; w.field = field; w.value = value; writes.push_back(w);
    };
    if (s.hardwareVersion >= 2) for (unsigned int i = 0; i < 4; ++i) write("lemoMux", "L" + std::to_string(i), lemo[i]);
    for (unsigned int i = 0; i < 4; ++i) write("readDataDelay", "AGET" + std::to_string(i), 14);
    for (const char *name : {"triggerDelay", "triggerDelay_2p", "triggerWindowStart_2p"}) write(name, "", 0);
    write("triggerWindowEnd_2p", "", 100000); write("multThreshold_2p", "", 3000);
    write("triggerTime_10ns_2p", "", 1000); write("multThreshold", "", 3000);
    write("triggerPeriod_10ns", "", 500000000); write("triggerMask", "", 0);
    write("multWindSize", "", 16); write("readDepth", "", 512);
    for (const char *suffix : {"1_4", "5_36", "37_68"}) write(std::string("readAlwaysMask") + suffix, "", 0);
    write("readIfHitMask1_4", "", 15);
    write("readIfHitMask5_36", "", UINT32_MAX); write("readIfHitMask37_68", "", UINT32_MAX);
    if (s.bufferLayout == CoBoReadoutSettings::PerAsAdBuffers) {
        // Prepare 初值总是四块各 32MiB；Configure 才按活动范围/实验配置重新分配。
        for (unsigned int i = 0; i < 4; ++i) {
            const std::uint32_t start = 0x08000000U + i * 0x02000000U;
            const auto suffix = "_asad" + std::to_string(i);
            write("memStart" + suffix, "", start); write("memEnd" + suffix, "", start + 0x02000000U);
            write("readPtr" + suffix, "", start);
        }
    } else {
        write("memStart", "", 0x08000000); write("memEnd", "", 0x10000000); write("readPtr", "", 0x08000000);
    }
    if (s.interruptRateRegister) write("interruptRate", "", 1);
    write("aFullLevel", "", 0x01000000); write("aEmptyLevel", "", 0x01000000);
    return writes;
}
AsAdInputImage CompileAsAdInputs(const AsAdInputSettings &s)
{
    if (s.asadMask == 0 || s.asadMask > 15) throw std::invalid_argument("Active AsAd mask must be 1..15");
    AsAdInputImage image; image.asadMask = s.asadMask;
    unsigned int seen = 0;
    for (const auto &board : s.boards) {
        if (board.asadId > 3) throw std::invalid_argument("AsAd input board ID must be 0..3");
        const unsigned int bit = 1U << board.asadId;
        if (!(s.asadMask & bit) || (seen & bit)) throw std::invalid_argument("Inactive or duplicate AsAd input board");
        seen |= bit;
        AsAdInputBoardImage b; b.asadId = board.asadId;
        for (unsigned int i = 0; i < 4; ++i) {
            const auto &p = board.positions[i];
            if (!p.active || !p.enabled) b.adc0 |= 1U << (1 + i);
            else b.im1 |= (1U << i) | (1U << (4 + i));
            if (p.active) image.agetMask |= 1U << (4 * board.asadId + i);
            if (p.antiSparklingEnabled) b.im0 |= 1U << i;
            if (p.connectGroundToZAP) b.im0 |= 1U << (4 + i);
        }
        image.boards.push_back(b);
    }
    if (seen != s.asadMask) throw std::invalid_argument("Missing active AsAd input board");
    std::sort(image.boards.begin(), image.boards.end(), [](const AsAdInputBoardImage &a, const AsAdInputBoardImage &b) { return a.asadId < b.asadId; });
    return image;
}
void ValidateCoBoPrepareSettings(const CoBoPrepareSettings &s)
{
    CompileCoBoPrepareFoundation(s.foundation);
    const auto inputs = CompileAsAdInputs(s.inputs);
    const auto clock = CompileCoBoClock(s.clock);
    const auto monitoring = CompileCoBoMonitoring(s.monitoring);
    const auto calibration = CompileCoBoCalibration(s.calibration);
    if (inputs.asadMask != clock.asadMask || inputs.asadMask != monitoring.asadMask || inputs.asadMask != calibration.asadMask)
        throw std::invalid_argument("Prepare fragments must use the same active AsAd set");
    if (inputs.agetMask != calibration.agetMask)
        throw std::invalid_argument("Prepare input activity and calibration chip versions must use the same active AGET set");
    if (s.foundation.hardwareVersion != s.clock.hardwareVersion || s.foundation.verifyHardwareVersion != s.clock.verifyHardwareVersion)
        throw std::invalid_argument("Prepare foundation/clock hardware version policies differ");
    if (s.monitoring.ledPolicy == CoBoMonitoringSettings::RequiredLedManager && s.monitoring.ledHardwareVersion != s.foundation.hardwareVersion)
        throw std::invalid_argument("Prepare LED hardware version differs from bound board version");
    if (s.manualReadDataDelay > 255) throw std::invalid_argument("Prepare manual read data delay must be 0..255");
}
}
