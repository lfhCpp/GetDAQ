#include "hardwareclient_description_p.h"
#include "hardwareclient_p.h"

namespace GetDAQ {
namespace HardwareClientInternal {

HardwareDescriptionPayload BuildHardwareDescriptionPayload(const HardwareDescription &description)
{
    HardwareDescriptionPayload payload;
    payload.nodeConfig.id = description.nodeId;
    payload.devicesJson = "[";
    for (const auto &source : description.devices) {
        if (!payload.nodeConfig.devices.empty()) payload.devicesJson += ',';
        mdaq::hw::DeviceConfig device;
        device.descr.name = source.name;
        device.descr.registerAccess = source.access;
        device.descr.baseAddress = Wire(source.baseAddress);
        device.descr.registerWidth = source.registerBytes;
        payload.devicesJson += "{\"name\":" + JsonString(source.name) + ",\"access\":" + JsonString(source.access) +
            ",\"base_address\":" + JsonString(std::to_string(source.baseAddress)) +
            ",\"register_bytes\":" + std::to_string(source.registerBytes) + ",\"registers\":[";
        for (const auto &sourceReg : source.registers) {
            if (!device.registers.empty()) payload.devicesJson += ',';
            mdaq::hw::RegisterConfig reg;
            reg.descr.name = sourceReg.name;
            reg.descr.offset = Wire(sourceReg.offset);
            payload.devicesJson += "{\"name\":" + JsonString(sourceReg.name) + ",\"offset\":" + JsonString(std::to_string(sourceReg.offset)) +
                ",\"read_only\":" + (sourceReg.readOnly ? "true" : "false") + ",\"fields\":[";
            for (const auto &sourceField : sourceReg.fields) {
                if (!reg.fields.empty()) payload.devicesJson += ',';
                mdaq::hw::FieldDescription field;
                field.name = sourceField.name;
                field.offset = sourceField.offset;
                field.width = sourceField.width;
                field.readOnly = sourceField.readOnly || sourceReg.readOnly;
                reg.fields.push_back(field);
                payload.devicesJson += "{\"name\":" + JsonString(field.name) + ",\"offset_bits\":" + std::to_string(field.offset) +
                    ",\"width_bits\":" + std::to_string(field.width) + ",\"read_only\":" + (field.readOnly ? "true" : "false") + "}";
            }
            payload.devicesJson += "]}";
            device.registers.push_back(reg);
        }
        payload.devicesJson += "]}";
        payload.nodeConfig.devices.push_back(device);
    }
    payload.devicesJson += ']';
    return payload;
}

} // namespace HardwareClientInternal
} // namespace GetDAQ
