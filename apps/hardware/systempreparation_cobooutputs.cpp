// Selected-board LED and pulser maintenance operations.
#include "systempreparation_p.h"
#include <stdexcept>
#include <algorithm>
#include <limits>
namespace GetDAQ {
using namespace SystemPreparationInternal;
void SystemPreparationClient::SetCoBoLed(const std::string &id,unsigned int asadId,HardwareStep::LedType type,
                                         HardwareStep::LedState ledState,const std::string &newAuditDirectory,
                                         const std::function<void()> &checkpoint)
{
    auto &p=*fImpl;
    std::unique_lock<std::mutex> operation(p.operation,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("System preparation busy; LED command was not queued");
    if (p.state!=State::Prepared || !p.options.exclusiveOwnerConfirmed || asadId>3 ||
        type>HardwareStep::LedD || ledState>HardwareStep::LedFastPulse)
        throw std::logic_error("CoBo LED operation requires known Prepared, owned hardware, and a supported LED selection");
    const auto found=std::find_if(p.options.cobos.begin(),p.options.cobos.end(),[&](const SystemCoBoSettings &c) {return c.id==id;});
    if (found==p.options.cobos.end() || !found->enabled)
        throw std::invalid_argument("CoBo LED operation requires an active target from the original configuration");
    const auto index=std::size_t(found-p.options.cobos.begin());
    if (!p.preparations.at(index) || p.preparations[index]->Status()!=CoBoPrepareClient::Complete)
        throw std::logic_error("CoBo LED operation lost the original prepared board object");
    p.Check();p.state=State::Maintaining;bool hardwareAttempted=false;
    try {
        CreateHardwareAuditDirectory(newAuditDirectory);
        HardwareAudit audit(newAuditDirectory+"/led.jsonl");
        const auto version=found->prepare.settings.monitoring.ledHardwareVersion;
        audit.Append("{\"schema_version\":1,\"phase\":\"intent\",\"operation\":\"set-cobo-led\",\"cobo_id\":"+JsonString(id)+
            ",\"asad_id\":"+std::to_string(asadId)+",\"led_type\":"+std::to_string(unsigned(type))+
            ",\"led_state\":"+std::to_string(unsigned(ledState))+",\"hardware_version\":"+std::to_string(version)+
            ",\"physical_lamp_state_verified\":false,\"automatic_retry\":false}");
        p.ReleasePreflight(false);
        const auto steps=CoBoLedModify(type,asadId,ledState,version);
        p.cobos[index]->Validate(steps);
        if (checkpoint) checkpoint();
        hardwareAttempted=true;
        p.cobos[index]->Execute(steps,newAuditDirectory+"/hardware-led.jsonl",[&p,&checkpoint] {p.Check();if(checkpoint)checkpoint();});
        if (checkpoint) checkpoint();
        audit.Append("{\"phase\":\"complete\",\"hardware_acknowledged\":true,\"hardware_chain_sha256\":"+
            JsonString(p.cobos[index]->CompletedAuditChainSha256())+",\"physical_lamp_state_verified\":false}");
        p.Record("{\"phase\":\"led_maintenance_complete\",\"cobo_id\":"+JsonString(id)+",\"asad_id\":"+std::to_string(asadId)+
            ",\"led_type\":"+std::to_string(unsigned(type))+",\"led_state\":"+std::to_string(unsigned(ledState))+
            ",\"audit_directory\":"+JsonString(newAuditDirectory)+",\"hardware_chain_sha256\":"+JsonString(p.cobos[index]->CompletedAuditChainSha256())+"}");
        p.state=State::Prepared;
    } catch (...) {if(hardwareAttempted)p.MarkCoBoUnknown(index,id);p.Fail();throw;}
}
void SystemPreparationClient::BlinkCoBoLeds(const std::string &id,const std::string &newAuditDirectory,
                                            const std::function<void()> &checkpoint)
{
    auto &p=*fImpl;
    std::unique_lock<std::mutex> operation(p.operation,std::try_to_lock);
    if(!operation.owns_lock())throw std::runtime_error("System preparation busy; LED blink was not queued");
    if(p.state!=State::Prepared || !p.options.exclusiveOwnerConfirmed)
        throw std::logic_error("CoBo LED blink requires known Prepared and owned hardware");
    const auto found=std::find_if(p.options.cobos.begin(),p.options.cobos.end(),[&](const SystemCoBoSettings &c){return c.id==id;});
    if(found==p.options.cobos.end() || !found->enabled)throw std::invalid_argument("CoBo LED blink requires an active target from the original configuration");
    const auto index=std::size_t(found-p.options.cobos.begin());
    if(!p.preparations.at(index) || p.preparations[index]->Status()!=CoBoPrepareClient::Complete)
        throw std::logic_error("CoBo LED blink lost the original prepared board object");
    p.Check();p.state=State::Maintaining;bool hardwareAttempted=false;
    try {
        CreateHardwareAuditDirectory(newAuditDirectory);
        HardwareAudit audit(newAuditDirectory+"/led-blink.jsonl");
        audit.Append("{\"schema_version\":1,\"phase\":\"intent\",\"operation\":\"blink-cobo-leds\",\"cobo_id\":"+JsonString(id)+
            ",\"pattern\":\"sequential-once\",\"physical_lamp_state_verified\":false,\"automatic_retry\":false}");
        p.ReleasePreflight(false);
        const auto steps=CoBoLedBlinkAll();p.cobos[index]->Validate(steps);
        if(checkpoint)checkpoint();
        hardwareAttempted=true;
        p.cobos[index]->Execute(steps,newAuditDirectory+"/hardware-led-blink.jsonl",[&p,&checkpoint]{p.Check();if(checkpoint)checkpoint();});
        if(checkpoint)checkpoint();
        const auto chain=p.cobos[index]->CompletedAuditChainSha256();
        audit.Append("{\"phase\":\"complete\",\"hardware_acknowledged\":true,\"hardware_chain_sha256\":"+JsonString(chain)+
            ",\"physical_lamp_state_verified\":false}");
        p.Record("{\"phase\":\"led_blink_complete\",\"cobo_id\":"+JsonString(id)+",\"pattern\":\"sequential-once\",\"audit_directory\":"+
            JsonString(newAuditDirectory)+",\"hardware_chain_sha256\":"+JsonString(chain)+",\"physical_lamp_state_verified\":false}");
        p.state=State::Prepared;
    } catch(...) {if(hardwareAttempted)p.MarkCoBoUnknown(index,id);p.Fail();throw;}
}
void SystemPreparationClient::FlipCoBoLed(const std::string &id,unsigned int ledId,const std::string &newAuditDirectory,
                                           const std::function<void()> &checkpoint)
{
    auto &p=*fImpl;
    std::unique_lock<std::mutex> operation(p.operation,std::try_to_lock);
    if(!operation.owns_lock())throw std::runtime_error("System preparation busy; LED flip was not queued");
    if(p.state!=State::Prepared || !p.options.exclusiveOwnerConfirmed || ledId>16)
        throw std::logic_error("CoBo LED flip requires known Prepared, owned hardware, and selection 0..15 or 16 for all");
    const auto found=std::find_if(p.options.cobos.begin(),p.options.cobos.end(),[&](const SystemCoBoSettings &c){return c.id==id;});
    if(found==p.options.cobos.end() || !found->enabled)throw std::invalid_argument("CoBo LED flip requires an active target from the original configuration");
    const auto index=std::size_t(found-p.options.cobos.begin());
    if(!p.preparations.at(index) || p.preparations[index]->Status()!=CoBoPrepareClient::Complete)
        throw std::logic_error("CoBo LED flip lost the original prepared board object");
    p.Check();p.state=State::Maintaining;bool hardwareAttempted=false;
    try {
        CreateHardwareAuditDirectory(newAuditDirectory);
        HardwareAudit audit(newAuditDirectory+"/led-flip.jsonl");
        audit.Append("{\"schema_version\":1,\"phase\":\"intent\",\"operation\":\"flip-cobo-led\",\"cobo_id\":"+JsonString(id)+
            ",\"led_id\":"+std::to_string(ledId)+",\"selection\":"+JsonString(ledId==16 ? "all" : "single")+
            ",\"physical_lamp_state_verified\":false,\"automatic_retry\":false}");
        p.ReleasePreflight(false);
        const auto steps=CoBoLedFlip(ledId);p.cobos[index]->Validate(steps);
        if(checkpoint)checkpoint();
        hardwareAttempted=true;
        p.cobos[index]->Execute(steps,newAuditDirectory+"/hardware-led-flip.jsonl",[&p,&checkpoint]{p.Check();if(checkpoint)checkpoint();});
        if(checkpoint)checkpoint();
        const auto chain=p.cobos[index]->CompletedAuditChainSha256();
        audit.Append("{\"phase\":\"complete\",\"hardware_acknowledged\":true,\"hardware_chain_sha256\":"+JsonString(chain)+
            ",\"physical_lamp_state_verified\":false}");
        p.Record("{\"phase\":\"led_flip_complete\",\"cobo_id\":"+JsonString(id)+",\"led_id\":"+std::to_string(ledId)+
            ",\"audit_directory\":"+JsonString(newAuditDirectory)+",\"hardware_chain_sha256\":"+JsonString(chain)+
            ",\"physical_lamp_state_verified\":false}");
        p.state=State::Prepared;
    } catch(...) {if(hardwareAttempted)p.MarkCoBoUnknown(index,id);p.Fail();throw;}
}
void SystemPreparationClient::PulseCoBoLed(const std::string &id,unsigned int ledId,unsigned int periodMs,const std::string &newAuditDirectory,
                                            const std::function<void()> &checkpoint)
{
    auto &p=*fImpl;
    std::unique_lock<std::mutex> operation(p.operation,std::try_to_lock);
    if(!operation.owns_lock())throw std::runtime_error("System preparation busy; LED pulse was not queued");
    if(p.state!=State::Prepared || !p.options.exclusiveOwnerConfirmed || ledId>16 || periodMs>unsigned(std::numeric_limits<int>::max()))
        throw std::logic_error("CoBo LED pulse requires known Prepared, owned hardware, LED 0..15 or all 16, and an int-range half-period");
    const auto found=std::find_if(p.options.cobos.begin(),p.options.cobos.end(),[&](const SystemCoBoSettings &c){return c.id==id;});
    if(found==p.options.cobos.end() || !found->enabled)throw std::invalid_argument("CoBo LED pulse requires an active target from the original configuration");
    const auto index=std::size_t(found-p.options.cobos.begin());
    if(!p.preparations.at(index) || p.preparations[index]->Status()!=CoBoPrepareClient::Complete)
        throw std::logic_error("CoBo LED pulse lost the original prepared board object");
    p.Check();p.state=State::Maintaining;bool hardwareAttempted=false;
    try {
        CreateHardwareAuditDirectory(newAuditDirectory);
        HardwareAudit audit(newAuditDirectory+"/led-pulse.jsonl");
        audit.Append("{\"schema_version\":1,\"phase\":\"intent\",\"operation\":\"pulse-cobo-led\",\"cobo_id\":"+JsonString(id)+
            ",\"led_id\":"+std::to_string(ledId)+",\"period_ms\":"+std::to_string(periodMs)+
            ",\"period_semantics\":\"half-period-ms\",\"zero_stops_selected_pulser\":true,\"physical_lamp_state_verified\":false,\"automatic_retry\":false}");
        p.ReleasePreflight(false);
        const auto steps=CoBoLedPulse(ledId,periodMs);p.cobos[index]->Validate(steps);
        if(checkpoint)checkpoint();
        hardwareAttempted=true;
        p.cobos[index]->Execute(steps,newAuditDirectory+"/hardware-led-pulse.jsonl",[&p,&checkpoint]{p.Check();if(checkpoint)checkpoint();});
        if(checkpoint)checkpoint();
        const auto chain=p.cobos[index]->CompletedAuditChainSha256();
        audit.Append("{\"phase\":\"complete\",\"hardware_acknowledged\":true,\"hardware_chain_sha256\":"+JsonString(chain)+
            ",\"physical_lamp_state_verified\":false}");
        p.Record("{\"phase\":\"led_pulse_complete\",\"cobo_id\":"+JsonString(id)+",\"led_id\":"+std::to_string(ledId)+
            ",\"period_ms\":"+std::to_string(periodMs)+",\"audit_directory\":"+JsonString(newAuditDirectory)+
            ",\"hardware_chain_sha256\":"+JsonString(chain)+",\"physical_lamp_state_verified\":false}");
        p.state=State::Prepared;
    } catch(...) {if(hardwareAttempted)p.MarkCoBoUnknown(index,id);p.Fail();throw;}
}
void SystemPreparationClient::TriggerAsAdPulser(const std::string &id,int voltageMv,const std::string &newAuditDirectory,
                                                const std::function<void()> &checkpoint)
{
    auto &p=*fImpl;
    std::unique_lock<std::mutex> operation(p.operation,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("System preparation busy; pulser trigger was not queued");
    if (p.state!=State::Prepared || !p.options.exclusiveOwnerConfirmed || voltageMv < -1000 || voltageMv > 1000)
        throw std::logic_error("Single pulser trigger requires known Prepared, owned hardware, and voltage within -1000..1000 mV");
    const auto found=std::find_if(p.options.cobos.begin(),p.options.cobos.end(),[&](const SystemCoBoSettings &c) {return c.id==id;});
    if (found==p.options.cobos.end() || !found->enabled)
        throw std::invalid_argument("Pulser trigger requires an active target from the original configuration");
    const auto index=std::size_t(found-p.options.cobos.begin());
    if (!p.preparations.at(index) || p.preparations[index]->Status()!=CoBoPrepareClient::Complete)
        throw std::logic_error("Pulser trigger lost the original prepared board object");
    p.Check();p.state=State::Maintaining;bool hardwareAttempted=false;
    try {
        CreateHardwareAuditDirectory(newAuditDirectory);
        HardwareAudit audit(newAuditDirectory+"/pulser.jsonl");
        audit.Append("{\"schema_version\":1,\"phase\":\"intent\",\"operation\":\"trigger-asad-pulser\",\"cobo_id\":"+JsonString(id)+
            ",\"voltage_mv\":"+std::to_string(voltageMv)+",\"physical_pulse_verified\":false,\"automatic_retry\":false}");
        p.ReleasePreflight(false);
        HardwareStep step;step.kind=HardwareStep::PulserCommand;step.pulserOperation=HardwareStep::TriggerPulser;
        step.voltageMv=voltageMv;step.requireBoundDescription=true;
        const std::vector<HardwareStep> steps(1,step);
        p.cobos[index]->Validate(steps);
        if (checkpoint) checkpoint();
        hardwareAttempted=true;
        p.cobos[index]->Execute(steps,newAuditDirectory+"/hardware-pulser.jsonl",[&p,&checkpoint] {p.Check();if(checkpoint)checkpoint();});
        if (checkpoint) checkpoint();
        audit.Append("{\"phase\":\"complete\",\"hardware_acknowledged\":true,\"hardware_chain_sha256\":"+
            JsonString(p.cobos[index]->CompletedAuditChainSha256())+",\"physical_pulse_verified\":false}");
        p.Record("{\"phase\":\"pulser_maintenance_complete\",\"cobo_id\":"+JsonString(id)+",\"voltage_mv\":"+std::to_string(voltageMv)+
            ",\"audit_directory\":"+JsonString(newAuditDirectory)+",\"hardware_chain_sha256\":"+JsonString(p.cobos[index]->CompletedAuditChainSha256())+"}");
        p.state=State::Prepared;
    } catch (...) {if(hardwareAttempted){p.MarkCoBoUnknown(index,id);p.Fail();}else p.state=State::Prepared;throw;}
}
}
