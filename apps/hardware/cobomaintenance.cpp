#include "hardwareclient.h"
#include <stdexcept>

// 序列来源：GET GetBench 20210208 / CoBoNode.cpp::pipeInit 与 pipeInterruptEnable。
// 原序列 © CEA，遵循原文件 CeCILL 声明；原文件及声明保留在 source/ 下。
// 只迁移该子步骤，不把它当作完整 Prepare，也不能用于保留校准的单板重对齐。
namespace GetDAQ {
std::vector<HardwareStep> CoBoSwitchToSlowControl(unsigned int mask)
{
    if (mask == 0 || mask > 0xffff) throw std::invalid_argument("Active AGET mask must be 1..65535");
    std::vector<HardwareStep> steps;
    const auto write = [&steps](const char *reg, const char *field, std::uint64_t value) {
        HardwareStep step; step.device = "ctrl"; step.reg = reg; step.field = field;
        step.value = value; steps.push_back(step);
    };
    // GET CoBoNode::switchToSlowControl：CoBo 和 AGET 状态机均需要切换。
    write("pipeCtrl", "firmwareEnable", 0);
    write("agetController", "reset", 1); write("agetController", "reset", 0);
    write("aget_sc_configuration", "SC_En", 0);
    write("aget_sc_configuration", "pulseEnable", mask);
    write("agetController", "hitRegister", 0); write("agetController", "calibration", 0);
    write("aget_sc_rw", "", 1);
    HardwareStep delay; delay.kind = HardwareStep::Delay; delay.delayMs = 1000; steps.push_back(delay);
    write("aget_sc_configuration", "SC_En", mask); write("aget_sc_configuration", "pulseEnable", 0);
    return steps;
}
std::vector<HardwareStep> CoBoPipelineInit()
{
    std::vector<HardwareStep> steps;
    const auto write = [&steps](const char *field, std::uint64_t value) {
        HardwareStep step; step.device = "ctrl"; step.reg = "pipeCtrl";
        step.field = field; step.value = value; steps.push_back(step);
    };
    write("multiplicityReadOut", 0);
    write("modeEnable_2p", 0);
    write("bitSlipCalibrate", 0);
    write("partialReadOut", 0);
    write("testMode", 0);
    write("zeroSuppressionEnable", 0);
    write("firmwareEnable", 0);
    write("hitRegisterOverride", 0);
    write("SCW", 0);
    write("SCR", 0);
    write("ScrScwOverride", 1);
    write("reset", 1);
    write("reset", 0);
    write("backpressure", 0);
    write("intEnable", 0);
    HardwareStep pause; pause.kind = HardwareStep::Delay; pause.delayMs = 500; steps.push_back(pause);
    return steps;
}
std::vector<HardwareStep> CoBoAsAdCalibrate(unsigned int mask)
{
    if (mask == 0 || mask > 15) throw std::invalid_argument("Active AsAd mask must be 1..15");
    std::vector<HardwareStep> steps;
    const auto append = [&steps](HardwareStep::Kind kind, const std::string &device,
                                 const std::string &reg, const std::string &field, std::uint64_t value) {
        HardwareStep step; step.kind = kind; step.device = device; step.reg = reg;
        step.field = field; step.value = value; steps.push_back(step);
    };
    // 来源同文件 CoBoNode::asadCal/selectAsAdForSlowControl/asadGetIdentifier。
    append(HardwareStep::WriteValue, "ctrl", "ASAD_sc_mask", "select", mask);
    append(HardwareStep::WriteValue, "ctrl", "asadController", "reset", 1);
    append(HardwareStep::WriteValue, "ctrl", "asadController", "reset", 0);
    append(HardwareStep::WriteValue, "ctrl", "asadController", "manualCal", 0);
    append(HardwareStep::WriteValue, "ctrl", "asadController", "calibration", mask);
    append(HardwareStep::WriteValue, "ctrl", "asad_sc_start", "", 1);
    append(HardwareStep::WriteValue, "ctrl", "asadController", "calibration", 0);
    for (unsigned int board = 0; board < 4; ++board) {
        if (!(mask & (1U << board))) continue;
        append(HardwareStep::WriteValue, "ctrl", "ASAD_sc_mask", "select", 1U << board);
        append(HardwareStep::WriteValue, "ctrl", "asadSelect", "d", board);
        append(HardwareStep::ExpectValue, "asad", "monitorID", "", 0x41);
        append(HardwareStep::WriteValue, "asad", "AIM0", "", board + 1);
        append(HardwareStep::ExpectValue, "asad", "AIM0", "", board + 1);
        steps.back().mask = 0xff; // 原 asadGetIdentifier 把每个 AIM 读值取低 8 位。
        // AIM 寄存器必须依次读取。仅 AIM0 的用户标签有已知期望值；其余标识保留在日志。
        for (unsigned int index = 1; index < 4; ++index)
            append(HardwareStep::ReadValue, "asad", "AIM" + std::to_string(index), "", 0);
    }
    append(HardwareStep::WriteValue, "ctrl", "ASAD_sc_mask", "select", mask);
    return steps;
}
}
