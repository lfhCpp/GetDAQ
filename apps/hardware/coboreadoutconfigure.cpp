// Adapted from CEA GET 2.3.0 CoBoNode Configure/readout routines.
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "hardwareclient.h"
#include "coboreadoutconfiguration.h"
namespace GetDAQ {
std::vector<HardwareStep> CoBoReadoutConfigure(const CoBoReadoutSettings &settings)
{
    const auto image = CompileCoBoReadout(settings); // 全部参数/缓冲区/跨芯片约束先检查。
    auto steps = CoBoAgetConfigure(settings.aget);
    for (const auto &value : image.writes) {
        HardwareStep step; step.device = value.device; step.reg = value.reg; step.field = value.field; step.value = value.value;
        steps.push_back(step);
        // 这里明确选择可读的配置目标；硬件客户端本身仍不隐式读回未知寄存器。
        step.kind = HardwareStep::ExpectValue; steps.push_back(step);
    }
    // Execute 对完整组合计划预检查；晚位置的设备/权限错误不会留下早期 AGET 写入。
    return steps;
}
}
