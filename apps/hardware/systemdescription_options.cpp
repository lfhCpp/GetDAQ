// Host description flow adapted from the received CEA GET/Mordicus sources.
// Original CeCILL license: third_party/licenses/getbench/COPYING.
#include "systemdescription_p.h"
#include "strictjson.h"
#include <Ice/Ice.h>
#include <algorithm>
#include <set>
#include <stdexcept>

namespace GetDAQ {
namespace {
void Text(const std::string &value, std::size_t limit) {
    if (value.empty() || value.size() > limit)
        throw std::invalid_argument("Invalid bounded system description text");
    ValidateJsonUtf8(value);
    for (unsigned char c : value)
        if (c < 32 || c == 127)
            throw std::invalid_argument("Control byte in system description text");
}

void FieldPolicy(const HardwareDescription &description, const char *device,
                 const char *reg, const char *field, bool write) {
    for (const auto &candidate : description.devices) {
        if (candidate.name != device) continue;
        for (const auto &registerDescription : candidate.registers) {
            if (registerDescription.name != reg) continue;
            for (const auto &candidateField : registerDescription.fields) {
                if (candidateField.name != field) continue;
                if (candidateField.width != 1 ||
                    (write && (registerDescription.readOnly || candidateField.readOnly)))
                    throw std::invalid_argument("Description status/stop field width or permission differs");
                return;
            }
        }
    }
    throw std::invalid_argument("Description missing required status/stop field: " +
                                std::string(device) + '.' + reg + '.' + field);
}

void RequiredFields(const HardwareDescription &description,
                    SystemDescriptionNode::Role role, bool statusWrites) {
    if (role == SystemDescriptionNode::Role::CoBo) {
        FieldPolicy(description, "ctrl", "pipeCtrl", "firmwareEnable", false);
        FieldPolicy(description, "ctrl", "pipeCtrl", "intEnable", false);
        if (statusWrites) {
            FieldPolicy(description, "ctrl", "mutantConfig", "initDone", true);
            FieldPolicy(description, "ctrl", "mutantConfig", "mutantMode", true);
        }
    } else {
        FieldPolicy(description, "CARD_B_EMC", "TRIGGER_CTRL", "run", false);
    }
}

struct Parser {
    Ice::CommunicatorPtr communicator;
    Parser() {
        Ice::InitializationData init;
        init.properties = Ice::createProperties();
        init.properties->setProperty("Ice.RetryIntervals", "-1");
        communicator = Ice::initialize(init);
    }
    ~Parser() {
        if (communicator) try { communicator->destroy(); } catch (...) {}
    }
};

std::vector<std::string> TargetKeys(const Ice::ObjectPrx &proxy) {
    if (!proxy || proxy->ice_getRouter() || proxy->ice_getLocator() ||
        !proxy->ice_getAdapterId().empty())
        throw std::invalid_argument("Description requires direct TCP endpoints without router/locator");
    const auto id = proxy->ice_getIdentity();
    Text(id.name, 512);
    if (!id.category.empty()) Text(id.category, 512);
    if (!proxy->ice_getFacet().empty()) Text(proxy->ice_getFacet(), 512);
    const auto prefix = JsonString(id.category) + JsonString(id.name) + JsonString(proxy->ice_getFacet());
    std::set<std::string> unique;
    for (const auto &endpoint : proxy->ice_getEndpoints()) {
        const auto info = Ice::TCPEndpointInfoPtr::dynamicCast(endpoint->getInfo());
        if (!info || info->host.empty() || info->port < 1 || info->port > 65535)
            throw std::invalid_argument("Description requires explicit host/port TCP endpoints");
        auto host = info->host;
        Text(host, 512);
        std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) {
            return char(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        });
        unique.insert(prefix + JsonString(host) + ':' + std::to_string(info->port));
    }
    if (unique.empty() || unique.size() > 16)
        throw std::invalid_argument("Invalid bounded description endpoint set");
    return std::vector<std::string>(unique.begin(), unique.end());
}
}

namespace SystemDescriptionInternal {
const char *PolicyName(DescriptionPolicy policy) {
    switch (policy) {
    case DescriptionPolicy::UseExisting: return "use-existing";
    case DescriptionPolicy::CreateEmpty: return "create-empty";
    case DescriptionPolicy::ReplaceExisting: return "replace-existing";
    }
    throw std::invalid_argument("Invalid description policy");
}

const char *RoleName(SystemDescriptionNode::Role role) {
    if (role == SystemDescriptionNode::Role::CoBo) return "CoBo";
    if (role == SystemDescriptionNode::Role::Mutant) return "Mutant";
    throw std::invalid_argument("Invalid description role");
}

HardwareStep Bit(const char *field, HardwareStep::Kind kind) {
    HardwareStep step;
    step.kind = kind;
    step.device = "ctrl";
    step.reg = "mutantConfig";
    step.field = field;
    step.value = 0;
    step.requireBoundDescription = true;
    return step;
}

std::vector<HardwareStep> ClearBit(const char *field) {
    return {Bit(field, HardwareStep::WriteValue), Bit(field, HardwareStep::ExpectValue)};
}

void Inventory(HardwareClient &hardware, const HardwareDescription &description) {
    if (hardware.Name() != description.nodeId)
        throw std::runtime_error("Actual description node name differs");
    hardware.UseDescription(description);
    const auto actual = hardware.Devices();
    std::set<std::string> names(actual.begin(), actual.end());
    if (names.size() != description.devices.size())
        throw std::runtime_error("Actual description device inventory differs");
    for (const auto &device : description.devices) {
        if (!names.count(device.name))
            throw std::runtime_error("Actual description device missing");
        hardware.Describe(device.name); // 核对全部元数据，不能只检查本次要写的ctrl。
    }
}

void Stopped(HardwareClient &hardware, const SystemDescriptionNode &node) {
    if (node.role == SystemDescriptionNode::Role::CoBo) {
        if (hardware.Read("ctrl", "pipeCtrl", "firmwareEnable") ||
            hardware.Read("ctrl", "pipeCtrl", "intEnable"))
            throw std::runtime_error("Description requires observed CoBo production stopped");
    } else if (hardware.Read("CARD_B_EMC", "TRIGGER_CTRL", "run")) {
        throw std::runtime_error("Description requires observed MuTaNT production stopped");
    }
}
}

void ValidateSystemDescriptionOptions(const SystemDescriptionOptions &options) {
    if (options.nodes.empty() || options.nodes.size() > 195 || options.encodingMinor < 0 ||
        options.encodingMinor > 1 || options.timeoutMs < 100 || options.timeoutMs > 60000)
        throw std::invalid_argument("Invalid system description node count/encoding/timeout");
    if (!options.exclusiveOwnerConfirmed || !options.dataProcessesStopped)
        throw std::invalid_argument("Description requires exclusive owner and stopped data process declarations");
    Parser parser;
    std::set<std::string> ids, names, targets;
    std::size_t policies = 0;
    for (const auto &node : options.nodes) {
        SystemDescriptionInternal::RoleName(node.role);
        SystemDescriptionInternal::PolicyName(node.policy);
        Text(node.id, 128);
        Text(node.nodeProxy, 4096);
        Text(node.description.nodeId, 128);
        ValidateHardwareDescription(node.description);
        RequiredFields(node.description, node.role, true);
        if (!ids.insert(std::string(SystemDescriptionInternal::RoleName(node.role)) + ':' + node.id).second ||
            !names.insert(node.description.nodeId).second)
            throw std::invalid_argument("Duplicate description node ID/name");
        for (const auto &key : TargetKeys(parser.communicator->stringToProxy(node.nodeProxy)))
            if (!targets.insert(key).second)
                throw std::invalid_argument("Duplicate canonical description node/overlapping endpoints");
        const auto desired = HardwarePolicyJson(node.description);
        ValidateJsonUtf8(desired);
        policies += desired.size();
        if (node.policy == DescriptionPolicy::ReplaceExisting) {
            Text(node.currentDescription.nodeId, 128);
            ValidateHardwareDescription(node.currentDescription);
            RequiredFields(node.currentDescription, node.role, false);
            const auto current = HardwarePolicyJson(node.currentDescription);
            ValidateJsonUtf8(current);
            policies += current.size();
        } else if (!node.currentDescription.nodeId.empty() || !node.currentDescription.devices.empty()) {
            throw std::invalid_argument("Current description is only valid for explicit replacement");
        }
        if (policies > 32 * 1024 * 1024)
            throw std::invalid_argument("System description policy budget exceeded");
    }
}

SystemDescriptionOptions BuildSystemDescriptionOptions(const SystemAlignmentOptions &topology,
                                                       DescriptionPolicy policy) {
    SystemDescriptionOptions options;
    options.encodingMinor = topology.encodingMinor;
    options.timeoutMs = topology.timeoutMs;
    options.exclusiveOwnerConfirmed = topology.exclusiveOwnerConfirmed;
    options.dataProcessesStopped = topology.daqStoppedConfirmed;
    const auto add = [&](SystemDescriptionNode::Role role, const std::string &id,
                         const std::string &proxy, const std::string &name,
                         const HardwareDescription &description) {
        SystemDescriptionNode node;
        node.role = role;
        node.id = id;
        node.nodeProxy = proxy;
        node.description = description;
        node.description.nodeId = name;
        node.policy = policy;
        if (policy == DescriptionPolicy::ReplaceExisting) node.currentDescription = node.description;
        options.nodes.push_back(std::move(node));
    };
    for (const auto &cobo : topology.cobos)
        add(SystemDescriptionNode::Role::CoBo, cobo.id, cobo.nodeProxy,
            cobo.expectedNodeName, cobo.description);
    for (const auto &mutant : topology.mutants)
        add(SystemDescriptionNode::Role::Mutant, mutant.id, mutant.nodeProxy,
            mutant.expectedNodeName, mutant.description);
    ValidateSystemDescriptionOptions(options);
    return options;
}
}
