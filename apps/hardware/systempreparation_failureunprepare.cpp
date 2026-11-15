// Failure-only unprepare and its retained read-only recovery proofs.
#include "systempreparation_p.h"
#include "systempreparationreleaseaudit_p.h"
#include "hardwareaudit.h"
#include "cobolifecycle.h"
#include "cobolifecyclejournal.h"
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace GetDAQ {
using namespace SystemPreparationInternal;
void SystemPreparationClient::UnprepareAfterFailure(const std::string &path,bool dataStopped)
{
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);
    if(p.state!=State::Failed || !dataStopped || !p.options.exclusiveOwnerConfirmed)
        throw std::logic_error("Failure unprepare requires retained Failed preparation and freshly stopped data");
    p.ValidateIdentities();
    const auto confirmed=[&](const std::string &id,const char *stage) {
        const auto found=p.productionStops.find(std::make_pair(id,std::string(stage)));
        if(found==p.productionStops.end() || !found->second)
            throw std::logic_error("Failure unprepare requires every original production stop acknowledged");
    };
    for(const auto &c:p.options.cobos)if(c.enabled)
        for(const auto *stage:{"cobo-stop-pulser","cobo-stop-firmware","cobo-stop-interrupts"})confirmed(c.id,stage);
    for(const auto &m:p.options.mutants)confirmed(m.endpoint.id,"mutant-stop-production");
    std::vector<std::unique_ptr<HardwareClient>> clients;
    std::vector<CoBoAlarmClient *> alarms;
    // Validate the full immutable target set before any power/monitoring action.
    for(std::size_t i=0;i<p.options.cobos.size();++i) {
        const auto &c=p.options.cobos[i];std::unique_ptr<HardwareClient> hw(new HardwareClient(c.prepare.nodeProxy,p.options.encodingMinor,p.options.timeoutMs));
        hw->UseDescription(c.prepare.description);
        Inventory(*hw,c.prepare.description,c.prepare.expectedNodeName);
        hw->Validate(CoBoStopped());hw->Validate(CoBoBit("initDone",false));hw->Validate(CoBoBit("mutantMode",false));
        hw->Validate(CoBoUnprepareMonitoring());hw->Validate(CoBoAsAdPowerOff());
        if(hw->Read("ctrl","pipeCtrl","firmwareEnable") || hw->Read("ctrl","pipeCtrl","intEnable"))
            throw std::runtime_error("Failure unprepare found production enabled; no power change");
        CoBoAlarmClient *alarm=nullptr;
        if(i<p.preparations.size() && p.preparations[i] && p.preparations[i]->Status()!=CoBoPrepareClient::Fresh)
            alarm=p.preparations[i]->Alarms();
        if(alarm) {
            const auto subscription=alarm->Snapshot().subscription;
            if(subscription==CoBoAlarmSnapshot::Active)alarm->ValidateUnsubscribe();
            else if(subscription!=CoBoAlarmSnapshot::Fresh && subscription!=CoBoAlarmSnapshot::Closed)
                throw std::logic_error("Failure unprepare cannot adopt an unknown alarm subscription");
        }
        clients.push_back(std::move(hw));alarms.push_back(alarm);
    }
    for(const auto &m:p.options.mutants) {
        HardwareClient hw(m.endpoint.nodeProxy,p.options.encodingMinor,p.options.timeoutMs);
        hw.UseDescription(m.endpoint.description);
        Inventory(hw,m.endpoint.description,m.endpoint.expectedNodeName);
        if(hw.Read("CARD_B_EMC","TRIGGER_CTRL","run"))throw std::runtime_error("Failure unprepare found MuTaNT running");
    }
    CreateHardwareAuditDirectory(path);SystemPreparationReleaseAudit audit(path,"unprepare-after-preparation-failure",p.options);
    p.failureCleanupChains.clear();p.failureVerificationChains.clear();
    p.state=State::Unpreparing;
    std::vector<bool> touched(clients.size(),false);
    for(const auto &entry:p.failureCleanupSteps)for(std::size_t i=0;i<p.options.cobos.size();++i)
        if(entry.first.first==p.options.cobos[i].id)touched[i]=true;
    const auto action=[&](std::size_t i,const char *stage,const std::function<void(const std::string &)> &execute) {
        const auto &id=p.options.cobos[i].id;const auto key=std::make_pair(id,std::string(stage));const auto old=p.failureCleanupSteps.find(key);
        if(old!=p.failureCleanupSteps.end()) {
            audit.Record("{\"phase\":\"cleanup_not_repeated\",\"id\":"+JsonString(id)+",\"stage\":"+JsonString(stage)+",\"previously_confirmed\":"+(old->second ? "true" : "false")+"}");return old->second;
        }
        const auto file=audit.Begin(id,stage);p.failureCleanupSteps.emplace(key,false);touched[i]=true;
        try {execute(file);audit.End(id,stage);p.failureCleanupSteps.at(key)=true;return true;}
        catch(...) {audit.End(id,stage,false);return false;}
    };
    try {
        bool complete=true;
        for(std::size_t i=0;i<clients.size();++i) {
            complete=action(i,"clear-not-ready",[&](const std::string &file){auto checks=CoBoBit("initDone",false);const auto mode=CoBoBit("mutantMode",false);checks.insert(checks.end(),mode.begin(),mode.end());clients[i]->Execute(checks,file);p.failureCleanupChains.emplace_back(file,clients[i]->CompletedAuditChainSha256());}) && complete;
            complete=action(i,"monitor-before-power-off",[&](const std::string &file){clients[i]->Execute(CoBoUnprepareMonitoring(),file);p.failureCleanupChains.emplace_back(file,clients[i]->CompletedAuditChainSha256());}) && complete;
            complete=action(i,"power-off",[&](const std::string &file){clients[i]->Execute(CoBoAsAdPowerOff(),file);p.failureCleanupChains.emplace_back(file,clients[i]->CompletedAuditChainSha256());}) && complete;
        }
        if(!complete)throw std::runtime_error("Failure unprepare incomplete; independent power-off attempted, no replay or alarm cancellation");
        for(std::size_t i=0;i<clients.size();++i) {
            if(clients[i]->Read("ctrl","asadEnable") || clients[i]->Read("ctrl","mutantConfig","initDone") || clients[i]->Read("ctrl","mutantConfig","mutantMode"))
                throw std::runtime_error("Failure unprepare power/ready readback does not match");
        }
        for(std::size_t i=0;i<alarms.size();++i)if(alarms[i] && alarms[i]->Snapshot().subscription==CoBoAlarmSnapshot::Active)
            complete=action(i,"unsubscribe-owned-alarm",[&](const std::string &file){alarms[i]->Unsubscribe(file);p.failureCleanupChains.emplace_back(file,alarms[i]->ClosedCommandAuditChainSha256());}) && complete;
        if(!complete)throw std::runtime_error("Failure unprepare owned subscription closure unconfirmed; no replay");
        audit.Record("{\"phase\":\"complete\",\"all_cobo_power_off_observed\":true,\"owned_subscriptions_closed\":true,\"receiver_sealed\":false,\"parent_ecc_reconciled\":false}");
        p.Record("{\"phase\":\"failure_unprepare_complete\",\"parent_ecc_reconciled\":false}");
        for(std::size_t i=0;i<p.coboLifecycle->Size();++i)if(p.options.cobos[i].enabled) {
            p.SetCoBoLifecycle(i,CoBoLifecycleState::Fresh);
        }
        p.failureCleanupChains.emplace_back(path+"/system-release.jsonl",audit.ChainSha256());
        p.failureCleanupChains.emplace_back(p.directory+"/system.jsonl",p.audit->ChainSha256());
        p.failureCleanupComplete=true;p.state=State::Unprepared;
    } catch(...) {
        for(std::size_t i=0;i<touched.size();++i)if(touched[i] && p.options.cobos[i].enabled)p.MarkCoBoUnknown(i,p.options.cobos[i].id);
        p.Fail();try{audit.Record("{\"phase\":\"failed\",\"automatic_retry\":false,\"parent_ecc_reconciled\":false}");}catch(...){}throw;
    }
}
void SystemPreparationClient::VerifyUnpreparedAfterFailure(const std::string &path)
{
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);
    if(p.state!=State::Unprepared || (!p.failureCleanupComplete && !p.unprepareComplete))
        throw std::logic_error("Preparation recovery requires confirmed retained failure cleanup");
    for(const auto &stage:p.failureCleanupSteps)if(!stage.second)
        throw std::logic_error("An original cleanup step remains unconfirmed");
    p.ValidateIdentities();p.failureVerificationChains.clear();
    CreateHardwareAuditDirectory(path);SystemPreparationReleaseAudit audit(path,"verify-failed-preparation-unprepared",p.options);
    try {
        for(std::size_t i=0;i<p.options.cobos.size();++i) {
            const auto &c=p.options.cobos[i];
            HardwareClient hw(c.prepare.nodeProxy,p.options.encodingMinor,p.options.timeoutMs);
            hw.UseDescription(c.prepare.description);Inventory(hw,c.prepare.description,c.prepare.expectedNodeName);
            std::vector<HardwareStep> checks=CoBoStopped();
            checks.push_back(Field(HardwareStep::ExpectValue,"ctrl","mutantConfig","mutantMode",0));
            checks.push_back(Field(HardwareStep::ExpectValue,"ctrl","asadEnable","",0));
            const auto file=audit.Begin(c.id,"verify-stopped-not-ready-power-off");
            hw.Execute(checks,file);audit.End(c.id,"verify-stopped-not-ready-power-off");
            p.failureVerificationChains.emplace_back(file,hw.CompletedAuditChainSha256());
            auto *alarm=i<p.preparations.size() && p.preparations[i] ? p.preparations[i]->Alarms() : nullptr;
            if(alarm) {
                const auto subscription=alarm->Snapshot().subscription;
                if(subscription!=CoBoAlarmSnapshot::Fresh && subscription!=CoBoAlarmSnapshot::Closed)
                    throw std::logic_error("Original owned subscription is not confirmed closed");
            }
        }
        for(const auto &m:p.options.mutants) {
            HardwareClient hw(m.endpoint.nodeProxy,p.options.encodingMinor,p.options.timeoutMs);
            hw.UseDescription(m.endpoint.description);Inventory(hw,m.endpoint.description,m.endpoint.expectedNodeName);
            const auto file=audit.Begin(m.endpoint.id,"verify-mutant-stopped");
            hw.Execute({Field(HardwareStep::ExpectValue,"CARD_B_EMC","TRIGGER_CTRL","run",0)},file);
            audit.End(m.endpoint.id,"verify-mutant-stopped");
            p.failureVerificationChains.emplace_back(file,hw.CompletedAuditChainSha256());
        }
        audit.Record("{\"phase\":\"complete\",\"read_only\":true,\"all_power_off_observed\":true,\"owned_subscriptions_closed\":true}");
        for(std::size_t i=0;i<p.coboLifecycle->Size();++i)if(p.options.cobos[i].enabled) {
            p.SetCoBoLifecycle(i,CoBoLifecycleState::Fresh);
        }
        // Lifecycle confirmation above appends to the retained preparation
        // journal. Include its refreshed writer chain in the read-only
        // verification proof so callers can advance only the authorized
        // append while preserving the previously verified prefix.
        p.failureVerificationChains.emplace_back(p.directory+"/system.jsonl",p.audit->ChainSha256());
        p.failureVerificationChains.emplace_back(path+"/system-release.jsonl",audit.ChainSha256());
    } catch(...) {
        for(std::size_t i=0;i<p.coboLifecycle->Size();++i)if(p.options.cobos[i].enabled)p.MarkCoBoUnknown(i,p.options.cobos[i].id);
        p.failureVerificationChains.clear();throw;
    }
}
std::vector<std::pair<std::string,std::string>> SystemPreparationClient::FailureCleanupAuditChains() const
{
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);
    if(p.state!=State::Unprepared || !p.failureCleanupComplete || p.failureCleanupChains.empty())
        throw std::logic_error("No complete retained failure cleanup proof");
    return p.failureCleanupChains;
}
std::vector<std::pair<std::string,std::string>> SystemPreparationClient::FailureVerificationAuditChains() const
{
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);
    if(p.state!=State::Unprepared || p.failureVerificationChains.empty())
        throw std::logic_error("No complete read-only preparation verification proof");
    return p.failureVerificationChains;
}
}
