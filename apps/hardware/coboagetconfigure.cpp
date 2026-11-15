// Adapted from CEA GET 2.3.0 CoBoNode slow-control and AGET configure routines.
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "hardwareclient.h"
#include "coboagetconfiguration.h"
namespace GetDAQ {
std::vector<HardwareStep> CoBoAgetConfigure(const CoBoAgetSettings &settings)
{
    const auto board = CompileCoBoAget(settings); // 所有芯片的参数先计算/核对。
    std::vector<HardwareStep> steps;
    const auto append = [&steps](HardwareStep::Kind kind, const std::string &device, const std::string &reg,
                                 const std::string &field, std::uint64_t value) {
        HardwareStep step; step.kind = kind; step.device = device; step.reg = reg; step.field = field; step.value = value; steps.push_back(step);
    };
    const auto write = [&append](const std::string &reg, const std::string &field, std::uint64_t value) {
        append(HardwareStep::WriteValue, "ctrl", reg, field, value);
    };
    // 仅是硬件位约束，不能代替完整 ECC Prepared/所有权联锁。
    append(HardwareStep::ExpectValue, "ctrl", "pipeCtrl", "firmwareEnable", 0);
    append(HardwareStep::ExpectValue, "ctrl", "pipeCtrl", "intEnable", 0);
    // 旧 switchToSlowControl 全板广播：三脉冲/等待/恢复慢控制掩码。
    const auto slow = CoBoSwitchToSlowControl(board.agetMask);
    steps.insert(steps.end(), slow.begin(), slow.end());
    // 先核对全板所有型号，避免后面的型号错误发生在前面芯片寄存器已写入以后。
    for (const auto &chip : board.chips) {
        write("agetChipSelect", "d", 4 * chip.asadId + chip.agetId);
        append(HardwareStep::ExpectValue, "aget", "reg5", "", chip.image.chipVersion); steps.back().mask = 0xffff;
    }
    for (const auto &chip : board.chips) {
        const unsigned int id = 4 * chip.asadId + chip.agetId;
        for (const auto &reg : chip.image.registers) {
            write("aget_sc_configuration", "SC_En", 1U << id);
            append(HardwareStep::WriteValue, "aget", reg.name, "", reg.value);
            write("agetChipSelect", "d", id);
            append(HardwareStep::ExpectValue, "aget", reg.name, "", reg.value);
            if (reg.name == "reg2" && settings.allowReg2BankResetDifference) steps.back().allowedDifference = UINT64_C(0x200000);
            if (reg.name == "reg12") write("readDepth", "", chip.image.registerStageDepth);
        }
        const char *prefixes[2] = {"readIfHitMask", "readAlwaysMask"};
        const char *suffixes[3] = {"1_4", "5_36", "37_68"};
        for (unsigned int type = 0; type < 2; ++type) {
            write("maskSelect", "", id);
            for (unsigned int group = 0; group < 3; ++group) {
                const std::string reg = std::string(prefixes[type]) + suffixes[group];
                const auto value = type == 0 ? chip.image.readIfHit[group] : chip.image.readAlways[group];
                write(reg, "", value); append(HardwareStep::ExpectValue, "ctrl", reg, "", value);
            }
        }
    }
    write("ASAD_sc_mask", "select", board.asadMask); write("aget_sc_configuration", "SC_En", board.agetMask);
    append(HardwareStep::ExpectValue, "ctrl", "ASAD_sc_mask", "select", board.asadMask);
    append(HardwareStep::ExpectValue, "ctrl", "aget_sc_configuration", "SC_En", board.agetMask);
    append(HardwareStep::ExpectValue, "ctrl", "aget_sc_configuration", "pulseEnable", 0);
    write("pipeCtrl", "isFPNRead", board.fpnRead);
    append(HardwareStep::ExpectValue, "ctrl", "pipeCtrl", "isFPNRead", board.fpnRead);
    write("pipeCtrl", "modeEnable_2p", board.twoPulseMode);
    append(HardwareStep::ExpectValue, "ctrl", "pipeCtrl", "modeEnable_2p", board.twoPulseMode);
    write("readDepth", "", board.readoutDepth);
    append(HardwareStep::ExpectValue, "ctrl", "readDepth", "", board.readoutDepth);
    write("readOffset", "value", board.readoutOffset);
    append(HardwareStep::ExpectValue, "ctrl", "readOffset", "value", board.readoutOffset);
    return steps;
}
}
