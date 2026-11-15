#include "systempreparation_p.h"
#include <stdexcept>
#include <algorithm>

namespace GetDAQ {
using namespace SystemPreparationInternal;

void SystemPreparationClient::ReprepareCoBo(const std::string &id,const std::string &newAuditDirectory,bool stopped)
{
    auto &p=*fImpl;
    std::unique_lock<std::mutex> operation(p.operation,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("System preparation busy; maintenance is not queued");
    if (p.state!=State::Prepared || !stopped || !p.options.exclusiveOwnerConfirmed)
        throw std::logic_error("CoBo maintenance requires retained Prepared and freshly confirmed stopped data processes");
    const auto found=std::find_if(p.options.cobos.begin(),p.options.cobos.end(),[&](const SystemCoBoSettings &c) {return c.id==id;});
    if (found==p.options.cobos.end() || !found->enabled)
        throw std::invalid_argument("CoBo maintenance requires a selected active node from the original configuration");
    const auto index=std::size_t(found-p.options.cobos.begin());
    if (!p.preparations.at(index) || p.preparations[index]->Status()!=CoBoPrepareClient::Complete)
        throw std::logic_error("CoBo maintenance lost the original prepared board object");
    const auto previousLifecycle=p.coboLifecycle->StateAt(index);
    bool boardLifecycleTouched=false;
    p.Check();p.state=State::Maintaining;
    try {
        p.SetCoBoLifecycle(index,CoBoLifecycleState::Preparing);
        CreateHardwareAuditDirectory(newAuditDirectory);
        HardwareAudit audit(newAuditDirectory+"/maintenance.jsonl");
        audit.Append("{\"schema_version\":1,\"phase\":\"intent\",\"operation\":\"reprepare-cobo\",\"cobo_id\":"+JsonString(id)+
            ",\"original_prepare_directory\":"+JsonString(p.directory)+",\"configuration_sha256\":"+JsonString(p.options.configurationSha256)+
            ",\"artifact_sha256\":"+JsonString(p.options.artifactSha256)+",\"automatic_retry\":false,\"raw_data_handled\":false}");
        p.Record("{\"phase\":\"maintenance_begin\",\"cobo_id\":"+JsonString(id)+",\"audit_directory\":"+JsonString(newAuditDirectory)+"}");
        // 全部节点仅作元数据/停止观察；首个目标写入前先检查完整原计划。
        p.ReleasePreflight(false);
        for (const auto &hw:p.cobos)
            if (hw->Read("ctrl","mutantConfig","initDone"))
                throw std::runtime_error("CoBo maintenance requires every declared board not ready");
        const auto plan=BuildCoBoPreparePlan(found->prepare.settings);
        auto complete=plan.beforeSubscription;AppendHardwareSteps(complete,plan.afterSubscription);
        p.cobos[index]->Validate(complete);p.cobos[index]->Validate(p.configurePlans[index]);
        auto *oldAlarm=p.preparations[index]->Alarms();
        if (oldAlarm) oldAlarm->ValidateUnsubscribe();
        p.Check();
        audit.Append("{\"phase\":\"preflight_complete\",\"other_board_writes_sent\":false}");
        // 目标电源/时钟重置期间不能保留旧通知；绝不重置整个服务的订阅者。
        if (oldAlarm) {
            boardLifecycleTouched=true;
            audit.Append("{\"phase\":\"old_subscription_close_begin\"}");
            oldAlarm->Unsubscribe(newAuditDirectory+"/old-subscription-close.jsonl");
            audit.Append("{\"phase\":\"old_subscription_closed\",\"command_chain_sha256\":"+JsonString(oldAlarm->ClosedCommandAuditChainSha256())+"}");
        }
        // 先销毁已知关闭的旧监听器，使固定callback端口也可重新绑定。
        p.preparations[index].reset();
        p.preparations[index].reset(new CoBoPrepareClient(found->prepare));
        p.preparations[index]->Preflight(newAuditDirectory+"/board-prepare",p.configurePlans[index]);
        boardLifecycleTouched=true;
        p.preparations[index]->Prepare([&p] {p.Check();});
        p.Check();p.ReleasePreflight(false);
        for (const auto &hw:p.cobos)
            if (hw->Read("ctrl","mutantConfig","initDone"))
                throw std::runtime_error("CoBo maintenance unexpectedly made a board ready");
        audit.Append("{\"phase\":\"complete\",\"other_board_writes_sent\":false,\"system_alignment_repeated\":false,\"physical_alignment_verified\":false,\"configure_or_start_sent\":false}");
        p.SetCoBoLifecycle(index,CoBoLifecycleState::Prepared);
        p.Record("{\"phase\":\"maintenance_complete\",\"cobo_id\":"+JsonString(id)+",\"audit_directory\":"+JsonString(newAuditDirectory)+
            ",\"maintenance_chain_sha256\":"+JsonString(audit.ChainSha256())+"}");
        p.state=State::Prepared;
    } catch (...) {
        if(boardLifecycleTouched)p.MarkCoBoUnknown(index,id);
        else try {p.SetCoBoLifecycle(index,previousLifecycle);} catch (...) {}
        p.Fail();throw;
    }
}
void SystemPreparationClient::ConfigureCoBo(const std::string &id,const std::string &newAuditDirectory,bool stopped)
{
    auto &p=*fImpl;
    std::unique_lock<std::mutex> operation(p.operation,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("System preparation busy; CoBo Configure was not queued");
    if (p.state!=State::Prepared || !stopped || !p.options.exclusiveOwnerConfirmed)
        throw std::logic_error("CoBo Configure requires retained Prepared and freshly confirmed stopped data processes");
    const auto found=std::find_if(p.options.cobos.begin(),p.options.cobos.end(),[&](const SystemCoBoSettings &c) {return c.id==id;});
    if (found==p.options.cobos.end() || !found->enabled)
        throw std::invalid_argument("CoBo Configure requires a selected active node from the original configuration");
    const auto index=std::size_t(found-p.options.cobos.begin());
    if (p.coboLifecycle->StateAt(index)!=CoBoLifecycleState::Prepared)
        throw std::logic_error("CoBo Configure requires this board's known Prepared lifecycle; repeated Configure is rejected");
    if (!p.preparations.at(index) || p.preparations[index]->Status()!=CoBoPrepareClient::Complete)
        throw std::logic_error("CoBo Configure lost the original prepared board object");
    p.Check();p.state=State::Maintaining;bool hardwareAttempted=false;
    try {
        CreateHardwareAuditDirectory(newAuditDirectory);
        HardwareAudit audit(newAuditDirectory+"/configure-cobo.jsonl");
        audit.Append("{\"schema_version\":1,\"phase\":\"intent\",\"operation\":\"configure-cobo\",\"cobo_id\":"+JsonString(id)+
            ",\"configuration_sha256\":"+JsonString(p.options.configurationSha256)+",\"artifact_sha256\":"+JsonString(p.options.artifactSha256)+
            ",\"system_alignment_repeated\":false,\"mutant_configure_sent\":false,\"system_ready_claimed\":false,\"run_start_sent\":false,\"automatic_retry\":false}");
        p.Record("{\"phase\":\"cobo_configure_begin\",\"cobo_id\":"+JsonString(id)+",\"audit_directory\":"+JsonString(newAuditDirectory)+"}");
        // Confirm the complete retained topology is stopped and not ready before
        // any selected-board write; configuration itself never advances initDone.
        p.ReleasePreflight(false);
        if (p.cobos[index]->Name()!=found->prepare.expectedNodeName)
            throw std::runtime_error("CoBo Configure actual node name changed before selected-board operation");
        p.cobos[index]->Validate(p.configurePlans[index]);
        hardwareAttempted=true;
        p.SetCoBoLifecycle(index,CoBoLifecycleState::Configuring);
        p.cobos[index]->Execute(p.configurePlans[index],newAuditDirectory+"/hardware.jsonl",[&p] {p.Check();});
        p.Check();p.ReleasePreflight(false);
        audit.Append("{\"phase\":\"complete\",\"only_selected_cobo_written\":true,\"all_nodes_stopped_and_not_ready\":true,\"system_alignment_repeated\":false,\"mutant_configure_sent\":false,\"system_ready_claimed\":false,\"run_start_sent\":false}");
        p.Record("{\"phase\":\"cobo_configure_complete\",\"cobo_id\":"+JsonString(id)+",\"audit_directory\":"+JsonString(newAuditDirectory)+
            ",\"operation_chain_sha256\":"+JsonString(audit.ChainSha256())+"}");
        p.SetCoBoLifecycle(index,CoBoLifecycleState::Configured);
        p.state=State::Prepared;
    } catch (...) {if(hardwareAttempted){p.MarkCoBoUnknown(index,id);p.Fail();}else p.state=State::Prepared;throw;}
}

}
