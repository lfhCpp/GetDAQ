#ifndef GETDAQ_TEST_HARDWARE_FIXTURE_H
#define GETDAQ_TEST_HARDWARE_FIXTURE_H
#include <Ice/Ice.h>
#include <mdaq/hw/Control.h>
#include <atomic>
#include <mutex>
#include <vector>
namespace GetDAQTest {
using namespace mdaq::hw;
class TestDevice : public Device {
public:
    DeviceConfig fConfig;
    std::atomic<Ice::Long> fValue{0};
    std::atomic<int> fReads{0}, fWrites{0}, fFailure{0};
    std::mutex fMutex;
    std::vector<std::string> fFields;
    explicit TestDevice(int bytes) {
        fConfig.descr.name = "ctrl"; fConfig.descr.baseAddress = 4096;
        fConfig.descr.registerAccess = "test-memory"; fConfig.descr.registerWidth = bytes;
        RegisterConfig reg; reg.descr.name = "control"; reg.descr.offset = 8;
        FieldDescription field; field.name = "mode"; field.offset = 0; field.width = 4; field.readOnly = false;
        reg.fields.push_back(field); fConfig.registers.push_back(reg);
        RegisterConfig pipe; pipe.descr.name = "pipeCtrl"; pipe.descr.offset = 0;
        const char *fields[] = {"multiplicityReadOut", "modeEnable_2p", "bitSlipCalibrate", "partialReadOut",
            "testMode", "zeroSuppressionEnable", "firmwareEnable", "hitRegisterOverride", "SCW", "SCR",
            "ScrScwOverride", "reset", "backpressure", "intEnable"};
        for (int i = 0; i < 14; ++i) {
            FieldDescription description; description.name = fields[i]; description.offset = i;
            description.width = 1; description.readOnly = false; pipe.fields.push_back(description);
        }
        fConfig.registers.push_back(pipe);
        reg.descr.name = "mixed"; field.name = "status"; field.offset = 7; field.width = 1; field.readOnly = true;
        reg.fields.push_back(field); fConfig.registers.push_back(reg);
    }
    std::string name(const Ice::Current &) override { return fConfig.descr.name; }
    Ice::Long baseAddress(const Ice::Current &) override { return fConfig.descr.baseAddress; }
    std::string registerAccess(const Ice::Current &) override { return fConfig.descr.registerAccess; }
    Ice::Short registerWidth(const Ice::Current &) override { return fConfig.descr.registerWidth; }
    void addRegister(const RegisterConfig &, const Ice::Current &) override { Unsupported(); }
    void addRegisters(const RegisterConfigList &, const Ice::Current &) override { Unsupported(); }
    Ice::Long readRegister(const std::string &, const Ice::Current &) override { ++fReads; return fValue; }
    Ice::Long readField(const std::string &, const std::string &, const Ice::Current &) override { ++fReads; return fValue & 15; }
    void writeRegister(const std::string &, Ice::Long value, const Ice::Current &current) override {
        ++fWrites; fValue = value;
        if (fFailure == 1) throw mdaq::utl::CmdException("Changed value before error / 中文");
        if (fFailure == 2) current.con->close(true); // 先生效，再丢失应答。
    }
    void writeField(const std::string &reg, const std::string &field, Ice::Long value, const Ice::Current &current) override {
        { std::lock_guard<std::mutex> lock(fMutex); fFields.push_back(reg + "." + field + "=" + std::to_string(value)); }
        writeRegister("control", (fValue.load() & ~Ice::Long(15)) | value, current);
    }
    void execBatch(const RegCmdSeq &, RegCmdSeq &, const Ice::Current &) override { Unsupported(); }
    void getDeviceDescription(DeviceDescription &out, const Ice::Current &) override { out = fConfig.descr; }
    void getDeviceConfig(DeviceConfig &out, const Ice::Current &) override { out = fConfig; }
    void getListOfRegisters(RegisterList &out, const Ice::Current &) override { out = {"control", "mixed"}; }
    void getListOfFields(const std::string &, FieldList &out, const Ice::Current &) override { out = {"mode"}; }
    void getRegisterDescription(const std::string &, RegisterDescription &, const Ice::Current &) override { Unsupported(); }
    void getRegisterConfig(const std::string &, RegisterConfig &, const Ice::Current &) override { Unsupported(); }
    void getFieldDescription(const std::string &, const std::string &, FieldDescription &, const Ice::Current &) override { Unsupported(); }
    void setVerbose(bool, const Ice::Current &) override { Unsupported(); }
    bool isVerbose(const Ice::Current &) override { return false; }
    static void Unsupported() { throw mdaq::utl::CmdException("Unused test operation"); }
};
template<class Interface> class BasicTestNode : public Interface {
public:
    DeviceMap fDevices;
    std::mutex fMutex;
    std::string fName = "test CoBo 中文";
    std::atomic<int> fCreates{0}, fCreateFailure{0};
    void testConnectionToHardware(const Ice::Current &) override {}
    void shutdown(const Ice::Current &) override { TestDevice::Unsupported(); }
    void reboot(const Ice::Current &) override { TestDevice::Unsupported(); }
    std::string name(const Ice::Current &) override { std::lock_guard<std::mutex> lock(fMutex); return fName; }
    void setName(const std::string &, const Ice::Current &) override { TestDevice::Unsupported(); }
    void create(const NodeConfig &config, const Ice::Current &current) override {
        std::lock_guard<std::mutex> lock(fMutex); ++fCreates; fName = config.id;
        for (const auto &description : config.devices) {
            if (fDevices.count(description.descr.name)) throw mdaq::utl::CmdException("Already exists");
            IceUtil::Handle<TestDevice> device = new TestDevice(description.descr.registerWidth);
            device->fConfig = description;
            // 与旧 DeviceImpl 一样，不保存权限位；验证主机绑定策略仍能拒绝写入。
            for (auto &reg : device->fConfig.registers) for (auto &field : reg.fields) field.readOnly = false;
            if (fCreateFailure == 3 && !device->fConfig.registers.empty()) ++device->fConfig.registers[0].descr.offset;
            fDevices[description.descr.name] = DevicePrx::uncheckedCast(current.adapter->addWithUUID(device));
            if (fCreateFailure == 1) throw mdaq::utl::CmdException("Partially created");
            if (fCreateFailure == 2) { current.con->close(true); return; }
        }
    }
    void destroy(const Ice::Current &) override { TestDevice::Unsupported(); }
    void getNodeConfig(NodeConfig &, const Ice::Current &) override { TestDevice::Unsupported(); }
    void getListOfDevices(DeviceList &out, const Ice::Current &) override { std::lock_guard<std::mutex> lock(fMutex); for (const auto &entry : fDevices) out.push_back(entry.first); }
    void getMapOfDevices(DeviceMap &out, const Ice::Current &) override { std::lock_guard<std::mutex> lock(fMutex); out = fDevices; }
    DevicePrx findDevice(const std::string &name, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fMutex);
        auto it = fDevices.find(name); if (it == fDevices.end()) throw mdaq::utl::CmdException("Unknown test device"); return it->second;
    }
    void removeDevice(const std::string &, const Ice::Current &) override { TestDevice::Unsupported(); }
    void removeAllDevices(const Ice::Current &) override { TestDevice::Unsupported(); }
    void addDevice(const DeviceDescription &, const Ice::Current &) override { TestDevice::Unsupported(); }
};
using TestNode = BasicTestNode<Node>;
}
#endif
