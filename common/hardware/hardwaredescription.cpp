#include "hardwaredescription.h"
#include "strictjson.h"
#include <fstream>
#include <set>
#include <stdexcept>
#include <limits>
namespace GetDAQ {
namespace {
void Name(const std::string &value) {
    if (value.empty() || value.size()>512 || value=="*" || value.find('\0')!=std::string::npos)
        throw std::invalid_argument("Invalid concrete hardware name/access");
    ValidateJsonUtf8(value);
}
std::string PolicyText(const std::string &value) {
    const char *hex="0123456789abcdef";std::string out="\"";
    for (unsigned char c:value) {
        if (c=='"' || c=='\\') {out+='\\';out+=char(c);}
        else if (c<32) {out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
        else out+=char(c);
    }
    return out+'"';
}
void Keys(const JsonValue &node,std::initializer_list<const char *> allowed) {
    if (node.type!=JsonValue::Object) throw std::invalid_argument("Expected description object");
    std::set<std::string> names;for (const auto key:allowed) names.insert(key);
    if (node.members.size()!=names.size()) throw std::invalid_argument("Missing/unknown description key");
    for (const auto &entry:node.members) if (!names.count(entry.first)) throw std::invalid_argument("Unknown description key: "+entry.first);
}
const std::string &Text(const JsonValue &node) {
    if (node.type!=JsonValue::String) throw std::invalid_argument("Expected description JSON string");
    return node.text;
}
std::uint64_t Unsigned(const std::string &text) {
    unsigned base=10;std::size_t begin=0;
    if (text.size()>2 && text[0]=='0' && (text[1]=='x' || text[1]=='X')) {base=16;begin=2;}
    if (text.empty() || begin==text.size()) throw std::invalid_argument("Empty description integer");
    std::uint64_t result=0;
    for (std::size_t i=begin;i<text.size();++i) {
        const unsigned char c=text[i];const unsigned digit=c>='0' && c<='9' ? c-'0' : c>='a' && c<='f' ? c-'a'+10 : c>='A' && c<='F' ? c-'A'+10 : 255;
        if (digit>=base || result>(std::numeric_limits<std::uint64_t>::max()-digit)/base) throw std::invalid_argument("Invalid/overflowing description integer");
        result=result*base+digit;
    }
    return result;
}
int Small(const JsonValue &node,unsigned maximum) {
    if (node.type!=JsonValue::Number || node.text.find_first_not_of("0123456789")!=std::string::npos) throw std::invalid_argument("Expected description unsigned JSON integer");
    const auto value=Unsigned(node.text);if (value>maximum) throw std::invalid_argument("Description integer outside bounds");return int(value);
}
bool Boolean(const JsonValue &node) {
    if (node.type!=JsonValue::Boolean) throw std::invalid_argument("Expected description JSON boolean");
    return node.text=="true";
}
const std::vector<JsonValue> &Sequence(const JsonValue &node,std::size_t maximum) {
    if (node.type!=JsonValue::Array || node.items.size()>maximum) throw std::invalid_argument("Invalid/excessive description array");
    return node.items;
}
}
std::string HardwarePolicyJson(const HardwareDescription &description)
{
    // 保存本次真正绑定的权限/布局对象；不从后来可能变化的文件重载。
    std::string json = "{\"node_id\":"+PolicyText(description.nodeId)+",\"devices\":[";
    bool firstDevice = true;
    for (const auto &device : description.devices) {
        if (!firstDevice) json += ',';
        firstDevice = false;
        json += "{\"name\":"+PolicyText(device.name)+",\"access\":"+PolicyText(device.access)+
            ",\"base_address\":"+PolicyText(std::to_string(device.baseAddress))+",\"register_bytes\":"+std::to_string(device.registerBytes)+",\"registers\":[";
        bool firstRegister = true;
        for (const auto &reg : device.registers) {
            if (!firstRegister) json += ',';
            firstRegister = false;
            json += "{\"name\":"+PolicyText(reg.name)+",\"offset\":"+PolicyText(std::to_string(reg.offset))+",\"read_only\":"+(reg.readOnly ? "true" : "false")+",\"fields\":[";
            bool firstField = true;
            for (const auto &field : reg.fields) {
                if (!firstField) json += ',';
                firstField = false;
                json += "{\"name\":"+PolicyText(field.name)+",\"offset_bits\":"+std::to_string(field.offset)+
                    ",\"width_bits\":"+std::to_string(field.width)+",\"read_only\":"+(field.readOnly ? "true" : "false")+"}";
            }
            json += "]}";
        }
        json += "]}";
    }
    return json+"]}";
}

void ValidateHardwareDescription(const HardwareDescription &description)
{
    Name(description.nodeId);
    if (description.devices.empty() || description.devices.size() > 4096) throw std::invalid_argument("Description needs 1..4096 devices");
    std::size_t registerCount = 0, fieldCount = 0, textBytes = description.nodeId.size();
    std::set<std::string> devices;
    for (const auto &device : description.devices) {
        Name(device.name); Name(device.access); textBytes += device.name.size() + device.access.size();
        if (!devices.insert(device.name).second) throw std::invalid_argument("Duplicate description device");
        if (device.registerBytes < 1 || device.registerBytes > 8) throw std::invalid_argument("Invalid register byte width");
        std::set<std::string> registers;
        for (const auto &reg : device.registers) {
            Name(reg.name); textBytes += reg.name.size();
            if (!registers.insert(reg.name).second || ++registerCount > 65536) throw std::invalid_argument("Duplicate/excessive register");
            // 地址按无符号位模式传输，拒绝设备基址加偏移发生回绕。
            if (reg.offset > std::numeric_limits<std::uint64_t>::max() - device.baseAddress)
                throw std::invalid_argument("Register address overflow");
            std::set<std::string> fields;
            if (reg.fields.size() > 256) throw std::invalid_argument("Too many register fields");
            for (const auto &field : reg.fields) {
                Name(field.name); textBytes += field.name.size();
                if (!fields.insert(field.name).second || ++fieldCount > 100000)
                    throw std::invalid_argument("Duplicate/excessive bit field");
                if (field.offset < 0 || field.width < 1 || field.width > device.registerBytes * 8 ||
                    field.offset > device.registerBytes * 8 - field.width)
                    throw std::invalid_argument("Description bit field exceeds register width");
            }
        }
    }
    if (textBytes > 2 * 1024 * 1024) throw std::invalid_argument("Description name bytes exceed limit");
}
HardwareDescription LoadHardwareDescription(const std::string &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open compiled hardware description");
    std::string bytes(8 * 1024 * 1024 + 1, '\0');
    input.read(&bytes[0], bytes.size()); bytes.resize(static_cast<std::size_t>(input.gcount()));
    if (input.bad() || bytes.size() > 8 * 1024 * 1024) throw std::runtime_error("Unreadable/excessive compiled description");
    return DecodeHardwareDescription(bytes);
}
HardwareDescription DecodeHardwareDescription(const std::string &bytes)
{
    if (bytes.size()>8*1024*1024) throw std::invalid_argument("Compiled hardware description exceeds 8 MiB");
    JsonLimits limits;limits.arrayItems=65536;
    const auto root=ParseStrictJson(bytes,limits);
    Keys(root,{"schema_version","kind","node_id","devices","sources","warnings"});
    if (Small(root.At("schema_version"),1)!=1 || Text(root.At("kind"))!="getdaq.hardware-description") throw std::invalid_argument("Unsupported hardware description format");
    for (const auto &entry:Sequence(root.At("sources"),256)) {
        Keys(entry,{"path","sha256"});const auto &path=Text(entry.At("path")),&digest=Text(entry.At("sha256"));
        if (path.empty() || path.size()>4096 || path.find('\0')!=std::string::npos || digest.size()!=64 || digest.find_first_not_of("0123456789abcdef")!=std::string::npos)
            throw std::invalid_argument("Invalid description source provenance");
    }
    for (const auto &warning:Sequence(root.At("warnings"),4096)) if (Text(warning).size()>4096) throw std::invalid_argument("Excessive description warning");
    HardwareDescription result;result.nodeId=Text(root.At("node_id"));
    for (const auto &entry:Sequence(root.At("devices"),4096)) {
        Keys(entry,{"name","access","base_address","register_bytes","registers"});
        DeviceInfo device;device.name=Text(entry.At("name"));device.access=Text(entry.At("access"));
        device.baseAddress=Unsigned(Text(entry.At("base_address")));device.registerBytes=Small(entry.At("register_bytes"),8);
        for (const auto &input:Sequence(entry.At("registers"),65536)) {
            Keys(input,{"name","offset","read_only","fields"});RegisterInfo reg;
            reg.name=Text(input.At("name"));reg.offset=Unsigned(Text(input.At("offset")));reg.readOnly=Boolean(input.At("read_only"));
            for (const auto &bit:Sequence(input.At("fields"),256)) {
                Keys(bit,{"name","offset_bits","width_bits","read_only"});FieldInfo field;
                field.name=Text(bit.At("name"));field.offset=Small(bit.At("offset_bits"),63);field.width=Small(bit.At("width_bits"),64);field.readOnly=Boolean(bit.At("read_only"));
                reg.fields.push_back(field);
            }
            device.registers.push_back(reg);
        }
        result.devices.push_back(device);
    }
    ValidateHardwareDescription(result);return result;
}
}
