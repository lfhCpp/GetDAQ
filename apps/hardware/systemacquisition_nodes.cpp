#include "systemacquisition_p.h"
#include <stdexcept>
#include "systemacquisition_preflight.h"
#include "hardwareclient.h"

namespace GetDAQ {
namespace {
HardwareStep Expect(const char *device,const char *reg,const char *field,std::uint64_t value) {
    HardwareStep s; s.kind = HardwareStep::ExpectValue; s.device = device; s.reg = reg; s.field = field;
    s.value = value; s.requireBoundDescription = true; return s;
}
bool Trigger(const MutantSettings &s) { return s.level0Enabled || s.level1Enabled || s.level2Enabled; }
}
void SystemAcquisitionClient::Impl::Check() const {
    if (prepared.GetState() != SystemPreparationClient::State::Configured) throw std::logic_error("Prepared system is no longer Configured");
    prepared.CheckInterlock();
}

void SystemAcquisitionClient::Impl::Execute(HardwareClient &hw,const std::vector<HardwareStep> &steps,const std::string &id,const char *stage,bool interlock) {
    const auto path = File(id,stage);
    hw.Execute(steps,path,interlock ? std::function<void()>([this] { Check(); }) : std::function<void()>());
    if(captureFailureHardware)failureBreakupChains.emplace_back(path,hw.CompletedAuditChainSha256());
    Record("{\"phase\":\"step_complete\",\"id\":"+JsonString(id)+",\"stage\":"+JsonString(stage)+"}");
}

void SystemAcquisitionClient::Impl::Hardware() {
    if (!cobos.empty()) return;
    std::vector<std::unique_ptr<HardwareClient>> nextCoBo,nextMutant;
    for (const auto &c : options.cobos) {
        std::unique_ptr<HardwareClient> hw(new HardwareClient(c.prepare.nodeProxy,options.encodingMinor,options.timeoutMs));
        hw->UseDescription(c.prepare.description); nextCoBo.push_back(std::move(hw));
    }
    for (const auto &m : options.mutants) {
        std::unique_ptr<HardwareClient> hw(new HardwareClient(m.endpoint.nodeProxy,options.encodingMinor,options.timeoutMs));
        hw->UseDescription(m.endpoint.description); nextMutant.push_back(std::move(hw));
    }
    cobos = std::move(nextCoBo); mutants = std::move(nextMutant);
}

void SystemAcquisitionClient::Impl::Name(HardwareClient &hw,const std::string &expected) {
    if (hw.Name() != expected) throw std::runtime_error("System run actual node name differs from configured endpoint");
}

void SystemAcquisitionClient::Impl::Preflight(bool start) {
    Hardware();
    const auto execute=[this,start](HardwareClient &hw,const std::vector<HardwareStep> &steps,
                                    const std::string &id,const char *stage) {
        Execute(hw,steps,id,stage,start);
    };
    RunSystemAcquisitionPreflight(options,plans,cobos,mutants,*links,execute,[this](const std::string &row) {Record(row);});
}

void SystemAcquisitionClient::Impl::StartNodes(bool resume,bool restart) {
    Preflight(true); Check();
    const auto ready = (resume || restart) ? DataSenderSnapshot::Stopped : DataSenderSnapshot::Connected;
    for (const auto &c : options.cobos) if (c.enabled && links->Sender(c.id).Snapshot().state != ready)
        throw std::logic_error("CoBo sender state differs from run boundary");
    for (const auto &m : options.mutants) if (m.settings.readoutEnabled) {
        const bool used = !m.settings.isSlave && Trigger(m.settings);
        if (links->Sender(m.endpoint.id).Snapshot().state != (used ? ready : DataSenderSnapshot::Connected))
            throw std::logic_error("MuTaNT sender state differs from run boundary");
    }
    productionStops.clear(); // 新的Start/Resume边界；前一次停止已成功且sender状态已核对。
    if (!resume) for (std::size_t i = 0; i < options.cobos.size(); ++i) if (options.cobos[i].enabled) {
        Lifecycle(i,CoBoLifecycleState::Starting);
        Execute(*cobos[i],plans[i].runInit,options.cobos[i].id,"cobo-run-init");
    }
    for (std::size_t i = 0; i < options.cobos.size(); ++i) if (options.cobos[i].enabled) {
        const auto &c = options.cobos[i];
        if(resume)Lifecycle(i,CoBoLifecycleState::Resuming);
        else if(coboLifecycle->StateAt(i)!=CoBoLifecycleState::Starting)Lifecycle(i,CoBoLifecycleState::Starting);
        Execute(*cobos[i],plans[i].beforeSenderStart,c.id,"cobo-arm");
        links->Sender(c.id).Start(File(c.id,"sender-start"),[this] { Check(); });
        Execute(*cobos[i],plans[i].afterSenderStart,c.id,"cobo-release-fast-control");
    }
    const auto startMutant = [&](std::size_t i) {
        const auto &m = options.mutants[i];
        const auto init = MutantRunInit(m.settings);
        if (!resume && !init.empty()) Execute(*mutants[i],init,m.endpoint.id,"mutant-run-init");
        if (!m.settings.isSlave && Trigger(m.settings) && m.settings.readoutEnabled)
            links->Sender(m.endpoint.id).Start(File(m.endpoint.id,"sender-start"),[this] { Check(); });
        if (m.settings.isSlave || Trigger(m.settings)) Execute(*mutants[i],MutantAcquisitionRun(true),m.endpoint.id,"mutant-run");
    };
    for (std::size_t i = 0; i < options.mutants.size(); ++i) if (options.mutants[i].settings.isSlave) startMutant(i);
    if (!options.mutants.empty()) startMutant(master);
    Check();LifecycleAll(CoBoLifecycleState::Running);
}

bool SystemAcquisitionClient::Impl::StopProducers() {
    // 显式停止请求：告警保持不能阻断它。先禁止所有生产，再停读出/flush。
    // 失败仍尝试其他独立生产停止目标，绝不重复同一失败动作或自动Disconnect。
    bool failed = false;
    const auto stop = [&](HardwareClient &hw,const std::vector<HardwareStep> &steps,const std::string &id,const char *stage,const std::string &expected) {
        const auto key = std::make_pair(id,std::string(stage));
        const auto earlier = productionStops.find(key);
        if (earlier != productionStops.end()) {
            failed = failed || !earlier->second;
            Record("{\"phase\":\"stop_not_repeated\",\"id\":"+JsonString(id)+",\"stage\":"+JsonString(stage)+
                ",\"previously_confirmed\":"+(earlier->second ? "true" : "false")+"}");
            return earlier->second;
        }
        productionStops.emplace(key,false);
        try { Name(hw,expected); Execute(hw,steps,id,stage,false); productionStops.at(key) = true; return true; }
        catch (const std::exception &e) {
            failed = true; Record("{\"phase\":\"stop_target_failed\",\"id\":"+JsonString(id)+",\"stage\":"+JsonString(stage)+",\"error\":"+JsonString(e.what())+"}");
            return false;
        }
    };
    const auto stopMutant = [&](std::size_t i) {
        const auto &m = options.mutants[i]; stop(*mutants[i],MutantAcquisitionRun(false),m.endpoint.id,"mutant-stop-production",m.endpoint.expectedNodeName);
    };
    if (!options.mutants.empty()) stopMutant(master);
    for (std::size_t i = 0; i < options.mutants.size(); ++i) if (options.mutants[i].settings.isSlave) stopMutant(i);
    for (std::size_t i = 0; i < options.cobos.size(); ++i) if (options.cobos[i].enabled) {
        const auto &c = options.cobos[i];
        NegativeLifecycle(i,CoBoLifecycleState::Stopping);
        const bool pulser=stop(*cobos[i],plans[i].stopPulser,c.id,"cobo-stop-pulser",c.prepare.expectedNodeName);
        const bool firmware=stop(*cobos[i],plans[i].stopFirmware,c.id,"cobo-stop-firmware",c.prepare.expectedNodeName);
        const bool interrupts=stop(*cobos[i],plans[i].stopInterrupts,c.id,"cobo-stop-interrupts",c.prepare.expectedNodeName);
        NegativeLifecycle(i,pulser && firmware && interrupts ? CoBoLifecycleState::Stopped : CoBoLifecycleState::Unknown);
    }
    return !failed;
}

void SystemAcquisitionClient::Impl::VerifyBrokenProductionStopped() {
    if(prepared.GetState()!=SystemPreparationClient::State::Prepared)
        throw std::logic_error("Broken acquisition lost the same Prepared object");
    // Breakup completed before receiver Finish failed. Never repeat a producer
    // command or adopt another connection; require every retained Stop outcome.
    for(const auto &source:senderOptions) {
        const auto status=links->Sender(source.senderId).Snapshot();
        const bool retired=source.protocol=="ICE" && status.outcomeUnknown && status.failureRetirementAcknowledged;
        if(retired){if(!sourceRetirementComplete)throw std::logic_error("Missing original source retirement");links->Sender(source.senderId).VerifyIceFailureRetirement();}
        if((status.outcomeUnknown && !retired) || (!retired && status.state!=DataSenderSnapshot::Fresh && status.state!=DataSenderSnapshot::Disconnected))
            throw std::runtime_error("Broken acquisition sender disconnect unconfirmed");
    }
    const auto confirmed=[&](const std::string &id,const char *stage) {
        const auto found=productionStops.find(std::make_pair(id,std::string(stage)));
        if(found==productionStops.end() || !found->second)
            throw std::runtime_error("Broken acquisition lacks retained successful producer stop");
    };
    for(const auto &c:options.cobos) if(c.enabled)
        for(const char *stage:{"cobo-stop-pulser","cobo-stop-firmware","cobo-stop-interrupts"})confirmed(c.id,stage);
    for(const auto &m:options.mutants)confirmed(m.endpoint.id,"mutant-stop-production");
    Hardware();
    const std::vector<HardwareStep> coboStopped={Expect("ctrl","pipeCtrl","firmwareEnable",0),Expect("ctrl","pipeCtrl","intEnable",0),
        Expect("ctrl","mutantConfig","initDone",0),Expect("ctrl","mutantConfig","mutantMode",0)};
    const std::vector<HardwareStep> mutantStopped={Expect("CARD_B_EMC","TRIGGER_CTRL","run",0)};
    for(std::size_t i=0;i<options.cobos.size();++i) {Name(*cobos[i],options.cobos[i].prepare.expectedNodeName);cobos[i]->Validate(coboStopped);}
    for(std::size_t i=0;i<options.mutants.size();++i) {Name(*mutants[i],options.mutants[i].endpoint.expectedNodeName);mutants[i]->Validate(mutantStopped);}
    for(std::size_t i=0;i<options.cobos.size();++i)Execute(*cobos[i],coboStopped,options.cobos[i].id,"broken-cobo-stopped-readback",false);
    for(std::size_t i=0;i<options.mutants.size();++i)Execute(*mutants[i],mutantStopped,options.mutants[i].endpoint.id,"broken-mutant-stopped-readback",false);
    Record("{\"phase\":\"broken_production_stop_verified\",\"retained_stop_outcomes_required\":true,\"fresh_register_readbacks\":true,\"hardware_changed\":false,\"sender_changed\":false,\"hardware_global_ownership_verified\":false,\"recovery_required\":true}");
}

void SystemAcquisitionClient::Impl::Quiesce(CoBoLifecycleState terminalLifecycle) {
    if (!StopProducers()) throw std::runtime_error("System producer stop incomplete; data connections retained for explicit recovery");
    bool failed = false;
    const auto stopSender = [&](const std::string &id) {
        auto &sender = links->Sender(id);
        try { sender.Stop(File(id,"sender-stop-flush")); }
        catch (const std::exception &e) { failed = true; Record("{\"phase\":\"sender_stop_failed\",\"id\":"+JsonString(id)+",\"error\":"+JsonString(e.what())+"}"); }
    };
    if (!options.mutants.empty()) { const auto &m = options.mutants[master]; if (Trigger(m.settings) && m.settings.readoutEnabled) stopSender(m.endpoint.id); }
    for (const auto &c : options.cobos) if (c.enabled) stopSender(c.id);
    if (failed) throw std::runtime_error("System sender flush incomplete; no Disconnect or receiver seal assertion");
    for (std::size_t i = 0; i < options.cobos.size(); ++i) if (options.cobos[i].enabled) {
        try { Execute(*cobos[i],plans[i].temperatures,options.cobos[i].id,"post-stop-temperatures",false); }
        catch (const std::exception &e) { Record("{\"phase\":\"temperature_read_failed\",\"id\":"+JsonString(options.cobos[i].id)+",\"error\":"+JsonString(e.what())+"}"); }
    }
    LifecycleAll(terminalLifecycle);
}
}
