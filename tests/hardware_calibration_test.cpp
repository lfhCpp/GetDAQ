#include "hardwareclient.h"
#include "cobocalibrationconfiguration.h"
#include "hardware_fixture.h"
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <unistd.h>
namespace {
using namespace GetDAQ;
using namespace GetDAQTest;
using namespace mdaq::hw;
void Require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
template<class F> void Rejects(F call, const std::string &text) {
    try { call(); } catch (const std::exception &e) { Require(std::string(e.what()).find(text) != std::string::npos,e.what()); return; }
    throw std::runtime_error("Expected rejection: " + text);
}
Ice::Long Wire(std::uint64_t value) { Ice::Long result; static_assert(sizeof(result) == sizeof(value),"64-bit wire"); std::memcpy(&result,&value,sizeof(value)); return result; }
std::uint64_t Bits(Ice::Long value) { std::uint64_t result; std::memcpy(&result,&value,sizeof(value)); return result; }
struct State {
    std::mutex mutex;
    std::map<std::string,Ice::Long> ctrl;
    std::array<std::map<std::string,std::uint64_t>,16> chips;
    std::array<std::map<std::string,std::uint64_t>,4> boards;
    std::vector<std::string> operations;
    int writes = 0, metadata = 0, hitWrites = 0, reg5Reads = 0, firmwareStarts = 0, scaInit = 0;
    bool failFirmware = false;
    void Reset() {
        std::lock_guard<std::mutex> lock(mutex); ctrl.clear(); operations.clear();
        writes = metadata = hitWrites = reg5Reads = firmwareStarts = scaInit = 0; failFirmware = false;
        for (unsigned int id = 0; id < 16; ++id) {
            chips[id].clear(); chips[id]["reg5"] = UINT64_C(0xa500000000000000) | (id == 2 ? 0x201 : id == 15 ? 0x203 : 0x202);
            chips[id]["reg1"] = UINT64_C(0x0040000000400100)+id; // 极性位 22 为 1，阈值写入必须保留。
        }
        for (auto &board : boards) { board.clear(); board["ADC0"] = 0xff; }
    }
};
class CalibrationDevice : public TestDevice {
    State &fState;
    unsigned int Selected() const { const auto found = fState.ctrl.find("agetChipSelect/d"); return found == fState.ctrl.end() ? 0 : unsigned(found->second); }
    void Broadcast(const std::string &reg, std::uint64_t value) {
        const auto mask = unsigned(fState.ctrl["aget_sc_configuration/SC_En"]); Require(mask != 0 && mask <= 0xffff,"Missing AGET broadcast mask");
        for (unsigned int id = 0; id < 16; ++id) if (mask & (1U << id)) fState.chips[id][reg] = value;
    }
public:
    struct Field { const char *name; int offset,width; };
    CalibrationDevice(const std::string &name, State &state) : TestDevice(name == "ctrl" ? 4 : 8), fState(state) {
        fConfig.descr.name = name; fConfig.registers.clear();
        if (name == "ctrl") {
            Add("pipeCtrl",{{"firmwareEnable",0,1},{"intEnable",1,1},{"SCRAutoDelayEnable",2,1},{"SCRAutoDelayCalibrate",3,1},
                {"SCW",4,1},{"SCR",5,1},{"ScrScwOverride",6,1},{"reset",7,1},
                {"SCRAutoDelayEnable0",8,1},{"SCRAutoDelayEnable1",9,1},{"SCRAutoDelayEnable2",10,1},{"SCRAutoDelayEnable3",11,1}});
            Add("agetController",{{"reset",0,1},{"calibration",1,1},{"hitRegister",2,1}});
            Add("aget_sc_configuration",{{"SC_En",0,16},{"pulseEnable",16,16}});
            Add("ASAD_sc_mask",{{"select",0,4}}); Add("agetChipSelect",{{"d",0,4}});
            Add("calibrationAget",{{"AsAd0",0,2},{"AsAd1",2,2},{"AsAd2",4,2},{"AsAd3",6,2}});
            Add("ckpFastDivider",{{"d",0,3}}); Add("aget_sc_rw",{}); Add("initSCAReadOutAfterPowerOnNow",{});
        } else if (name == "asad") {
            for (const char *reg : {"resetFast","ADC0","ADC2","ADC3","ADC4","ADC5"}) Add(reg,{});
        } else { Add("reg1",{{"DAC",19,4},{"DACthreshold",19,3}}); Add("reg2",{}); Add("reg5",{}); Add("hit",{}); }
    }
    void Add(const char *name, const std::vector<Field> &fields) {
        RegisterConfig reg; reg.descr.name = name; reg.descr.offset = fConfig.registers.size()*8;
        for (const auto &field : fields) { FieldDescription d; d.name = field.name; d.offset = field.offset; d.width = field.width; d.readOnly = false; reg.fields.push_back(d); }
        fConfig.registers.push_back(reg);
    }
    void getDeviceConfig(DeviceConfig &result, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.metadata; result = fConfig;
    }
    Ice::Long readField(const std::string &reg, const std::string &field, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex);
        if (fConfig.descr.name == "aget") { Require(reg == "reg1" && (field == "DAC" || field == "DACthreshold"),"Unexpected AGET field read"); return Ice::Long((fState.chips[Selected()][reg] >> 19) & (field == "DAC" ? 15 : 7)); }
        Require(fConfig.descr.name == "ctrl","AsAd command unexpectedly read back"); return fState.ctrl[reg + '/' + field];
    }
    Ice::Long readRegister(const std::string &reg, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex);
        Require(fConfig.descr.name == "aget" && reg == "reg5","Command register unexpectedly read back");
        const auto id = Selected(); Require(id < 16 && (fState.ctrl["aget_sc_configuration/SC_En"] & (1U << id)),"Inactive AGET selected for version observation");
        ++fState.reg5Reads; fState.operations.push_back("R:aget.reg5"); return Wire(fState.chips[id][reg]);
    }
    void writeField(const std::string &reg, const std::string &field, Ice::Long value, const Ice::Current &current) override {
        bool close = false;
        {
            std::lock_guard<std::mutex> lock(fState.mutex); ++fState.writes;
            fState.operations.push_back("W:" + fConfig.descr.name + '.' + reg + '.' + field + '=' + std::to_string(value));
            if (fConfig.descr.name == "aget") {
                Require(reg == "reg1" && (field == "DAC" || field == "DACthreshold") && value >= 0 && value <= 7,"Unexpected AGET field write");
                // 与旧 DeviceImpl::writeField 相同：读选择器指定的 reg1，更新字段后按 SC_En 广播。
                const std::uint64_t mask = field == "DAC" ? 15 : 7;
                auto original = fState.chips[Selected()][reg]; original = (original & ~(mask << 19)) | (Bits(value) << 19); Broadcast(reg,original);
            } else {
                fState.ctrl[reg + '/' + field] = value;
                if (reg == "pipeCtrl" && field == "firmwareEnable" && value == 1) { ++fState.firmwareStarts; close = fState.failFirmware; }
            }
        }
        if (close) current.con->close(true); // 先生效再丢失应答，状态未知，绝不能自动补写 firmwareEnable=0。
    }
    void writeRegister(const std::string &reg, Ice::Long value, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.writes;
        fState.operations.push_back("W:" + fConfig.descr.name + '.' + reg + '=' + std::to_string(Bits(value)));
        if (fConfig.descr.name == "aget") {
            if (reg == "hit") { Require(Bits(value) == UINT64_C(0xa000000000000005),"64-bit calibration vector corrupted"); ++fState.hitWrites; }
            Broadcast(reg,Bits(value));
        } else if (fConfig.descr.name == "asad") {
            for (unsigned int id = 0; id < 4; ++id) if (fState.ctrl["ASAD_sc_mask/select"] & (1U << id)) {
                if (reg == "ADC0" && value == 0x400) { fState.boards[id].clear(); fState.boards[id][reg] = 0; }
                else fState.boards[id][reg] = Bits(value);
            }
        } else { fState.ctrl[reg + '/'] = value; if (reg == "initSCAReadOutAfterPowerOnNow") ++fState.scaInit; }
    }
};
CoBoCalibrationSettings Settings() {
    CoBoCalibrationSettings s; s.asadMask = 11; s.chipVersions = {{2,0x201},{5,0x202},{15,0x203}};
    for (unsigned int id : {0,1,3}) { AsAdCalibrationSettings b; b.asadId = id; s.boards.push_back(b); }
    return s;
}
std::string ReadFile(const std::string &path) { std::ifstream f(path); return std::string(std::istreambuf_iterator<char>(f),{}); }
RegisterInfo &Register(DeviceInfo &device, const std::string &name) {
    for (auto &reg : device.registers) if (reg.name == name) return reg;
    throw std::runtime_error("Missing test register");
}
void RemoveRegister(DeviceInfo &device, const std::string &name) {
    for (auto it = device.registers.begin(); it != device.registers.end(); ++it) if (it->name == name) { device.registers.erase(it); return; }
    throw std::runtime_error("Missing test removal target");
}
}
int main()
{
    Ice::CommunicatorPtr communicator;
    try {
        communicator = Ice::initialize(); auto adapter = communicator->createObjectAdapterWithEndpoints("CalibrationTest","tcp -h 127.0.0.1 -p 0");
        State state; IceUtil::Handle<TestNode> node = new TestNode;
        IceUtil::Handle<CalibrationDevice> ctrl = new CalibrationDevice("ctrl",state), asad = new CalibrationDevice("asad",state), aget = new CalibrationDevice("aget",state);
        node->fDevices["ctrl"] = DevicePrx::uncheckedCast(adapter->addWithUUID(ctrl)); node->fDevices["asad"] = DevicePrx::uncheckedCast(adapter->addWithUUID(asad));
        node->fDevices["aget"] = DevicePrx::uncheckedCast(adapter->addWithUUID(aget));
        const auto address = communicator->proxyToString(adapter->addWithUUID(node)); adapter->activate();
        char directory[] = "/tmp/getdaq-calibration-tcp-XXXXXX"; Require(mkdtemp(directory) != nullptr,"Cannot create test directory");
        unsigned int counter = 0; const auto audit = [&] { return std::string(directory) + "/audit-" + std::to_string(counter++) + ".jsonl"; };
        for (int minor : {0,1}) {
            HardwareClient client(address,minor,1000); HardwareDescription policy; policy.nodeId = "calibration";
            policy.devices = {client.Describe("ctrl"),client.Describe("asad"),client.Describe("aget")}; client.UseDescription(policy);
            const auto settings = Settings(); const auto plan = CoBoAgetInit(settings); const auto image = CompileCoBoCalibration(settings);
            state.Reset(); const auto normal = audit(); client.Execute(plan,normal);
            Require(state.hitWrites == 2 && state.reg5Reads == 6 && state.scaInit == 1,"Serial calibration/active version/readout init coverage");
            Require(state.ctrl["aget_sc_configuration/SC_En"] == image.agetMask && state.ctrl["aget_sc_configuration/pulseEnable"] == 0,"Final slow control mask");
            Require(state.ctrl["calibrationAget/AsAd0"] == 2 && state.ctrl["calibrationAget/AsAd1"] == 1 && state.ctrl["calibrationAget/AsAd2"] == 0 && state.ctrl["calibrationAget/AsAd3"] == 3,"Calibration selection, including inactive default");
            for (unsigned int id : {0,1,3}) Require(state.boards[id]["ADC0"] == 0 && state.boards[id]["ADC2"] == 0x2a0 && state.boards[id]["ADC3"] == 1 && state.boards[id]["ADC5"] == 0x401,"Serial ADC reset/test vector mismatch");
            Require(state.boards[2]["ADC0"] == 0xff,"Inactive AsAd was reset");
            Require(ReadFile(normal).find("11529215046068469765") != std::string::npos && ReadFile(normal).find("\"phase\":\"complete\"") != std::string::npos,"High-bit intent or completion missing");
            state.Reset(); const auto before = state.chips;
            client.Execute(CoBoAgetGlobalThreshold(settings,2),audit());
            for (const auto &version : settings.chipVersions) Require(state.chips[version.first]["reg1"] == ((before[version.first].at("reg1") & ~(UINT64_C(7)<<19)) | (UINT64_C(2)<<19)),"Per-chip DAC field overwrote unrelated reg1 bits");
            Require(state.chips[0] == before[0] && state.ctrl["aget_sc_configuration/SC_En"] == image.agetMask,"Threshold changed inactive chip/mask");
            state.Reset(); client.Execute(CoBoAutomaticReadDelayCalibrate(settings),audit());
            Require(state.firmwareStarts == 1 && state.ctrl["pipeCtrl/firmwareEnable"] == 0 && state.ctrl["pipeCtrl/SCRAutoDelayCalibrate"] == 0 && state.ctrl["pipeCtrl/ScrScwOverride"] == 0,"Transient enable/cleanup sequence");
            for (const auto &version : settings.chipVersions) Require(state.chips[version.first]["reg1"] == 0x3e3a41a7 && state.chips[version.first]["reg2"] == 0x0028a197,"Automatic delay broadcast image");
            Require(state.ctrl["pipeCtrl/SCRAutoDelayEnable"] == 1,"Computing did not enable delay measurement");
            client.Execute(CoBoSelectReadDataDelay(settings),audit()); Require(state.ctrl["pipeCtrl/SCRAutoDelayEnable"] == 0,"Compute/use incorrectly coupled");
            state.Reset(); auto perAsAd = settings; perAsAd.delayLayout = CoBoCalibrationSettings::PerAsAdDelayFields; perAsAd.computeAutomaticReadDelay = false;
            perAsAd.boards[0].useAutomaticReadDelay = perAsAd.boards[2].useAutomaticReadDelay = true;
            client.Execute(CoBoAutomaticReadDelayCalibrate(perAsAd),audit()); Require(state.writes == 0,"Disabled compute still wrote calibration registers");
            client.Execute(CoBoSelectReadDataDelay(perAsAd),audit());
            Require(state.ctrl["pipeCtrl/SCRAutoDelayEnable0"] == 1 && state.ctrl["pipeCtrl/SCRAutoDelayEnable1"] == 0 && state.ctrl["pipeCtrl/SCRAutoDelayEnable3"] == 1 && state.ctrl.count("pipeCtrl/SCRAutoDelayEnable2") == 0,"Per-board selector used field0 or inactive board");
            state.Reset(); auto conflicting = perAsAd; conflicting.delayLayout = CoBoCalibrationSettings::GlobalDelayField;
            Rejects([&] { CoBoSelectReadDataDelay(conflicting); },"different active AsAd policies"); Require(state.metadata == 0 && state.writes == 0,"Conflicting global policy accessed hardware");
            HardwareClient unbound(address,minor,1000); state.Reset();
            Rejects([&] { unbound.Execute(plan,audit()); },"bound hardware"); Require(state.metadata == 0 && state.writes == 0,"Unbound calibration accessed hardware");
            auto missing = policy; RemoveRegister(missing.devices[0],"initSCAReadOutAfterPowerOnNow");
            HardwareClient lateMissing(address,minor,1000); lateMissing.UseDescription(missing); state.Reset();
            Rejects([&] { lateMissing.Execute(plan,audit()); },"description"); Require(state.writes == 0,"Late missing initialization command allowed resets");
            auto deniedPolicy = policy; Register(deniedPolicy.devices[2],"hit").readOnly = true;
            HardwareClient denied(address,minor,1000); denied.UseDescription(deniedPolicy); state.Reset();
            Rejects([&] { denied.Execute(plan,audit()); },"read-only"); Require(state.writes == 0,"Readonly hit allowed initial ADC writes");
            auto noAuto = policy; auto &fields = Register(noAuto.devices[0],"pipeCtrl").fields;
            for (auto it = fields.begin(); it != fields.end(); ++it) if (it->name == "SCRAutoDelayEnable") { fields.erase(it); break; }
            HardwareClient noField(address,minor,1000); noField.UseDescription(noAuto); state.Reset();
            Rejects([&] { noField.Execute(CoBoAutomaticReadDelayCalibrate(settings),audit()); },"description"); Require(state.writes == 0,"Missing auto field allowed reg1/reg2 writes/fallback");
            state.Reset(); state.ctrl["pipeCtrl/intEnable"] = 1;
            Rejects([&] { client.Execute(plan,audit()); },"mismatch"); Require(state.writes == 0,"Running pipeline was calibrated");
            state.Reset(); state.chips[5]["reg5"] = 0x203; const auto versionFail = audit();
            Rejects([&] { client.Execute(plan,versionFail); },"mismatch");
            Require(state.hitWrites == 2 && state.scaInit == 0 && state.writes > 0,"Known but unexpected chip model was accepted or initialization continued");
            state.Reset(); state.failFirmware = true; const auto unknown = audit();
            Rejects([&] { client.Execute(CoBoAutomaticReadDelayCalibrate(settings),unknown); },"UNKNOWN");
            Require(state.firmwareStarts == 1 && state.ctrl["pipeCtrl/firmwareEnable"] == 1 && state.ctrl["pipeCtrl/SCRAutoDelayCalibrate"] == 1 && state.ctrl["pipeCtrl/SCW"] == 0,"Lost reply replayed/continued or automatically cleaned up");
            Require(ReadFile(unknown).find("\"phase\":\"unknown\"") != std::string::npos,"Unknown calibration not audited");
            state.Reset(); auto legacy = settings; legacy.calibrationAgetRegister = false; legacy.chipVersions = {{0,0x202},{4,0x202},{12,0x202}};
            auto oldPolicy = policy; RemoveRegister(oldPolicy.devices[0],"calibrationAget");
            const auto originalRegisters = ctrl->fConfig.registers;
            for (auto it = ctrl->fConfig.registers.begin(); it != ctrl->fConfig.registers.end(); ++it) if (it->descr.name == "calibrationAget") { ctrl->fConfig.registers.erase(it); break; }
            HardwareClient oldClient(address,minor,1000); oldClient.UseDescription(oldPolicy);
            oldClient.Execute(CoBoAgetCalibrate(legacy),audit()); // 不借写失败降级；实际模拟远端也没有该寄存器。
            Require(state.ctrl.count("calibrationAget/AsAd0") == 0 && state.hitWrites == 2,"Explicit legacy plan still probed calibration register");
            state.Reset(); oldClient.Execute(CoBoSelectReadDataDelay(legacy),audit()); Require(state.writes == 1,"Legacy explicit policy unexpectedly accessed calibration register");
            ctrl->fConfig.registers = originalRegisters;
            Rejects([&] { CoBoAgetGlobalThreshold(settings,8); },"0..7");
        }
        communicator->destroy(); std::cout << "PASS GET TCP 1.0/1.1 calibration: broadcast selection, ADC test vector, high-bit hit, expected models, DAC preservation, explicit delay layout and unknown stop\n";
        return 0;
    } catch (const std::exception &e) { if (communicator) communicator->destroy(); std::cerr << e.what() << '\n'; return 1; }
}
