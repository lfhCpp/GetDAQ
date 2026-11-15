#ifndef GETDAQ_SYSTEMDESCRIPTION_P_H
#define GETDAQ_SYSTEMDESCRIPTION_P_H

#include "hardwareclient.h"
#include "systemdescription.h"

namespace GetDAQ {
namespace SystemDescriptionInternal {

const char *PolicyName(DescriptionPolicy policy);
const char *RoleName(SystemDescriptionNode::Role role);
HardwareStep Bit(const char *field, HardwareStep::Kind kind);
std::vector<HardwareStep> ClearBit(const char *field);
void Inventory(HardwareClient &hardware, const HardwareDescription &description);
void Stopped(HardwareClient &hardware, const SystemDescriptionNode &node);

}
}

#endif
