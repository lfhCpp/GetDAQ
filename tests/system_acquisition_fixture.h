#ifndef GETDAQ_TEST_SYSTEM_ACQUISITION_FIXTURE_H
#define GETDAQ_TEST_SYSTEM_ACQUISITION_FIXTURE_H
#include "systemacquisition.h"
#include "acquisitionplans.h"
#include "system_preparation_fixture.h"
#include <mdaq/DaqControl.h>
#include <csignal>
namespace GetDAQAcquisitionFixture {
using namespace GetDAQSystemFixture;
class Sender : public mdaq::DaqCtrlNode {
    World &w; std::string id;
    void Record(const char *s) { w.Record(id+".sender-"+s); }
public:
    std::atomic<bool> valid{true}, failStart{false}, failStop{false}, failDisconnect{false};
    std::string expectedProtocol="TCP";
    explicit Sender(World &world,const std::string &name) : w(world),id(name) {}
    bool ice_isA(const std::string &type,const Ice::Current &c) const override { return valid && mdaq::DaqCtrlNode::ice_isA(type,c); }
    void connect(const std::string &type,const std::string &,const Ice::Current &) override { Require(type==expectedProtocol,"Unexpected transport"); Record("connect"); }
    void setCircularBuffersEnabled(int value,const Ice::Current &) override { w.Record(id+".sender-mask="+std::to_string(value)); }
    void setAlwaysFlushData(bool value,const Ice::Current &) override { w.Record(id+".sender-flush="+std::to_string(value)); }
    void daqStart(const Ice::Current &c) override { Record("start"); if (failStart) c.con->close(true); }
    void daqStop(const Ice::Current &c) override { Record("stop"); if (failStop) c.con->close(true); }
    void disconnect(const Ice::Current &c) override { Record("disconnect"); if (failDisconnect) c.con->close(true); }
    void shutdown(const Ice::Current &) override { throw mdaq::utl::CmdException("Forbidden fixture shutdown"); }
};
class AcquisitionNode : public ConfigureNode {
    World &w; unsigned int id;
public:
    bool failPulserStop = false, failPulserStart = false;
    AcquisitionNode(World &world,unsigned int i) : ConfigureNode(*world.alarms[i],world,i),w(world),id(i) {}
    void startPeriodicPulser(const Ice::Current &c) override { w.Record("C"+std::to_string(id)+".pulser-start"); if (failPulserStart) c.con->close(true); }
    void stopPeriodicPulser(const Ice::Current &) override {
        w.Record("C"+std::to_string(id)+".pulser-stop");
        if (failPulserStop) throw mdaq::utl::CmdException("Fixture pulser stop failed");
    }
};
class AcquisitionDevice : public GetDAQPrepareFixture::PrepareDevice {
public:
    std::function<void()> beforeReadField;
    bool failFirmwareStop = false, failPowerOff = false, failStatusClear = false, failMonitoring = false;
    Ice::Long monitoringStatus = 0;
    AcquisitionDevice(const DeviceInfo &d,GetDAQPrepareFixture::PrepareFixtureState &s) : PrepareDevice(d,s) {}
    Ice::Long readField(const std::string &reg,const std::string &field,const Ice::Current &c) override {
        if (beforeReadField) beforeReadField();
        return PrepareDevice::readField(reg,field,c);
    }
    Ice::Long readRegister(const std::string &reg,const Ice::Current &c) override {
        if (fConfig.descr.name=="asad" && reg=="interruptStatus1") {
            if (failMonitoring) throw mdaq::utl::CmdException("Fixture monitoring observation failed");
            return monitoringStatus;
        }
        return PrepareDevice::readRegister(reg,c);
    }
    void writeRegister(const std::string &reg,Ice::Long value,const Ice::Current &c) override {
        PrepareDevice::writeRegister(reg,value,c);
        if (failPowerOff && fConfig.descr.name=="ctrl" && reg=="asadEnable" && value==0) c.con->close(true);
    }
    void writeField(const std::string &reg,const std::string &field,Ice::Long value,const Ice::Current &c) override {
        PrepareDevice::writeField(reg,field,value,c);
        if (failFirmwareStop && reg=="pipeCtrl" && field=="firmwareEnable" && value==0) c.con->close(true);
        if (failStatusClear && reg=="mutantConfig" && field=="initDone" && value==0) c.con->close(true);
    }
};
struct Rig {
    World w;
    SystemPreparationOptions options;
    std::vector<DataSenderOptions> senders;
    std::array<IceUtil::Handle<AcquisitionNode>,3> nodes;
    std::array<std::map<std::string,IceUtil::Handle<AcquisitionDevice>>,3> devices;
    std::map<std::string,IceUtil::Handle<Sender>> sender;
    std::array<IceUtil::Handle<GetDAQTest::TestNode>,3> mutants;
    Rig(Ice::CommunicatorPtr communicator,Ice::ObjectAdapterPtr adapter,const HardwareDescription &cobo,const HardwareDescription &mutant,int minor) {
        options.encodingMinor=minor; options.timeoutMs=1000; options.exclusiveOwnerConfirmed=options.dataProcessesStopped=true;
        for (unsigned int i=0;i<3;++i) {
            w.m[i].onAction=[this,i](const std::string &s) { w.Record("M"+std::to_string(i)+'.'+s); };
            mutants[i]=new GetDAQTest::TestNode; mutants[i]->fName="MuTaNT"+std::to_string(i);
            for (const auto &d:mutant.devices) mutants[i]->fDevices[d.name]=mdaq::hw::DevicePrx::uncheckedCast(adapter->addWithUUID(new GetDAQMutantFixture::MutantDevice(d,w.m[i])));
            SystemMutantSettings m; m.endpoint.id="M"+std::to_string(i); m.endpoint.expectedNodeName=mutants[i]->fName;
            m.endpoint.nodeProxy=communicator->proxyToString(adapter->addWithUUID(mutants[i])); m.endpoint.master=i==0;
            m.endpoint.alignmentProxy=communicator->proxyToString(adapter->addWithUUID(new Alignment(w,i))); m.endpoint.description=mutant;
            m.settings.isSlave=i!=0; m.settings.slaveId=i==2; m.settings.slavePortA=i!=2; m.settings.readoutEnabled=i!=2;
            if (i) m.settings.level0Enabled=m.settings.level1Enabled=m.settings.level2Enabled=false;
            options.mutants.push_back(m);
        }
        for (unsigned int i=0;i<3;++i) {
            auto &s=w.c[i]; for (const auto &d:cobo.devices) s.descriptions[d.name]=d;
            s.onAction=[this,i](const std::string &action) { w.Record("C"+std::to_string(i)+'.'+action); };
            w.alarms[i]=new GetDAQPrepareFixture::PrepareAlarmService; nodes[i]=new AcquisitionNode(w,i);
            nodes[i]->service=get::mt::AlarmServicePrx::uncheckedCast(adapter->addWithUUID(w.alarms[i]));
            for (const auto &d:cobo.devices) {
                devices[i][d.name]=new AcquisitionDevice(d,s);
                nodes[i]->fDevices[d.name]=mdaq::hw::DevicePrx::uncheckedCast(adapter->addWithUUID(devices[i][d.name]));
            }
            SystemCoBoSettings c; c.id="C"+std::to_string(i); c.enabled=i!=2;
            c.prepare.nodeProxy=communicator->proxyToString(adapter->addWithUUID(nodes[i])); c.prepare.expectedNodeName=nodes[i]->fName;
            c.linkProxy=communicator->proxyToString(adapter->addWithUUID(new Link(w,i))); c.prepare.description=cobo;
            c.prepare.settings=GetDAQPrepareFixture::Settings(); c.prepare.callbackEndpoints="tcp -h 127.0.0.1 -p 0";
            c.configure=ConfigureSettings(); c.configure.asad.generator["periodicModeEnabled"]=i==0 ? "true" : "false";
            c.configure.readout.module["useExternalTimeReset"]=i==1 ? "true" : "false"; options.cobos.push_back(c);
        }
        for (const auto &id:{"C0","C1","M0","M1"}) {
            sender[id]=new Sender(w,id); DataSenderOptions s; s.senderId=id;
            s.controlProxy=communicator->proxyToString(adapter->addWithUUID(sender[id]));
            s.receiverAddress="127.0.0.1:"+std::to_string(47000+senders.size()); s.encodingMinor=minor;
            s.circularBufferMask=id[0]=='C' ? 9 : 1; s.exclusiveStoppedOwnerConfirmed=true; s.timeoutMs=1000; senders.push_back(s);
        }
        // Deliberately reverse topology order: coordinator must use configured IDs/order.
        std::reverse(senders.begin(),senders.end()); w.Reset();
    }
    std::vector<std::string> Actions() { std::lock_guard<std::mutex> lock(w.logMutex); return w.actions; }
    void Clear() { std::lock_guard<std::mutex> lock(w.logMutex); w.actions.clear(); }
    unsigned int Count(const std::string &s) { const auto a=Actions(); return unsigned(std::count(a.begin(),a.end(),s)); }
    bool Has(const std::string &s) { return Count(s)!=0; }
    void Before(const std::string &a,const std::string &b) { Require(w.Find(a)<w.Find(b),"Run action order differs"); }
    void Baseline() {
        // Test-only independent board reconciliation, never presented as a production recovery API.
        for (auto &s:w.c) { std::lock_guard<std::mutex> lock(s.mutex); s.Set("pipeCtrl","firmwareEnable",0); s.Set("pipeCtrl","intEnable",0); s.mode=0; }
        for (std::size_t i=0;i<w.m.size();++i) {
            auto &s=w.m[i]; std::lock_guard<std::mutex> lock(s.mutex);
            for (const auto &d:options.mutants[i].endpoint.description.devices) if (d.name=="CARD_B_EMC")
                for (const auto &reg:d.registers) if (reg.name=="TRIGGER_CTRL")
                    for (const auto &field:reg.fields) if (field.name=="run") s.values["CARD_B_EMC/TRIGGER_CTRL"]&=~(UINT64_C(1)<<field.offset);
        }
        for (auto &n:nodes) n->failPulserStart=n->failPulserStop=false;
        for (auto &d:devices) d.at("ctrl")->failFirmwareStop=false;
        for (auto &s:sender) { s.second->valid=true; s.second->failStart=s.second->failStop=s.second->failDisconnect=false; }
        Clear();
    }
    void StartOrder(bool resume) {
        const auto &a=Actions();
        Require(!Has("C2.sender-start") && !Has("M1.sender-start") && !Has("M2.sender-start"),"Inactive/slave sender started");
        Require(Count("C0.ctrl.resetTime=0")==unsigned(!resume) && !Has("C1.ctrl.resetTime=0"),"Time reset policy/resume reset differs");
        for (const auto &id:{"C0","C1"}) {
            const auto prefix=std::string(id)+'.';
            Before(prefix+"ctrl.pipeCtrl.reset=1",prefix+"sender-start");
            Before(prefix+"sender-start",prefix+"ctrl.pipeCtrl.firmwareEnable=1");
            Before(prefix+"ctrl.pipeCtrl.firmwareEnable=1",prefix+"ctrl.agetController.reset=1");
            Before(prefix+"ctrl.aget_sc_configuration.pulseEnable=0","M1.CARD_B_EMC/TRIGGER_CTRL/run=1");
            const auto image=CompileCoBoConfigure(options.cobos[id[1]-'0'].configure);
            for (unsigned int j=0;j<4;++j) Require(Has(prefix+"ctrl.readPtr_asad"+std::to_string(j)+'='+std::to_string(image.readout.bufferStart[j])),"Read pointer not reset to immutable configured buffer start");
        }
        Before("C0.ctrl.aget_sc_configuration.pulseEnable=0","C0.pulser-start");
        Before("C0.pulser-start","C1.sender-start");
        Before("M1.CARD_B_EMC/TRIGGER_CTRL/run=1","M2.CARD_B_EMC/TRIGGER_CTRL/run=1");
        Before("M2.CARD_B_EMC/TRIGGER_CTRL/run=1","M0.sender-start"); Before("M0.sender-start","M0.CARD_B_EMC/TRIGGER_CTRL/run=1");
        bool counterReset=false; for (const auto &s:a) if (s.find("/TRIGGER_CMD/")!=std::string::npos || s.find("/SCALER_CTRL/cs")!=std::string::npos) counterReset=true;
        Require(counterReset!=resume,"MuTaNT RunInit repeated on resume or omitted on first start");
    }
    void StopOrder(bool retainConnection=true) {
        Before("M0.CARD_B_EMC/TRIGGER_CTRL/run=0","M1.CARD_B_EMC/TRIGGER_CTRL/run=0");
        Before("M1.CARD_B_EMC/TRIGGER_CTRL/run=0","M2.CARD_B_EMC/TRIGGER_CTRL/run=0");
        Before("M2.CARD_B_EMC/TRIGGER_CTRL/run=0","C0.pulser-stop");
        Before("C0.pulser-stop","C0.ctrl.pipeCtrl.firmwareEnable=0");
        Before("C1.ctrl.pipeCtrl.intEnable=0","M0.sender-stop");
        Before("M0.sender-stop","C0.sender-stop"); Before("C0.sender-stop","C1.sender-stop");
        if (retainConnection) for (const auto &s:Actions()) Require(s.find("sender-disconnect")==std::string::npos,"Stop closed retained TCP connection");
    }
    void Index(std::ofstream &index,const std::string &directory,const char *op) {
        if (!index.is_open()) return;
        unsigned int active=0;
        for (std::size_t i=0;i<options.cobos.size();++i) if (options.cobos[i].enabled) active|=1U<<i;
        index<<"{\"directory\":"<<JsonString(directory)<<",\"operation\":"<<JsonString(op)<<",\"cobos\":"<<options.cobos.size()<<",\"mutants\":"<<options.mutants.size()<<",\"active\":"<<active<<",\"actions\":[";
        const auto actions=Actions(); for (std::size_t i=0;i<actions.size();++i) { if (i) index<<','; index<<JsonString(actions[i]); }
        index<<"],\"boards\":[";
        for (std::size_t i=0;i<options.cobos.size();++i) {
            if (i) index<<',';
            const auto &s=options.cobos[i].configure;
            index<<"{\"enabled\":"<<(options.cobos[i].enabled ? "true" : "false")<<",\"external\":"<<(i==1 ? "true" : "false")
                 <<",\"periodic\":"<<(s.asad.generator.at("periodicModeEnabled")=="true" ? "true" : "false")<<",\"shared\":"<<(s.readout.bufferLayout==CoBoReadoutSettings::SharedBuffer ? "true" : "false")
                 <<",\"aget_mask\":"<<w.c[i].Get("aget_sc_configuration","SC_En")<<",\"mem_start\":[";
            for (unsigned int j=0;j<4;++j) { if (j) index<<','; index<<w.c[i].ctrl[s.readout.bufferLayout==CoBoReadoutSettings::SharedBuffer ? "memStart" : "memStart_asad"+std::to_string(j)]; }
            index<<"]}";
        }
        const bool trigger=options.mutants.empty() || options.mutants[0].settings.level0Enabled || options.mutants[0].settings.level1Enabled || options.mutants[0].settings.level2Enabled;
        index<<"],\"master_trigger\":"<<(trigger ? "true" : "false")<<",\"master_init\":"<<(options.mutants.empty() || options.mutants[0].settings.clearScalers || options.mutants[0].settings.clearCounters ? "true" : "false")<<"}\n";
        index.flush(); Require(bool(index),"Save acquisition evidence index");
    }
};
}
#endif
