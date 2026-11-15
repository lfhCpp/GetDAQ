#ifndef GETDAQ_CONFIGURATIONRANGE_H
#define GETDAQ_CONFIGURATIONRANGE_H
#include <cstdint>
#include <string>
#include <vector>

namespace GetDAQ {
// 配置标量/范围核心无Qt、Boost或硬件依赖；整数比较不经过double。
struct ConfigurationValue {
    enum Type { Integer, Hexadecimal, Real, Boolean, String };
    Type type=String;
    std::int64_t integer=0;
    std::uint64_t hexadecimal=0;
    double real=0;
    bool boolean=false;
    std::string text;
};
ConfigurationValue ParseConfigurationValue(const std::string &text,const std::string &type="");
std::string ConfigurationValueType(const ConfigurationValue &value);
std::string NormalizeConfigurationValue(const ConfigurationValue &value);
struct ConfigurationRange {
    enum Kind { Enumeration, Interval };
    Kind kind=Enumeration;
    std::vector<ConfigurationValue> values;
    ConfigurationValue lower,upper;
    bool lowerUnbounded=false,upperUnbounded=false,includeLower=true,includeUpper=true;
    bool Contains(const ConfigurationValue &value) const;
    bool Equivalent(const ConfigurationRange &other) const;
};
ConfigurationRange ParseConfigurationRange(const std::string &text);
// 长度字段编码与快照schema1一致，保留类型、集合和端点语义。
std::string EncodeConfigurationRange(const std::string &text);
}
#endif
