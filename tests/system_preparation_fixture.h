#ifndef GETDAQ_TEST_SYSTEM_PREPARATION_FIXTURE_H
#define GETDAQ_TEST_SYSTEM_PREPARATION_FIXTURE_H
#include "systempreparation.h"
#include "systemexperimentbinding.h"
#include "cobo_prepare_fixture.h"
#include "mutant_fixture.h"
#include "cobo_configure_fixture.h"
#include <get/cobo/MutantLinkManager.h>
#include <get/mutant/AlignmentServer.h>
#include <array>
namespace GetDAQSystemFixture {
using namespace GetDAQ;
using GetDAQPrepareFixture::Require;
using GetDAQPrepareFixture::Rejects;
struct World {
    std::mutex logMutex, linkMutex;
    std::vector<std::string> actions;
    std::array<unsigned int,3> modes{{0,0,0}};
    std::array<GetDAQPrepareFixture::PrepareFixtureState,3> c;
    std::array<GetDAQMutantFixture::State,3> m;
    std::array<IceUtil::Handle<GetDAQPrepareFixture::PrepareAlarmService>,3> alarms;
    std::atomic<int> tdc{0}, multi{0}, links{0};
    std::array<unsigned int,3> pulserCalls{{0,0,0}};
    std::array<int,3> pulserVoltages{{0,0,0}};
    bool failPulserAfterTrigger = false;
    bool alarmDuringConfigure = false;
    bool failLedAfterCommand = false;
    unsigned int ledDelayMs = 0;
    void Record(const std::string &s) { std::lock_guard<std::mutex> lock(logMutex); actions.push_back(s); }
    std::size_t Find(const std::string &s,std::size_t from = 0) {
        std::lock_guard<std::mutex> lock(logMutex);
        for (std::size_t i = from; i < actions.size(); ++i) if (actions[i] == s) return i;
        throw std::runtime_error("Missing system action: "+s);
    }
    void Reset() {
        for (auto &x : c) x.Reset();
        for (auto &x : m) x.Reset();
        for (auto &a : alarms) a->Reset(0);
        { std::lock_guard<std::mutex> lock(logMutex); actions.clear(); }
        { std::lock_guard<std::mutex> lock(linkMutex); modes = {{0,0,0}}; }
        tdc = multi = links = 0;
        pulserCalls={{0,0,0}};pulserVoltages={{0,0,0}};
        failPulserAfterTrigger=false;
        alarmDuringConfigure = false;
        failLedAfterCommand=false;
    }
    int Writes() const {
        int n = 0; for (const auto &s : c) n += s.writes; for (const auto &s : m) n += int(s.writes.size()); return n;
    }
};
class ConfigureNode : public GetDAQPrepareFixture::PrepareNode {
    GetDAQPrepareFixture::PrepareFixtureState &fState;
    World &fWorld;
    unsigned int fId;
    void Voltage(Ice::Long v) {
        std::lock_guard<std::mutex> lock(fState.mutex);
        const auto code = AsAdDacCode(int(v));
        const auto mask = fState.Get("ASAD_sc_mask","select");
        for (unsigned int i = 0; i < 4; ++i) if (mask & (1u<<i)) {
            fState.boards[i]["TCM1"] = code>>8; fState.boards[i]["TCM2"] = code&255;
        }
    }
public:
    ConfigureNode(GetDAQPrepareFixture::PrepareAlarmService &alarms,World &world,unsigned int id)
        : PrepareNode(alarms),fState(world.c[id]),fWorld(world),fId(id) { fName = "CoBo"+std::to_string(id); }
    void setDefaultPulserVoltage(Ice::Long v,const Ice::Current &) override { Voltage(v); }
    void triggerPulser(Ice::Long v,const Ice::Current &) override { ++fWorld.pulserCalls[fId];fWorld.pulserVoltages[fId]=int(v);Voltage(v);fWorld.Record("C"+std::to_string(fId)+".pulser-trigger="+std::to_string(v));if(fWorld.failPulserAfterTrigger)throw std::runtime_error("injected pulser acknowledgement loss"); }
    void configureExternalPulser(bool,Ice::Long,const Ice::Current &) override {}
    void configurePeriodicPulser(Ice::Long,Ice::Long,const Ice::Current &) override {}
    void setRandomPulserEnabled(bool,const Ice::Current &) override {}
    void configureDoublePulserMode(Ice::Long,const Ice::Current &) override {}
    void modifyLED(get::cobo::LedType type,int asad,get::cobo::LedState state,const Ice::Current &current) override {
        if(fWorld.ledDelayMs) std::this_thread::sleep_for(std::chrono::milliseconds(fWorld.ledDelayMs));
        PrepareNode::modifyLED(type,asad,state,current);
        fWorld.Record("C"+std::to_string(fId)+".led="+std::to_string(unsigned(type))+":"+std::to_string(asad)+":"+std::to_string(unsigned(state)));
        if(fWorld.failLedAfterCommand)throw std::runtime_error("injected LED acknowledgement loss");
    }
    void setLEDs(bool enabled,const Ice::Current &current) override {PrepareNode::setLEDs(enabled,current);if(fWorld.failLedAfterCommand)throw std::runtime_error("injected LED acknowledgement loss");}
    void blinkLEDs(const Ice::Current &current) override {PrepareNode::blinkLEDs(current);if(fWorld.failLedAfterCommand)throw std::runtime_error("injected LED acknowledgement loss");}
    void flipLED(int led,const Ice::Current &current) override {PrepareNode::flipLED(led,current);if(fWorld.failLedAfterCommand)throw std::runtime_error("injected LED acknowledgement loss");}
    void flipLEDs(const Ice::Current &current) override {PrepareNode::flipLEDs(current);if(fWorld.failLedAfterCommand)throw std::runtime_error("injected LED acknowledgement loss");}
    void pulseLED(int led,int period,const Ice::Current &current) override {PrepareNode::pulseLED(led,period,current);if(fWorld.failLedAfterCommand)throw std::runtime_error("injected LED acknowledgement loss");}
    void stopPeriodicPulser(const Ice::Current &) override {
        fWorld.Record("C"+std::to_string(fId)+".configure-begin");
        if (fWorld.alarmDuringConfigure) fWorld.alarms[0]->sendAsadAlarm("CoBo0",1,Ice::Current());
    }
};
class Link : public get::cobo::MutantLinkManager {
    World &fWorld; unsigned int fId;
public:
    Link(World &w,unsigned int id) : fWorld(w),fId(id) {}
    void setupMutantLink(get::cobo::MutantLinkMode mode,const Ice::Current &) override {
        { std::lock_guard<std::mutex> lock(fWorld.linkMutex); fWorld.modes[fId] = unsigned(mode); }
        ++fWorld.links; fWorld.Record("C"+std::to_string(fId)+".link="+std::to_string(unsigned(mode)));
    }
};
class Alignment : public get::mutant::AlignmentServer {
    World &fWorld; unsigned int fId;
    void Multi(unsigned int step,get::mutant::Clock clock,get::mutant::MultiShelveAlignmentResults &out) {
        ++fWorld.multi; out.measuredDelay = -5; out.programmedDelay = 9; out.comment = "simulated";
        { std::lock_guard<std::mutex> lock(fWorld.m[fId].mutex); fWorld.m[fId].values["GPIO_CLK_TREE/CLOCK_TREE_CONFIG1"] = 0xffffff; }
        fWorld.Record("M"+std::to_string(fId)+".multi="+std::to_string(unsigned(clock))+':'+std::to_string(step));
    }
public:
    Alignment(World &world,unsigned int id) : fWorld(world),fId(id) {}
    void alignDelayTDC(get::mutant::Clock clock,bool verbose,get::mutant::AlignmentResults &out,const Ice::Current &) override {
        Require(!verbose,"Unexpected verbose single-crate mode"); ++fWorld.tdc;
        unsigned int mask = 0;
        { std::lock_guard<std::mutex> lock(fWorld.linkMutex);
          if (fId == 0 && fWorld.modes[0] == unsigned(clock)+2) mask |= 1;
          if (fId == 2 && fWorld.modes[1] == unsigned(clock)+2) mask |= 1<<11; }
        for (unsigned int slot = 0; slot < 12; ++slot) {
            get::mutant::SlotAlignmentResult r; r.isAmcPresent = (mask & (1<<slot)) != 0; r.amcDeltaDelay = 100; r.amcProgDelay = 7;
            out[Ice::Short(slot)] = r;
        }
        { std::lock_guard<std::mutex> lock(fWorld.m[fId].mutex);
          auto &value = fWorld.m[fId].values["GPIO_CLK_TREE/CLOCK_TREE_CONFIG1"];
          const auto shift = 12*unsigned(clock); value = (value&~(UINT64_C(4095)<<shift))|(std::uint64_t(mask)<<shift); }
        fWorld.Record("M"+std::to_string(fId)+".tdc="+std::to_string(unsigned(clock)));
    }
    void multiShelveAlignDelayStep1(get::mutant::Clock c,bool,get::mutant::MultiShelveAlignmentResults &r,const Ice::Current &) override { Multi(1,c,r); }
    void multiShelveAlignDelayStep2(get::mutant::Clock c,bool,get::mutant::MultiShelveAlignmentResults &r,const Ice::Current &) override { Multi(2,c,r); }
    void multiShelveAlignDelayStep3(get::mutant::Clock c,bool,get::mutant::MultiShelveAlignmentResults &r,const Ice::Current &) override { Multi(3,c,r); }
};
inline CoBoConfigureSettings ConfigureSettings()
{
    auto s = GetDAQTest::Configure(9); s.readout.aget.chips.clear(); s.readout.chips.clear();
    for (const auto &entry : GetDAQPrepareFixture::Settings().calibration.chipVersions) {
        CoBoAgetChipSettings a; a.asadId = entry.first/4; a.agetId = entry.first%4;
        a.settings = GetDAQTest::Neutral(); a.settings.chipVersion = entry.second;
        if (entry.second == 0x203) a.settings.globals["Reg1/peackingTime"] = "68";
        s.readout.aget.chips.push_back(a);
        CoBoChipReadout r; r.asadId = a.asadId; r.agetId = a.agetId; s.readout.chips.push_back(r);
    }
    return s;
}
}
#endif
