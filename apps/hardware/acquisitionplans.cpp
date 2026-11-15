// Host run steps adapted from received CEA GET CoBoNode/MutantNode.
// Original CeCILL source/license retained in source/ and third_party/licenses/.
#include "acquisitionplans.h"
#include <stdexcept>
namespace GetDAQ {
namespace {
void Add(std::vector<HardwareStep> &steps,HardwareStep::Kind kind,const char *device,
         const std::string &reg,const std::string &field,std::uint64_t value) {
    HardwareStep s; s.kind = kind; s.device = device; s.reg = reg; s.field = field;
    s.value = value; s.requireBoundDescription = true; steps.push_back(s);
}
void Write(std::vector<HardwareStep> &steps,const std::string &reg,const std::string &field,std::uint64_t value) {
    Add(steps,HardwareStep::WriteValue,"ctrl",reg,field,value);
}
void Verified(std::vector<HardwareStep> &steps,const std::string &reg,const std::string &field,std::uint64_t value) {
    Write(steps,reg,field,value); Add(steps,HardwareStep::ExpectValue,"ctrl",reg,field,value);
}
std::uint64_t SharedMemoryValue(const CoBoReadoutImage &image,const char *reg) {
    // bufferStart/End描述逐AsAd分配；shared固件实际写入的是全局范围，取编译写入像。
    const CoBoRegisterValue *found = nullptr;
    for (const auto &w : image.writes) if (w.device == "ctrl" && w.reg == reg && w.field.empty()) {
        if (found) throw std::logic_error("Duplicate compiled shared memory register");
        found = &w;
    }
    if (!found) throw std::logic_error("Missing compiled shared memory register");
    return found->value;
}
}
CoBoProductionStopPlan BuildCoBoProductionStopPlan() {
    CoBoProductionStopPlan plan;
    HardwareStep pulser; pulser.kind = HardwareStep::PulserCommand;
    pulser.pulserOperation = HardwareStep::StopPeriodic; pulser.requireBoundDescription = true;
    plan.pulser.push_back(pulser);
    Verified(plan.firmware,"pipeCtrl","firmwareEnable",0);
    Verified(plan.interrupts,"pipeCtrl","intEnable",0);
    return plan;
}
CoBoAcquisitionPlan BuildCoBoAcquisitionPlan(const CoBoConfigureSettings &settings) {
    const auto image = CompileCoBoConfigure(settings); CoBoAcquisitionPlan plan;
    Add(plan.stopped,HardwareStep::ExpectValue,"ctrl","pipeCtrl","firmwareEnable",0);
    Add(plan.stopped,HardwareStep::ExpectValue,"ctrl","pipeCtrl","intEnable",0);
    Add(plan.stopped,HardwareStep::ExpectValue,"ctrl","mutantConfig","initDone",1);
    const bool shared = settings.readout.bufferLayout == CoBoReadoutSettings::SharedBuffer;
    const auto sharedStart = shared ? SharedMemoryValue(image.readout,"memStart") : 0;
    const auto sharedEnd = shared ? SharedMemoryValue(image.readout,"memEnd") : 0;
    // 配置不可变：先实际读回memStart/memEnd匹配本次配置，再按该值复位指针。
    // 不在旧布局写失败后猜另一种布局，也不接受运行时被另一控制者改变的范围。
    for (unsigned int i = 0; i < (shared ? 1U : 4U); ++i) {
        const auto suffix = shared ? std::string() : "_asad"+std::to_string(i);
        Add(plan.stopped,HardwareStep::ExpectValue,"ctrl","memStart"+suffix,"",shared ? sharedStart : image.readout.bufferStart[i]);
        Add(plan.stopped,HardwareStep::ExpectValue,"ctrl","memEnd"+suffix,"",shared ? sharedEnd : image.readout.bufferEnd[i]);
    }
    for (unsigned int id = 0; id < 4; ++id) if (image.readout.aget.asadMask & (1U << id)) {
        Write(plan.temperatures,"asadSelect","d",id);
        for (const char *name : {"internalTemperature","externalTemperature"}) {
            Add(plan.temperatures,HardwareStep::PollValue,"asad",name,"",0);
            auto &s = plan.temperatures.back(); s.maximum = s.mask = 255; s.zeroReadRetries = 10;
        }
    }
    plan.runInit = plan.temperatures;
    if (!image.readout.useExternalTimeReset) Write(plan.runInit,"resetTime","",0);
    Verified(plan.beforeSenderStart,"pipeCtrl","firmwareEnable",0);
    Verified(plan.beforeSenderStart,"pipeCtrl","intEnable",0);
    Write(plan.beforeSenderStart,"pipeCtrl","reset",1);
    Verified(plan.beforeSenderStart,"pipeCtrl","backpressure",1);
    for (unsigned int i = 0; i < (shared ? 1U : 4U); ++i) {
        const auto suffix = shared ? std::string() : "_asad"+std::to_string(i);
        Add(plan.beforeSenderStart,HardwareStep::ExpectValue,"ctrl","memStart"+suffix,"",shared ? sharedStart : image.readout.bufferStart[i]);
        Verified(plan.beforeSenderStart,"readPtr"+suffix,"",shared ? sharedStart : image.readout.bufferStart[i]);
    }
    Verified(plan.afterSenderStart,"pipeCtrl","backpressure",0);
    Write(plan.afterSenderStart,"pipeCtrl","reset",0);
    Verified(plan.afterSenderStart,"pipeCtrl","intEnable",1);
    Verified(plan.afterSenderStart,"pipeCtrl","firmwareEnable",1);
    Write(plan.afterSenderStart,"agetController","reset",1);
    Write(plan.afterSenderStart,"agetController","reset",0);
    Write(plan.afterSenderStart,"aget_sc_configuration","SC_En",0);
    Write(plan.afterSenderStart,"aget_sc_configuration","pulseEnable",image.readout.aget.agetMask);
    Write(plan.afterSenderStart,"agetController","hitRegister",1);
    Write(plan.afterSenderStart,"agetController","calibration",0);
    Write(plan.afterSenderStart,"aget_sc_rw","",1);
    HardwareStep delay; delay.kind = HardwareStep::Delay; delay.delayMs = 100; plan.afterSenderStart.push_back(delay);
    Write(plan.afterSenderStart,"aget_sc_configuration","SC_En",image.readout.aget.agetMask);
    Write(plan.afterSenderStart,"aget_sc_configuration","pulseEnable",0);
    if (image.asad.pulser.periodic) {
        HardwareStep pulser; pulser.kind = HardwareStep::PulserCommand;
        pulser.pulserOperation = HardwareStep::StartPeriodic; pulser.requireBoundDescription = true;
        plan.afterSenderStart.push_back(pulser);
    }
    const auto stop = BuildCoBoProductionStopPlan();
    plan.stopPulser = stop.pulser; plan.stopFirmware = stop.firmware; plan.stopInterrupts = stop.interrupts;
    return plan;
}
std::vector<HardwareStep> MutantAcquisitionRun(bool enabled) {
    std::vector<HardwareStep> steps;
    Add(steps,HardwareStep::WriteValue,"CARD_B_EMC","TRIGGER_CTRL","run",enabled);
    Add(steps,HardwareStep::ExpectValue,"CARD_B_EMC","TRIGGER_CTRL","run",enabled);
    return steps;
}
}
