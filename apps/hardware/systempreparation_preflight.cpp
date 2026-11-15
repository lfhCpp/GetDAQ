#include "systempreparation_p.h"
#include "hardwareclient.h"
#include <Ice/Ice.h>
#include <algorithm>
#include <set>
#include <stdexcept>

namespace GetDAQ {
namespace {
std::string NodeKey(const Ice::ObjectPrx &proxy) {
    if (!proxy || proxy->ice_getRouter() || proxy->ice_getLocator()) throw std::invalid_argument("System preparation requires direct node endpoints");
    std::vector<std::string> endpoints;
    for (const auto &e : proxy->ice_getEndpoints()) endpoints.push_back(e->toString());
    if (endpoints.empty() || endpoints.size() > 16) throw std::invalid_argument("Invalid bounded direct node endpoint set");
    std::sort(endpoints.begin(),endpoints.end());
    const auto id = proxy->ice_getIdentity();
    std::string key = JsonString(id.category)+JsonString(id.name)+JsonString(proxy->ice_getFacet());
    for (const auto &e : endpoints) key += std::to_string(e.size())+':'+e;
    return key;
}
struct ProxyParser {
    Ice::CommunicatorPtr communicator;
    explicit ProxyParser(const Ice::InitializationData &init) : communicator(Ice::initialize(init)) {}
    ~ProxyParser() { try { communicator->destroy(); } catch (...) {} }
};
}
void SystemPreparationClient::Impl::ValidateIdentities() const{
    // Canonical identity检查也覆盖无MuTaNT模式，不能用字符串不同作为节点不同的证明。
    Ice::InitializationData init; init.properties = Ice::createProperties();
    init.properties->setProperty("Ice.RetryIntervals","-1");
    ProxyParser parser(init);
    std::set<std::string> keys;
    const auto unique = [&](const std::string &proxy) {
        if (!keys.insert(NodeKey(parser.communicator->stringToProxy(proxy))).second) throw std::invalid_argument("Duplicate canonical system hardware node");
    };
    for (const auto &m : options.mutants) unique(m.endpoint.nodeProxy);
    for (const auto &c : options.cobos) unique(c.prepare.nodeProxy);
}

void SystemPreparationClient::Impl::Preflight() {
    ValidateIdentities();
    if (alignment) alignment->ValidateInterfaces(); // 先检查全部板端链路/TDC服务，尚未设置MuTaNT角色。
    for (std::size_t i = 0; i < options.mutants.size(); ++i) {
        const auto &m = options.mutants[i];
        std::unique_ptr<HardwareClient> hw(new HardwareClient(m.endpoint.nodeProxy,options.encodingMinor,options.timeoutMs));
        hw->UseDescription(m.endpoint.description);
        if (hw->Name() != m.endpoint.expectedNodeName) throw std::runtime_error("System MuTaNT actual node name mismatch");
        hw->Validate(mutantPreparePlans[i]);
        hw->Validate(MutantConfigure(m.settings)); // 对齐后只替换12位掩码，目标/方法已提前检查。
        hw->Validate(MutantWscaClock(false)); hw->Validate(MutantClockShelfMasks(0,0)); hw->Validate(MutantSyncPulse(true));
        hw->Validate(MutantSyncPulse(false)); mutants.push_back(std::move(hw));
    }
    for (std::size_t i = 0; i < options.cobos.size(); ++i) {
        const auto &c = options.cobos[i];
        std::unique_ptr<HardwareClient> hw(new HardwareClient(c.prepare.nodeProxy,options.encodingMinor,options.timeoutMs));
        hw->UseDescription(c.prepare.description);
        if (hw->Name() != c.prepare.expectedNodeName) throw std::runtime_error("System CoBo actual node name mismatch");
        hw->Validate(CoBoStopped()); hw->Validate(CoBoBit("initDone",c.enabled));
        hw->Validate(CoBoBit("mutantMode",c.enabled && !options.mutants.empty()));
        hw->Validate(CoBoBit("l2Trigger",c.enabled && level2));
        cobos.push_back(std::move(hw));
        if (preparations[i]) preparations[i]->Preflight(directory+"/cobo-"+std::to_string(i),configurePlans[i]);
    }
    // 即使不活动的CoBo也在受控拓扑内；不能为配置另一块板而修改正在采集的停用板。
    for (std::size_t i = 0; i < cobos.size(); ++i) Execute(*cobos[i],CoBoStopped(),options.cobos[i].id,"stopped-precondition");
    for (std::size_t i = 0; i < mutants.size(); ++i)
        Execute(*mutants[i],{Field(HardwareStep::ExpectValue,"CARD_B_EMC","TRIGGER_CTRL","run",0)},options.mutants[i].endpoint.id,"stopped-precondition");
    Record("{\"phase\":\"global_preflight_complete\",\"hardware_changed\":false,\"control_lock_verified\":false,\"data_process_stop_verified\":false}");
}

void SystemPreparationClient::Impl::ValidateConfigure() {
    for (std::size_t i = 0; i < cobos.size(); ++i) {
        Check(); const auto &c = options.cobos[i];
        if (cobos[i]->Name() != c.prepare.expectedNodeName) throw std::runtime_error("System CoBo actual node name changed before Configure");
        if (c.enabled) cobos[i]->Validate(configurePlans[i]);
        cobos[i]->Validate(CoBoBit("l2Trigger",c.enabled && level2));
        cobos[i]->Validate(CoBoBit("mutantMode",c.enabled && !mutants.empty()));
        cobos[i]->Validate(CoBoBit("initDone",c.enabled));
    }
    for (std::size_t i = 0; i < mutants.size(); ++i) {
        Check(); auto s = options.mutants[i].settings;
        if (mutants[i]->Name() != options.mutants[i].endpoint.expectedNodeName) throw std::runtime_error("System MuTaNT actual node name changed before Configure");
        s.gmcShelfMask = result.crates.at(i).gmcShelfMask; s.wscaShelfMask = result.crates.at(i).wscaShelfMask;
        mutants[i]->Validate(MutantConfigure(s));
        mutants[i]->Validate(MutantWscaClock(false)); mutants[i]->Validate(MutantWscaClock(true));
        mutants[i]->Validate(MutantClockShelfMasks(s.gmcShelfMask,s.wscaShelfMask));
        mutants[i]->Validate(MutantSyncPulse(true)); mutants[i]->Validate(MutantSyncPulse(false));
    }
    Record("{\"phase\":\"configure_metadata_revalidated\",\"hardware_changed\":false}");
}

void SystemPreparationClient::Impl::ReleasePreflight(bool powerOff) {
    if (cobos.size()!=options.cobos.size() || mutants.size()!=options.mutants.size())
        throw std::logic_error("System release requires the same completed preparation clients");
    for (std::size_t i=0;i<cobos.size();++i) {
        const auto &c=options.cobos[i]; Inventory(*cobos[i],c.prepare.description,c.prepare.expectedNodeName);
        cobos[i]->Validate(CoBoStopped()); cobos[i]->Validate(CoBoBit("initDone",false)); cobos[i]->Validate(CoBoBit("mutantMode",false));
        if (powerOff) {
            cobos[i]->Validate(CoBoUnprepareMonitoring()); cobos[i]->Validate(CoBoAsAdPowerOff());
            auto *alarm=preparations[i] ? preparations[i]->Alarms() : nullptr;
            if (alarm) {
                const auto s=alarm->Snapshot().subscription;
                if (s==CoBoAlarmSnapshot::Active) alarm->ValidateUnsubscribe();
                else if (s!=CoBoAlarmSnapshot::Closed) throw std::logic_error("System Unprepare has an unknown/incomplete owned alarm subscription");
            }
        }
    }
    for (std::size_t i=0;i<mutants.size();++i) {
        const auto &m=options.mutants[i]; Inventory(*mutants[i],m.endpoint.description,m.endpoint.expectedNodeName);
        mutants[i]->Validate({Field(HardwareStep::ExpectValue,"CARD_B_EMC","TRIGGER_CTRL","run",0)});
    }
    // 全部元数据/服务先检查，再只观察生产停止；保持告警不阻断负向释放。
    for (std::size_t i=0;i<cobos.size();++i) {
        if (cobos[i]->Read("ctrl","pipeCtrl","firmwareEnable") || cobos[i]->Read("ctrl","pipeCtrl","intEnable"))
            throw std::runtime_error("System release requires observed CoBo production stopped");
        if (powerOff && cobos[i]->Read("ctrl","mutantConfig","initDone"))
            throw std::runtime_error("System Unprepare requires CoBo not ready");
    }
    for (const auto &hw:mutants) if (hw->Read("CARD_B_EMC","TRIGGER_CTRL","run"))
        throw std::runtime_error("System release requires observed MuTaNT production stopped");
}
}
