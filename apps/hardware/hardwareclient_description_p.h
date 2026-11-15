#ifndef GETDAQ_HARDWARECLIENT_DESCRIPTION_P_H
#define GETDAQ_HARDWARECLIENT_DESCRIPTION_P_H

#include "hardwareclient.h"
#include <mdaq/hw/Control.h>
#include <string>

namespace GetDAQ {
namespace HardwareClientInternal {

struct HardwareDescriptionPayload {
    mdaq::hw::NodeConfig nodeConfig;
    std::string devicesJson;
};

HardwareDescriptionPayload BuildHardwareDescriptionPayload(const HardwareDescription &description);

} // namespace HardwareClientInternal
} // namespace GetDAQ

#endif
