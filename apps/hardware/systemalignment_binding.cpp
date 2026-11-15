#include "systemalignment_p.h"
#include <set>
#include <stdexcept>

namespace GetDAQ {
using namespace SystemAlignmentInternal;

void SystemAlignmentClient::Impl::BindInterfaces() {
    if(interfacesValidated)return;
    Ice::InitializationData init;init.properties=Ice::createProperties();
    init.properties->setProperty("Ice.RetryIntervals","-1");
    init.properties->setProperty("Ice.Override.ConnectTimeout",std::to_string(options.timeoutMs));
    init.properties->setProperty("Ice.Override.Timeout",std::to_string(options.timeoutMs));
    init.properties->setProperty("Ice.MessageSizeMax","64");
    communicator=Ice::initialize(init);
    std::set<std::string> nodeKeys,serviceKeys;
    const auto bind=[&](const std::string &node,const std::string &service) {
        const auto n=Options(communicator->stringToProxy(node)),s=Options(communicator->stringToProxy(service));
        if(!n || !s)throw std::invalid_argument("Empty topology proxy");
        if(EndpointKey(n)!=EndpointKey(s))throw std::invalid_argument("Hardware and its manager must use the same explicit endpoint set");
        if(!nodeKeys.insert(ObjectKey(n)).second || !serviceKeys.insert(ObjectKey(s)).second)
            throw std::invalid_argument("Canonical topology proxies contain duplicate objects");
        return s;
    };
    for(const auto &m:options.mutants) {
        const auto service=get::mutant::AlignmentServerPrx::checkedCast(bind(m.nodeProxy,m.alignmentProxy));
        if(!service)throw std::runtime_error("MuTaNT object does not implement AlignmentServer");
        servers.push_back(Options(service));
        std::unique_ptr<HardwareClient> hardware(new HardwareClient(m.nodeProxy,options.encodingMinor,options.timeoutMs));
        hardware->UseDescription(m.description);
        if(hardware->Name()!=m.expectedNodeName)throw std::runtime_error("MuTaNT node name differs from explicitly expected name");
        hardware->Validate(MutantPreflight());mutantHardware.push_back(std::move(hardware));
    }
    for(const auto &c:options.cobos) {
        const auto service=get::cobo::MutantLinkManagerPrx::checkedCast(bind(c.nodeProxy,c.linkProxy));
        if(!service)throw std::runtime_error("CoBo object does not implement MutantLinkManager");
        links.push_back(Options(service));
        std::unique_ptr<HardwareClient> hardware(new HardwareClient(c.nodeProxy,options.encodingMinor,options.timeoutMs));
        hardware->UseDescription(c.description);
        if(hardware->Name()!=c.expectedNodeName)throw std::runtime_error("CoBo node name differs from explicitly expected name");
        hardware->Validate({ReadStep("ctrl","mutantConfig","initDone"),ReadStep("ctrl","pipeCtrl","firmwareEnable")});
        coboHardware.push_back(std::move(hardware));
    }
    interfacesValidated=true;
}

void SystemAlignmentClient::Impl::Preflight() {
    BindInterfaces();
    // 所有节点/权限/类型先通过，然后读取运行前提；任何失败均没有链路或对齐写入。
    for(std::size_t i=0;i<options.mutants.size();++i) {
        const auto run=mutantHardware[i]->Read("CARD_B_EMC","TRIGGER_CTRL","run");
        const auto master=mutantHardware[i]->Read("CARD_B_EMC","GLOBAL_CONF","M/S");
        const auto gmc=mutantHardware[i]->Read("CARD_B_EMC","GMC_WSCA_SEL","GMC");
        const auto wsca=mutantHardware[i]->Read("CARD_B_EMC","GMC_WSCA_SEL","WSCA");
        const auto prompt=mutantHardware[i]->Read("CARD_B_EMC","GMC_WSCA_SEL","PromptWSCA");
        Record("{\"phase\":\"precondition\",\"mutant_id\":"+JsonString(options.mutants[i].id)+",\"run\":"+std::to_string(run)+
            ",\"master\":"+std::to_string(master)+",\"gmc_source\":"+std::to_string(gmc)+",\"wsca_source\":"+std::to_string(wsca)+",\"prompt_wsca\":"+std::to_string(prompt)+"}");
        if(run!=0)throw std::runtime_error("MuTaNT run must be zero before alignment");
        if(master!=unsigned(options.mutants[i].master) || gmc!=0 ||
           (options.mutants[i].master ? wsca!=0 || prompt!=unsigned(options.mutants.size()>1) : wsca<1 || wsca>2 || prompt!=0))
            throw std::runtime_error("MuTaNT prepared role/clock sources differ from explicit topology");
    }
    for(std::size_t i=0;i<options.cobos.size();++i) {
        const auto initialized=coboHardware[i]->Read("ctrl","mutantConfig","initDone");
        const auto running=coboHardware[i]->Read("ctrl","pipeCtrl","firmwareEnable");
        Record("{\"phase\":\"precondition\",\"cobo_id\":"+JsonString(options.cobos[i].id)+
            ",\"init_done\":"+std::to_string(initialized)+",\"firmware_enabled\":"+std::to_string(running)+"}");
        if(initialized || running)throw std::runtime_error("CoBo initDone/firmwareEnable must both be zero before alignment");
    }
    Record("{\"phase\":\"preflight_complete\",\"sender_stopped_verified\":false,\"ownership_verified\":false}");
}
}
