#include "systemalignment.h"
#include "strictjson.h"
#include <fstream>
#include <set>
#include <stdexcept>
namespace GetDAQ {
namespace {
void Keys(const JsonValue &node,std::initializer_list<const char *> allowed)
{
    if (node.type!=JsonValue::Object) throw std::invalid_argument("Expected topology JSON object");
    std::set<std::string> permitted;for (const auto key:allowed) permitted.insert(key);
    if (node.members.size()!=permitted.size()) throw std::invalid_argument("Missing/unknown topology key");
    for (const auto &entry:node.members) if (!permitted.count(entry.first)) throw std::invalid_argument("Unknown topology key: "+entry.first);
}
std::string Text(const JsonValue &node)
{
    if (node.type!=JsonValue::String || node.text.empty() || node.text.size()>4096 || node.text.find('\0')!=std::string::npos)
        throw std::invalid_argument("Expected bounded nonempty topology JSON string");
    return node.text;
}
unsigned int UInt(const JsonValue &node,unsigned int maximum)
{
    if (node.type!=JsonValue::Number || node.text.empty() || node.text.find_first_not_of("0123456789")!=std::string::npos)
        throw std::invalid_argument("Expected topology unsigned JSON integer");
    const auto value=ParseUnsigned(node.text);if (value>maximum) throw std::invalid_argument("Topology number outside bounds");return unsigned(value);
}
void Array(const JsonValue &node,std::size_t maximum)
{
    if (node.type!=JsonValue::Array || node.items.size()>maximum) throw std::invalid_argument("Topology array exceeds bounds or has wrong type");
}
bool Boolean(const JsonValue &node)
{
    if (node.type!=JsonValue::Boolean) throw std::invalid_argument("Expected topology JSON true/false");
    return node.text=="true";
}

}
SystemAlignmentOptions LoadSystemAlignmentOptions(const std::string &path,bool allowStandalone)
{
    if (path.empty() || path.size() > 4096 || path.find('\0') != std::string::npos)
        throw std::invalid_argument("Invalid topology filename");
    std::ifstream file(path,std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open alignment topology");
    std::string bytes(1024*1024+1,'\0'); file.read(&bytes[0],bytes.size()); bytes.resize(std::size_t(file.gcount()));
    if (file.bad() || bytes.size() > 1024*1024) throw std::runtime_error("Unreadable/excessive topology (maximum 1 MiB)");
    const auto slash = path.find_last_of('/');
    const std::string parent = slash == std::string::npos ? "." : slash == 0 ? "/" : path.substr(0,slash);
    return DecodeSystemAlignmentOptions(bytes,[&](const std::string &name) { return LoadHardwareDescription(name.front()=='/' ? name : parent+'/'+name); },allowStandalone);
}
SystemAlignmentOptions DecodeSystemAlignmentOptions(const std::string &bytes,
    const std::function<HardwareDescription(const std::string &)> &descriptionReader,bool allowStandalone)
{
    if (bytes.size()>1024*1024 || !descriptionReader) throw std::invalid_argument("Invalid bounded topology/description reader");
    const auto root=ParseStrictJson(bytes);
    Keys(root,{"schema_version","kind","encoding","timeout_ms","mutants","cobos"});
    if (UInt(root.At("schema_version"),1) != 1 || Text(root.At("kind")) != "getdaq.alignment-topology")
        throw std::invalid_argument("Unsupported alignment topology format");
    SystemAlignmentOptions options;
    const auto encoding = Text(root.At("encoding"));
    if (encoding != "1.0" && encoding != "1.1") throw std::invalid_argument("Topology encoding must be 1.0 or 1.1");
    options.encodingMinor = encoding == "1.1" ? 1 : 0; options.timeoutMs = int(UInt(root.At("timeout_ms"),60000));
    const auto description = [&](const JsonValue &source) {
        return descriptionReader(Text(source));
    };
    Array(root.At("mutants"),16); Array(root.At("cobos"),192);
    for (const auto &source : root.At("mutants").items) {
        Keys(source,{"id","expected_node_name","node_proxy","alignment_proxy","role","hardware_description_file"});
        AlignmentMutantEndpoint m; m.id = Text(source.At("id")); m.expectedNodeName = Text(source.At("expected_node_name"));
        m.nodeProxy = Text(source.At("node_proxy")); m.alignmentProxy = Text(source.At("alignment_proxy"));
        const auto role = Text(source.At("role"));
        if (role != "master" && role != "slave") throw std::invalid_argument("Explicit MuTaNT role must be master or slave");
        m.master = role == "master"; m.description = description(source.At("hardware_description_file"));
        options.mutants.push_back(std::move(m));
    }
    for (const auto &source : root.At("cobos").items) {
        Keys(source,{"id","expected_node_name","node_proxy","link_proxy","enabled","hardware_description_file"});
        AlignmentCoBoEndpoint c; c.id = Text(source.At("id")); c.expectedNodeName = Text(source.At("expected_node_name"));
        c.nodeProxy = Text(source.At("node_proxy")); c.linkProxy = Text(source.At("link_proxy")); c.enabled = Boolean(source.At("enabled"));
        c.description = description(source.At("hardware_description_file")); options.cobos.push_back(std::move(c));
    }
    if (!allowStandalone || !options.mutants.empty()) ValidateSystemAlignmentOptions(options);
    // Standalone system preparation has no MuTaNT alignment stage; its binder validates all CoBo endpoints/descriptions.
    return options;
}
}
