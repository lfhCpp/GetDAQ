#ifndef GETDAQ_LEGACY_REGISTER_OFFSETS_P_H
#define GETDAQ_LEGACY_REGISTER_OFFSETS_P_H
#include "../../common/hardware/hardwaredescription.h"
#include <cstdint>
#include <limits>
#include <map>
#include <set>
namespace GetDAQ { namespace HardwareClientInternal {
// Some 32-bit GET servers report (relative offset - device base) in Ice::Long.
// Only accept a whole-device, unsigned-underflow representation proven against
// every register of an independently bound description. Never infer it unbound.
inline bool NormalizeLegacyRegisterOffsets(DeviceInfo &actual,const DeviceInfo &expected) {
    const bool legacyAccess=actual.access=="MemBus" || actual.access=="AGetBus" || actual.access=="AsAdBus";
    if(actual.name!=expected.name || !legacyAccess || actual.access!=expected.access ||
       actual.registerBytes!=expected.registerBytes || actual.baseAddress!=expected.baseAddress ||
       !actual.baseAddress || actual.baseAddress>UINT32_MAX || actual.registers.empty() ||
       actual.registers.size()!=expected.registers.size())return false;
    std::map<std::string,std::uint64_t> offsets;
    for(const auto &reg:expected.registers)if(!offsets.emplace(reg.name,reg.offset).second)return false;
    std::set<std::string> seen;
    const auto wrapBoundary=std::numeric_limits<std::uint64_t>::max()-actual.baseAddress;
    for(const auto &reg:actual.registers) {
        const auto found=offsets.find(reg.name);
        if(found==offsets.end() || !seen.insert(reg.name).second || found->second>=actual.baseAddress ||
           reg.offset<=wrapBoundary || reg.offset+actual.baseAddress!=found->second)return false;
    }
    // All names and offsets have matched before changing any local metadata.
    for(auto &reg:actual.registers)reg.offset+=actual.baseAddress;
    return true;
}
} }
#endif
