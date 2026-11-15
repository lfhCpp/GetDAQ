// Adapted from CEA GET CoBoNode AsAd power/monitoring preparation.
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "hardwareclient.h"
#include "coboled_p.h"
#include "cobomonitoringconfiguration.h"
#include <algorithm>
#include <stdexcept>
namespace GetDAQ {
namespace {
void Append(std::vector<HardwareStep> &steps, HardwareStep::Kind kind, const std::string &device,
            const std::string &reg, const std::string &field, std::uint64_t value = 0)
{
    HardwareStep step; step.kind = kind; step.device = device; step.reg = reg; step.field = field; step.value = value; steps.push_back(step);
}
void Write(std::vector<HardwareStep> &steps, const std::string &device, const std::string &reg, const std::string &field, std::uint64_t value)
{ Append(steps,HardwareStep::WriteValue,device,reg,field,value); }
void Verified(std::vector<HardwareStep> &steps, const char *reg, const char *field, unsigned int value)
{
    Write(steps,"ctrl",reg,field,value); Append(steps,HardwareStep::ExpectValue,"ctrl",reg,field,value);
}
std::vector<HardwareStep> Begin(const CoBoMonitoringImage &image, const char *context)
{
    std::vector<HardwareStep> steps;
    Append(steps,HardwareStep::ExpectValue,"ctrl","pipeCtrl","firmwareEnable",0);
    steps.back().requireBoundDescription = true;
    steps.back().annotation = std::string(context) + "; not complete Prepared; exclusive ownership/power premises remain caller responsibility; mode_mapping=" +
        (image.modeMapping == CoBoMonitoringSettings::DeclaredParameters ? "declared" : "legacy-call-arguments");
    Append(steps,HardwareStep::ExpectValue,"ctrl","pipeCtrl","intEnable",0);
    return steps;
}
void Select(std::vector<HardwareStep> &steps, unsigned int id)
{
    Verified(steps,"ASAD_sc_mask","select",1U << id); Verified(steps,"asadSelect","d",id);
}
void Delay(std::vector<HardwareStep> &steps, unsigned int milliseconds)
{
    HardwareStep step; step.kind = HardwareStep::Delay; step.delayMs = int(milliseconds); steps.push_back(step);
}
void Limits(std::vector<HardwareStep> &steps, const AsAdMonitoringBoardImage &b)
{
    Write(steps,"asad","configuration1","",0x2c);
    const char *names[] = {"extTempLowLimit","extTempHighLimit","VDDLowLimit","VDDHighLimit","IDDLowLimit","IDDHighLimit"};
    for (unsigned int i = 0; i < 6; ++i) Write(steps,"asad",names[i],"",b.limits[i]);
}
void Mode(std::vector<HardwareStep> &steps, const AsAdMonitoringBoardImage &b, bool protection)
{
    Write(steps,"asad","configuration3","",b.configuration3);
    Write(steps,"asad","configuration2","",b.configuration2);
    Write(steps,"asad","configuration1","",protection ? b.protectionConfiguration1 : 0x2d);
}
void Poll(std::vector<HardwareStep> &steps, const char *reg, unsigned int minimum, const CoBoMonitoringSettings &s, unsigned int attempts)
{
    HardwareStep step; step.kind = HardwareStep::PollValue; step.device = "asad"; step.reg = reg;
    step.minimum = minimum; step.maximum = 255; step.pollAttempts = attempts; step.zeroReadRetries = s.zeroReadRetries;
    step.pollDelayMs = attempts > 1 ? s.settleDelayMs : 0; steps.push_back(step);
}
void PowerGuard(std::vector<HardwareStep> &steps, const char *reg, const std::string &field, unsigned int value,
                unsigned int mask, unsigned int allowed, unsigned int id, bool leds)
{
    if (!leds) {
        Append(steps,HardwareStep::ExpectValue,"ctrl",reg,field,value); steps.back().mask = mask; steps.back().allowedDifference = allowed; return;
    }
    Append(steps,HardwareStep::ReadValue,"ctrl",reg,field); const int observation = int(steps.size()-1);
    if (!allowed) {
        HardwareClientInternal::AppendLedModify(steps,id,HardwareStep::LedB,HardwareStep::LedSlowPulse);
        steps.back().conditionStep = observation; steps.back().conditionMask = mask; steps.back().conditionValue = value ? 0 : mask;
    }
    HardwareStep expected; expected.kind = HardwareStep::ExpectObservedValue; expected.observationStep = observation;
    expected.value = value; expected.mask = mask; expected.allowedDifference = allowed; steps.push_back(expected);
}
}
void AppendHardwareSteps(std::vector<HardwareStep> &destination, const std::vector<HardwareStep> &fragment)
{
    if (destination.size() > 4096 || fragment.size() > 4096-destination.size()) throw std::invalid_argument("Combined plan exceeds 4096 steps");
    // 片段条件是片段内索引；组合时必须平移，不能直接 vector::insert。
    for (std::size_t i = 0; i < fragment.size(); ++i) if (fragment[i].conditionStep < -1 || (fragment[i].conditionStep >= 0 &&
        (std::size_t(fragment[i].conditionStep) >= i || fragment[std::size_t(fragment[i].conditionStep)].kind != HardwareStep::ReadValue ||
         fragment[std::size_t(fragment[i].conditionStep)].conditionStep != -1)))
        throw std::invalid_argument("Fragment condition must reference its earlier unconditional ReadValue");
    for (std::size_t i = 0; i < fragment.size(); ++i) {
        const auto &step = fragment[i];
        if (step.kind == HardwareStep::ExpectObservedValue) {
            if (step.observationStep < 0 || std::size_t(step.observationStep) >= i || fragment[std::size_t(step.observationStep)].kind != HardwareStep::ReadValue ||
                fragment[std::size_t(step.observationStep)].conditionStep != -1) throw std::invalid_argument("Fragment cached expectation must reference its earlier unconditional ReadValue");
        } else if (step.observationStep != -1) throw std::invalid_argument("Fragment observation reference requires cached expectation");
    }
    auto combined = destination; combined.reserve(destination.size()+fragment.size());
    for (auto step : fragment) {
        if (step.conditionStep >= 0) step.conditionStep += int(destination.size());
        if (step.observationStep >= 0) step.observationStep += int(destination.size());
        combined.push_back(step);
    }
    destination.swap(combined);
}
std::vector<HardwareStep> CoBoAsAdPowerCycle(const CoBoMonitoringSettings &s)
{
    const auto image = CompileCoBoMonitoring(s); auto steps = Begin(image,"CoBoAsAdPowerCycle; alarm subscription managed separately");
    const bool leds = s.ledPolicy == CoBoMonitoringSettings::RequiredLedManager;
    if (leds) AppendHardwareSteps(steps,CoBoLedSetAll(false,s.ledHardwareVersion));
    Write(steps,"ctrl","asadController","reset",1); Write(steps,"ctrl","asadController","manualCal",0);
    Write(steps,"ctrl","asadController","calibration",0); Write(steps,"ctrl","asadController","reset",0);
    for (unsigned int id = 0; id < 4; ++id) {
        const auto connection = "PLG" + std::to_string(id);
        Append(steps,HardwareStep::ReadValue,"ctrl","asadConnection",connection);
        const int observation = int(steps.size()-1); const auto start = steps.size();
        Select(steps,id); Append(steps,HardwareStep::ReadValue,"asad","interruptStatus1","");
        steps.back().annotation = "prior monitoring alert status; observation only, not an alarm-clear command";
        for (std::size_t i = start; i < steps.size(); ++i) { steps[i].conditionStep = observation; steps[i].conditionMask = 1; steps[i].conditionValue = 1; }
        const auto power = "powerON" + std::to_string(id);
        Write(steps,"ctrl","asadEnable",power,0); Delay(steps,s.powerDelayMs);
        if (!(image.asadMask & (1U << id))) continue;
        const auto &b = *std::find_if(image.boards.begin(),image.boards.end(),[id](const AsAdMonitoringBoardImage &board) { return board.asadId == id; });
        PowerGuard(steps,"asadConnection",connection,1,1,0,id,leds);
        PowerGuard(steps,"asadStatus","alarm",1U << id,1U << id,b.checkPowerSupply ? 0 : 1U << id,id,leds);
        steps.back().annotation = b.checkPowerSupply ? "AL before power-on: supply present expected" : "explicit checkPowerSupply=false; before-power AL difference tolerated and audited";
        Write(steps,"ctrl","asadEnable",power,1); Delay(steps,s.powerDelayMs);
        PowerGuard(steps,"asadConnection",connection,1,1,0,id,leds);
        PowerGuard(steps,"asadStatus","alarm",0,1U << id,0,id,leds);
        steps.back().annotation = "AL after power-on: zero means no alarm; different meaning than before power-on";
        PowerGuard(steps,"asadEnable",power,1,1,0,id,leds);
        if (leds) HardwareClientInternal::AppendLedModify(steps,id,HardwareStep::LedP,HardwareStep::LedOn);
    }
    Verified(steps,"ASAD_sc_mask","select",image.asadMask); return steps;
}
std::vector<HardwareStep> CoBoAsAdMonitoringInit(const CoBoMonitoringSettings &s)
{
    const auto image = CompileCoBoMonitoring(s); auto steps = Begin(image,"CoBoAsAdMonitoringInit; auto-protection disabled during settling");
    for (const auto &b : image.boards) {
        Select(steps,b.asadId); Write(steps,"asad","configuration2","",0x80); // 软件复位；不要求按 0x80 回显。
        Write(steps,"asad","interruptMask1","",0xff); Limits(steps,b); Mode(steps,b,false);
    }
    Verified(steps,"ASAD_sc_mask","select",image.asadMask); return steps;
}
std::vector<HardwareStep> CoBoAsAdProtectionInit(const CoBoMonitoringSettings &s)
{
    const auto image = CompileCoBoMonitoring(s); auto steps = Begin(image,"CoBoAsAdProtectionInit; hardware protection only, no continuous host alarm subscription");
    for (const auto &b : image.boards) {
        Append(steps,HardwareStep::ExpectValue,"ctrl","asadStatus","alarm",0); steps.back().mask = 1U << b.asadId;
        Append(steps,HardwareStep::ExpectValue,"ctrl","asadEnable","powerON"+std::to_string(b.asadId),1);
        Select(steps,b.asadId); Limits(steps,b); Write(steps,"asad","interruptMask1","",b.protectionMask); Mode(steps,b,true);
    }
    Verified(steps,"ASAD_sc_mask","select",image.asadMask); return steps;
}
std::vector<HardwareStep> CoBoAsAdMonitoringRead(const CoBoMonitoringSettings &s)
{
    const auto image = CompileCoBoMonitoring(s); auto steps = Begin(image,"CoBoAsAdMonitoringRead; raw samples, not continuous alarm service");
    const char *names[] = {"VDD","VDD1","IDD","internalTemperature","externalTemperature"};
    for (const auto &b : image.boards) {
        Select(steps,b.asadId);
        for (const auto *name : names) Poll(steps,name,0,s,1); // 保留旧零值最多十次读取；不重发写命令。
    }
    Verified(steps,"ASAD_sc_mask","select",image.asadMask); return steps;
}
std::vector<HardwareStep> CoBoAsAdPrepare(const CoBoMonitoringSettings &s)
{
    const auto image = CompileCoBoMonitoring(s); auto steps = CoBoAsAdPowerCycle(s);
    AppendHardwareSteps(steps,CoBoAsAdCalibrate(image.asadMask));
    AppendHardwareSteps(steps,CoBoAsAdMonitoringInit(s));
    for (const auto &b : image.boards) {
        Select(steps,b.asadId); Poll(steps,"VDD",b.vddSettleMinimum,s,s.settleAttempts);
    }
    AppendHardwareSteps(steps,CoBoAsAdInputProtections(image.asadMask));
    return steps; // ClockInit、AGET/ADC/DAC、最终保护和告警订阅另按完整 Prepare 次序组合。
}
std::vector<HardwareStep> CoBoAsAdAlarmMonitorInit(const CoBoMonitoringSettings &s)
{
    const auto image = CompileCoBoMonitoring(s); auto steps = Begin(image,"CoBoAsAdAlarmMonitorInit; global board checker, separate owned host subscription; no automatic ECC Stop");
    HardwareStep step; step.kind = HardwareStep::AsAdAlarmMonitorCommand; step.enabled = s.checkAsAdAlarms; steps.push_back(step); return steps;
}
}
