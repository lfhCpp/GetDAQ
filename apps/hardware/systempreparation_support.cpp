#include "systempreparation_p.h"
#include "hardwareclient.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace GetDAQ {
namespace SystemPreparationInternal {
void Text(const std::string &s,std::size_t max) {
    if (s.empty() || s.size() > max) throw std::invalid_argument("Invalid bounded system preparation text");
    for (unsigned char c : s) if (c < 32 || c == 127) throw std::invalid_argument("Control byte in system preparation text");
}

HardwareStep Field(HardwareStep::Kind kind,const char *device,const char *reg,const char *field,std::uint64_t value) {
    HardwareStep s; s.kind = kind; s.device = device; s.reg = reg; s.field = field; s.value = value;
    s.requireBoundDescription = true; return s;
}

std::vector<HardwareStep> CoBoBit(const char *name,bool enabled) {
    return {Field(HardwareStep::WriteValue,"ctrl","mutantConfig",name,enabled),
            Field(HardwareStep::ExpectValue,"ctrl","mutantConfig",name,enabled)};
}

std::vector<HardwareStep> CoBoStopped() {
    return {Field(HardwareStep::ExpectValue,"ctrl","pipeCtrl","firmwareEnable",0),
            Field(HardwareStep::ExpectValue,"ctrl","pipeCtrl","intEnable",0),
            Field(HardwareStep::ExpectValue,"ctrl","mutantConfig","initDone",0)};
}

void Inventory(HardwareClient &hw,const HardwareDescription &policy,const std::string &name) {
    if (hw.Name()!=name) throw std::runtime_error("System release actual node name changed");
    const auto devices=hw.Devices(); const std::set<std::string> names(devices.begin(),devices.end());
    if (names.size()!=policy.devices.size()) throw std::runtime_error("System release device inventory differs");
    for (const auto &d:policy.devices) {
        if (!names.count(d.name)) throw std::runtime_error("System release device missing");
        hw.Describe(d.name);
    }
}
}
}
