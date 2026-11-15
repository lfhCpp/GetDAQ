#include "systemacquisition_preflight.h"

#include "hardwareclient.h"

#include <stdexcept>

namespace GetDAQ {
namespace {

HardwareStep Expect(const char *device,const char *reg,const char *field,std::uint64_t value)
{
    HardwareStep step;
    step.kind=HardwareStep::ExpectValue;
    step.device=device;
    step.reg=reg;
    step.field=field;
    step.value=value;
    step.requireBoundDescription=true;
    return step;
}

void RequireExpectedNode(HardwareClient &hardware,const std::string &expected)
{
    if(hardware.Name()!=expected)
        throw std::runtime_error("System run actual node name differs from configured endpoint");
}

}

void RunSystemAcquisitionPreflight(
    const SystemPreparationOptions &options,
    const std::vector<CoBoAcquisitionPlan> &plans,
    const std::vector<std::unique_ptr<HardwareClient>> &cobos,
    const std::vector<std::unique_ptr<HardwareClient>> &mutants,
    DataLinkSetClient &links,
    const SystemAcquisitionPreflightExecutor &execute,
    const SystemAcquisitionPreflightRecorder &record)
{
    if(plans.size()!=options.cobos.size() || cobos.size()!=options.cobos.size() || mutants.size()!=options.mutants.size())
        throw std::logic_error("System acquisition preflight topology differs from its frozen configuration");

    for(std::size_t i=0;i<options.cobos.size();++i) {
        const auto &cobo=options.cobos[i];
        RequireExpectedNode(*cobos[i],cobo.prepare.expectedNodeName);
        if(cobo.enabled) {
            for(const auto *plan:{&plans[i].stopped,&plans[i].runInit,&plans[i].beforeSenderStart,&plans[i].afterSenderStart,
                                  &plans[i].stopPulser,&plans[i].stopFirmware,&plans[i].stopInterrupts,&plans[i].temperatures})
                cobos[i]->Validate(*plan);
            links.Sender(cobo.id).Validate();
        } else {
            cobos[i]->Validate({Expect("ctrl","pipeCtrl","firmwareEnable",0),
                                Expect("ctrl","pipeCtrl","intEnable",0),
                                Expect("ctrl","mutantConfig","initDone",0)});
        }
    }

    for(std::size_t i=0;i<options.mutants.size();++i) {
        const auto &mutant=options.mutants[i];
        RequireExpectedNode(*mutants[i],mutant.endpoint.expectedNodeName);
        const auto init=MutantRunInit(mutant.settings);
        if(!init.empty())mutants[i]->Validate(init);
        mutants[i]->Validate(MutantAcquisitionRun(true));
        mutants[i]->Validate(MutantAcquisitionRun(false));
        if(mutant.settings.readoutEnabled)links.Sender(mutant.endpoint.id).Validate();
    }

    // Validate every later target before observing stopped state. The first RunInit write remains unsent.
    for(std::size_t i=0;i<options.cobos.size();++i) {
        const auto &cobo=options.cobos[i];
        if(cobo.enabled)execute(*cobos[i],plans[i].stopped,cobo.id,"cobo-stopped");
        else execute(*cobos[i],{Expect("ctrl","pipeCtrl","firmwareEnable",0),
                                Expect("ctrl","pipeCtrl","intEnable",0),
                                Expect("ctrl","mutantConfig","initDone",0)},cobo.id,"disabled-cobo-stopped");
    }
    for(std::size_t i=0;i<options.mutants.size();++i)
        execute(*mutants[i],{Expect("CARD_B_EMC","TRIGGER_CTRL","run",0)},options.mutants[i].endpoint.id,"mutant-stopped");

    record("{\"phase\":\"global_preflight_complete\",\"hardware_changed\":false,\"actual_loaded_configuration_verified\":false}");
}

}
