#ifndef GETDAQ_TEST_COBO_PREPARE_FIXTURE_H
#define GETDAQ_TEST_COBO_PREPARE_FIXTURE_H
#include "coboprepareclient.h"
#include "hardware_fixture.h"
#include <get/cobo/CtrlNode.h>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>
namespace GetDAQPrepareFixture {
using namespace GetDAQ;
using namespace GetDAQTest;
using namespace mdaq::hw;
inline void Require(bool value, const char *why) { if (!value) throw std::runtime_error(why); }
template<class F> void Rejects(F action, const std::string &text) {
    try { action(); } catch (const std::exception &e) { Require(std::string(e.what()).find(text) != std::string::npos,e.what()); return; }
    throw std::runtime_error("Expected rejection: "+text);
}
inline std::uint64_t Bits(Ice::Long wire) { std::uint64_t result; std::memcpy(&result,&wire,sizeof(result)); return result; }
inline Ice::Long Wire(std::uint64_t value) { Ice::Long result; std::memcpy(&result,&value,sizeof(result)); return result; }
inline std::uint64_t Mask(int width) { return width == 64 ? UINT64_MAX : (UINT64_C(1)<<width)-1; }
inline std::string ReadFile(const std::string &path) { std::ifstream file(path); return std::string(std::istreambuf_iterator<char>(file),{}); }
struct PrepareFixtureState {
    std::mutex mutex;
    std::map<std::string,DeviceInfo> descriptions;
    std::map<std::string,std::uint64_t> ctrl;
    std::array<std::map<std::string,std::uint64_t>,4> boards;
    std::array<std::map<std::string,std::uint64_t>,16> chips;
    std::vector<std::string> actions;
    std::atomic<bool> readInterlock{false};
    bool triggerReadInterlock = false;
    unsigned int powered = 0, hardwareVersion = 2;
    int writes = 0, firmwareStarts = 0, finalThresholds = 0, mode = 0;
    std::function<void()> notify;
    std::function<void(const std::string &)> onAction;
    FieldInfo Field(const std::string &device, const std::string &reg, const std::string &field) const {
        for (const auto &r : descriptions.at(device).registers) if (r.name == reg)
            for (const auto &f : r.fields) if (f.name == field) return f;
        throw std::runtime_error("Fixture missing actual field "+device+'.'+reg+'.'+field);
    }
    std::uint64_t Get(const std::string &reg, const std::string &field) {
        const auto f = Field("ctrl",reg,field); return (ctrl[reg]>>f.offset)&Mask(f.width);
    }
    void Set(const std::string &reg, const std::string &field, std::uint64_t value) {
        const auto f = Field("ctrl",reg,field); const auto mask = Mask(f.width)<<f.offset;
        ctrl[reg] = (ctrl[reg]&~mask)|(value<<f.offset);
    }
    void Reset(int failure = 0) {
        std::lock_guard<std::mutex> lock(mutex); ctrl.clear(); powered = 0; hardwareVersion = 2;
        writes = firmwareStarts = finalThresholds = 0; mode = failure; actions.clear(); readInterlock = false; triggerReadInterlock = false;
        for (auto &board : boards) { board.clear(); board["ADC0"] = 0x1e; }
        for (unsigned int id = 0; id < 16; ++id) { chips[id].clear(); chips[id]["reg5"] = UINT64_C(0xa500000000000000)|(id == 2 ? 0x201 : id == 15 ? 0x203 : 0x202); }
    }
};
class PrepareDevice : public TestDevice {
    PrepareFixtureState &fState;
    std::string fName;
    std::uint64_t Read(const std::string &reg) {
        if (fName == "ctrl") {
            if (reg == "asadEnable") return fState.powered;
            if (reg == "hardwareVersion") return fState.hardwareVersion;
            if (reg == "firmwareDate") return 20210208;
            if (reg == "firmwareRelease") return 15;
            return fState.ctrl[reg];
        }
        if (fName == "aget") return fState.chips[fState.Get("agetChipSelect","d")][reg];
        if (fName == "pll") return 0;
        if (fName.substr(0,12) == "zeroSuppress") return fState.ctrl[fName+'/'+reg];
        Require(fName == "asad","Unused fixture device was read");
        Require(reg.substr(0,3) != "ADC","ADC is write-only; implicit readback is forbidden");
        const auto id = fState.Get("asadSelect","d");
        if (reg == "monitorID") return 0x41;
        if (reg == "VDD" || reg == "VDD1") return 225;
        if (reg == "IDD") return 70;
        if (reg == "internalTemperature" || reg == "externalTemperature") return 25;
        if (reg == "interruptStatus1") return 0;
        if (reg.size() == 4 && reg.substr(0,3) == "AIM" && reg != "AIM0") return 0x80+unsigned(reg.back()-'0');
        return fState.boards[id][reg];
    }
    void Write(const std::string &reg, std::uint64_t value) {
        if (fName == "ctrl") { fState.ctrl[reg] = value; if (reg=="asadEnable") fState.powered=unsigned(value)&15; return; }
        if (fName == "pll") return;
        if (fName.substr(0,12) == "zeroSuppress") { fState.ctrl[fName+'/'+reg] = value; return; }
        if (fName == "aget") {
            const auto mask = fState.Get("aget_sc_configuration","SC_En"); Require(mask != 0,"AGET write with empty broadcast mask");
            for (unsigned int id = 0; id < 16; ++id) if (mask & (1U<<id)) fState.chips[id][reg] = value;
            return;
        }
        Require(fName == "asad","Unused fixture device was written");
        const auto mask = fState.Get("ASAD_sc_mask","select");
        for (unsigned int id = 0; id < 4; ++id) if (mask & (1U<<id)) {
            if (reg == "ADC0" && value == 0x400) {
                // ADC复位只影响ADC，不清除独立IM/监控/LCM/DAC寄存器。
                for (auto it = fState.boards[id].begin(); it != fState.boards[id].end();) {
                    if (it->first.substr(0,3) == "ADC") it = fState.boards[id].erase(it); else ++it;
                }
                fState.boards[id]["ADC0"] = 0;
            } else fState.boards[id][reg] = value;
        }
    }
public:
    PrepareDevice(const DeviceInfo &description, PrepareFixtureState &state) : TestDevice(description.registerBytes), fState(state), fName(description.name) {
        fConfig.descr.name = description.name; fConfig.descr.registerAccess = description.access;
        fConfig.descr.baseAddress = Wire(description.baseAddress); fConfig.registers.clear();
        for (const auto &source : description.registers) {
            RegisterConfig reg; reg.descr.name = source.name; reg.descr.offset = Wire(source.offset);
            for (const auto &s : source.fields) { FieldDescription f; f.name = s.name; f.offset = s.offset; f.width = s.width; f.readOnly = s.readOnly; reg.fields.push_back(f); }
            fConfig.registers.push_back(reg);
        }
    }
    Ice::Long readRegister(const std::string &reg, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); return Wire(Read(reg));
    }
    Ice::Long readField(const std::string &reg, const std::string &field, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex);
        if (fState.triggerReadInterlock && reg == "triggerMask") fState.readInterlock = true;
        if (reg == "asadConnection") return field.back() == '0' || field.back() == '3';
        if (reg == "asadStatus" && field == "alarm") return (~fState.powered)&15;
        if (reg == "asadEnable") return (fState.powered>>unsigned(field.back()-'0'))&1;
        const auto f = fState.Field(fName,reg,field); return Wire((Read(reg)>>f.offset)&Mask(f.width));
    }
    void writeRegister(const std::string &reg, Ice::Long value, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.writes;
        fState.actions.push_back(fName+'.'+reg+'='+std::to_string(Bits(value))); Write(reg,Bits(value));
        if (fState.onAction) fState.onAction(fState.actions.back());
    }
    void writeField(const std::string &reg, const std::string &field, Ice::Long value, const Ice::Current &current) override {
        bool notify = false, close = false;
        {
            std::lock_guard<std::mutex> lock(fState.mutex); ++fState.writes;
            fState.actions.push_back(fName+'.'+reg+'.'+field+'='+std::to_string(Bits(value)));
            const auto f = fState.Field(fName,reg,field); const auto mask = Mask(f.width)<<f.offset;
            Write(reg,(Read(reg)&~mask)|(Bits(value)<<f.offset));
            if (reg == "asadEnable") { const auto bit = 1U<<unsigned(field.back()-'0'); if (value) fState.powered |= bit; else fState.powered &= ~bit; }
            if (fName == "aget" && field == "DACthreshold") ++fState.finalThresholds;
            if (reg == "pipeCtrl" && field == "firmwareEnable" && value == 1) {
                ++fState.firmwareStarts; notify = fState.mode == 4; close = fState.mode == 3;
            }
            if (fState.onAction) fState.onAction(fState.actions.back());
        }
        if (notify) fState.notify();
        if (close) current.con->close(true);
    }
};
class PrepareAlarmService : public get::mt::AlarmService {
public:
    std::mutex mutex;
    std::map<std::string,get::mt::AlarmCallbackPrx> subscribers;
    int subscribes = 0, unsubscribes = 0, resets = 0, mode = 0;
    bool valid = true;
    bool ice_isA(const std::string &type,const Ice::Current &c) const override { return valid && get::mt::AlarmService::ice_isA(type,c); }
    void Reset(int failure) { std::lock_guard<std::mutex> lock(mutex); subscribers.clear(); mode = failure; subscribes = unsubscribes = resets = 0; }
    void subscribe(const std::string &id, const get::mt::AlarmCallbackPrx &callback, const Ice::Current &current) override {
        { std::lock_guard<std::mutex> lock(mutex); ++subscribes; subscribers[id] = callback; }
        if (mode == 1) callback->sendAsadAlarm("CoBo0",8);
        if (mode == 2) current.con->close(true);
    }
    void unsubscribe(const std::string &id, const Ice::Current &c) override {
        { std::lock_guard<std::mutex> lock(mutex); ++unsubscribes; subscribers.erase(id); }
        if (mode==3) c.con->close(true);
    }
    void reset(const Ice::Current &) override { ++resets; throw std::runtime_error("Must not reset unrelated alarm subscribers"); }
    void sendAsadAlarm(const std::string &id, Ice::Long bits, const Ice::Current &) override {
        std::vector<get::mt::AlarmCallbackPrx> callbacks;
        { std::lock_guard<std::mutex> lock(mutex); for (const auto &entry : subscribers) if (entry.second) callbacks.push_back(entry.second); }
        for (const auto &callback : callbacks) callback->sendAsadAlarm(id,bits);
    }
};
class PrepareNode : public BasicTestNode<get::cobo::CtrlNode> {
    PrepareAlarmService &fAlarms;
public:
    get::mt::AlarmServicePrx service;
    std::thread alarmThread;
    bool missingService = false, wrongService = false;
    std::atomic<int> monitorCalls{0}, ledCalls{0}, blinkCalls{0}, flipCalls{0}, flipAllCalls{0}, pulseCalls{0};
    int lastLedType=-1,lastLedAsad=-1,lastLedState=-1,lastFlipLed=-1,lastPulseLed=-1,lastPulsePeriod=-1;
    int alarmMode = 0;
    explicit PrepareNode(PrepareAlarmService &alarms) : fAlarms(alarms) { fName = "CoBo0"; }
    ~PrepareNode() { Join(); }
    void Join() { if (alarmThread.joinable()) alarmThread.join(); }
    get::mt::AlarmServicePrx getAlarmService(const Ice::Current &) override {
        if (missingService) return nullptr;
        if (wrongService) return get::mt::AlarmServicePrx::uncheckedCast(fDevices.at("ctrl"));
        return service;
    }
    void setAsAdAlarmMonitoringEnabled(bool, const Ice::Current &) override {
        ++monitorCalls;
        if (alarmMode == 5) alarmThread = std::thread([this] { std::this_thread::sleep_for(std::chrono::milliseconds(70)); fAlarms.sendAsadAlarm("CoBo0",1,Ice::Current()); });
    }
    void modifyLED(get::cobo::LedType type,int asad,get::cobo::LedState state,const Ice::Current &) override {
        ++ledCalls;lastLedType=int(type);lastLedAsad=asad;lastLedState=int(state);
    }
    void setLEDs(bool,const Ice::Current &) override { ++ledCalls; }
    void blinkLEDs(const Ice::Current &) override { ++blinkCalls; }
    void setLED(int,bool,const Ice::Current &) override { TestDevice::Unsupported(); }
    void flipLED(int led,const Ice::Current &) override { ++flipCalls;lastFlipLed=led; }
    void flipLEDs(const Ice::Current &) override { ++flipAllCalls; }
    void pulseLED(int led,int period,const Ice::Current &) override { ++pulseCalls;lastPulseLed=led;lastPulsePeriod=period; }
    void setDefaultPulserVoltage(Ice::Long,const Ice::Current &) override { TestDevice::Unsupported(); }
    void triggerPulser(Ice::Long,const Ice::Current &) override { TestDevice::Unsupported(); }
    void configureExternalPulser(bool,Ice::Long,const Ice::Current &) override { TestDevice::Unsupported(); }
    void configurePeriodicPulser(Ice::Long,Ice::Long,const Ice::Current &) override { TestDevice::Unsupported(); }
    void setRandomPulserEnabled(bool,const Ice::Current &) override { TestDevice::Unsupported(); }
    void configureDoublePulserMode(Ice::Long,const Ice::Current &) override { TestDevice::Unsupported(); }
    void startPeriodicPulser(const Ice::Current &) override { TestDevice::Unsupported(); }
    void stopPeriodicPulser(const Ice::Current &) override { TestDevice::Unsupported(); }
};
inline CoBoPrepareSettings Settings() {
    CoBoPrepareSettings s; s.inputs.asadMask = s.clock.asadMask = s.monitoring.asadMask = s.calibration.asadMask = 9;
    s.monitoring.ledPolicy = CoBoMonitoringSettings::RequiredLedManager; s.monitoring.powerDelayMs = s.monitoring.settleDelayMs = 0;
    s.manualReadDataDelay = 19; s.calibration.chipVersions = {{2,0x201},{13,0x202},{15,0x203}};
    for (unsigned int id : {0,3}) {
        AsAdInputBoardSettings b; b.asadId = id; b.positions[id == 0 ? 2 : 3].active = true;
        if (id == 3) { b.positions[1].active = true; b.positions[1].enabled = false; b.positions[1].connectGroundToZAP = true; }
        s.inputs.boards.push_back(b);
        AsAdBoardClockSettings c; c.asadId = id; s.clock.boards.push_back(c);
        AsAdMonitoringBoardSettings m; m.asadId = id; s.monitoring.boards.push_back(m);
        AsAdCalibrationSettings a; a.asadId = id; s.calibration.boards.push_back(a);
    }
    return s;
}
inline void RemoveField(HardwareDescription &description, const std::string &reg, const std::string &field) {
    for (auto &d : description.devices) if (d.name == "ctrl") for (auto &r : d.registers) if (r.name == reg)
        for (auto it = r.fields.begin(); it != r.fields.end(); ++it) if (it->name == field) { r.fields.erase(it); return; }
    throw std::runtime_error("Missing fixture field removal target");
}
}
#endif
