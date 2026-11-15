// Adapted from CEA GET CoBoNode Prepare helpers.
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "hardwareclient.h"
#include "coboprepareconfiguration.h"
#include <stdexcept>
namespace GetDAQ {
namespace {
void Append(std::vector<HardwareStep> &steps, HardwareStep::Kind kind, const std::string &device,
            const std::string &reg, const std::string &field, std::uint64_t value)
{
    HardwareStep s; s.kind = kind; s.device = device; s.reg = reg; s.field = field; s.value = value; steps.push_back(s);
}
void Verified(std::vector<HardwareStep> &steps, const std::string &device, const std::string &reg,
              const std::string &field, std::uint64_t value)
{
    Append(steps, HardwareStep::WriteValue, device, reg, field, value);
    Append(steps, HardwareStep::ExpectValue, device, reg, field, value);
}
std::vector<HardwareStep> BeginAsAd(unsigned int mask)
{
    if (mask == 0 || mask > 15) throw std::invalid_argument("Active AsAd mask must be 1..15");
    std::vector<HardwareStep> steps;
    Append(steps, HardwareStep::ExpectValue, "ctrl", "pipeCtrl", "firmwareEnable", 0);
    Append(steps, HardwareStep::ExpectValue, "ctrl", "pipeCtrl", "intEnable", 0);
    Verified(steps, "ctrl", "ASAD_sc_mask", "select", mask);
    return steps;
}
void Delay(std::vector<HardwareStep> &steps, int milliseconds)
{
    HardwareStep s; s.kind = HardwareStep::Delay; s.delayMs = milliseconds; steps.push_back(s);
}
}
std::vector<HardwareStep> CoBoPrepareFoundation(const CoBoPrepareFoundationSettings &s)
{
    const auto writes = CompileCoBoPrepareFoundation(s); // 后段全部合法才生成计划。
    std::vector<HardwareStep> steps;
    Append(steps, HardwareStep::ExpectValue, "ctrl", "pipeCtrl", "firmwareEnable", 0);
    Append(steps, HardwareStep::ExpectValue, "ctrl", "pipeCtrl", "intEnable", 0);
    if (s.mutantStatusRegister) Verified(steps, "ctrl", "mutantConfig", "initDone", 0);
    if (s.firmwareInfoRegisters) {
        Append(steps, HardwareStep::ReadValue, "ctrl", "firmwareRelease", "", 0);
        Append(steps, HardwareStep::ReadValue, "ctrl", "firmwareDate", "", 0);
    }
    if (s.verifyHardwareVersion) Append(steps, HardwareStep::ExpectValue, "ctrl", "hardwareVersion", "", s.hardwareVersion);
    const auto pipeline = CoBoPipelineInit(); steps.insert(steps.end(), pipeline.begin(), pipeline.end());
    for (const auto &w : writes) Verified(steps, w.device, w.reg, w.field, w.value);
    return steps;
}
std::vector<HardwareStep> CoBoAsAdInputProtections(unsigned int mask)
{
    auto steps = BeginAsAd(mask);
    // 校准后、输入槽位配置前的初始保护；先开启抗放电，再禁止采样/读出。
    Append(steps, HardwareStep::WriteValue, "asad", "IM0", "", 0x0f);
    Append(steps, HardwareStep::WriteValue, "asad", "IM1", "", 0);
    return steps;
}
std::vector<HardwareStep> CoBoAsAdInputsInit(const AsAdInputSettings &s)
{
    const auto image = CompileAsAdInputs(s);
    auto steps = BeginAsAd(image.asadMask);
    for (const auto &b : image.boards) {
        Verified(steps, "ctrl", "ASAD_sc_mask", "select", 1U << b.asadId);
        Verified(steps, "ctrl", "asadSelect", "d", b.asadId);
        // ADC 是只写寄存器；IM 也不添加未经硬件确认的隐式读回。
        Append(steps, HardwareStep::WriteValue, "asad", "ADC0", "", b.adc0);
        Append(steps, HardwareStep::WriteValue, "asad", "IM0", "", b.im0);
        Append(steps, HardwareStep::WriteValue, "asad", "IM1", "", b.im1);
    }
    Verified(steps, "ctrl", "ASAD_sc_mask", "select", image.asadMask);
    return steps;
}
std::vector<HardwareStep> CoBoAsAdAdcChannelsFinal(const AsAdInputSettings &s)
{
    const auto image = CompileAsAdInputs(s);
    auto steps = BeginAsAd(image.asadMask);
    for (const auto &board : image.boards) {
        Verified(steps,"ctrl","ASAD_sc_mask","select",1U << board.asadId);
        Append(steps,HardwareStep::WriteValue,"asad","ADC0","",board.adc0);
        steps.back().annotation = "final ADC channels after last ADC reset; write-only, no electrical/readback proof";
    }
    Verified(steps,"ctrl","ASAD_sc_mask","select",image.asadMask);
    return steps;
}
std::vector<HardwareStep> CoBoAsAdAdcInit(unsigned int mask, bool ddr)
{
    auto steps = BeginAsAd(mask);
    const auto write = [&steps](const char *reg, std::uint64_t value) { Append(steps, HardwareStep::WriteValue, "asad", reg, "", value); };
    write("ADC0", 0x400); // 自清零软件复位，不能按 0x400 校验读回。
    write("ADC2", 0x2c0); write("ADC3", 0x0c3); write("ADC4", 0); write("ADC5", ddr ? 0x401 : 0x403);
    Append(steps, HardwareStep::WriteValue, "ctrl", "pipeCtrl", "adcDeskew", 1); Delay(steps, 1000);
    Append(steps, HardwareStep::WriteValue, "ctrl", "pipeCtrl", "adcDeskew", 0);
    write("ADC2", 0x2a0);
    Append(steps, HardwareStep::WriteValue, "ctrl", "pipeCtrl", "bitSlipCalibrate", 1); Delay(steps, 1000);
    Append(steps, HardwareStep::WriteValue, "ctrl", "pipeCtrl", "bitSlipCalibrate", 0);
    write("ADC2", 0x200); write("ADC5", ddr ? 0x401 : 0x403);
    return steps;
}
std::vector<HardwareStep> CoBoAsAdDacInit(unsigned int mask)
{
    auto steps = BeginAsAd(mask);
    Append(steps, HardwareStep::WriteValue, "asad", "DAC0", "", 0x81);
    for (unsigned int i = 1; i < 8; ++i) if (i != 2)
        Append(steps, HardwareStep::WriteValue, "asad", "DAC" + std::to_string(i), "", 0);
    return steps; // DAC2 为 VERSION 只读寄存器，始终不写。
}
std::vector<HardwareStep> CoBoManualReadDataDelay(unsigned int delay)
{
    if (delay > 255) throw std::invalid_argument("Manual read data delay must be 0..255");
    std::vector<HardwareStep> steps;
    Append(steps, HardwareStep::ExpectValue, "ctrl", "pipeCtrl", "firmwareEnable", 0);
    Append(steps, HardwareStep::ExpectValue, "ctrl", "pipeCtrl", "intEnable", 0);
    for (unsigned int i = 0; i < 4; ++i) Verified(steps, "ctrl", "readDataDelay", "AGET" + std::to_string(i), delay);
    return steps;
}
}
