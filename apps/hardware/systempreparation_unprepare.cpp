// Normal preparation release flow and its retained audit proof.
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
void SystemPreparationClient::Unprepare(const std::string &path,bool dataStopped) {
    auto &p=*fImpl; std::lock_guard<std::mutex> lock(p.operation);
    if (p.state!=State::Prepared) throw std::logic_error("System Unprepare requires Prepared; first Breakup or reconcile failures");
    if (!dataStopped || !p.options.exclusiveOwnerConfirmed)
        throw std::logic_error("System Unprepare requires a fresh stopped data process declaration and retained exclusive owner");
    p.ReleasePreflight(true);
    CreateHardwareAuditDirectory(path); SystemPreparationReleaseAudit audit(path,"unprepare",p.options);
    p.unprepareComplete=false;p.unprepareChains.clear();
    p.state=State::Unpreparing;
    std::vector<bool> touched(p.cobos.size(),false);
    try {
        bool monitoring=true,powerOff=true;
        const auto attempt=[&](std::size_t i,const char *stage,const std::vector<HardwareStep> &steps) {
            const auto &id=p.options.cobos[i].id; const auto file=audit.Begin(id,stage);
            try { touched[i]=true;p.cobos[i]->Execute(steps,file);p.unprepareChains.emplace_back(file,p.cobos[i]->CompletedAuditChainSha256());audit.End(id,stage);return true; }
            catch (...) { audit.End(id,stage,false); return false; }
        };
        // 每块板先观察实际已上电的AsAd，然后独立尝试断电；前一块失败仍处理其他板。
        for (std::size_t i=0;i<p.cobos.size();++i) {
            if (!attempt(i,"monitor-before-power-off",CoBoUnprepareMonitoring())) monitoring=false;
            if (!attempt(i,"power-off",CoBoAsAdPowerOff())) powerOff=false;
        }
        if (!monitoring || !powerOff)
            throw std::runtime_error("System Unprepare incomplete; power-off attempted independently, preserve owned alarms and reconcile without replay");
        audit.Record("{\"phase\":\"all_power_off_confirmed\",\"all_monitoring_observations_complete\":true}");
        for (std::size_t i=0;i<p.preparations.size();++i) {
            auto *alarm=p.preparations[i] ? p.preparations[i]->Alarms() : nullptr;
            if (!alarm) continue;
            const auto &id=p.options.cobos[i].id;
            if (alarm->Snapshot().subscription==CoBoAlarmSnapshot::Closed) {
                // Its already-confirmed closure belongs to this same client.
                audit.Record("{\"phase\":\"owned_alarm_already_closed\",\"id\":"+JsonString(id)+"}"); continue;
            }
            const auto file=audit.Begin(id,"unsubscribe-owned-alarm");alarm->Unsubscribe(file);
            p.unprepareChains.emplace_back(file,alarm->ClosedCommandAuditChainSha256());audit.End(id,"unsubscribe-owned-alarm");
        }
        audit.Record("{\"phase\":\"complete\",\"all_declared_cobo_power_off_confirmed\":true,\"owned_subscriptions_closed\":true,\"mutant_power_changed\":false,\"receiver_sealed\":false,\"production_ecc_state_committed\":false}");
        p.Record("{\"phase\":\"unprepare_complete\",\"production_ecc_state_committed\":false}");
        for(std::size_t i=0;i<p.coboLifecycle->Size();++i)if(p.options.cobos[i].enabled) {
            p.SetCoBoLifecycle(i,CoBoLifecycleState::Fresh);
        }
        p.unprepareChains.emplace_back(path+"/system-release.jsonl",audit.ChainSha256());
        p.unprepareChains.emplace_back(p.directory+"/system.jsonl",p.audit->ChainSha256());
        p.unprepareComplete=true;p.state=State::Unprepared;
    } catch (...) {
        for(std::size_t i=0;i<touched.size();++i)if(touched[i] && p.options.cobos[i].enabled)p.MarkCoBoUnknown(i,p.options.cobos[i].id);
        p.Fail();
        try { audit.Record("{\"phase\":\"failed\",\"automatic_retry\":false,\"automatic_cleanup\":false,\"recovery_required\":true}"); } catch (...) {}
        throw;
    }
}
std::vector<std::pair<std::string,std::string>> SystemPreparationClient::UnprepareAuditChains() const {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);
    if(p.state!=State::Unprepared || !p.unprepareComplete || p.unprepareChains.empty())
        throw std::logic_error("No confirmed retained Unprepare audit proof");
    return p.unprepareChains;
}
}
