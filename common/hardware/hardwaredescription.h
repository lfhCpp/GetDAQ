#ifndef GETDAQ_HARDWAREDESCRIPTION_H
#define GETDAQ_HARDWAREDESCRIPTION_H
#include <cstdint>
#include <string>
#include <vector>

// 标准C++11描述数据/校验；GUI与原生控制共用，不包含Qt、Ice或yaml-cpp。
namespace GetDAQ {
struct FieldInfo {
    std::string name;
    int offset=0,width=0;
    bool readOnly=false;
};
struct RegisterInfo {
    std::string name;
    std::uint64_t offset=0;
    bool readOnly=false; // 本地策略；旧GET板端不完整保留此字段。
    std::vector<FieldInfo> fields;
};
struct DeviceInfo {
    std::string name,access;
    std::uint64_t baseAddress=0;
    int registerBytes=0;
    std::vector<RegisterInfo> registers;
};
struct HardwareDescription {
    std::string nodeId;
    std::vector<DeviceInfo> devices;
};
HardwareDescription DecodeHardwareDescription(const std::string &bytes);
HardwareDescription LoadHardwareDescription(const std::string &path);
void ValidateHardwareDescription(const HardwareDescription &description);
std::string HardwarePolicyJson(const HardwareDescription &description);
}
#endif
