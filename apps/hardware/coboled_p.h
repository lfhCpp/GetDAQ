#ifndef GETDAQ_COBOLED_P_H
#define GETDAQ_COBOLED_P_H

#include "hardwareclient.h"

namespace GetDAQ {
namespace HardwareClientInternal {

void AppendLedModify(std::vector<HardwareStep> &steps, unsigned int id,
                     HardwareStep::LedType type, HardwareStep::LedState state);

} // namespace HardwareClientInternal
} // namespace GetDAQ

#endif
