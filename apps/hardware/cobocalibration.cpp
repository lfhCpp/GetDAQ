// Adapted from CEA GET CoBoNode agetInit/agetCal and SCR delay calibration.
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "hardwareclient.h"
#include "cobocalibrationconfiguration.h"
#include <stdexcept>
namespace GetDAQ {
namespace {
void Append(std::vector<HardwareStep> &steps, HardwareStep::Kind kind, const std::string &device,
            const std::string &reg, const std::string &field, std::uint64_t value = 0)
{
    HardwareStep step; step.kind = kind; step.device = device; step.reg = reg; step.field = field; step.value = value; steps.push_back(step);
}
void Write(std::vector<HardwareStep> &steps, const char *device, const char *reg, const std::string &field, std::uint64_t value)
{ Append(steps,HardwareStep::WriteValue,device,reg,field,value); }
void Verified(std::vector<HardwareStep> &steps, const char *reg, const char *field, unsigned int value)
{
    Write(steps,"ctrl",reg,field,value); Append(steps,HardwareStep::ExpectValue,"ctrl",reg,field,value);
}
void Delay(std::vector<HardwareStep> &steps, int milliseconds)
{ HardwareStep step; step.kind = HardwareStep::Delay; step.delayMs = milliseconds; steps.push_back(step); }
std::vector<HardwareStep> Begin(const CoBoCalibrationSettings &settings, const char *context)
{
    CompileCoBoCalibration(settings);
    std::vector<HardwareStep> steps;
    Append(steps,HardwareStep::ExpectValue,"ctrl","pipeCtrl","firmwareEnable",0);
    steps.back().requireBoundDescription = true;
    steps.back().annotation = std::string(context) + "; not complete Prepared; caller must ensure clocks/power, exclusive control and receiver stopped; delay_layout=" +
        (settings.delayLayout == CoBoCalibrationSettings::GlobalDelayField ? "global" : "per-asad") +
        "; calibrationAget_register=" + (settings.calibrationAgetRegister ? "present" : "explicit legacy absent; chip0 only");
    Append(steps,HardwareStep::ExpectValue,"ctrl","pipeCtrl","intEnable",0);
    return steps;
}
void EnableDelay(std::vector<HardwareStep> &steps, const CoBoCalibrationSettings &settings,
                 const CoBoCalibrationImage &image, bool computing)
{
    if (settings.delayLayout == CoBoCalibrationSettings::GlobalDelayField) {
        Verified(steps,"pipeCtrl","SCRAutoDelayEnable",computing || image.globalAutomaticReadDelay ? 1 : 0);
    } else for (unsigned int id = 0; id < 4; ++id) if (image.asadMask & (1U << id)) {
        const auto field = "SCRAutoDelayEnable" + std::to_string(id);
        Verified(steps,"pipeCtrl",field.c_str(),computing || (image.automaticReadDelayMask & (1U << id)) ? 1 : 0);
    }
}
}
std::vector<HardwareStep> CoBoAgetCalibrate(const CoBoCalibrationSettings &s)
{
    const auto image = CompileCoBoCalibration(s); auto steps = Begin(s,"CoBoAgetCalibrate; ADC reset/test pattern; not final ADC initialization");
    // 明确广播 AsAd 与读取芯片，避免沿用上一个子流程留下的非活动选择器。
    Verified(steps,"ASAD_sc_mask","select",image.asadMask);
    Verified(steps,"agetChipSelect","d",image.firstCalibrationChip);
    if (s.calibrationAgetRegister) for (unsigned int id = 0; id < 4; ++id)
        Verified(steps,"calibrationAget",("AsAd" + std::to_string(id)).c_str(),image.calibrationAgetIds[id]);
    Write(steps,"asad","ADC0","",0x400); // 软件复位自清零，不添加回显。
    Write(steps,"asad","ADC2","",0x2a0); Write(steps,"asad","ADC3","",1);
    Write(steps,"asad","ADC4","",0); Write(steps,"asad","ADC5","",0x401);
    Write(steps,"ctrl","ckpFastDivider","d",0);
    Write(steps,"ctrl","agetController","reset",1); Write(steps,"ctrl","agetController","reset",0);
    Write(steps,"ctrl","aget_sc_configuration","SC_En",image.agetMask);
    Write(steps,"ctrl","aget_sc_configuration","pulseEnable",image.agetMask);
    Write(steps,"ctrl","agetController","hitRegister",0); Write(steps,"ctrl","agetController","calibration",0);
    Delay(steps,200); Append(steps,HardwareStep::ReadValue,"aget","reg5","");
    Write(steps,"ctrl","agetController","reset",1); Write(steps,"ctrl","agetController","reset",0);
    Write(steps,"ctrl","aget_sc_configuration","pulseEnable",image.agetMask);
    Write(steps,"ctrl","agetController","hitRegister",0); Write(steps,"ctrl","agetController","calibration",1);
    Append(steps,HardwareStep::ReadValue,"aget","reg5","");
    Write(steps,"ctrl","agetController","reset",1); Write(steps,"ctrl","agetController","reset",0);
    Write(steps,"ctrl","agetController","calibration",0);
    Write(steps,"ctrl","aget_sc_configuration","pulseEnable",image.agetMask);
    Write(steps,"ctrl","agetController","hitRegister",1);
    Write(steps,"aget","hit","",UINT64_C(0xa000000000000005));
    Write(steps,"ctrl","agetController","calibration",1);
    Write(steps,"ctrl","aget_sc_configuration","pulseEnable",0);
    Write(steps,"ctrl","agetController","hitRegister",1);
    Write(steps,"aget","hit","",UINT64_C(0xa000000000000005));
    Write(steps,"ctrl","agetController","calibration",0);
    Write(steps,"ctrl","aget_sc_configuration","pulseEnable",image.agetMask);
    Write(steps,"ctrl","agetController","hitRegister",0); Append(steps,HardwareStep::ReadValue,"aget","reg5","");
    Write(steps,"ctrl","agetController","reset",1); Write(steps,"ctrl","agetController","reset",0);
    AppendHardwareSteps(steps,CoBoSwitchToSlowControl(image.agetMask));
    for (const auto &version : s.chipVersions) {
        Verified(steps,"agetChipSelect","d",version.first);
        Append(steps,HardwareStep::ExpectValue,"aget","reg5","",version.second); steps.back().mask = 0xffff;
    }
    return steps;
}
std::vector<HardwareStep> CoBoAgetInit(const CoBoCalibrationSettings &s)
{
    const auto image = CompileCoBoCalibration(s); auto steps = Begin(s,"CoBoAgetInit; serial calibration and SCA readout initialization only");
    Write(steps,"ctrl","agetController","reset",0); Write(steps,"ctrl","agetController","reset",1); Write(steps,"ctrl","agetController","reset",0);
    Verified(steps,"aget_sc_configuration","SC_En",image.agetMask);
    Verified(steps,"ASAD_sc_mask","select",image.asadMask); Write(steps,"asad","resetFast","",0);
    AppendHardwareSteps(steps,CoBoAgetCalibrate(s));
    Write(steps,"aget","reg1","DAC",1); // 与旧 agetInit 相同的板端字段访问，广播写入。
    Write(steps,"ctrl","initSCAReadOutAfterPowerOnNow","",1); // 命令，不按 1 验证回显。
    return steps;
}
std::vector<HardwareStep> CoBoAutomaticReadDelayCalibrate(const CoBoCalibrationSettings &s)
{
    const auto image = CompileCoBoCalibration(s); auto steps = Begin(s,"CoBoAutomaticReadDelayCalibrate; overwrites AGET reg1/reg2; transient firmwareEnable is calibration, not Start");
    if (!s.computeAutomaticReadDelay) { steps.front().annotation += "; compute disabled: no calibration writes"; return steps; }
    Verified(steps,"aget_sc_configuration","SC_En",image.agetMask);
    Write(steps,"aget","reg1","",0x3e3a41a7); Write(steps,"aget","reg2","",0x0028a197);
    EnableDelay(steps,s,image,true);
    Write(steps,"ctrl","pipeCtrl","SCRAutoDelayCalibrate",1);
    Write(steps,"ctrl","pipeCtrl","intEnable",0); Write(steps,"ctrl","pipeCtrl","SCR",0); Write(steps,"ctrl","pipeCtrl","SCW",0);
    Write(steps,"ctrl","pipeCtrl","ScrScwOverride",1); Write(steps,"ctrl","pipeCtrl","reset",0);
    Write(steps,"ctrl","pipeCtrl","firmwareEnable",1); Write(steps,"ctrl","pipeCtrl","SCW",1); Delay(steps,200);
    Write(steps,"ctrl","pipeCtrl","SCW",0); Write(steps,"ctrl","pipeCtrl","SCR",1); Write(steps,"ctrl","pipeCtrl","SCR",0); Delay(steps,200);
    Write(steps,"ctrl","pipeCtrl","firmwareEnable",0); Write(steps,"ctrl","pipeCtrl","SCRAutoDelayCalibrate",0);
    Write(steps,"ctrl","pipeCtrl","ScrScwOverride",0);
    Append(steps,HardwareStep::ExpectValue,"ctrl","pipeCtrl","firmwareEnable",0);
    Append(steps,HardwareStep::ExpectValue,"ctrl","pipeCtrl","SCRAutoDelayCalibrate",0);
    Append(steps,HardwareStep::ExpectValue,"ctrl","pipeCtrl","ScrScwOverride",0);
    return steps; // SCRAutoDelayEnable 暂时为 1；由独立的选择子计划应用最终策略。
}
std::vector<HardwareStep> CoBoSelectReadDataDelay(const CoBoCalibrationSettings &s)
{
    const auto image = CompileCoBoCalibration(s); auto steps = Begin(s,"CoBoSelectReadDataDelay; selecting a stored result does not prove valid physical calibration");
    EnableDelay(steps,s,image,false); return steps;
}
std::vector<HardwareStep> CoBoAgetGlobalThreshold(const CoBoCalibrationSettings &s, unsigned int threshold)
{
    const auto image = CompileCoBoCalibration(s);
    if (threshold > 7) throw std::invalid_argument("AGET global threshold must be 0..7");
    auto steps = Begin(s,"CoBoAgetGlobalThreshold; board DACthreshold field preserves other reg1 bits including DAC polarity");
    AppendHardwareSteps(steps,CoBoSwitchToSlowControl(image.agetMask));
    for (const auto &version : s.chipVersions) {
        Verified(steps,"aget_sc_configuration","SC_En",1U << version.first);
        Verified(steps,"agetChipSelect","d",version.first);
        Write(steps,"aget","reg1","DACthreshold",threshold);
        Append(steps,HardwareStep::ExpectValue,"aget","reg1","DACthreshold",threshold);
    }
    Verified(steps,"aget_sc_configuration","SC_En",image.agetMask); return steps;
}
}
