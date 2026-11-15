// CoBoNode::impl_prepare sequence adapted from received CEA GET source.
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "coboprepareclient.h"
#include <stdexcept>
namespace GetDAQ {
namespace {
void Expect(std::vector<HardwareStep> &steps, const char *reg, const char *field, std::uint64_t value)
{
    HardwareStep step; step.kind = HardwareStep::ExpectValue; step.device = "ctrl"; step.reg = reg; step.field = field;
    step.value = value; step.requireBoundDescription = true; steps.push_back(step);
}
void Stage(std::vector<HardwareStep> &destination, const char *name, std::vector<HardwareStep> fragment)
{
    if (fragment.empty()) throw std::invalid_argument("Prepare stage must not be empty");
    fragment.front().annotation = std::string("prepare_stage=")+name+"; "+fragment.front().annotation;
    AppendHardwareSteps(destination,fragment);
}
}
CoBoPreparePlan BuildCoBoPreparePlan(const CoBoPrepareSettings &s)
{
    ValidateCoBoPrepareSettings(s);
    CoBoPreparePlan plan;
    auto &before = plan.beforeSubscription, &after = plan.afterSubscription;
    Stage(before,"foundation",CoBoPrepareFoundation(s.foundation));
    before.front().requireBoundDescription = true;
    before.front().annotation += "single CoBo Prepare; whole-plan preflight; no ECC Prepared or system alignment assertion";
    Stage(before,"asadInit",CoBoAsAdPrepare(s.monitoring));
    Stage(before,"clockInit",CoBoClockInit(s.clock));
    // 旧CoBoNode对所有板型都写手动延迟；只有R-CoBo实际使用它。
    Stage(before,"setReadDataDelay",CoBoManualReadDataDelay(s.manualReadDataDelay));
    Stage(before,"asadAgetInputManagerInit",CoBoAsAdInputsInit(s.inputs));
    Stage(before,"agetInit",CoBoAgetInit(s.calibration));
    Stage(before,"asadAdcInit",CoBoAsAdAdcInit(s.inputs.asadMask,s.adcDoubleDataRate));
    // 校准与ADCInit均会复位ADC；只在最后一次复位之后重应用通道，保留最终ADC2/5。
    Stage(before,"adcChannelsFinal",CoBoAsAdAdcChannelsFinal(s.inputs));
    Stage(before,"asadDacInit",CoBoAsAdDacInit(s.inputs.asadMask));
    Stage(before,"asadAlertsInit",CoBoAsAdProtectionInit(s.monitoring));
    // 先建立自有订阅，再开启板端检查器，避免开启时尚无本客户端订阅的空隙。
    Stage(after,"boardAlarmMonitor",CoBoAsAdAlarmMonitorInit(s.monitoring));
    Stage(after,"asadMonitoringRead",CoBoAsAdMonitoringRead(s.monitoring));
    Stage(after,"setGlobalRegThresholds",CoBoAgetGlobalThreshold(s.calibration,2));
    Stage(after,"calibrateAutoReadDataDelay",CoBoAutomaticReadDelayCalibrate(s.calibration));
    Stage(after,"selectReadDataDelay",CoBoSelectReadDataDelay(s.calibration));
    Expect(after,"pipeCtrl","firmwareEnable",0); Expect(after,"pipeCtrl","intEnable",0);
    if (s.calibration.computeAutomaticReadDelay) {
        Expect(after,"pipeCtrl","SCRAutoDelayCalibrate",0); Expect(after,"pipeCtrl","ScrScwOverride",0);
    }
    Expect(after,"ASAD_sc_mask","select",s.inputs.asadMask);
    Expect(after,"aget_sc_configuration","SC_En",CompileCoBoCalibration(s.calibration).agetMask);
    if (s.foundation.mutantStatusRegister) Expect(after,"mutantConfig","initDone",0);
    // 用组合副本检查整体上限和观察索引；跨订阅执行的两段保持各自索引。
    auto complete = before; AppendHardwareSteps(complete,after);
    return plan;
}
}
