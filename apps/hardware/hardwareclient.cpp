#include "hardwareclient_p.h"
#include "hardwareclient_description_p.h"
#include "hardwareaudit.h"
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>

namespace GetDAQ {
std::string JsonString(const std::string &value)
{
    const char *hex = "0123456789abcdef";
    std::string out = "\"";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
        else out += char(c);
    }
    return out + '"';
}
std::uint64_t ParseUnsigned(const std::string &value)
{
    unsigned base = 10; std::size_t begin = 0;
    if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) { base = 16; begin = 2; }
    if (begin == value.size()) throw std::invalid_argument("Empty unsigned value");
    std::uint64_t result = 0;
    for (std::size_t i = begin; i < value.size(); ++i) {
        const unsigned char c = value[i];
        const unsigned digit = c >= '0' && c <= '9' ? c - '0' :
            c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 255;
        if (digit >= base || result > (std::numeric_limits<std::uint64_t>::max() - digit) / base)
            throw std::invalid_argument("Invalid or overflowing unsigned value: " + value);
        result = result * base + digit;
    }
    return result;
}
using namespace HardwareClientInternal;
HardwareClient::HardwareClient(const std::string &proxy, int minor, int timeoutMs) : fImpl(new Impl)
{
    if (proxy.empty() || proxy.size() > 4096 || proxy.find('\0') != std::string::npos ||
        minor < 0 || minor > 1 || timeoutMs < 100 || timeoutMs > 60000)
        throw std::invalid_argument("Invalid hardware connection parameters");
    fImpl->minor = minor; fImpl->timeout = timeoutMs; fImpl->address = proxy;
    Ice::InitializationData init; init.properties = Ice::createProperties();
    init.properties->setProperty("Ice.RetryIntervals", "-1");
    init.properties->setProperty("Ice.Override.ConnectTimeout", std::to_string(timeoutMs));
    init.properties->setProperty("Ice.Override.Timeout", std::to_string(timeoutMs));
    init.properties->setProperty("Ice.MessageSizeMax", "8192");
    fImpl->communicator = Ice::initialize(init);
    Ice::EncodingVersion encoding; encoding.major = 1; encoding.minor = minor;
    auto base = fImpl->communicator->stringToProxy(proxy);
    if (!base) throw std::invalid_argument("Empty hardware proxy");
    fImpl->node = mdaq::hw::NodePrx::checkedCast(base->ice_twoway()->ice_collocationOptimized(false)->ice_timeout(timeoutMs)->ice_encodingVersion(encoding));
    if (!fImpl->node) throw std::runtime_error("Object does not implement mdaq::hw::Node");
}
HardwareClient::~HardwareClient() {}
std::string HardwareClient::Name() {
    std::lock_guard<std::mutex> lock(fImpl->mutex); return fImpl->node->name();
}
std::vector<std::string> HardwareClient::Devices() {
    std::lock_guard<std::mutex> lock(fImpl->mutex);
    mdaq::hw::DeviceList names; fImpl->node->getListOfDevices(names);
    if (names.size() > 4096) throw std::runtime_error("Too many hardware devices");
    std::set<std::string> unique;
    for (const auto &name : names) { CheckName(name); if (!unique.insert(name).second) throw std::runtime_error("Duplicate device name"); }
    return names;
}
DeviceInfo HardwareClient::Describe(const std::string &device) {
    std::lock_guard<std::mutex> lock(fImpl->mutex);
    return fImpl->Description(fImpl->Device(device), device);
}
void HardwareClient::UseDescription(const HardwareDescription &description) {
    ValidateHardwareDescription(description);
    std::lock_guard<std::mutex> lock(fImpl->mutex);
    std::map<std::string, DeviceInfo> next;
    for (const auto &device : description.devices) next.emplace(device.name, device);
    fImpl->expected.swap(next);
    fImpl->boundNodeId = description.nodeId;
}
void HardwareClient::Create(const HardwareDescription &description, const std::string &auditPath) {
    ValidateHardwareDescription(description);
    std::lock_guard<std::mutex> lock(fImpl->mutex);
    mdaq::hw::DeviceList present; fImpl->node->getListOfDevices(present);
    if (!present.empty()) throw std::runtime_error("Hardware node is not empty; refusing implicit destroy/replace");
    if (fImpl->descriptionOutcomeUnknown) throw std::runtime_error("Hardware description outcome UNKNOWN; explicit recovery required before CREATE");
    const auto payload = BuildHardwareDescriptionPayload(description);
    Audit audit(auditPath);
    audit.Append("{\"schema_version\":1,\"operation\":\"create-description\",\"phase\":\"intent\",\"node\":" +
        JsonString(fImpl->address) + ",\"node_id\":" + JsonString(description.nodeId) + ",\"devices\":" + payload.devicesJson + "}");
    fImpl->descriptionOutcomeUnknown = true;
    try { fImpl->node->create(payload.nodeConfig); }
    catch (const std::exception &e) {
        try { audit.Append("{\"phase\":\"unknown\",\"error\":" + JsonString(Message(e)) + "}"); } catch (...) {}
        throw std::runtime_error("Hardware CREATE outcome UNKNOWN; may be partially applied; do not retry automatically: " + Message(e));
    }
    try {
        audit.Append("{\"phase\":\"acknowledged\"}");
        fImpl->expected.clear();
        fImpl->boundNodeId = description.nodeId;
        for (const auto &device : description.devices) fImpl->expected.emplace(device.name, device);
        present.clear(); fImpl->node->getListOfDevices(present);
        std::set<std::string> actual(present.begin(), present.end());
        if (actual.size() != present.size() || actual.size() != description.devices.size() || fImpl->node->name() != description.nodeId)
            throw std::runtime_error("Created node inventory/name differs from description");
        for (const auto &device : description.devices) {
            if (!actual.count(device.name)) throw std::runtime_error("Created device missing");
            fImpl->Description(fImpl->Device(device.name), device.name); // 核对元数据，不读取硬件寄存器值。
        }
        audit.Append("{\"phase\":\"complete\",\"metadata_verified\":true,\"register_values_read\":false,\"board_permissions_verified\":false}");
        fImpl->descriptionOutcomeUnknown = false;
    } catch (const std::exception &e) {
        try { audit.Append("{\"phase\":\"verification_failed\",\"error\":" + JsonString(Message(e)) + "}"); } catch (...) {}
        throw std::runtime_error("Hardware CREATE acknowledged but metadata/audit verification failed; do not repeat CREATE: " + Message(e));
    }
}
void HardwareClient::DestroyDescription(const std::string &auditPath) {
    std::lock_guard<std::mutex> lock(fImpl->mutex);
    if (fImpl->destroyAttempted || fImpl->descriptionOutcomeUnknown)
        throw std::runtime_error("Hardware description destroy already attempted or outcome UNKNOWN; explicit recovery required");
    if (fImpl->expected.empty() || fImpl->boundNodeId.empty())
        throw std::logic_error("DestroyDescription requires bound current description");
    mdaq::hw::DeviceList present; fImpl->node->getListOfDevices(present);
    const std::set<std::string> names(present.begin(),present.end());
    if (names.size() != present.size() || names.size() != fImpl->expected.size() || fImpl->node->name() != fImpl->boundNodeId)
        throw std::runtime_error("Current node inventory/name differs before destroy");
    for (const auto &entry : fImpl->expected) {
        if (!names.count(entry.first)) throw std::runtime_error("Current device missing before destroy");
        fImpl->Description(fImpl->Device(entry.first),entry.first);
    }
    Audit audit(auditPath);
    audit.Append("{\"schema_version\":1,\"operation\":\"destroy-description\",\"phase\":\"intent\",\"node\":"+
        JsonString(fImpl->address)+",\"node_id\":"+JsonString(fImpl->boundNodeId)+
        ",\"devices\":"+std::to_string(names.size())+",\"hardware_stop_sent\":false,\"data_sender_disconnected\":false}");
    fImpl->destroyAttempted = fImpl->descriptionOutcomeUnknown = true;
    try {
        fImpl->node->destroy(); // Ice.RetryIntervals=-1，包含idempotent调用也禁止自动重发。
        audit.Append("{\"phase\":\"acknowledged\"}");
        present.clear(); fImpl->node->getListOfDevices(present);
        if (!present.empty()) throw std::runtime_error("Destroyed node still has devices");
        audit.Append("{\"phase\":\"complete\",\"empty_inventory_verified\":true,\"hardware_stop_verified\":false}");
        fImpl->expected.clear(); fImpl->boundNodeId.clear(); fImpl->descriptionOutcomeUnknown = false;
    } catch (const std::exception &e) {
        try { audit.Append("{\"phase\":\"unknown\",\"automatic_retry\":false,\"error\":"+JsonString(Message(e))+"}"); } catch (...) {}
        throw std::runtime_error("Hardware DESTROY outcome UNKNOWN; do not retry or CREATE automatically: "+Message(e));
    }
}
std::uint64_t HardwareClient::Read(const std::string &device, const std::string &reg, const std::string &field) {
    std::lock_guard<std::mutex> lock(fImpl->mutex);
    const auto target = fImpl->Device(device);
    const int width = fImpl->Width(fImpl->Description(target, device), reg, field, false);
    const Ice::Long value = field.empty() ? target->readRegister(reg) : target->readField(reg, field);
    return Bits(value) & Mask(width);
}
void HardwareClient::Write(const std::string &device, const std::string &reg, const std::string &field,
                           std::uint64_t value, const std::string &auditPath) {
    HardwareStep step; step.device = device; step.reg = reg; step.field = field; step.value = value;
    Execute({step}, auditPath);
}
void HardwareClient::Execute(const std::vector<HardwareStep> &steps, const std::string &auditPath) {
    Process(steps,auditPath,true,std::function<void()>());
}
void HardwareClient::Execute(const std::vector<HardwareStep> &steps, const std::string &auditPath,
                             const std::function<void()> &checkpoint) {
    Process(steps,auditPath,true,checkpoint);
}
void HardwareClient::Validate(const std::vector<HardwareStep> &steps) {
    Process(steps,std::string(),false,std::function<void()>());
}
std::string HardwareClient::CompletedAuditChainSha256() const {
    std::lock_guard<std::mutex> lock(fImpl->mutex);return fImpl->completedAuditChain;
}

}
