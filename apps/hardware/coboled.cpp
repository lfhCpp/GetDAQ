// Adapted from CEA GET CoBoNode LED manager commands.
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "hardwareclient.h"
#include "coboled_p.h"

#include <limits>
#include <stdexcept>

namespace GetDAQ {
namespace HardwareClientInternal {

void AppendLedModify(std::vector<HardwareStep> &steps, unsigned int id,
                     HardwareStep::LedType type, HardwareStep::LedState state)
{
    HardwareStep step;
    step.kind = HardwareStep::LedCommand;
    step.ledOperation = HardwareStep::ModifyLed;
    step.asadId = id;
    step.ledType = type;
    step.ledState = state;
    steps.push_back(step);
}

} // namespace HardwareClientInternal

std::vector<HardwareStep> CoBoLedSetAll(bool enabled, unsigned int version)
{
    if (version != 2 && version != 5)
        throw std::invalid_argument("Received LED manager only supports hardware version 2 or 5");
    std::vector<HardwareStep> steps;
    HardwareStep versionCheck;
    versionCheck.kind = HardwareStep::ExpectValue;
    versionCheck.device = "ctrl";
    versionCheck.reg = "hardwareVersion";
    versionCheck.value = version;
    versionCheck.requireBoundDescription = true;
    versionCheck.annotation = "board LED manager; acknowledgment is not physical GPIO/lamp verification";
    steps.push_back(versionCheck);
    HardwareStep command;
    command.kind = HardwareStep::LedCommand;
    command.enabled = enabled;
    steps.push_back(command);
    return steps;
}

std::vector<HardwareStep> CoBoLedModify(HardwareStep::LedType type, unsigned int id,
                                        HardwareStep::LedState state, unsigned int version)
{
    if (type > HardwareStep::LedD || state > HardwareStep::LedFastPulse || id > 3)
        throw std::invalid_argument("Invalid LED type/state/AsAd ID");
    auto steps = CoBoLedSetAll(false, version);
    steps.pop_back();
    HardwareClientInternal::AppendLedModify(steps, id, type, state);
    return steps;
}

std::vector<HardwareStep> CoBoLedBlinkAll()
{
    HardwareStep step;
    step.kind = HardwareStep::LedCommand;
    step.ledOperation = HardwareStep::BlinkAllLeds;
    step.annotation = "legacy LedManager::blinkLEDs; sequential one-shot blink; acknowledgment is not physical lamp verification";
    return {step};
}

std::vector<HardwareStep> CoBoLedFlip(unsigned int ledId)
{
    if (ledId > 16) throw std::invalid_argument("LED flip selection must be 0..15 or 16 for all LEDs");
    HardwareStep step;
    step.kind = HardwareStep::LedCommand;
    step.ledOperation = ledId == 16 ? HardwareStep::FlipAllLeds : HardwareStep::FlipOneLed;
    step.ledId = ledId == 16 ? 0 : ledId;
    step.annotation = "legacy non-idempotent GPIO LED state inversion; acknowledgment is not physical lamp verification";
    return {step};
}

std::vector<HardwareStep> CoBoLedPulse(unsigned int ledId, unsigned int periodMs)
{
    if (ledId > 16 || periodMs > unsigned(std::numeric_limits<int>::max()))
        throw std::invalid_argument("Invalid LED pulse selection/period");
    HardwareStep step;
    step.kind = HardwareStep::LedCommand;
    step.ledOperation = HardwareStep::PulseOneLed;
    step.ledId = ledId;
    step.periodMs = periodMs;
    step.annotation = "legacy LedManager::pulseLED half-period in milliseconds; zero stops the selected pulser; acknowledgment is not physical lamp verification";
    return {step};
}

} // namespace GetDAQ
