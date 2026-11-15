// Host sequence adapted from CEA GET SystemManager (received 2021 release).
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "systempreparation_p.h"
#include "systempreparationplan_p.h"
#include "cobolifecycle.h"
#include "cobolifecyclejournal.h"
#include "hardwareaudit.h"
#include "systempreparationreleaseaudit_p.h"
#include "systempreparationauditcapture_p.h"
#include "acquisitionplans.h"
#include "configurationreadback.h"
#include "configurationsha256.h"
#include <Ice/Ice.h>
#include <algorithm>
#include <atomic>
#include <limits>
#include <mutex>
#include <map>
#include <stdexcept>
#include <utility>
namespace GetDAQ {
using namespace SystemPreparationInternal;
SystemPreparationClient::SystemPreparationClient(const SystemPreparationOptions &options) : fImpl(new Impl)
{
    auto plan = CompileSystemPreparationPlan(options);
    auto &p = *fImpl;
    p.options = std::move(plan.options);
    p.configurePlans = std::move(plan.configurePlans);
    p.mutantPreparePlans = std::move(plan.mutantPreparePlans);
    p.master = plan.master;
    p.level2 = plan.level2;
    p.coboLifecycle.reset(new CoBoLifecycleLedger(plan.coboTopology));
    for (const auto &cobo : p.options.cobos)
        p.preparations.emplace_back(cobo.enabled ? new CoBoPrepareClient(cobo.prepare) : nullptr);
    if (!p.options.mutants.empty()) p.alignment.reset(new SystemAlignmentClient(plan.alignmentOptions));
}
SystemPreparationClient::~SystemPreparationClient() {}
SystemPreparationClient::State SystemPreparationClient::GetState() const { return fImpl->state.load(); }
void SystemPreparationClient::CheckInterlock() const
{
    const auto state = GetState();
    if (state != State::Prepared && state != State::Configured) throw std::logic_error("System interlock inspection requires a terminated successful phase");
    fImpl->Check();
}
CoBoAlarmClient *SystemPreparationClient::Alarms(const std::string &id) const
{
    const auto state = GetState();
    if (state == State::Preparing || state == State::Configuring || state == State::Fresh || state==State::Invalidating || state==State::Unpreparing || state==State::Maintaining)
        throw std::logic_error("Alarm owners are available after a system phase terminates");
    for (std::size_t i = 0; i < fImpl->options.cobos.size(); ++i) if (fImpl->options.cobos[i].id == id)
        return fImpl->preparations[i] && fImpl->preparations[i]->Status() != CoBoPrepareClient::Fresh ? fImpl->preparations[i]->Alarms() : nullptr;
    throw std::invalid_argument("Unknown system CoBo ID");
}
SystemAlignmentResult SystemPreparationClient::AlignmentResult() const
{
    const auto s = GetState();
    if (s != State::Prepared && s != State::Configured) throw std::logic_error("Alignment snapshot requires completed system Prepare");
    return fImpl->result;
}
void SystemPreparationClient::Prepare(const std::string &newAuditDirectory)
{
    auto &p = *fImpl; std::lock_guard<std::mutex> lock(p.operation);
    if (p.state != State::Fresh) throw std::logic_error("System Prepare is single-use; explicit reconciliation is required");
    p.state = State::Preparing;
    try {
        if (!p.options.exclusiveOwnerConfirmed || !p.options.dataProcessesStopped) throw std::logic_error("System preparation requires exclusive owner and stopped data processes declarations");
        CreateHardwareAuditDirectory(newAuditDirectory); p.directory = newAuditDirectory;
        p.audit.reset(new HardwareAudit(p.directory+"/system.jsonl"));
        p.Record("{\"schema_version\":1,\"phase\":\"intent\",\"operation\":\"system-prepare-configure\",\"encoding_minor\":"+std::to_string(p.options.encodingMinor)+
            ",\"timeout_ms\":"+std::to_string(p.options.timeoutMs)+",\"configuration_copied_at_construction\":true,\"configuration_sha256\":"+JsonString(p.options.configurationSha256)+
            ",\"artifact_sha256\":"+JsonString(p.options.artifactSha256)+",\"actual_loaded_configuration_verified\":false,\"owner_and_stopped_are_declarations\":true,\"production_ecc_state_committed\":false}");
        for (const auto &m : p.options.mutants) p.Record("{\"phase\":\"bound_mutant\",\"id\":"+JsonString(m.endpoint.id)+",\"node_proxy\":"+JsonString(m.endpoint.nodeProxy)+
            ",\"expected_node_name\":"+JsonString(m.endpoint.expectedNodeName)+",\"hardware_policy\":"+HardwarePolicyJson(m.endpoint.description)+"}");
        for (const auto &c : p.options.cobos) p.Record("{\"phase\":\"bound_cobo\",\"id\":"+JsonString(c.id)+",\"node_proxy\":"+JsonString(c.prepare.nodeProxy)+
            ",\"expected_node_name\":"+JsonString(c.prepare.expectedNodeName)+",\"enabled\":"+(c.enabled ? "true" : "false")+",\"hardware_policy\":"+HardwarePolicyJson(c.prepare.description)+"}");
        // Bind the continuous per-board journal to this unique preparation
        // directory and its immutable intent/topology audit prefix.
        const auto lifecycleScope=ConfigurationSha256(p.directory+'\n'+p.audit->ChainSha256());
        p.coboLifecycle->BeginPersistence(p.directory+"/cobo-lifecycle.jsonl",lifecycleScope);
        p.Record("{\"phase\":\"cobo_lifecycle_journal\",\"file\":\"cobo-lifecycle.jsonl\",\"scope_sha256\":"+JsonString(lifecycleScope)+
                 ",\"lifecycle_checkpoint\":"+EncodeCoBoLifecycleCheckpoint(p.coboLifecycle->PersistenceCheckpoint())+"}");
        p.Preflight();
        for (std::size_t i = 0; i < p.mutants.size(); ++i) p.Execute(*p.mutants[i],p.mutantPreparePlans[i],p.options.mutants[i].endpoint.id,"mutant-prepare");
        if (p.alignment) {
            p.Record("{\"phase\":\"alignment_begin\"}");
            p.result = p.alignment->Align(p.directory+"/alignment");
            p.Record("{\"phase\":\"alignment_complete\"}");
        }
        for (std::size_t i = 0; i < p.preparations.size(); ++i) if (p.preparations[i]) {
            p.SetCoBoLifecycle(i,CoBoLifecycleState::Preparing);
            p.Record("{\"phase\":\"cobo_prepare_begin\",\"id\":"+JsonString(p.options.cobos[i].id)+"}");
            p.preparations[i]->Prepare(std::function<void()>([&p] { p.Check(); }));
            p.SetCoBoLifecycle(i,CoBoLifecycleState::Prepared);
            p.Record("{\"phase\":\"cobo_prepare_complete\",\"id\":"+JsonString(p.options.cobos[i].id)+"}");
        }
        p.Check();
        p.Record("{\"phase\":\"prepare_complete\",\"production_ecc_state_committed\":false,\"sync_pulse_sent\":false,\"physical_calibration_verified\":false}");
        p.state = State::Prepared;
    } catch (...) {
        for (std::size_t i=0;i<p.coboLifecycle->Size();++i) if (p.coboLifecycle->StateAt(i)==CoBoLifecycleState::Preparing) {
            try {p.SetCoBoLifecycle(i,CoBoLifecycleState::Unknown);} catch (...) {}
        }
        p.Fail(); throw;
    }
}
void SystemPreparationClient::Configure()
{
    auto &p = *fImpl; std::lock_guard<std::mutex> lock(p.operation);
    if (p.state != State::Prepared) throw std::logic_error("System Configure requires the same successfully prepared object and cannot be replayed");
    p.state = State::Configuring;
    p.configureReadbacks.clear();
    try {
        p.Check();
        p.ValidateConfigure();
        // 全部停止观察先通过；后段局部错误不能让前板Configure先写入。
        for (std::size_t i = 0; i < p.cobos.size(); ++i) p.Execute(*p.cobos[i],CoBoStopped(),p.options.cobos[i].id,"configure-precondition");
        for (std::size_t i = 0; i < p.mutants.size(); ++i)
            p.Execute(*p.mutants[i],{Field(HardwareStep::ExpectValue,"CARD_B_EMC","TRIGGER_CTRL","run",0)},p.options.mutants[i].endpoint.id,"configure-precondition");
        for (std::size_t i = 0; i < p.cobos.size(); ++i) if (p.options.cobos[i].enabled) {
            p.SetCoBoLifecycle(i,CoBoLifecycleState::Configuring);
            p.Execute(*p.cobos[i],p.configurePlans[i],p.options.cobos[i].id,"cobo-configure");
        }
        for (std::size_t i = 0; i < p.cobos.size(); ++i) p.Execute(*p.cobos[i],CoBoBit("l2Trigger",p.options.cobos[i].enabled && p.level2),p.options.cobos[i].id,"level2");
        for (std::size_t i = 0; i < p.cobos.size(); ++i) p.Execute(*p.cobos[i],CoBoBit("mutantMode",p.options.cobos[i].enabled && !p.mutants.empty()),p.options.cobos[i].id,"mutant-mode");
        for (std::size_t i = 0; i < p.cobos.size(); ++i) {
            auto checks = CoBoStopped();
            checks.push_back(Field(HardwareStep::ExpectValue,"ctrl","mutantConfig","mutantMode",p.options.cobos[i].enabled && !p.mutants.empty()));
            p.Execute(*p.cobos[i],checks,p.options.cobos[i].id,"sync-precondition");
        }
        // PLL Prepare和AsAd Configure之后，initDone仍为0，WSCA关闭CLK2才发送同步脉冲。
        for (std::size_t i = 0; i < p.mutants.size(); ++i) p.Execute(*p.mutants[i],MutantWscaClock(false),p.options.mutants[i].endpoint.id,"wsca-clock-low");
        for (std::size_t i = 0; i < p.mutants.size(); ++i) {
            const auto &crate = p.result.crates.at(i);
            if (crate.mutantId != p.options.mutants[i].endpoint.id) throw std::logic_error("Alignment snapshot module order mismatch");
            p.Execute(*p.mutants[i],MutantClockShelfMasks(crate.gmcShelfMask,crate.wscaShelfMask),crate.mutantId,"restore-saved-masks");
            p.Execute(*p.mutants[i],MutantSyncPulse(true),crate.mutantId,"sync-pulse-high");
        }
        for (std::size_t i = 0; i < p.mutants.size(); ++i) p.Execute(*p.mutants[i],MutantSyncPulse(false),p.options.mutants[i].endpoint.id,"sync-pulse-low");
        for (std::size_t i = 0; i < p.mutants.size(); ++i) p.Execute(*p.mutants[i],MutantWscaClock(true),p.options.mutants[i].endpoint.id,"wsca-clock-high");
        for (std::size_t i = 0; i < p.cobos.size(); ++i) p.Execute(*p.cobos[i],CoBoBit("initDone",p.options.cobos[i].enabled),p.options.cobos[i].id,"init-done");
        for (std::size_t i = 0; i < p.mutants.size(); ++i) {
            auto settings = p.options.mutants[i].settings;
            settings.gmcShelfMask = p.result.crates.at(i).gmcShelfMask; settings.wscaShelfMask = p.result.crates.at(i).wscaShelfMask;
            p.Execute(*p.mutants[i],MutantConfigure(settings),p.options.mutants[i].endpoint.id,"mutant-configure");
        }
        p.Check();
        p.Record("{\"phase\":\"configure_complete\",\"production_ecc_state_committed\":false,\"data_senders_connected\":false,\"physical_sync_verified\":false}");
        for(std::size_t i=0;i<p.coboLifecycle->Size();++i) if(p.options.cobos[i].enabled) {
            p.SetCoBoLifecycle(i,CoBoLifecycleState::Configured);
        }
        p.state = State::Configured;
    } catch (const std::exception &e) {
        for(std::size_t i=0;i<p.coboLifecycle->Size();++i) if(p.coboLifecycle->StateAt(i)==CoBoLifecycleState::Configuring) {
            try {p.SetCoBoLifecycle(i,CoBoLifecycleState::Unknown);} catch (...) {}
        }
        try { p.Record("{\"phase\":\"configure_failed\",\"error\":"+JsonString(e.what())+"}"); } catch (...) {}
        p.Fail(); throw;
    } catch (...) {
        for(std::size_t i=0;i<p.coboLifecycle->Size();++i) if(p.coboLifecycle->StateAt(i)==CoBoLifecycleState::Configuring) {
            try {p.SetCoBoLifecycle(i,CoBoLifecycleState::Unknown);} catch (...) {}
        }
        p.Fail(); throw;
    }
}
std::string SystemPreparationClient::CoBoLifecycleJson() const {
    auto &p=*fImpl;
    std::lock_guard<std::mutex> lock(p.operation);
    std::string result="[";
    for(std::size_t i=0;i<p.options.cobos.size();++i) {
        if(i) result+=',';
        const auto state=p.coboLifecycle->StateAt(i);
        result+="{\"id\":"+JsonString(p.options.cobos[i].id)+",\"state\":"+JsonString(CoBoLifecycleStateName(state))+"}";
    }
    return result+"]";
}
std::shared_ptr<CoBoLifecycleLedger> SystemPreparationClient::CoBoLifecycleLedgerShared() const {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);
    return p.coboLifecycle;
}
std::string SystemPreparationClient::ConfigureReadbacksJson() const {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);
    if (p.state!=State::Configured) throw std::logic_error("Configure readback evidence requires completed Configure");
    std::string result="[";
    for (const auto &entry:p.configureReadbacks) {if (result.size()>1) result+=',';result+=entry;}
    return result+"]";
}
SystemPreparationOptions SystemPreparationClient::Configuration() const { return fImpl->options; }
}
