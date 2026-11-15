#include "../apps/hardware/legacyregisteroffsets_p.h"
#include <stdexcept>
#include <iostream>
using namespace GetDAQ;
void Check(bool condition){if(!condition)throw std::runtime_error("legacy offset regression failed");}
int main(){
    DeviceInfo expected;expected.name="ctrl";expected.access="MemBus";expected.baseAddress=0x20000000;expected.registerBytes=4;
    RegisterInfo version;version.name="hardwareVersion";version.offset=0x254;version.readOnly=true;expected.registers.push_back(version);
    RegisterInfo pipe;pipe.name="pipeCtrl";pipe.offset=4;FieldInfo field;field.name="firmwareEnable";field.width=1;field.readOnly=true;pipe.fields.push_back(field);expected.registers.push_back(pipe);
    DeviceInfo wire=expected;for(auto &reg:wire.registers)reg.offset-=wire.baseAddress;
    Check(HardwareClientInternal::NormalizeLegacyRegisterOffsets(wire,expected));Check(wire.registers[0].offset==0x254 && wire.registers[1].offset==4);Check(wire.registers[0].readOnly && wire.registers[1].fields[0].readOnly);
    Check(!HardwareClientInternal::NormalizeLegacyRegisterOffsets(wire,expected)); // Canonical values stay canonical.
    wire=expected;for(auto &reg:wire.registers)reg.offset-=wire.baseAddress;wire.registers[1].offset+=1;const auto before=wire.registers[0].offset;Check(!HardwareClientInternal::NormalizeLegacyRegisterOffsets(wire,expected));Check(wire.registers[0].offset==before);
    wire=expected;wire.registers[0].offset-=wire.baseAddress;Check(!HardwareClientInternal::NormalizeLegacyRegisterOffsets(wire,expected)); // Mixed formats rejected.
    wire=expected;for(auto &reg:wire.registers)reg.offset-=wire.baseAddress;wire.baseAddress+=4;Check(!HardwareClientInternal::NormalizeLegacyRegisterOffsets(wire,expected));
    wire=expected;for(auto &reg:wire.registers)reg.offset-=wire.baseAddress;wire.registers[1].name="other";Check(!HardwareClientInternal::NormalizeLegacyRegisterOffsets(wire,expected));
    for(const auto &access:{"AGetBus","AsAdBus"}){auto policy=expected;policy.access=access;policy.registerBytes=8;wire=policy;for(auto &reg:wire.registers)reg.offset-=wire.baseAddress;Check(HardwareClientInternal::NormalizeLegacyRegisterOffsets(wire,policy));Check(wire.registers[0].offset==policy.registers[0].offset);}
    std::cout<<"PASS: bound legacy underflow, canonical, mixed, wrong offset/base/name; permissions preserved\n";
}
