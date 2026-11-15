#ifndef GETDAQ_STRICTJSON_H
#define GETDAQ_STRICTJSON_H
#include <map>
#include <cstddef>
#include <string>
#include <vector>
namespace GetDAQ {
// 控制配置的小型JSON DOM：严格UTF-8/语法、重复键拒绝、有界；不用于原始数据。
struct JsonValue {
    enum Type { Null, Boolean, Number, String, Array, Object };
    Type type = Null;
    std::string text;
    std::vector<JsonValue> items;
    std::map<std::string,JsonValue> members;
    const JsonValue &At(const std::string &key) const;
};
JsonValue ParseStrictJson(const std::string &bytes);
// 默认控制配置预算保持不变；有效配置快照可显式采用更大的字符串/数组预算。
struct JsonLimits {
    std::size_t stringBytes=4096, arrayItems=4096;
};
JsonValue ParseStrictJson(const std::string &bytes,const JsonLimits &limits);
std::string ReadConfigurationFile(const std::string &path); // 最大64MiB。
void ValidateJsonUtf8(const std::string &text);
}
#endif
