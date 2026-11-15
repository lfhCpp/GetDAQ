#include "hardwareclient_p.h"
#include <stdexcept>

namespace GetDAQ {
namespace HardwareClientInternal {

void ValidatePlanStructure(const std::vector<HardwareStep> &steps, bool hasBoundDescription) {
    // 纯本地结构检查必须先于远端元数据访问。
    for (std::size_t i = 0; i < steps.size(); ++i) {
        const auto &step = steps[i];
        if (step.requireBoundDescription && !hasBoundDescription)
            throw std::invalid_argument("Plan requires a bound hardware description");
        if (step.annotation.size() > 1024) throw std::invalid_argument("Plan annotation exceeds 1024 bytes");
        for (unsigned char c : step.annotation) if (c < 32 || c > 126)
            throw std::invalid_argument("Plan annotation must contain printable ASCII");
        if (step.conditionStep < -1 || (step.conditionStep >= 0 && (std::size_t(step.conditionStep) >= i ||
            steps[std::size_t(step.conditionStep)].kind != HardwareStep::ReadValue || steps[std::size_t(step.conditionStep)].conditionStep != -1)))
            throw std::invalid_argument("Condition must reference an earlier unconditional ReadValue");
        if (step.conditionStep == -1 && (step.conditionMask != UINT64_MAX || step.conditionValue))
            throw std::invalid_argument("Condition parameters require a condition step");
        if (step.kind != HardwareStep::PollValue && (step.minimum || step.maximum != UINT64_MAX || step.pollAttempts != 1 || step.zeroReadRetries != 1 || step.pollDelayMs))
            throw std::invalid_argument("Polling parameters are only valid on PollValue");
        if (step.kind == HardwareStep::ExpectObservedValue) {
            if (step.observationStep < 0 || std::size_t(step.observationStep) >= i ||
                steps[std::size_t(step.observationStep)].kind != HardwareStep::ReadValue || steps[std::size_t(step.observationStep)].conditionStep != -1)
                throw std::invalid_argument("Cached expectation must reference an earlier unconditional ReadValue");
        } else if (step.observationStep != -1) throw std::invalid_argument("Observation reference requires ExpectObservedValue");
        if (step.kind != HardwareStep::LedCommand && (step.ledOperation != HardwareStep::SetAllLeds || step.ledType != HardwareStep::LedP || step.ledState != HardwareStep::LedOff || step.asadId || step.ledId))
            throw std::invalid_argument("LED parameters require LedCommand");
    }
}

}
}
