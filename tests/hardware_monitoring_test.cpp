#include "hardwareclient.h"
#include "cobomonitoringconfiguration.h"
#include "hardware_fixture.h"
#include <array>
#include <deque>
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
void Require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
template<class F> void Rejects(F call, const std::string &message) {
    try { call(); } catch (const std::exception &e) { Require(std::string(e.what()).find(message) != std::string::npos,e.what()); return; }
    throw std::runtime_error("Expected failure: " + message);
}
struct State {
    std::mutex mutex;
    unsigned int plugged = 11, supplied = 15, powered = 0, afterAlarm = 0, selected = 0;
    int writes = 0, metadata = 0, failPower = -1, failRead = 0;
    std::map<std::string,Ice::Long> ctrl;
    std::array<std::map<std::string,Ice::Long>,4> registers;
    std::array<std::map<std::string,std::deque<Ice::Long>>,4> samples;
    std::array<std::map<std::string,unsigned int>,4> reads;
    std::array<unsigned int,4> powerUps{{0,0,0,0}};
    std::array<int,4> lastAim{{-1,-1,-1,-1}};
    void Reset(unsigned int mask = 11) {
        std::lock_guard<std::mutex> lock(mutex);
        plugged = mask; supplied = 15; powered = afterAlarm = selected = 0;
        writes = metadata = failRead = 0; failPower = -1; ctrl.clear();
        for (unsigned int id = 0; id < 4; ++id) {
            registers[id].clear(); samples[id].clear(); reads[id].clear(); powerUps[id] = 0; lastAim[id] = -1;
            samples[id]["VDD"] = {0,204,205}; samples[id]["VDD1"] = {1}; samples[id]["IDD"] = {100};
            samples[id]["internalTemperature"] = {255}; samples[id]["externalTemperature"] = {128};
        }
    }
};
class MonitoringDevice : public TestDevice {
    State &fState;
public:
    struct Field { const char *name; int offset,width; };
    MonitoringDevice(const std::string &name, State &state) : TestDevice(name == "asad" ? 8 : 4), fState(state) {
        fConfig.descr.name = name; fConfig.registers.clear();
        if (name == "ctrl") {
            Add("pipeCtrl",{{"firmwareEnable",0,1},{"intEnable",1,1}});
            Add("asadController",{{"reset",0,1},{"manualCal",1,1},{"calibration",2,4}});
            Add("ASAD_sc_mask",{{"select",0,4}}); Add("asadSelect",{{"d",0,2}});
            Add("asadConnection",{{"PLG0",0,1},{"PLG1",1,1},{"PLG2",2,1},{"PLG3",3,1}});
            Add("asadStatus",{{"alarm",0,4}});
            Add("asadEnable",{{"powerON0",0,1},{"powerON1",1,1},{"powerON2",2,1},{"powerON3",3,1}});
            Add("asad_sc_start",{});
        } else {
            for (const char *reg : {"interruptStatus1","monitorID","AIM0","AIM1","AIM2","AIM3","configuration1","configuration2","configuration3",
                    "interruptMask1","extTempLowLimit","extTempHighLimit","VDDLowLimit","VDDHighLimit","IDDLowLimit","IDDHighLimit",
                    "VDD","VDD1","IDD","internalTemperature","externalTemperature","IM0","IM1"}) Add(reg,{});
        }
    }
    void Add(const char *name, const std::vector<Field> &fields) {
        RegisterConfig reg; reg.descr.name = name; reg.descr.offset = fConfig.registers.size()*8;
        for (const auto &f : fields) { FieldDescription desc; desc.name = f.name; desc.offset = f.offset; desc.width = f.width; desc.readOnly = false; reg.fields.push_back(desc); }
        fConfig.registers.push_back(reg);
    }
    void getDeviceConfig(DeviceConfig &out, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.metadata; out = fConfig;
    }
    Ice::Long readField(const std::string &reg, const std::string &field, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex);
        if (reg == "asadConnection") return (fState.plugged >> unsigned(field.back()-'0')) & 1;
        if (reg == "asadEnable") return (fState.powered >> unsigned(field.back()-'0')) & 1;
        if (reg == "asadStatus") return ((~fState.powered) & fState.supplied & 15) | (fState.powered & fState.afterAlarm);
        return fState.ctrl[reg + '/' + field];
    }
    Ice::Long readRegister(const std::string &reg, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex);
        const auto id = fState.selected; Require(id < 4 && (fState.plugged & (1U << id)),"Disconnected AsAd was read");
        ++fState.reads[id][reg];
        if (reg == "interruptStatus1") return 0x44; // 观察旧告警，不偷偷清除。
        if (reg == "monitorID") return 0x41;
        if (reg.size() == 4 && reg.substr(0,3) == "AIM") {
            const int index = reg[3]-'0'; Require(index == fState.lastAim[id]+1,"AIM read order"); fState.lastAim[id] = index;
            return index ? 0x80+index : fState.registers[id][reg];
        }
        const auto found = fState.samples[id].find(reg);
        if (found == fState.samples[id].end()) { Unsupported(); return 0; } // 命令寄存器禁止回显。
        if (fState.failRead && reg == "VDD") throw mdaq::utl::CmdException("Injected monitoring read failure");
        auto &queue = found->second; Require(!queue.empty(),"Empty sample queue"); const auto value = queue.front();
        if (queue.size() > 1) queue.pop_front();
        return value;
    }
    void writeField(const std::string &reg, const std::string &field, Ice::Long value, const Ice::Current &current) override {
        bool close = false;
        {
            std::lock_guard<std::mutex> lock(fState.mutex); ++fState.writes; fState.ctrl[reg + '/' + field] = value;
            if (reg == "asadSelect") fState.selected = unsigned(value);
            if (reg == "asadEnable") {
                const auto id = unsigned(field.back()-'0');
                if (value) { fState.powered |= 1U << id; ++fState.powerUps[id]; close = fState.failPower == int(id); }
                else fState.powered &= ~(1U << id);
            }
        }
        if (close) current.con->close(true); // 上电先生效，再丢失应答。
    }
    void writeRegister(const std::string &reg, Ice::Long value, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.writes;
        if (fConfig.descr.name == "ctrl") { fState.ctrl[reg + '/'] = value; return; }
        if (reg == "IM0" || reg == "IM1") { // 广播到活动掩码，不由读取选择决定。
            for (unsigned int id = 0; id < 4; ++id) if (fState.ctrl["ASAD_sc_mask/select"] & (1U << id)) fState.registers[id][reg] = value;
        } else fState.registers[fState.selected][reg] = reg == "configuration2" && value == 0x80 ? 0 : value;
    }
};
CoBoMonitoringSettings Settings(unsigned int mask = 11) {
    CoBoMonitoringSettings s; s.asadMask = mask; s.powerDelayMs = s.settleDelayMs = 0; s.settleAttempts = 2; s.zeroReadRetries = 3;
    for (unsigned int id = 0; id < 4; ++id) if (mask & (1U << id)) { AsAdMonitoringBoardSettings b; b.asadId = id; s.boards.push_back(b); }
    return s;
}
HardwareStep ReadStep(const char *reg, const char *field) { HardwareStep step; step.kind = HardwareStep::ReadValue; step.device = "ctrl"; step.reg = reg; step.field = field; return step; }
HardwareStep Poll(unsigned int attempts = 1, unsigned int zero = 1) {
    HardwareStep step; step.kind = HardwareStep::PollValue; step.device = "asad"; step.reg = "VDD";
    step.minimum = 205; step.maximum = 255; step.pollAttempts = attempts; step.zeroReadRetries = zero; return step;
}
std::string ReadFile(const std::string &path) { std::ifstream f(path); return std::string(std::istreambuf_iterator<char>(f),{}); }
RegisterInfo &Register(DeviceInfo &device, const std::string &name) {
    for (auto &reg : device.registers) if (reg.name == name) return reg;
    throw std::runtime_error("Missing test register");
}
}
int main()
{
    Ice::CommunicatorPtr communicator;
    try {
        communicator = Ice::initialize(); auto adapter = communicator->createObjectAdapterWithEndpoints("MonitoringTest","tcp -h 127.0.0.1 -p 0");
        State state; IceUtil::Handle<TestNode> node = new TestNode;
        IceUtil::Handle<MonitoringDevice> ctrl = new MonitoringDevice("ctrl",state), asad = new MonitoringDevice("asad",state);
        node->fDevices["ctrl"] = DevicePrx::uncheckedCast(adapter->addWithUUID(ctrl)); node->fDevices["asad"] = DevicePrx::uncheckedCast(adapter->addWithUUID(asad));
        const auto address = communicator->proxyToString(adapter->addWithUUID(node)); adapter->activate();
        char directory[] = "/tmp/getdaq-monitoring-tcp-XXXXXX"; Require(mkdtemp(directory) != nullptr,"Cannot create monitoring test directory");
        unsigned int counter = 0; const auto audit = [&] { return std::string(directory)+"/audit-"+std::to_string(counter++)+".jsonl"; };
        for (int minor : {0,1}) {
            HardwareClient client(address,minor,1000); HardwareDescription policy; policy.nodeId = "monitoring";
            policy.devices = {client.Describe("ctrl"),client.Describe("asad")}; client.UseDescription(policy);
            state.Reset(); const auto settings = Settings(); auto plan = CoBoAsAdPrepare(settings);
            std::vector<HardwareStep> prefixed{ReadStep("pipeCtrl","firmwareEnable")}; AppendHardwareSteps(prefixed,plan);
            const auto normalPath = audit(); client.Execute(prefixed,normalPath);
            Require(state.powered == 11 && state.ctrl["ASAD_sc_mask/select"] == 11 && state.reads[2].empty(),"Power/active mask/disconnected board semantics");
            for (unsigned int id : {0,1,3}) {
                Require(state.reads[id]["interruptStatus1"] == 1 && state.reads[id]["VDD"] == 3 && state.powerUps[id] == 1,"Condition rebasing or settle retries");
                Require(state.registers[id]["VDDLowLimit"] == 204 && state.registers[id]["VDDHighLimit"] == 238 && state.registers[id]["interruptMask1"] == 255,"Monitoring limits/initial disable");
                Require(state.registers[id]["configuration1"] == 45 && state.registers[id]["IM0"] == 15 && state.registers[id]["IM1"] == 0,"Prepare initial protections");
            }
            Require(ReadFile(normalPath).find("\"phase\":\"skipped\"") != std::string::npos && ReadFile(normalPath).find("\"phase\":\"complete\"") != std::string::npos,"Skip/complete audit absent");
            client.Execute(CoBoAsAdProtectionInit(settings),audit());
            for (unsigned int id : {0,1,3}) Require(state.registers[id]["interruptMask1"] == 35 && state.registers[id]["configuration1"] == 13,"Final protection mode");
            const auto readPath = audit(); client.Execute(CoBoAsAdMonitoringRead(settings),readPath);
            Require(ReadFile(readPath).find("\"actual\":\"255\"") != std::string::npos && ReadFile(readPath).find("\"actual\":\"128\"") != std::string::npos,"Raw signed samples lost");
            state.Reset(15); client.Execute(CoBoAsAdPowerCycle(Settings(1)),audit());
            Require(state.powered == 1 && state.reads[3]["interruptStatus1"] == 1 && state.powerUps[3] == 0,"Connected inactive board diagnostic/power-off behavior");
            state.Reset(1); auto mode = Settings(1); mode.boards[0].singleChannelItem = "IDD";
            mode.boards[0].fastConversionEnabled = mode.boards[0].singleChannelModeEnabled = true;
            client.Execute(CoBoAsAdMonitoringInit(mode),audit());
            Require(state.registers[0]["configuration2"] == 21 && state.registers[0]["configuration3"] == 1,"Declared conversion mode deployment");
            mode.modeMapping = CoBoMonitoringSettings::LegacyCallArguments;
            client.Execute(CoBoAsAdMonitoringInit(mode),audit());
            Require(state.registers[0]["configuration2"] == 49 && state.registers[0]["configuration3"] == 0,"Legacy reordered conversion mode deployment");
            state.Reset(1); const auto lateConnection = audit();
            Rejects([&] { client.Execute(CoBoAsAdPowerCycle(settings),lateConnection); },"mismatch");
            Require(state.powered == 1 && state.powerUps[0] == 1 && state.powerUps[1] == 0 && state.ctrl["ASAD_sc_mask/select"] != 11,"Late physical connection failure rolled back/continued/restored mask");
            HardwareClient unbound(address,minor,1000); state.Reset();
            Rejects([&] { unbound.Execute(plan,audit()); },"bound hardware"); Require(state.metadata == 0 && state.writes == 0,"Unbound monitoring accessed hardware");
            auto missing = policy; auto &regs = missing.devices[1].registers;
            for (auto it = regs.begin(); it != regs.end(); ++it) if (it->name == "VDD") { regs.erase(it); break; }
            HardwareClient noVdd(address,minor,1000); noVdd.UseDescription(missing); state.Reset();
            Rejects([&] { noVdd.Execute(plan,audit()); },"description"); Require(state.writes == 0,"Late missing VDD allowed power writes");
            auto deniedPolicy = policy; Register(deniedPolicy.devices[1],"IDDHighLimit").readOnly = true;
            HardwareClient denied(address,minor,1000); denied.UseDescription(deniedPolicy); state.Reset();
            Rejects([&] { denied.Execute(plan,audit()); },"read-only"); Require(state.writes == 0,"Late readonly limit allowed reset/power writes");
            auto falseTarget = ReadStep("asadConnection","PLG2"); HardwareStep absent; absent.device = "asad"; absent.reg = "absent"; absent.conditionStep = 0; absent.conditionMask = absent.conditionValue = 1;
            state.Reset(); Rejects([&] { client.Execute({falseTarget,absent},audit()); },"register"); Require(state.writes == 0,"False branch bypassed metadata validation");
            state.Reset(); state.supplied &= ~1U; const auto supplyPath = audit();
            Rejects([&] { client.Execute(CoBoAsAdPowerCycle(Settings(1)),supplyPath); },"mismatch"); Require(state.powerUps[0] == 0,"Missing supply powered board");
            state.Reset(); state.supplied &= ~1U; auto waived = Settings(1); waived.boards[0].checkPowerSupply = false; const auto warningPath = audit();
            client.Execute(CoBoAsAdPowerCycle(waived),warningPath); Require(state.powerUps[0] == 1 && ReadFile(warningPath).find("matched_with_warning") != std::string::npos,"Explicit supply waiver not audited");
            state.Reset(); state.supplied &= ~1U; state.afterAlarm = 1;
            Rejects([&] { client.Execute(CoBoAsAdPowerCycle(waived),audit()); },"mismatch"); Require(state.powerUps[0] == 1,"After-power alarm replayed power command");
            state.Reset(); state.failPower = 1; const auto unknownPath = audit();
            Rejects([&] { client.Execute(plan,unknownPath); },"UNKNOWN");
            Require(state.powerUps[1] == 1 && state.powerUps[3] == 0 && state.powered == 3 && ReadFile(unknownPath).find("\"phase\":\"unknown\"") != std::string::npos,"Lost power reply replayed/rolled back or continued");
            state.Reset(1); state.samples[0]["VDD"] = {0}; const auto zeroPath = audit();
            Rejects([&] { client.Execute(CoBoAsAdPrepare(Settings(1)),zeroPath); },"range not reached"); Require(state.reads[0]["VDD"] == 6 && state.registers[0].count("IM0") == 0,"Zero retries exceeded bounds/continued after failure");
            state.Reset(1); state.failRead = 1; const auto readFailure = audit();
            Rejects([&] { client.Execute(CoBoAsAdPrepare(Settings(1)),readFailure); },"read error"); Require(state.reads[0]["VDD"] == 1 && ReadFile(readFailure).find("read_failed") != std::string::npos,"Read error was retried");
            state.Reset(1); state.samples[0]["VDD"] = {204};
            Rejects([&] { client.Execute(CoBoAsAdPrepare(Settings(1)),audit()); },"range not reached"); Require(state.reads[0]["VDD"] == 2,"Nonzero low sample used zero retry budget");
            state.Reset(1); state.samples[0]["VDD"] = {0}; client.Execute(CoBoAsAdMonitoringRead(Settings(1)),audit());
            Require(state.reads[0]["VDD"] == 3,"Observation zero retry count");
            state.Reset(); auto bad = ReadStep("pipeCtrl","firmwareEnable"); bad.conditionStep = 0;
            Rejects([&] { client.Execute({bad},audit()); },"earlier unconditional"); Require(state.metadata == 0 && state.writes == 0,"Forward condition accessed hardware");
            std::vector<HardwareStep> cyclic{ReadStep("asadConnection","PLG0"),ReadStep("asadConnection","PLG1"),ReadStep("asadConnection","PLG3")};
            cyclic[1].conditionStep = 0; cyclic[2].conditionStep = 1;
            Rejects([&] { client.Execute(cyclic,audit()); },"earlier unconditional");
            std::vector<HardwareStep> unchanged{ReadStep("pipeCtrl","firmwareEnable")};
            Rejects([&] { AppendHardwareSteps(unchanged,cyclic); },"Fragment condition"); Require(unchanged.size() == 1,"Invalid append changed destination");
            auto condition = ReadStep("pipeCtrl","intEnable"); condition.conditionStep = 0; condition.conditionMask = 2; condition.conditionValue = 0;
            Rejects([&] { client.Execute({ReadStep("pipeCtrl","firmwareEnable"),condition},audit()); },"condition mask"); Require(state.writes == 0,"Out-of-width condition wrote");
            auto poll = Poll(); poll.minimum = 256; const auto invalidPath = audit();
            Rejects([&] { client.Execute({poll},invalidPath); },"poll parameters"); Require(access(invalidPath.c_str(),F_OK) != 0,"Invalid range created executable intent");
            poll = Poll(100,10); std::vector<HardwareStep> tooMany(5,poll);
            Rejects([&] { client.Execute(tooMany,audit()); },"4096 reads"); Require(state.writes == 0,"Polling read budget wrote");
            poll.pollDelayMs = 60000; Rejects([&] { client.Execute({poll},audit()); },"300 seconds");
            poll = Poll(); poll.pollAttempts = 101; Rejects([&] { client.Execute({poll},audit()); },"poll parameters");
            state.Reset(); state.ctrl["pipeCtrl/firmwareEnable"] = 1;
            Rejects([&] { client.Execute(plan,audit()); },"mismatch"); Require(state.writes == 0,"Running acquisition powered boards");
        }
        communicator->destroy(); std::cout << "PASS GET TCP 1.0/1.1 AsAd power/monitoring: conditional diagnostics, preflight, bounded polling, protection and no write replay\n";
        return 0;
    } catch (const std::exception &e) { if (communicator) communicator->destroy(); std::cerr << e.what() << '\n'; return 1; }
}
