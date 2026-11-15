#include "nativeeccpreflight.h"
#include "acquisitionplans.h"
#include <Ice/Ice.h>
#include <get/cobo/CtrlNode.h>
#include <set>
#include <stdexcept>
namespace GetDAQ {
namespace {
HardwareStep Bit(const char *field,bool value) {
    HardwareStep s; s.device="ctrl"; s.reg="mutantConfig"; s.field=field; s.value=value;
    s.requireBoundDescription=true; return s;
}
void Inventory(HardwareClient &hw,const HardwareDescription &d,const std::string &name) {
    hw.UseDescription(d);
    if (hw.Name()!=name) throw std::runtime_error("Native ECC actual node name differs");
    const auto actual=hw.Devices(); const std::set<std::string> names(actual.begin(),actual.end());
    if (names.size()!=actual.size() || names.size()!=d.devices.size()) throw std::runtime_error("Native ECC actual device inventory differs");
    for (const auto &dev:d.devices) {
        if (!names.count(dev.name)) throw std::runtime_error("Native ECC actual device is missing");
        hw.Describe(dev.name);
    }
}
struct Parser {
    Ice::CommunicatorPtr communicator;
    Parser() {
        Ice::InitializationData init; init.properties=Ice::createProperties(); init.properties->setProperty("Ice.RetryIntervals","-1");
        communicator=Ice::initialize(init);
    }
    ~Parser() { try { communicator->destroy(); } catch (...) {} }
    Ice::ObjectPrx Proxy(const std::string &text,int minor,int timeout) {
        Ice::EncodingVersion encoding; encoding.major=1; encoding.minor=minor;
        return communicator->stringToProxy(text)->ice_twoway()->ice_collocationOptimized(false)->ice_timeout(timeout)->ice_encodingVersion(encoding);
    }
};
void AlarmService(Parser &parser,const CoBoPrepareOptions &o,int minor,int timeout) {
    const auto proxy=parser.Proxy(o.nodeProxy,minor,timeout);
    if (!proxy->ice_isA(get::cobo::CtrlNode::ice_staticId())) throw std::runtime_error("Native ECC CoBo CtrlNode interface missing");
    const auto node=get::cobo::CtrlNodePrx::uncheckedCast(proxy);
    if (node->name()!=o.expectedNodeName) throw std::runtime_error("Native ECC alarm node name differs");
    const auto service=node->getAlarmService();
    if (!service || !service->ice_twoway()->ice_collocationOptimized(false)->ice_timeout(timeout)->ice_encodingVersion(proxy->ice_getEncodingVersion())->ice_isA(get::mt::AlarmService::ice_staticId()))
        throw std::runtime_error("Native ECC actual AlarmService interface missing");
}
void Observe(HardwareClient &hw,const std::vector<HardwareStep> &steps) {
    for (const auto &s:steps) {
        if (s.kind!=HardwareStep::ExpectValue) throw std::logic_error("Non-observation in native ECC preflight");
        if (hw.Read(s.device,s.reg,s.field)!=s.value) throw std::runtime_error("Native ECC stopped/configuration observation mismatch");
    }
}
}
void ValidateNativeEccHardware(const SystemPreparationOptions &o,const std::vector<DataSenderOptions> &senders,
                               EccCommand command,EccState from,SystemPreparationClient *prepared,
                               const std::string &maintenanceNode,unsigned int asadId,unsigned int ledType,unsigned int ledState,int pulserVoltageMv,unsigned int ledFlipId,unsigned int ledPulseId,unsigned int ledPulsePeriodMs) {
    const bool preparing=command==EccCommand::Prepare, configuring=command==EccCommand::Configure;
    const bool configuringOne=command==EccCommand::ConfigureCoBo;
    const bool starting=command==EccCommand::Start || command==EccCommand::Resume;
    const bool resetting=command==EccCommand::Reset, breaking=command==EccCommand::Breakup;
    const bool maintaining=command==EccCommand::ReprepareCoBo || configuringOne || command==EccCommand::SetCoBoLed || command==EccCommand::TriggerAsAdPulser || command==EccCommand::BlinkCoBoLeds || command==EccCommand::FlipCoBoLed || command==EccCommand::PulseCoBoLed;
    const bool stopping=command==EccCommand::Pause || command==EccCommand::Stop || breaking;
    std::vector<std::unique_ptr<HardwareClient>> cobos,mutants;
    Parser parser;
    if (preparing && !o.mutants.empty()) {
        SystemAlignmentOptions a; a.encodingMinor=o.encodingMinor; a.timeoutMs=o.timeoutMs;
        a.exclusiveOwnerConfirmed=o.exclusiveOwnerConfirmed; a.daqStoppedConfirmed=o.dataProcessesStopped;
        for (const auto &c:o.cobos) {
            AlignmentCoBoEndpoint e; e.id=c.id; e.nodeProxy=c.prepare.nodeProxy; e.linkProxy=c.linkProxy;
            e.expectedNodeName=c.prepare.expectedNodeName; e.description=c.prepare.description; e.enabled=c.enabled; a.cobos.push_back(e);
        }
        for (const auto &m:o.mutants) a.mutants.push_back(m.endpoint);
        SystemAlignmentClient alignment(a); alignment.ValidateInterfaces();
    }
    // 只检查实际结构/方法，不执行编译计划；全节点元数据通过后才观察停止值。
    for (const auto &c:o.cobos) {
        std::unique_ptr<HardwareClient> hw(new HardwareClient(c.prepare.nodeProxy,o.encodingMinor,o.timeoutMs));
        Inventory(*hw,c.prepare.description,c.prepare.expectedNodeName);
        hw->Validate({Bit("initDone",false),Bit("mutantMode",false)});
        if (c.enabled) {
            if(command==EccCommand::SetCoBoLed && c.id==maintenanceNode) {
                if(asadId>3 || ledType>unsigned(HardwareStep::LedD) || ledState>unsigned(HardwareStep::LedFastPulse))
                    throw std::invalid_argument("Invalid selected CoBo LED operation");
                hw->Validate(CoBoLedModify(HardwareStep::LedType(ledType),asadId,HardwareStep::LedState(ledState),c.prepare.settings.monitoring.ledHardwareVersion));
            }
            if(command==EccCommand::BlinkCoBoLeds && c.id==maintenanceNode)hw->Validate(CoBoLedBlinkAll());
            if(command==EccCommand::FlipCoBoLed && c.id==maintenanceNode)hw->Validate(CoBoLedFlip(ledFlipId));
            if(command==EccCommand::PulseCoBoLed && c.id==maintenanceNode)hw->Validate(CoBoLedPulse(ledPulseId,ledPulsePeriodMs));
            if(command==EccCommand::TriggerAsAdPulser && c.id==maintenanceNode) {
                if(pulserVoltageMv < -1000 || pulserVoltageMv > 1000) throw std::invalid_argument("Invalid single pulser voltage");
                HardwareStep step;step.kind=HardwareStep::PulserCommand;step.pulserOperation=HardwareStep::TriggerPulser;
                step.voltageMv=pulserVoltageMv;step.requireBoundDescription=true;
                hw->Validate({step});
            }
            if (preparing) {
                const auto p=BuildCoBoPreparePlan(c.prepare.settings);
                hw->Validate(p.beforeSubscription); hw->Validate(p.afterSubscription);
                if (c.prepare.settings.monitoring.subscribeAlarms) AlarmService(parser,c.prepare,o.encodingMinor,o.timeoutMs);
            }
            if (preparing || configuring || (configuringOne && c.id==maintenanceNode)) hw->Validate(CoBoConfigure(c.configure));
            if (preparing || configuring) hw->Validate({Bit("l2Trigger",false)});
            if (preparing || configuring || starting || stopping) {
                const auto p=BuildCoBoAcquisitionPlan(c.configure);
                hw->Validate(p.stopPulser); hw->Validate(p.stopFirmware); hw->Validate(p.stopInterrupts);
                if (preparing || configuring || starting) {
                    hw->Validate(p.stopped); hw->Validate(p.runInit); hw->Validate(p.beforeSenderStart); hw->Validate(p.afterSenderStart);
                }
            }
        }
        if (resetting && from==EccState::Prepared) {
            hw->Validate(CoBoUnprepareMonitoring()); hw->Validate(CoBoAsAdPowerOff());
            auto *alarm=prepared ? prepared->Alarms(c.id) : nullptr;
            if (alarm) {
                const auto s=alarm->Snapshot().subscription;
                if (s==CoBoAlarmSnapshot::Active) alarm->ValidateUnsubscribe();
                else if (s!=CoBoAlarmSnapshot::Closed) throw std::runtime_error("Native ECC owned alarm subscription is incomplete/unknown");
            }
        }
        cobos.push_back(std::move(hw));
    }
    for (const auto &m:o.mutants) {
        std::unique_ptr<HardwareClient> hw(new HardwareClient(m.endpoint.nodeProxy,o.encodingMinor,o.timeoutMs));
        Inventory(*hw,m.endpoint.description,m.endpoint.expectedNodeName);
        if (preparing) hw->Validate(MutantPrepare(m.settings));
        if (preparing || configuring) {
            hw->Validate(MutantConfigure(m.settings)); hw->Validate(MutantWscaClock(false)); hw->Validate(MutantWscaClock(true));
            hw->Validate(MutantClockShelfMasks(0,0)); hw->Validate(MutantSyncPulse(true)); hw->Validate(MutantSyncPulse(false));
        }
        if (preparing || configuring || starting) {
            const auto init=MutantRunInit(m.settings); if (!init.empty()) hw->Validate(init);
        }
        if (!resetting) { hw->Validate(MutantAcquisitionRun(true)); hw->Validate(MutantAcquisitionRun(false)); }
        mutants.push_back(std::move(hw));
    }
    if (preparing || configuring || starting || stopping) for (const auto &s:senders) { DataSenderClient sender(s); sender.Validate(); }
    const bool stopped=preparing || configuring || maintaining || starting || resetting || (breaking && from!=EccState::Running) || (command==EccCommand::Stop && from==EccState::Paused);
    if (stopped) {
        for (std::size_t i=0;i<o.cobos.size();++i) {
            auto &hw=*cobos[i]; const auto &c=o.cobos[i];
            if (hw.Read("ctrl","pipeCtrl","firmwareEnable") || hw.Read("ctrl","pipeCtrl","intEnable")) throw std::runtime_error("Native ECC observed CoBo production is not stopped");
            if (hw.Read("ctrl","mutantConfig","initDone")!=std::uint64_t(c.enabled && (starting || breaking || command==EccCommand::Stop)))
                throw std::runtime_error("Native ECC observed CoBo readiness differs");
            if (starting && c.enabled) Observe(hw,BuildCoBoAcquisitionPlan(c.configure).stopped);
        }
        for (const auto &hw:mutants) if (hw->Read("CARD_B_EMC","TRIGGER_CTRL","run")) throw std::runtime_error("Native ECC observed MuTaNT production is not stopped");
    }
}
}
