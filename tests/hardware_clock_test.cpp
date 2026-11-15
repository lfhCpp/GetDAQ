#include "hardwareclient.h"
#include "coboclockconfiguration.h"
#include "hardware_fixture.h"
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
void Require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
template<class F> void Rejects(F call, const std::string &message) {
    try { call(); } catch (const std::exception &e) { Require(std::string(e.what()).find(message) != std::string::npos,e.what()); return; }
    throw std::runtime_error("Expected failure: " + message);
}
std::uint64_t Bits(Ice::Long value) { std::uint64_t result; std::memcpy(&result,&value,8); return result; }
Ice::Long Wire(std::uint64_t value) { Ice::Long result; std::memcpy(&result,&value,8); return result; }
struct State {
    std::mutex mutex;
    std::map<std::string,std::uint64_t> values;
    std::map<unsigned int,std::uint64_t> lcm1,lcm2;
    std::vector<std::uint32_t> packets;
    unsigned int selected = 0;
    int writes = 0, metadata = 0, lcmReads = 0, failure = 0;
    void Reset(unsigned int version = 2) {
        std::lock_guard<std::mutex> lock(mutex); values.clear(); values["ctrl/hardwareVersion/"] = version;
        lcm1.clear(); lcm2.clear(); packets.clear(); writes = metadata = lcmReads = failure = 0; selected = 0;
    }
};
class ClockDevice : public TestDevice {
    State &fState;
public:
    ClockDevice(const std::string &name, State &s) : TestDevice(name == "asad" ? 8 : 4), fState(s) {
        fConfig.descr.name = name; fConfig.registers.clear();
        if (name == "ctrl") {
            Add("pipeCtrl",{{"firmwareEnable",0,1},{"intEnable",1,1}}); Add("hardwareVersion",{});
            Add("ASAD_sc_mask",{{"select",0,4}}); Add("asadSelect",{{"d",0,2}});
            Add("ckwDivider",{{"d",0,3}}); Add("ckrDivider",{{"d",0,3}});
        } else if (name == "asad") { Add("LCM1",{}); Add("LCM2",{}); }
        else { Add("WRFIFO_RST",{}); Add("RDFIFO_RST",{}); Add("WRFIFO_DATA",{}); Add("WRFIFO_SR",{{"Full",0,1}}); }
    }
    struct Field { const char *name; int offset,width; };
    void Add(const char *name, const std::vector<Field> &fields) {
        RegisterConfig reg; reg.descr.name = name; reg.descr.offset = fConfig.registers.size()*8;
        for (const auto &f : fields) { FieldDescription field; field.name = f.name; field.offset = f.offset; field.width = f.width; field.readOnly = false; reg.fields.push_back(field); }
        fConfig.registers.push_back(reg);
    }
    void getDeviceConfig(DeviceConfig &out, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.metadata; out = fConfig;
    }
    Ice::Long readRegister(const std::string &reg, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex);
        if (fConfig.descr.name == "asad" || fConfig.descr.name == "pll") { ++fState.lcmReads; Unsupported(); }
        return Wire(fState.values[fConfig.descr.name + '/' + reg + '/']);
    }
    Ice::Long readField(const std::string &reg, const std::string &field, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex);
        if (reg == "WRFIFO_SR" && field == "Full") return 1; // 观察 Full=1 也不能宣称锁定或发送完成。
        return Wire(fState.values[fConfig.descr.name + '/' + reg + '/' + field]);
    }
    void writeField(const std::string &reg, const std::string &field, Ice::Long value, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.writes;
        fState.values[fConfig.descr.name + '/' + reg + '/' + field] = Bits(value);
        if (reg == "asadSelect") fState.selected = unsigned(value);
    }
    void writeRegister(const std::string &reg, Ice::Long value, const Ice::Current &current) override {
        bool close = false, fail = false;
        {
            std::lock_guard<std::mutex> lock(fState.mutex); ++fState.writes;
            fState.values[fConfig.descr.name + '/' + reg + '/'] = Bits(value);
            if (reg == "WRFIFO_DATA") { fState.packets.push_back(std::uint32_t(value)); close = fState.failure == 1 && fState.packets.size() == 3; }
            if (reg == "LCM1") fState.lcm1[fState.selected] = Bits(value);
            if (reg == "LCM2") { fState.lcm2[fState.selected] = Bits(value); fail = fState.failure == 2; }
        }
        if (close) current.con->close(true);
        if (fail) throw mdaq::utl::CmdException("LCM2 applied before failure");
    }
};
CoBoClockSettings Settings(unsigned int mask = 11, unsigned int version = 2, float writing = 50) {
    CoBoClockSettings s; s.asadMask = mask; s.hardwareVersion = version; s.writingMHz = writing;
    for (unsigned int id = 0; id < 4; ++id) if (mask & (1U << id)) { AsAdBoardClockSettings b; b.asadId = id; s.boards.push_back(b); }
    return s;
}
std::string ReadFile(const std::string &path) { std::ifstream f(path); return std::string(std::istreambuf_iterator<char>(f),{}); }
}
int main()
{
    Ice::CommunicatorPtr communicator;
    try {
        communicator = Ice::initialize();
        auto adapter = communicator->createObjectAdapterWithEndpoints("ClockTest","tcp -h 127.0.0.1 -p 0");
        State state; IceUtil::Handle<TestNode> node = new TestNode;
        IceUtil::Handle<ClockDevice> ctrl = new ClockDevice("ctrl",state), asad = new ClockDevice("asad",state), pll = new ClockDevice("pll",state);
        node->fDevices["ctrl"] = DevicePrx::uncheckedCast(adapter->addWithUUID(ctrl));
        node->fDevices["asad"] = DevicePrx::uncheckedCast(adapter->addWithUUID(asad));
        node->fDevices["pll"] = DevicePrx::uncheckedCast(adapter->addWithUUID(pll));
        const auto address = communicator->proxyToString(adapter->addWithUUID(node)); adapter->activate();
        char directory[] = "/tmp/getdaq-clock-tcp-XXXXXX"; Require(mkdtemp(directory) != nullptr,"Cannot create clock test directory");
        unsigned int counter = 0;
        const auto audit = [&] { return std::string(directory) + "/audit-" + std::to_string(counter++) + ".jsonl"; };
        for (int minor : {0,1}) {
            HardwareClient client(address,minor,1000); HardwareDescription policy; policy.nodeId = "clock";
            policy.devices = {client.Describe("ctrl"),client.Describe("asad"),client.Describe("pll")};
            policy.devices.back().registers.back().fields[0].readOnly = true;
            client.UseDescription(policy);
            state.Reset(); const auto normal = Settings(); const auto image = CompileCoBoClock(normal); const auto normalPath = audit();
            client.Execute(CoBoClockInit(normal),normalPath);
            Require(state.packets == image.pll.packets && state.lcm1.size() == 3 && state.lcm2.size() == 3 && state.lcmReads == 0,"PLL/LCM command order/read behavior");
            for (const auto &b : image.boards) Require(state.lcm1.at(b.asadId) == b.sampling.value && state.lcm2.at(b.asadId) == b.reading.value,"64-bit LCM/board selection differs");
            Require(state.lcm1.at(0) >> 63 && state.values["ctrl/ASAD_sc_mask/select"] == 11,"High-bit LCM or final mask lost");
            Require(ReadFile(normalPath).find("no PLL lock/frequency/phase verification") != std::string::npos && ReadFile(normalPath).find("\"phase\":\"complete\"") != std::string::npos,"Clock scope audit absent");
            state.Reset(1); const auto reduced = Settings(5,1,6.25f); client.Execute(CoBoClockInit(reduced),audit());
            Require(state.packets.empty() && state.values["ctrl/ckwDivider/d"] == 4 && state.values["ctrl/ckrDivider/d"] == 2 && state.lcm2.size() == 2,"R-CoBo branch wrote PLL or wrong divider");
            state.Reset(); auto sync = Settings(); sync.pll["EN_CLKin1"] = "true"; sync.pll["SYNC_POL_INV"] = "true";
            client.Execute(CoBoClockInit(sync),audit()); Require(state.packets.size() == 27 && state.packets[12] == 0x0401000b && state.packets[13] == 0x0400000b,"Named PLL sync packets");
            state.Reset(); auto mismatch = Settings(1,2,123); Rejects([&] { CoBoClockInit(mismatch); },"allowNominalDifference");
            mismatch.allowNominalDifference = true; const auto mismatchPath = audit(); client.Execute(CoBoClockInit(mismatch),mismatchPath);
            Require(ReadFile(mismatchPath).find("nominal_difference_ack=true") != std::string::npos,"Nominal mismatch acknowledgment missing");
            state.Reset(); auto unknown = Settings(1); unknown.skipPllConfig = true;
            Rejects([&] { CoBoClockInit(unknown); },"allowUnverifiedUpstream"); unknown.allowUnverifiedUpstream = true;
            client.Execute(CoBoClockInit(unknown),audit()); Require(state.packets.empty(),"skipPLL programmed packets");
            state.Reset(); unknown = Settings(1); unknown.boards[0].samplingExpert = {{"FINDIV",18},{"FBDIV",18},{"OADIV",0},{"OAMUX",1},{"FBSEL",1},{"FBDLY",0},{"xDlySel",0},{"DlyGLA",0},{"VCOSel",2}};
            Rejects([&] { CoBoClockInit(unknown); },"allowUnverifiedUpstream"); unknown.allowUnverifiedUpstream = true;
            client.Execute(CoBoClockInit(unknown),audit()); Require(state.lcm1.size() == 1,"Expert unknown output lost");
            auto unsafe = Settings(1,2,1); unsafe.allowNominalDifference = unsafe.allowUnverifiedUpstream = true;
            Rejects([&] { CoBoClockInit(unsafe); },"outside 1..200");
            HardwareClient unbound(address,minor,1000); state.Reset();
            Rejects([&] { unbound.Execute(CoBoClockInit(normal),audit()); },"bound hardware"); Require(state.metadata == 0 && state.writes == 0,"Unbound plan accessed hardware");
            auto invalid = CoBoClockInit(normal); invalid.back().annotation = "line\n";
            Rejects([&] { client.Execute(invalid,audit()); },"printable ASCII"); Require(state.metadata == 0 && state.writes == 0,"Late annotation validation wrote hardware");
            invalid.back().annotation.assign(1025,'x'); Rejects([&] { client.Execute(invalid,audit()); },"1024 bytes");
            std::vector<HardwareStep> oversized(4096); for (auto &step : oversized) { step.kind = HardwareStep::Delay; step.annotation.assign(1024,'x'); }
            Rejects([&] { client.Execute(oversized,audit()); },"4 MiB");
            auto missing = policy; missing.devices[1].registers.pop_back();
            HardwareClient lateMissing(address,minor,1000); lateMissing.UseDescription(missing); state.Reset();
            Rejects([&] { lateMissing.Execute(CoBoClockInit(normal),audit()); },"description"); Require(state.writes == 0,"Late LCM2 metadata failure wrote hardware");
            auto readOnly = policy; readOnly.devices[2].registers[2].readOnly = true;
            HardwareClient denied(address,minor,1000); denied.UseDescription(readOnly); state.Reset();
            Rejects([&] { denied.Execute(CoBoClockInit(normal),audit()); },"read-only"); Require(state.writes == 0,"Read-only FIFO preflight wrote hardware");
            auto lateReadOnly = policy; lateReadOnly.devices[1].registers.back().readOnly = true;
            HardwareClient lateDenied(address,minor,1000); lateDenied.UseDescription(lateReadOnly); state.Reset();
            Rejects([&] { lateDenied.Execute(CoBoClockInit(normal),audit()); },"read-only"); Require(state.writes == 0,"Late read-only LCM2 allowed FIFO writes");
            auto narrow = policy; narrow.devices[1].registerBytes = 4; asad->fConfig.descr.registerWidth = 4;
            HardwareClient narrowClient(address,minor,1000); narrowClient.UseDescription(narrow); state.Reset();
            Rejects([&] { narrowClient.Execute(CoBoClockInit(normal),audit()); },"exceeds hardware"); Require(state.writes == 0,"LCM width failure allowed FIFO writes");
            asad->fConfig.descr.registerWidth = 8;
            auto lateBad = normal; lateBad.boards.back().delayPs = 5336; state.Reset();
            Rejects([&] { CoBoClockInit(lateBad); },"delay"); Require(state.writes == 0 && state.metadata == 0,"Late pure clock error touched hardware");
            state.Reset(1); Rejects([&] { client.Execute(CoBoClockInit(normal),audit()); },"mismatch"); Require(state.writes == 0,"Wrong actual hardware version wrote clock");
            state.Reset(); state.values["ctrl/pipeCtrl/firmwareEnable"] = 1;
            Rejects([&] { client.Execute(CoBoClockInit(normal),audit()); },"mismatch"); Require(state.writes == 0,"Running firmware wrote clock");
            state.Reset(); state.failure = 1; const auto lost = audit();
            Rejects([&] { client.Execute(CoBoClockInit(normal),lost); },"UNKNOWN");
            Require(state.packets.size() == 3 && state.lcm1.empty() && ReadFile(lost).find("\"phase\":\"unknown\"") != std::string::npos,"FIFO applied before disconnect was replayed");
            state.Reset(); state.failure = 2; const auto late = audit();
            Rejects([&] { client.Execute(CoBoClockInit(normal),late); },"UNKNOWN");
            Require(state.lcm1.size() == 3 && state.lcm2.size() == 1 && state.values["ctrl/ASAD_sc_mask/select"] == 1,"LCM2 failure replayed/restored mask");
            std::cout << "PASS ClockInit encoding 1." << minor << " PLL/R-CoBo/64-bit LCM, complete preflight, bounded intent and unknown stop\n";
        }
        communicator->destroy(); return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n'; if (communicator) { try { communicator->destroy(); } catch (...) {} } return 1;
    }
}
