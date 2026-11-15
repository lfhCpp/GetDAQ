#include "hardwareclient.h"
#include "coboalarmclient.h"
#include "cobomonitoringconfiguration.h"
#include "hardware_fixture.h"
#include <get/cobo/CtrlNode.h>
#include <array>
#include <csignal>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
namespace {
using namespace GetDAQ;
using namespace GetDAQTest;
using namespace mdaq::hw;
void Require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
template<class F> void Rejects(F call, const std::string &text) {
    try { call(); } catch (const std::exception &e) { Require(std::string(e.what()).find(text) != std::string::npos,e.what()); return; }
    throw std::runtime_error("Expected rejection: " + text);
}
struct State {
    std::mutex mutex;
    std::map<std::string,Ice::Long> fields;
    unsigned int plugged = 11, supplied = 15, powered = 0, afterAlarm = 0;
    int writes = 0, ledCalls = 0, monitorCalls = 0, failure = 0;
    bool monitor = false, fixConnectionOnRed = false;
    std::array<int,16> lamps;
    void Reset() {
        std::lock_guard<std::mutex> lock(mutex); fields.clear(); lamps.fill(0);
        plugged = 11; supplied = 15; powered = afterAlarm = 0; writes = ledCalls = monitorCalls = failure = 0; monitor = fixConnectionOnRed = false;
    }
};
class ServicesDevice : public TestDevice {
    State &fState;
public:
    ServicesDevice(const std::string &name, State &state) : TestDevice(name == "ctrl" ? 4 : 8), fState(state) {
        fConfig.descr.name = name; fConfig.registers.clear();
        const auto add = [this](const char *regName, const std::vector<std::string> &names, const std::vector<int> &widths) {
            RegisterConfig reg; reg.descr.name = regName; reg.descr.offset = fConfig.registers.size()*8; int offset = 0;
            for (std::size_t i = 0; i < names.size(); ++i) { FieldDescription d; d.name = names[i]; d.offset = offset; d.width = widths[i]; d.readOnly = false; offset += widths[i]; reg.fields.push_back(d); }
            fConfig.registers.push_back(reg);
        };
        if (name == "ctrl") {
            add("hardwareVersion",{},{}); add("pipeCtrl",{"firmwareEnable","intEnable"},{1,1});
            add("asadController",{"reset","manualCal","calibration"},{1,1,4});
            add("ASAD_sc_mask",{"select"},{4}); add("asadSelect",{"d"},{2});
            add("asadConnection",{"PLG0","PLG1","PLG2","PLG3"},{1,1,1,1}); add("asadStatus",{"alarm"},{4});
            add("asadEnable",{"powerON0","powerON1","powerON2","powerON3"},{1,1,1,1}); add("mutantConfig",{"initDone"},{1});
        } else add("interruptStatus1",{},{});
    }
    Ice::Long readField(const std::string &reg, const std::string &field, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex);
        if (reg == "asadConnection") return (fState.plugged >> unsigned(field.back()-'0')) & 1;
        if (reg == "asadEnable") return (fState.powered >> unsigned(field.back()-'0')) & 1;
        if (reg == "asadStatus") return ((~fState.powered) & fState.supplied & 15) | (fState.powered & fState.afterAlarm);
        return fState.fields[reg+'/'+field];
    }
    Ice::Long readRegister(const std::string &reg, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex);
        if (reg == "hardwareVersion") return 2;
        if (reg == "interruptStatus1") return 0;
        if (reg == "asadEnable") return fState.powered;
        Unsupported(); return 0;
    }
    void writeField(const std::string &reg, const std::string &field, Ice::Long value, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.writes; fState.fields[reg+'/'+field] = value;
        if (reg == "asadEnable") { const auto bit = 1U << unsigned(field.back()-'0'); if (value) fState.powered |= bit; else fState.powered &= ~bit; }
    }
};
class Service : public get::mt::AlarmService {
public:
    std::mutex mutex;
    std::map<std::string,get::mt::AlarmCallbackPrx> subscribers;
    int subscribes = 0, unsubscribes = 0, resets = 0, failure = 0;
    void subscribe(const std::string &id, const get::mt::AlarmCallbackPrx &cb, const Ice::Current &current) override {
        { std::lock_guard<std::mutex> lock(mutex); ++subscribes; subscribers[id] = cb; }
        if (failure == 3) cb->sendAsadAlarm("CoBo0",4);
        if (failure == 1) current.con->close(true);
    }
    void unsubscribe(const std::string &id, const Ice::Current &current) override {
        { std::lock_guard<std::mutex> lock(mutex); ++unsubscribes; subscribers.erase(id); }
        if (failure == 2) current.con->close(true);
    }
    void reset(const Ice::Current &) override { ++resets; throw std::runtime_error("reset must never be called"); }
    void sendAsadAlarm(const std::string &id, Ice::Long bits, const Ice::Current &) override {
        std::vector<get::mt::AlarmCallbackPrx> targets;
        { std::lock_guard<std::mutex> lock(mutex); for (const auto &entry : subscribers) targets.push_back(entry.second); }
        for (const auto &target : targets) target->sendAsadAlarm(id,bits);
    }
    get::mt::AlarmCallbackPrx Callback(const std::string &id) { std::lock_guard<std::mutex> lock(mutex); return subscribers.at(id); }
};
class ServicesNode : public BasicTestNode<get::cobo::CtrlNode> {
    State &fState;
public:
    get::mt::AlarmServicePrx service;
    explicit ServicesNode(State &state) : fState(state) { fName = "CoBo0"; }
    get::mt::AlarmServicePrx getAlarmService(const Ice::Current &) override { return service; }
    void setAsAdAlarmMonitoringEnabled(bool enabled, const Ice::Current &current) override {
        { std::lock_guard<std::mutex> lock(fState.mutex); ++fState.monitorCalls; fState.monitor = enabled; }
        if (fState.failure == 3) current.con->close(true);
    }
    void modifyLED(get::cobo::LedType type, int id, get::cobo::LedState value, const Ice::Current &current) override {
        { std::lock_guard<std::mutex> lock(fState.mutex); ++fState.ledCalls; fState.lamps.at(unsigned(4*id+type)) = int(value);
          if (type == get::cobo::LedB && fState.fixConnectionOnRed) fState.plugged |= 1U << unsigned(id); }
        if (fState.failure == 1) current.con->close(true);
    }
    void setLEDs(bool enabled, const Ice::Current &current) override {
        if (fState.failure == 2) throw Ice::OperationNotExistException(__FILE__,__LINE__,current.id,current.facet,current.operation);
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.ledCalls; fState.lamps.fill(enabled ? 1 : 0);
    }
    void blinkLEDs(const Ice::Current &) override { TestDevice::Unsupported(); }
    void setLED(int,bool,const Ice::Current &) override { TestDevice::Unsupported(); }
    void flipLED(int,const Ice::Current &) override { TestDevice::Unsupported(); }
    void flipLEDs(const Ice::Current &) override { TestDevice::Unsupported(); }
    void pulseLED(int,int,const Ice::Current &) override { TestDevice::Unsupported(); }
    void setDefaultPulserVoltage(Ice::Long,const Ice::Current &) override { TestDevice::Unsupported(); }
    void triggerPulser(Ice::Long,const Ice::Current &) override { TestDevice::Unsupported(); }
    void configureExternalPulser(bool,Ice::Long,const Ice::Current &) override { TestDevice::Unsupported(); }
    void configurePeriodicPulser(Ice::Long,Ice::Long,const Ice::Current &) override { TestDevice::Unsupported(); }
    void setRandomPulserEnabled(bool,const Ice::Current &) override { TestDevice::Unsupported(); }
    void configureDoublePulserMode(Ice::Long,const Ice::Current &) override { TestDevice::Unsupported(); }
    void startPeriodicPulser(const Ice::Current &) override { TestDevice::Unsupported(); }
    void stopPeriodicPulser(const Ice::Current &) override { TestDevice::Unsupported(); }
};
class External : public get::mt::AlarmCallback {
public:
    std::atomic<int> calls{0};
    void sendAsadAlarm(const std::string &,Ice::Long,const Ice::Current &) override { ++calls; }
};
CoBoMonitoringSettings Settings() {
    CoBoMonitoringSettings s; s.asadMask = 11; s.ledPolicy = CoBoMonitoringSettings::RequiredLedManager; s.powerDelayMs = 0;
    for (unsigned int id : {0,1,3}) { AsAdMonitoringBoardSettings b; b.asadId = id; s.boards.push_back(b); } return s;
}
std::string ReadFile(const std::string &path) { std::ifstream file(path); return std::string(std::istreambuf_iterator<char>(file),{}); }
}
int main(int argc, char **argv)
{
    Ice::CommunicatorPtr communicator;
    try {
        Ice::InitializationData init; init.properties = Ice::createProperties();
        // 模拟服务和外部订阅者共用适配器；回调仍走TCP，需允许入站回调与原请求并行。
        init.properties->setProperty("Ice.ThreadPool.Server.Size","2"); init.properties->setProperty("Ice.ThreadPool.Server.SizeMax","4");
        communicator = Ice::initialize(init); auto adapter = communicator->createObjectAdapterWithEndpoints("ServicesTest","tcp -h 127.0.0.1 -p 0");
        State state; IceUtil::Handle<ServicesNode> node = new ServicesNode(state); IceUtil::Handle<Service> service = new Service;
        IceUtil::Handle<ServicesDevice> ctrl = new ServicesDevice("ctrl",state), asad = new ServicesDevice("asad",state);
        node->fDevices["ctrl"] = DevicePrx::uncheckedCast(adapter->addWithUUID(ctrl)); node->fDevices["asad"] = DevicePrx::uncheckedCast(adapter->addWithUUID(asad));
        node->service = get::mt::AlarmServicePrx::uncheckedCast(adapter->addWithUUID(service));
        IceUtil::Handle<External> external = new External;
        const auto ext = get::mt::AlarmCallbackPrx::uncheckedCast(adapter->addWithUUID(external)->ice_collocationOptimized(false));
        const auto address = communicator->proxyToString(adapter->addWithUUID(node)); adapter->activate();
        if (argc == 2 && std::string(argv[1]) == "--serve") {
            service->subscribers["other-client"] = ext;
            std::cout << address << std::endl;
            std::string command;
            while (std::getline(std::cin,command) && command != "quit") {
                if (command == "alarm") service->sendAsadAlarm("CoBo0",8,Ice::Current());
                std::lock_guard<std::mutex> lock(service->mutex);
                std::cout << "{\"subscribers\":" << service->subscribers.size() << ",\"resets\":" << service->resets
                    << ",\"subscribes\":" << service->subscribes << ",\"unsubscribes\":" << service->unsubscribes << '}';
                std::cout << std::endl;
            }
            communicator->destroy(); return 0;
        }
        char folder[] = "/tmp/getdaq-services-tcp-XXXXXX"; Require(mkdtemp(folder) != nullptr,"Cannot create test directory");
        unsigned int count = 0; const auto audit = [&] { return std::string(folder)+"/audit-"+std::to_string(count++)+".jsonl"; };
        for (int minor : {0,1}) {
            HardwareClient client(address,minor,1000); HardwareDescription policy; policy.nodeId = "services"; policy.devices = {client.Describe("ctrl"),client.Describe("asad")}; client.UseDescription(policy);
            state.Reset(); auto settings = Settings(); auto plan = CoBoAsAdPowerCycle(settings);
            HardwareStep prefix; prefix.kind = HardwareStep::ReadValue; prefix.device = "ctrl"; prefix.reg = "pipeCtrl"; prefix.field = "firmwareEnable";
            std::vector<HardwareStep> combined{prefix}; AppendHardwareSteps(combined,plan); client.Execute(combined,audit());
            Require(state.powered == 11 && state.ledCalls == 4 && state.lamps[0] == 1 && state.lamps[4] == 1 && state.lamps[12] == 1 && state.lamps[8] == 0,"Power success LEDs/cached fragment rebase");
            state.Reset(); state.plugged = 9; state.fixConnectionOnRed = true;
            Rejects([&] { client.Execute(plan,audit()); },"cached expectation mismatch");
            Require(state.powered == 1 && state.lamps[5] == 2 && state.plugged == 11,"Red indication reread/continued or failed to use original cached connection");
            state.Reset(); state.failure = 2; Rejects([&] { client.Execute(plan,audit()); },"UNKNOWN"); Require(state.writes == 0,"Missing LED method was silently ignored");
            state.Reset(); state.failure = 1; const auto unknownLed = audit(); Rejects([&] { client.Execute(plan,unknownLed); },"UNKNOWN");
            Require(state.powered == 1 && state.ledCalls == 2 && ReadFile(unknownLed).find("\"phase\":\"unknown\"") != std::string::npos,"LED applied reply loss replayed or continued");
            state.Reset(); auto denied = policy; for (auto &r : denied.devices[0].registers) if (r.name == "mutantConfig") r.fields[0].readOnly = true;
            HardwareClient forbidden(address,minor,1000); forbidden.UseDescription(denied);
            Rejects([&] { forbidden.Execute(CoBoAsAdAlarmMonitorInit(settings),audit()); },"read-only"); Require(state.monitorCalls == 0,"Watcher indirect write bypassed permission");
            client.Execute(CoBoAsAdAlarmMonitorInit(settings),audit()); Require(state.monitor && state.monitorCalls == 1,"Board watcher did not start");
            settings.checkAsAdAlarms = false; client.Execute(CoBoAsAdAlarmMonitorInit(settings),audit()); Require(!state.monitor,"Board watcher disable failed");
            state.Reset(); state.failure = 3; Rejects([&] { client.Execute(CoBoAsAdAlarmMonitorInit(settings),audit()); },"UNKNOWN"); Require(state.monitorCalls == 1,"Watcher replay after lost reply");
            state.Reset(); settings.ledPolicy = CoBoMonitoringSettings::NoLedManager; client.Execute(CoBoAsAdPowerCycle(settings),audit()); Require(state.ledCalls == 0,"Explicit disabled LED policy called manager");
            HardwareStep cached; cached.kind = HardwareStep::ExpectObservedValue; cached.observationStep = 0;
            Rejects([&] { client.Execute({cached},audit()); },"earlier unconditional");
            auto invalid = CoBoLedModify(HardwareStep::LedB,0,HardwareStep::LedOn,2); invalid.back().ledState = static_cast<HardwareStep::LedState>(99);
            Rejects([&] { client.Execute(invalid,audit()); },"Invalid LED");
            Rejects([&] { CoBoLedSetAll(false,3); },"2 or 5");
            const auto setLeds = CoBoLedSetAll(true,5);
            Require(setLeds.size() == 2 && setLeds[0].kind == HardwareStep::ExpectValue && setLeds[0].device == "ctrl" &&
                    setLeds[0].reg == "hardwareVersion" && setLeds[0].value == 5 && setLeds[0].requireBoundDescription &&
                    setLeds[1].kind == HardwareStep::LedCommand && setLeds[1].enabled,
                    "Set-all LED plan lost version binding or enabled state");
            const auto modifyLed = CoBoLedModify(HardwareStep::LedU,2,HardwareStep::LedFastPulse,2);
            Require(modifyLed.size() == 2 && modifyLed[0].kind == HardwareStep::ExpectValue &&
                    modifyLed[1].kind == HardwareStep::LedCommand && modifyLed[1].ledOperation == HardwareStep::ModifyLed &&
                    modifyLed[1].asadId == 2 && modifyLed[1].ledType == HardwareStep::LedU &&
                    modifyLed[1].ledState == HardwareStep::LedFastPulse, "Modify LED plan fields changed");
            Require(CoBoLedBlinkAll().front().ledOperation == HardwareStep::BlinkAllLeds &&
                    CoBoLedFlip(15).front().ledOperation == HardwareStep::FlipOneLed &&
                    CoBoLedFlip(16).front().ledOperation == HardwareStep::FlipAllLeds &&
                    CoBoLedPulse(16,250).front().periodMs == 250 && CoBoLedPulse(0,0).front().periodMs == 0,
                    "Standalone LED command plan encoding changed");
            std::cout << "PASS LED/checker/cached observation encoding 1." << minor << std::endl;
            service->subscribers.clear(); service->failure = 0; service->subscribers["other-client"] = ext;
            CoBoAlarmOptions options; options.nodeProxy = address; options.expectedCoBoId = "CoBo0"; options.callbackEndpoints = "tcp -h 127.0.0.1 -p 0";
            options.encodingMinor = minor; options.timeoutMs = 1000; options.maxPendingEvents = 2; options.eventAuditPath = audit();
            CoBoAlarmClient first(options); options.eventAuditPath = audit(); CoBoAlarmClient second(options);
            Require(first.SubscriberId() != second.SubscriberId(),"Subscription owner IDs collided");
            first.Subscribe(audit()); second.Subscribe(audit()); const int otherBefore = external->calls;
            const auto notify = get::mt::AlarmServicePrx::uncheckedCast(node->service->ice_collocationOptimized(false));
            notify->sendAsadAlarm("CoBo0",1); notify->sendAsadAlarm("CoBo0",8); notify->sendAsadAlarm("CoBo0",2);
            auto snap = first.Snapshot(); Require(snap.received == 3 && snap.durable == 3 && snap.pending == 2 && snap.queueOverflow && snap.lost == 1 && snap.alarmMask == 11 && snap.stopRequested,"Bounded queue/durable/latching accounting");
            Require(external->calls == otherBefore+3 && service->resets == 0,"Other subscriber was reset or overwritten");
            const auto drained = first.Drain(2); Require(drained.size() == 2 && drained[0].alarmBits == 1 && drained[1].alarmBits == 8 && first.Snapshot().stopRequested,"Drain erased fault latch/order");
            const auto cb = service->Callback(first.SubscriberId()); cb->sendAsadAlarm("wrong-id",1); cb->sendAsadAlarm("CoBo0",-1); cb->sendAsadAlarm("CoBo0",0);
            Require(first.Snapshot().invalid == 3,"Invalid notification accepted");
            first.Unsubscribe(audit()); Require(service->subscribers.count("other-client") && service->subscribers.count(second.SubscriberId()) && !service->subscribers.count(first.SubscriberId()),"Unsubscribe changed other owners");
            second.Unsubscribe(audit()); Require(second.Snapshot().subscription == CoBoAlarmSnapshot::Closed && service->resets == 0,"Owned unsubscribe state");
            options.eventAuditPath = audit(); CoBoAlarmClient early(options); service->failure = 3;
            early.Subscribe(audit()); service->failure = 0;
            Require(early.Snapshot().received == 1 && early.Snapshot().subscription == CoBoAlarmSnapshot::Active,"Callback during subscribe deadlocked or vanished");
            early.Unsubscribe(audit());
            options.eventAuditPath = audit(); options.maxPendingEvents = 1; options.maxEventAuditBytes = 65536;
            CoBoAlarmClient bounded(options); bounded.Subscribe(audit()); const auto budgetCb = service->Callback(bounded.SubscriberId());
            for (unsigned int i = 0; i < 1000 && !bounded.Snapshot().budgetExceeded; ++i) { budgetCb->sendAsadAlarm("CoBo0",1); bounded.Drain(1); }
            Require(bounded.Snapshot().budgetExceeded && bounded.Snapshot().lost > 0 && ReadFile(options.eventAuditPath).size() <= 65536,"Event audit grew past bound or lost health flag");
            Rejects([&] { bounded.Unsubscribe(audit()); },"event audit failed"); Require(bounded.Snapshot().subscription == CoBoAlarmSnapshot::Closed,"Successful unsubscribe hidden by spool failure");
            options.eventAuditPath = audit(); options.maxEventAuditBytes = 16*1024*1024;
            CoBoAlarmClient broken(options); broken.Subscribe(audit()); struct rlimit original;
            Require(getrlimit(RLIMIT_FSIZE,&original) == 0,"getrlimit"); const auto oldSignal = std::signal(SIGXFSZ,SIG_IGN);
            struct rlimit limited = original; limited.rlim_cur = ReadFile(options.eventAuditPath).size()+1;
            Require(setrlimit(RLIMIT_FSIZE,&limited) == 0,"setrlimit"); service->Callback(broken.SubscriberId())->sendAsadAlarm("CoBo0",2);
            Require(setrlimit(RLIMIT_FSIZE,&original) == 0,"restore rlimit"); std::signal(SIGXFSZ,oldSignal);
            Require(broken.Snapshot().auditFailed && broken.Snapshot().stopRequested && broken.Snapshot().lost == 1,"Partial event audit failure was ignored");
            Rejects([&] { broken.Unsubscribe(audit()); },"event audit failed");
            std::cout << "PASS callbacks/ownership/budgets/audit failure encoding 1." << minor << std::endl;
            options.eventAuditPath = audit(); options.expectedCoBoId = "wrong"; CoBoAlarmClient wrong(options);
            const int before = service->subscribes; Rejects([&] { wrong.Subscribe(audit()); },"name differs"); Require(service->subscribes == before && wrong.Snapshot().subscription == CoBoAlarmSnapshot::Fresh,"Wrong node subscribed");
            options.expectedCoBoId = "CoBo0"; options.eventAuditPath = audit();
            const int destructorCloses = service->unsubscribes;
            {
                CoBoAlarmClient lost(options); service->failure = 1; const int attempts = service->subscribes;
                Rejects([&] { lost.Subscribe(audit()); },"UNKNOWN"); Require(service->subscribes == attempts+1 && lost.Snapshot().subscription == CoBoAlarmSnapshot::Unknown,"Idempotent subscribe replayed after lost reply");
                Rejects([&] { lost.Subscribe(audit()); },"current state"); Rejects([&] { lost.Unsubscribe(audit()); },"current state");
                service->failure = 0;
            }
            Require(destructorCloses == service->unsubscribes,"Destructor unexpectedly sent unsubscribe");
            service->subscribers.clear(); service->subscribers["other-client"] = ext;
            options.eventAuditPath = audit(); CoBoAlarmClient lostClose(options); lostClose.Subscribe(audit()); service->failure = 2;
            const int closes = service->unsubscribes; Rejects([&] { lostClose.Unsubscribe(audit()); },"UNKNOWN");
            Require(service->unsubscribes == closes+1 && lostClose.Snapshot().subscription == CoBoAlarmSnapshot::Unknown && service->subscribers.count("other-client"),"Unsubscribe replayed or affected other owner"); service->failure = 0;
            Require(service->resets == 0,"Alarm reset used");
        }
        communicator->destroy(); std::cout << "PASS GET services TCP 1.0/1.1: LED power/cached guards, indirect watcher permissions, owned callback, bounded queue/spool, partial audit and unknown no retry\n"; return 0;
    } catch (const std::exception &e) { if (communicator) communicator->destroy(); std::cerr << e.what() << '\n'; return 1; }
}
