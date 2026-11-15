// Recovery and release operations are kept separate from initial preparation.
#include "systempreparation_p.h"
#include "systempreparationreleaseaudit_p.h"
#include "hardwareaudit.h"
#include "cobolifecycle.h"
#include "cobolifecyclejournal.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace GetDAQ {
using namespace SystemPreparationInternal;
void SystemPreparationClient::StopProductionAfterFailure(const std::string &path,bool parentUnknown) {
    auto &p=*fImpl; std::lock_guard<std::mutex> lock(p.operation);
    const auto from=p.state.load();
    if (from!=State::Failed && !(parentUnknown && (from==State::Prepared || from==State::Configured)))
        throw std::logic_error("Preparation production stop requires Failed or a confirmed UNKNOWN parent preparation");
    if (!p.options.exclusiveOwnerConfirmed)
        throw std::logic_error("Preparation production stop requires retained exclusive owner declaration");
    p.ValidateIdentities(); // 非法别名不能把同一板的停止当作多个独立目标。
    CreateHardwareAuditDirectory(path); SystemPreparationReleaseAudit audit(path,"stop-production-after-preparation-failure",p.options);
    p.state=State::Failed;
    audit.Record("{\"phase\":\"negative_stop_context\",\"parent_ecc_unknown_confirmed\":"+std::string(parentUnknown ? "true" : "false")+
        ",\"previous_preparation_state\":"+std::to_string(unsigned(from))+",\"preparation_directory\":"+JsonString(p.directory)+"}");
    p.stopCobos.resize(p.options.cobos.size()); p.stopMutants.resize(p.options.mutants.size());
    bool complete=true;
    const auto stop=[&](std::unique_ptr<HardwareClient> &hw,const std::string &proxy,const HardwareDescription &description,
                        const std::string &name,const std::string &id,const char *stage,const std::vector<HardwareStep> &steps) {
        const auto key=std::make_pair(id,std::string(stage)); const auto earlier=p.productionStops.find(key);
        if (earlier!=p.productionStops.end()) {
            complete=complete && earlier->second;
            audit.Record("{\"phase\":\"stop_not_repeated\",\"id\":"+JsonString(id)+",\"stage\":"+JsonString(stage)+
                ",\"previously_confirmed\":"+(earlier->second ? "true" : "false")+"}");
            return;
        }
        const auto file=audit.Begin(id,stage); p.productionStops.emplace(key,false);
        try {
            if (!hw) { hw.reset(new HardwareClient(proxy,p.options.encodingMinor,p.options.timeoutMs)); hw->UseDescription(description); }
            if (hw->Name()!=name) throw std::runtime_error("Preparation stop actual node name differs");
            hw->Execute(steps,file); audit.End(id,stage); p.productionStops.at(key)=true;
        } catch (const std::exception &e) {
            complete=false; audit.End(id,stage,false);
            audit.Record("{\"phase\":\"stop_target_failed\",\"id\":"+JsonString(id)+",\"stage\":"+JsonString(stage)+",\"error\":"+JsonString(e.what())+"}");
        }
    };
    try {
        const auto stopMutant=[&](std::size_t i) {
            const auto &m=p.options.mutants[i];
            stop(p.stopMutants[i],m.endpoint.nodeProxy,m.endpoint.description,m.endpoint.expectedNodeName,m.endpoint.id,
                 "mutant-stop-production",MutantAcquisitionRun(false));
        };
        if (!p.options.mutants.empty()) stopMutant(p.master);
        for (std::size_t i=0;i<p.options.mutants.size();++i) if (p.options.mutants[i].settings.isSlave) stopMutant(i);
        const auto plan=BuildCoBoProductionStopPlan();
        for (std::size_t i=0;i<p.options.cobos.size();++i) if (p.options.cobos[i].enabled) {
            const auto &c=p.options.cobos[i];
            stop(p.stopCobos[i],c.prepare.nodeProxy,c.prepare.description,c.prepare.expectedNodeName,c.id,"cobo-stop-pulser",plan.pulser);
            stop(p.stopCobos[i],c.prepare.nodeProxy,c.prepare.description,c.prepare.expectedNodeName,c.id,"cobo-stop-firmware",plan.firmware);
            stop(p.stopCobos[i],c.prepare.nodeProxy,c.prepare.description,c.prepare.expectedNodeName,c.id,"cobo-stop-interrupts",plan.interrupts);
        }
        if (!complete) throw std::runtime_error("Preparation producer stop unconfirmed; no repeated command or automatic cleanup");
        audit.Record("{\"phase\":\"complete\",\"production_stop_confirmed\":true,\"preparation_failed_preserved\":true,\"recovery_required\":true,\"sender_changed\":false,\"power_changed\":false,\"owned_alarms_retained\":true}");
    } catch (...) {
        try { audit.Record("{\"phase\":\"failed\",\"recovery_required\":true,\"automatic_retry\":false,\"automatic_cleanup\":false}"); } catch (...) {}
        throw;
    }
}
void SystemPreparationClient::InvalidateConfiguration(const std::string &path) {
    auto &p=*fImpl; std::lock_guard<std::mutex> lock(p.operation);
    if (p.state!=State::Configured) throw std::logic_error("Configuration invalidation requires the same Configured system");
    p.ReleasePreflight(false);
    CreateHardwareAuditDirectory(path); SystemPreparationReleaseAudit audit(path,"invalidate-configuration",p.options);
    p.invalidationChains.clear();
    std::vector<bool> touched(p.cobos.size(),false);
    p.state=State::Invalidating;
    try {
        for (const char *field:{"initDone","mutantMode"}) for (std::size_t i=0;i<p.cobos.size();++i) {
            const auto &id=p.options.cobos[i].id; const auto file=audit.Begin(id,field);
            touched[i]=true;
            p.cobos[i]->Execute(CoBoBit(field,false),file); audit.End(id,field);
            p.invalidationChains.emplace_back(file,p.cobos[i]->CompletedAuditChainSha256());
        }
        audit.Record("{\"phase\":\"complete\",\"prepare_snapshot_retained\":true,\"owned_alarms_retained\":true,\"receiver_sealed\":false,\"production_ecc_state_committed\":false}");
        p.Record("{\"phase\":\"configuration_invalidated\",\"prepare_snapshot_retained\":true,\"production_ecc_state_committed\":false}");
        for (std::size_t i=0;i<p.coboLifecycle->Size();++i) if(p.options.cobos[i].enabled) {
            p.SetCoBoLifecycle(i,CoBoLifecycleState::Prepared);
        }
        p.invalidationChains.emplace_back(path+"/system-release.jsonl",audit.ChainSha256());
        p.invalidationChains.emplace_back(p.directory+"/system.jsonl",p.audit->ChainSha256());
        p.state=State::Prepared;
    } catch (...) {
        for (std::size_t i=0;i<touched.size();++i) if(touched[i] && p.options.cobos[i].enabled) p.MarkCoBoUnknown(i,p.options.cobos[i].id);
        p.Fail();
        try { audit.Record("{\"phase\":\"failed\",\"automatic_retry\":false,\"automatic_cleanup\":false}"); } catch (...) {}
        throw;
    }
}
std::vector<std::pair<std::string,std::string>> SystemPreparationClient::ConfigurationInvalidationAuditChains() const
{
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);
    if(p.state!=State::Prepared || p.invalidationChains.empty())
        throw std::logic_error("No confirmed original configuration invalidation proof");
    return p.invalidationChains;
}

}
