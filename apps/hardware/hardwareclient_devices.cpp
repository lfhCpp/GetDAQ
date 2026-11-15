#include "hardwareclient_p.h"
#include "legacyregisteroffsets_p.h"
#include "boundfieldpermissions_p.h"
#include <cstdlib>
#include <set>
#include <stdexcept>

namespace GetDAQ {
using namespace HardwareClientInternal;

HardwareClient::Impl::~Impl() {
    if (communicator) { try { communicator->destroy(); } catch (...) {} }
}

mdaq::hw::DevicePrx HardwareClient::Impl::Device(const std::string &name) {
    CheckName(name);
    if (!expected.empty() && !expected.count(name)) throw std::runtime_error("Device is absent from bound description");
    const auto found = node->findDevice(name);
    if (!found) throw std::runtime_error("Hardware node returned a null device");
    return Options(found); // 服务端返回的代理也必须固定编码/超时。
}

DeviceInfo HardwareClient::Impl::Description(const mdaq::hw::DevicePrx &device,const std::string &requested) {
    mdaq::hw::DeviceConfig wire; device->getDeviceConfig(wire);
    CheckName(wire.descr.name);
    if (wire.descr.registerWidth < 1 || wire.descr.registerWidth > 8 || wire.registers.size() > 65536)
        throw std::runtime_error("Invalid device register width/count");
    DeviceInfo info; info.name = wire.descr.name; info.access = wire.descr.registerAccess;
    info.baseAddress = Bits(wire.descr.baseAddress); info.registerBytes = wire.descr.registerWidth;
    std::set<std::string> names;
    for (const auto &source : wire.registers) {
        CheckName(source.descr.name);
        if (!names.insert(source.descr.name).second || source.fields.size() > 256)
            throw std::runtime_error("Duplicate register or excessive fields");
        RegisterInfo reg; reg.name = source.descr.name; reg.offset = Bits(source.descr.offset);
        std::set<std::string> fields;
        for (const auto &wireField : source.fields) {
            CheckName(wireField.name);
            if (!fields.insert(wireField.name).second || wireField.offset < 0 || wireField.width < 1 ||
                int(wireField.offset) + int(wireField.width) > info.registerBytes * 8)
                throw std::runtime_error("Invalid hardware field description");
            FieldInfo field; field.name = wireField.name; field.offset = wireField.offset;
            field.width = wireField.width; field.readOnly = wireField.readOnly; reg.fields.push_back(field);
        }
        info.registers.push_back(reg);
    }
    if (!expected.empty()) {
        if (info.name != requested) throw std::runtime_error("Remote device name differs from requested bound device");
        const auto wanted = expected.find(info.name);
        if (wanted == expected.end()) throw std::runtime_error("Device is absent from bound description");
        const auto &policy = wanted->second;
        if (info.access != policy.access || info.baseAddress != policy.baseAddress || info.registerBytes != policy.registerBytes ||
            info.registers.size() != policy.registers.size()) throw std::runtime_error("Remote device differs from bound description");
        NormalizeLegacyRegisterOffsets(info,policy);
        std::map<std::string, const RegisterInfo *> registerMap;
        for (const auto &candidate : policy.registers) registerMap.emplace(candidate.name, &candidate);
        for (auto &reg : info.registers) {
            const auto position = registerMap.find(reg.name);
            const RegisterInfo *match = position == registerMap.end() ? nullptr : position->second;
            if (!match || match->offset != reg.offset || match->fields.size() != reg.fields.size())
                throw std::runtime_error("Remote register differs from bound description");
            reg.readOnly = match->readOnly;
            std::map<std::string, const FieldInfo *> fieldMap;
            for (const auto &candidate : match->fields) fieldMap.emplace(candidate.name, &candidate);
            for (auto &field : reg.fields) {
                const auto position = fieldMap.find(field.name);
                const FieldInfo *defined = position == fieldMap.end() ? nullptr : position->second;
                if (!defined || defined->offset != field.offset || defined->width != field.width)
                    throw std::runtime_error("Remote bit field differs from bound description");
                const char *legacyProxy=std::getenv("GETDAQ_LEGACY_FIELD_POLICY_PROXY");
                const char *legacyNode=std::getenv("GETDAQ_LEGACY_FIELD_POLICY_NODE");
                if(legacyProxy && legacyNode && address==legacyProxy && boundNodeId==legacyNode)
                    ApplyBoundFieldPolicy(field,*defined,reg.readOnly);
                else field.readOnly = field.readOnly || defined->readOnly || reg.readOnly;
            }
        }
    }
    return info;
}

int HardwareClient::Impl::Width(const DeviceInfo &info,const std::string &regName,const std::string &fieldName,bool writing) {
    CheckName(regName); if (!fieldName.empty()) CheckName(fieldName);
    for (const auto &reg : info.registers) if (reg.name == regName) {
        if (writing && reg.readOnly) throw std::runtime_error("Register is declared read-only by local description");
        if (fieldName.empty()) {
            if (writing) for (const auto &field : reg.fields) if (field.readOnly)
                throw std::runtime_error("Whole-register write includes a declared read-only field; choose a writable field");
            return info.registerBytes * 8;
        }
        for (const auto &field : reg.fields) if (field.name == fieldName) {
            if (writing && field.readOnly) throw std::runtime_error("Field is declared read-only: "+info.name+"."+regName+"."+fieldName);
            return field.width;
        }
        throw std::runtime_error("Unknown hardware field: " + fieldName);
    }
    throw std::runtime_error("Unknown hardware register: " + regName);
}

} // namespace GetDAQ
