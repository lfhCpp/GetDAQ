// Host coordination adapted from received CEA GET AmcSlotManager/SystemManager.
// Original CeCILL license: third_party/licenses/getbench/COPYING.
#include "systemalignment_p.h"
#include "hardwareaudit.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace GetDAQ {
namespace {
void Text(const std::string &value,std::size_t limit,const char *name)
{
    if(value.empty() || value.size()>limit)throw std::invalid_argument(std::string("Invalid bounded ")+name);
    for(unsigned char c:value)if(c<32 || c==127)throw std::invalid_argument(std::string("Control byte in ")+name);
}
}

namespace SystemAlignmentInternal {
HardwareStep ReadStep(const char *device,const char *reg,const char *field)
{
    HardwareStep step;step.kind=HardwareStep::ReadValue;step.device=device;step.reg=reg;step.field=field;
    step.requireBoundDescription=true;return step;
}

std::vector<HardwareStep> MutantPreflight()
{
    auto steps=MutantWscaClock(true);
    AppendHardwareSteps(steps,MutantClockShelfMasks(0,0));
    steps.push_back(ReadStep("CARD_B_EMC","TRIGGER_CTRL","run"));
    steps.push_back(ReadStep("CARD_B_EMC","GLOBAL_CONF","M/S"));
    steps.push_back(ReadStep("CARD_B_EMC","GMC_WSCA_SEL","GMC"));
    steps.push_back(ReadStep("CARD_B_EMC","GMC_WSCA_SEL","WSCA"));
    steps.push_back(ReadStep("CARD_B_EMC","GMC_WSCA_SEL","PromptWSCA"));
    steps.push_back(ReadStep("GPIO_CLK_TREE","CLOCK_TREE_CONFIG1","amc_clk1_out_en"));
    steps.push_back(ReadStep("GPIO_CLK_TREE","CLOCK_TREE_CONFIG1","amc_clk2_out_en"));
    return steps;
}

std::string EndpointKey(const Ice::ObjectPrx &proxy)
{
    std::vector<std::string> endpoints;
    for(const auto &endpoint:proxy->ice_getEndpoints())endpoints.push_back(endpoint->toString());
    if(endpoints.empty() || endpoints.size()>16 || proxy->ice_getRouter() || proxy->ice_getLocator())
        throw std::invalid_argument("Alignment requires direct endpoint proxies without router/locator");
    std::sort(endpoints.begin(),endpoints.end());
    std::string key;
    for(const auto &endpoint:endpoints)key+=std::to_string(endpoint.size())+":"+endpoint;
    return key;
}

std::string ObjectKey(const Ice::ObjectPrx &proxy)
{
    const auto id=proxy->ice_getIdentity();
    return JsonString(id.category)+JsonString(id.name)+JsonString(proxy->ice_getFacet())+EndpointKey(proxy);
}
}

void ValidateSystemAlignmentOptions(const SystemAlignmentOptions &options)
{
    if(options.mutants.empty() || options.mutants.size()>16 || options.cobos.size()>192 ||
       options.encodingMinor<0 || options.encodingMinor>1 || options.timeoutMs<100 || options.timeoutMs>60000)
        throw std::invalid_argument("Alignment supports 1..16 MuTaNT/0..192 CoBo, encoding 1.0/1.1 and timeout 100..60000 ms");
    unsigned int masters=0,active=0;
    std::set<std::string> ids,nodes,services;
    for(const auto &mutant:options.mutants) {
        Text(mutant.id,128,"MuTaNT ID");Text(mutant.expectedNodeName,128,"expected node name");
        Text(mutant.nodeProxy,4096,"hardware proxy");Text(mutant.alignmentProxy,4096,"alignment proxy");
        ValidateHardwareDescription(mutant.description);
        if(!ids.insert(mutant.id).second || !nodes.insert(mutant.nodeProxy).second || !services.insert(mutant.alignmentProxy).second)
            throw std::invalid_argument("Duplicate alignment topology ID/node/service proxy");
        masters+=mutant.master;
    }
    for(const auto &cobo:options.cobos) {
        Text(cobo.id,128,"CoBo ID");Text(cobo.expectedNodeName,128,"expected node name");
        Text(cobo.nodeProxy,4096,"hardware proxy");Text(cobo.linkProxy,4096,"link proxy");
        ValidateHardwareDescription(cobo.description);
        if(!ids.insert(cobo.id).second || !nodes.insert(cobo.nodeProxy).second || !services.insert(cobo.linkProxy).second)
            throw std::invalid_argument("Duplicate alignment topology ID/node/service proxy");
        active+=cobo.enabled;
    }
    if(masters!=1)throw std::invalid_argument("Alignment topology requires exactly one explicit master MuTaNT");
    if(!active)throw std::invalid_argument("Alignment topology requires at least one active CoBo");
    if(active>options.mutants.size()*12)throw std::invalid_argument("Active CoBo count exceeds available AMC slots");
}

SystemAlignmentClient::SystemAlignmentClient(const SystemAlignmentOptions &options):fImpl(new Impl)
{
    ValidateSystemAlignmentOptions(options);fImpl->options=options;
}
SystemAlignmentClient::~SystemAlignmentClient() {}
SystemAlignmentClient::State SystemAlignmentClient::GetState() const { return fImpl->state.load(); }
void SystemAlignmentClient::ValidateInterfaces()
{
    std::lock_guard<std::mutex> lock(fImpl->operation);
    if(fImpl->state!=State::Fresh)throw std::logic_error("Alignment interface validation requires a fresh client");
    try { fImpl->BindInterfaces(); }
    catch(...) { fImpl->state=State::Failed;throw; }
}
SystemAlignmentResult SystemAlignmentClient::Align(const std::string &newAuditDirectory)
{
    std::lock_guard<std::mutex> lock(fImpl->operation);
    if(fImpl->state!=State::Fresh)throw std::logic_error("Alignment client is single-use; do not replay a completed/failed operation");
    if(!fImpl->options.exclusiveOwnerConfirmed || !fImpl->options.daqStoppedConfirmed)
        throw std::logic_error("Alignment requires explicit exclusive ownership and stopped DAQ from the controlling backend");
    fImpl->state=State::Running;
    auto &p=*fImpl;
    try {
        CreateHardwareAuditDirectory(newAuditDirectory);p.directory=newAuditDirectory;
        p.audit.reset(new HardwareAudit(p.directory+"/system.jsonl"));
        std::string nodes="[";
        for(const auto &mutant:p.options.mutants) {
            if(nodes.size()>1)nodes+=',';
            nodes+="{\"id\":"+JsonString(mutant.id)+",\"expected_node_name\":"+JsonString(mutant.expectedNodeName)+
                ",\"node_proxy\":"+JsonString(mutant.nodeProxy)+",\"alignment_proxy\":"+JsonString(mutant.alignmentProxy)+
                ",\"master\":"+(mutant.master ? "true" : "false")+",\"bound_description\":"+HardwarePolicyJson(mutant.description)+"}";
        }
        std::string cobos="[";
        for(const auto &cobo:p.options.cobos) {
            if(cobos.size()>1)cobos+=',';
            cobos+="{\"id\":"+JsonString(cobo.id)+",\"expected_node_name\":"+JsonString(cobo.expectedNodeName)+
                ",\"node_proxy\":"+JsonString(cobo.nodeProxy)+",\"link_proxy\":"+JsonString(cobo.linkProxy)+
                ",\"enabled\":"+(cobo.enabled ? "true" : "false")+",\"bound_description\":"+HardwarePolicyJson(cobo.description)+"}";
        }
        p.Record("{\"schema_version\":1,\"phase\":\"intent\",\"operation\":\"system-alignment\",\"encoding\":\"1."+
            std::to_string(p.options.encodingMinor)+"\",\"timeout_ms\":"+std::to_string(p.options.timeoutMs)+",\"mutants\":"+nodes+
            "],\"cobos\":"+cobos+"],\"owner_confirmed_by_caller\":true,\"daq_stopped_confirmed_by_caller\":true,\"physical_alignment_verified\":false}");
        p.Preflight();
        SystemAlignmentResult result;
        p.Links(CoBoLinkMode::Normal,false);
        for(std::size_t i=0;i<p.options.cobos.size();++i)if(p.options.cobos[i].enabled) {
            const auto gmc=p.Locate(i,AlignmentClock::Gmc),wsca=p.Locate(i,AlignmentClock::Wsca);
            if(gmc.mutantId!=wsca.mutantId || gmc.amcSlot!=wsca.amcSlot)
                throw std::runtime_error("GMC/WSCA independently located different AMC slots");
            result.located.push_back(gmc);
            p.Record("{\"phase\":\"located\",\"cobo_id\":"+JsonString(gmc.coboId)+",\"mutant_id\":"+
                JsonString(gmc.mutantId)+",\"amc_slot\":"+std::to_string(gmc.amcSlot)+"}");
        }
        for(const auto &mutant:p.options.mutants) { AlignmentCrateResult crate;crate.mutantId=mutant.id;result.crates.push_back(crate); }
        for(const auto clock:{AlignmentClock::Gmc,AlignmentClock::Wsca}) {
            p.Links(CoBoLinkMode::Disconnected,false);
            p.Links(clock==AlignmentClock::Gmc ? CoBoLinkMode::GmcFeedback : CoBoLinkMode::WscaFeedback,true);
            for(std::size_t i=0;i<p.options.mutants.size();++i) {
                const auto slots=p.Slots(i,clock);
                if(clock==AlignmentClock::Gmc)result.crates[i].gmc=slots;else result.crates[i].wsca=slots;
            }
            p.Links(CoBoLinkMode::Normal,false);
        }
        std::size_t master=0;
        for(std::size_t i=0;i<p.options.mutants.size();++i) {
            auto &crate=result.crates[i];
            crate.gmcShelfMask=unsigned(p.mutantHardware[i]->Read("GPIO_CLK_TREE","CLOCK_TREE_CONFIG1","amc_clk1_out_en"));
            crate.wscaShelfMask=unsigned(p.mutantHardware[i]->Read("GPIO_CLK_TREE","CLOCK_TREE_CONFIG1","amc_clk2_out_en"));
            p.Record("{\"phase\":\"saved_shelf_masks\",\"mutant_id\":"+JsonString(crate.mutantId)+
                ",\"gmc_mask\":"+std::to_string(crate.gmcShelfMask)+",\"wsca_mask\":"+std::to_string(crate.wscaShelfMask)+"}");
            if(p.options.mutants[i].master)master=i;
        }
        ValidateAlignmentResults(result.crates,result.located);
        p.Record("{\"phase\":\"crate_results_consistent\",\"physical_alignment_verified\":false}");
        if(p.options.mutants.size()>1) {
            p.Links(CoBoLinkMode::Disconnected,false);p.Links(CoBoLinkMode::Normal,true);
            for(std::size_t slave=0;slave<p.options.mutants.size();++slave)if(slave!=master)
                for(const auto clock:{AlignmentClock::Gmc,AlignmentClock::Wsca}) {
                    p.Links(clock==AlignmentClock::Gmc ? CoBoLinkMode::GmcFeedback : CoBoLinkMode::WscaFeedback,true);
                    for(unsigned int step=1;step<=3;++step) { p.Multi(master,clock,step,result);p.Multi(slave,clock,step,result); }
                }
            for(std::size_t i=0;i<p.options.mutants.size();++i)p.Wsca(i);
            p.Links(CoBoLinkMode::Normal,false);
        }
        // 旧SystemManager::prepare在机箱间对齐之后，无论单/多机箱都再设一次WSCA。
        for(std::size_t i=0;i<p.options.mutants.size();++i)p.Wsca(i);
        p.Record("{\"phase\":\"complete\",\"board_procedures_acknowledged\":true,\"reported_results_consistent\":true,\"physical_alignment_verified\":false,\"full_prepare\":false,\"sync_pulse_sent\":false}");
        p.state=State::Complete;return result;
    } catch(const std::exception &) {
        p.state=State::Failed;
        if(p.audit) { try { p.Record("{\"phase\":\"failed\",\"automatic_retry\":false,\"automatic_restoration\":false,\"full_prepare\":false}"); } catch(...) {} }
        throw;
    } catch(...) { p.state=State::Failed;throw; }
}
}
