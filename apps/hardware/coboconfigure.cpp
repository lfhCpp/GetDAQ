// Adapted from CEA GET CoBoNode::impl_configure and AsAdPulserMgrI.
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "hardwareclient.h"
#include "coboasadconfiguration.h"
namespace GetDAQ {
std::vector<HardwareStep> CoBoConfigure(const CoBoConfigureSettings &settings)
{
    const auto image = CompileCoBoConfigure(settings); // AsAd 和后段 Module 也必须先全部编译。
    std::vector<HardwareStep> steps;
    const auto append = [&steps](HardwareStep::Kind kind, const std::string &device, const std::string &reg,
                                 const std::string &field, std::uint64_t value) {
        HardwareStep s; s.kind = kind; s.device = device; s.reg = reg; s.field = field; s.value = value; steps.push_back(s);
    };
    const auto verified = [&append](const std::string &device, const std::string &reg, const std::string &field, std::uint64_t value) {
        append(HardwareStep::WriteValue, device, reg, field, value);
        append(HardwareStep::ExpectValue, device, reg, field, value);
    };
    const auto pulser = [&steps](HardwareStep::PulserOperation operation, int voltage, unsigned int period, bool enabled) {
        HardwareStep s; s.kind = HardwareStep::PulserCommand; s.pulserOperation = operation;
        s.voltageMv = voltage; s.periodMs = period; s.enabled = enabled; steps.push_back(s);
    };
    append(HardwareStep::ExpectValue, "ctrl", "pipeCtrl", "firmwareEnable", 0);
    append(HardwareStep::ExpectValue, "ctrl", "pipeCtrl", "intEnable", 0);
    // 旧计时器可能仍存在；配置阶段只停止，绝不启动。清除慢控制触发防止设置默认电压触发事件。
    pulser(HardwareStep::StopPeriodic, 0, 0, false);
    verified("ctrl", "triggerMask", "", 0);
    const auto aget = CoBoAgetConfigure(settings.readout.aget); steps.insert(steps.end(), aget.begin(), aget.end());
    // 所有 AGET 型号/寄存器之后写 FPGA 阈值，再设置 AsAd；Module 触发最后才开启。
    for (const auto &w : image.readout.writes) if (w.device != "ctrl") verified(w.device, w.reg, w.field, w.value);
    const auto &p = image.asad.pulser;
    for (const auto &b : image.asad.boards) {
        // 原 Configure 在每块 AsAd 的 TCM 前再次切换慢控制；不能仅凭模拟器省略时序。
        const auto slow = CoBoSwitchToSlowControl(image.readout.aget.agetMask);
        steps.insert(steps.end(), slow.begin(), slow.end());
        verified("ctrl", "ASAD_sc_mask", "select", 1U << b.asadId);
        verified("ctrl", "asadSelect", "d", b.asadId);
        verified("asad", "TCM0", "Calib", b.calibrationMode);
        verified("asad", "TCM0", "Rg", b.capacitor11pF);
        verified("asad", "TCM0", "Delay", b.triggerDelay);
        verified("asad", "TCM0", "Trg", 0);
        pulser(HardwareStep::SetDefaultVoltage, p.defaultVoltageMv, 0, false);
        append(HardwareStep::ExpectValue, "asad", "TCM1", "", p.defaultDac >> 8);
        steps.back().mask = 0x3f;
        append(HardwareStep::ExpectValue, "asad", "TCM2", "", p.defaultDac & 0xff);
        steps.back().mask = 0xff;
        if (p.external) {
            verified("asad", "TCM0", "Trg", 1);
            pulser(HardwareStep::TriggerPulser, p.voltageMv, 0, false);
            append(HardwareStep::ExpectValue, "asad", "TCM1", "", p.dac >> 8);
            steps.back().mask = 0x3f;
            append(HardwareStep::ExpectValue, "asad", "TCM2", "", p.dac & 0xff);
            steps.back().mask = 0xff;
        }
        verified("asad", "ISPM", "", b.inspection);
        steps.back().mask = 0xff;
    }
    verified("ctrl", "ASAD_sc_mask", "select", image.asad.asadMask);
    pulser(HardwareStep::ConfigureExternal, p.voltageMv, 0, p.external);
    for (const auto &w : image.readout.writes) if (w.device == "ctrl") verified(w.device, w.reg, w.field, w.value);
    if (p.periodic) {
        pulser(HardwareStep::ConfigurePeriodic, p.voltageMv, p.periodMs, false);
        pulser(HardwareStep::ConfigureDouble, p.doublePulse ? p.secondVoltageMv : 0, 0, false);
        pulser(HardwareStep::SetRandom, 0, 0, p.poisson);
    }
    // 旧 Configure 即使普通模式也发此初始化脉冲；命令寄存器不强加未知读回语义。
    append(HardwareStep::WriteValue, "ctrl", "aget_2p_initNow", "", 1);
    HardwareStep delay; delay.kind = HardwareStep::Delay; delay.delayMs = 20; steps.push_back(delay);
    append(HardwareStep::WriteValue, "ctrl", "aget_2p_initNow", "", 0);
    return steps;
}
}
