// Host description flow adapted from the received CEA GET/Mordicus sources.
// Original CeCILL license: third_party/licenses/getbench/COPYING.
#include "systemdescription.h"
#include "systemdescription_p.h"
#include "hardwareaudit.h"
#include "strictjson.h"
#include <Ice/Ice.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <set>
#include <stdexcept>
namespace GetDAQ {
using namespace SystemDescriptionInternal;
class SystemDescriptionClient::Impl {
public:
    SystemDescriptionOptions options;
    std::atomic<State> state{State::Fresh};
    std::mutex operation;
    std::vector<std::unique_ptr<HardwareClient>> clients;
    std::unique_ptr<HardwareAudit> audit;
    std::string directory;
    unsigned int sequence=0;
    bool readOnlyVerified=false;
    std::vector<std::pair<std::string,std::string>> verificationChains;
    void Record(const std::string &row) {
        const std::uint64_t limit=64*1024*1024;
        if (audit->Bytes()>limit || row.size()+1>limit-audit->Bytes()) throw std::runtime_error("System description audit budget exceeded");
        audit->Append(row);
    }
    void Preflight() {
        clients.clear();
        // 全部节点先接通与核对；末尾节点失败不能留下前面节点已被销毁。
        for (const auto &n:options.nodes) {
            std::unique_ptr<HardwareClient> hw(new HardwareClient(n.nodeProxy,options.encodingMinor,options.timeoutMs));
            if (n.policy==DescriptionPolicy::CreateEmpty) {
                if (!hw->Devices().empty()) throw std::runtime_error("Description create-empty target is not empty");
            } else {
                Inventory(*hw,n.policy==DescriptionPolicy::ReplaceExisting ? n.currentDescription : n.description);
                Stopped(*hw,n);
                if (n.policy==DescriptionPolicy::UseExisting && n.role==SystemDescriptionNode::Role::CoBo) {
                    hw->Validate(ClearBit("initDone")); hw->Validate(ClearBit("mutantMode"));
                }
            }
            clients.push_back(std::move(hw));
        }
    }
    void AllFinal() {
        for (std::size_t i=0;i<clients.size();++i) {
            Inventory(*clients[i],options.nodes[i].description);
            Stopped(*clients[i],options.nodes[i]);
            if (options.nodes[i].role==SystemDescriptionNode::Role::CoBo) {
                clients[i]->Validate(ClearBit("initDone")); clients[i]->Validate(ClearBit("mutantMode"));
            }
        }
    }
    void Action(std::size_t i,const char *stage,const std::function<void(const std::string &)> &action) {
        const auto file="step-"+std::to_string(sequence++)+".jsonl";
        const auto context="\"id\":"+JsonString(options.nodes[i].id)+",\"role\":"+JsonString(RoleName(options.nodes[i].role))+
            ",\"stage\":"+JsonString(stage)+",\"audit_file\":"+JsonString(file);
        Record("{\"phase\":\"node_begin\","+context+"}");
        action(directory+'/'+file);
        Record("{\"phase\":\"node_complete\","+context+"}");
    }
};
SystemDescriptionClient::SystemDescriptionClient(const SystemDescriptionOptions &o) : fImpl(new Impl) {
    ValidateSystemDescriptionOptions(o); fImpl->options=o;
}
SystemDescriptionClient::~SystemDescriptionClient() {}
void SystemDescriptionClient::Validate() {
    auto &p=*fImpl; std::lock_guard<std::mutex> lock(p.operation);
    if (p.state!=State::Fresh) throw std::logic_error("Description Validate requires Fresh");
    p.Preflight();
}
void SystemDescriptionClient::VerifyExistingDescription(const std::string &directory)
{
    auto &p=*fImpl;
    std::lock_guard<std::mutex> lock(p.operation);
    if(p.state!=State::Fresh)throw std::logic_error("Description verification requires a fresh observation object");
    for(const auto &node:p.options.nodes)
        if(node.policy!=DescriptionPolicy::UseExisting)
            throw std::logic_error("Description verification cannot create or replace hardware");
    CreateHardwareAuditDirectory(directory);
    p.directory=directory;
    p.audit.reset(new HardwareAudit(directory+"/system-description.jsonl"));
    p.Record("{\"schema_version\":1,\"phase\":\"verification_intent\",\"read_only\":true,\"actual_loaded_configuration_verified\":false,\"control_lock_verified\":false}");
    try {
        p.Preflight();
        p.AllFinal();
        for(std::size_t i=0;i<p.options.nodes.size();++i) {
            const auto &node=p.options.nodes[i];
            Stopped(*p.clients[i],node);
            if(node.role==SystemDescriptionNode::Role::CoBo) {
                std::vector<HardwareStep> checks;
                checks.push_back(Bit("initDone",HardwareStep::ExpectValue));
                checks.push_back(Bit("mutantMode",HardwareStep::ExpectValue));
                p.Action(i,"verify-not-ready",[&](const std::string &file) {
                    p.clients[i]->Execute(checks,file);
                    p.verificationChains.emplace_back(file,p.clients[i]->CompletedAuditChainSha256());
                });
            }
            p.Record("{\"phase\":\"node_verified\",\"id\":"+JsonString(node.id)+",\"role\":"+JsonString(RoleName(node.role))+",\"metadata_and_stopped_bits_verified\":true}");
        }
        // Recheck metadata and all production/ready fields after the complete
        // observation. This is not a multi-board atomic or global-owner proof.
        p.AllFinal();
        for(std::size_t i=0;i<p.options.nodes.size();++i)
            if(p.options.nodes[i].role==SystemDescriptionNode::Role::CoBo) {
                if(p.clients[i]->Read("ctrl","mutantConfig","initDone")!=0 ||
                   p.clients[i]->Read("ctrl","mutantConfig","mutantMode")!=0)
                    throw std::runtime_error("Description ready bits changed during verification");
            }
        p.Record("{\"phase\":\"description_verified\",\"read_only\":true,\"actual_loaded_configuration_verified\":false,\"production_ecc_state_committed\":false}");
        p.verificationChains.emplace_back(directory+"/system-description.jsonl",p.audit->ChainSha256());
        p.readOnlyVerified=true;p.state=State::Described;
    } catch(...) {
        p.state=State::Failed;
        try {p.Record("{\"phase\":\"verification_failed\",\"read_only\":true,\"production_ecc_state_committed\":false}");}catch(...){}
        throw;
    }
}
std::vector<std::pair<std::string,std::string>> SystemDescriptionClient::VerificationAuditChains() const
{
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);
    if(!p.readOnlyVerified || p.state!=State::Described)
        throw std::logic_error("No complete read-only description verification audit");
    return p.verificationChains;
}
void SystemDescriptionClient::Describe(const std::string &directory) {
    auto &p=*fImpl; std::lock_guard<std::mutex> lock(p.operation);
    if (p.state!=State::Fresh) throw std::logic_error("System Describe cannot be replayed; explicit recovery required");
    p.Preflight(); // 无修改失败仍Fresh；只有真实执行进入一次性状态。
    CreateHardwareAuditDirectory(directory); p.directory=directory;
    p.audit.reset(new HardwareAudit(directory+"/system-description.jsonl"));
    std::string nodes="[";
    for (std::size_t i=0;i<p.options.nodes.size();++i) {
        const auto &n=p.options.nodes[i]; if (i) nodes+=',';
        nodes+="{\"id\":"+JsonString(n.id)+",\"role\":"+JsonString(RoleName(n.role))+",\"node_proxy\":"+JsonString(n.nodeProxy)+
            ",\"policy\":"+JsonString(PolicyName(n.policy))+",\"description\":"+HardwarePolicyJson(n.description);
        if (n.policy==DescriptionPolicy::ReplaceExisting) nodes+=",\"current_description\":"+HardwarePolicyJson(n.currentDescription);
        nodes+='}';
    }
    nodes+=']';
    p.Record("{\"schema_version\":1,\"phase\":\"describe_intent\",\"nodes\":"+nodes+
        ",\"encoding_minor\":"+std::to_string(p.options.encodingMinor)+",\"timeout_ms\":"+std::to_string(p.options.timeoutMs)+
        ",\"initial_exclusive_stopped_owner_declared\":true,\"control_lock_verified\":false,\"empty_node_hardware_stopped_verified\":false"+
        ",\"data_sender_state_verified\":false,\"actual_loaded_configuration_verified\":false,\"production_ecc_state_committed\":false}");
    p.state=State::Describing;
    try {
        for (std::size_t i=0;i<p.options.nodes.size();++i) {
            const auto &n=p.options.nodes[i]; auto &hw=*p.clients[i];
            if (n.policy==DescriptionPolicy::ReplaceExisting) {
                Stopped(hw,n); // 接近实际destroy再检查，仍不能代替真实控制权。
                p.Action(i,"destroy",[&](const std::string &file) { hw.DestroyDescription(file); });
            }
            if (n.policy!=DescriptionPolicy::UseExisting)
                p.Action(i,"create",[&](const std::string &file) { hw.Create(n.description,file); });
        }
        p.AllFinal(); // 新描述创建完也必须全节点核对/观察，才允许第一条状态位写入。
        p.Record("{\"phase\":\"all_final_metadata_and_production_stop_observed\",\"sender_stopped_verified\":false}");
        // 保留旧SystemManager::describe的全部initDone→全部mutantMode顺序，包括停用实验板。
        for (const auto *field:{"initDone","mutantMode"})
            for (std::size_t i=0;i<p.options.nodes.size();++i) if (p.options.nodes[i].role==SystemDescriptionNode::Role::CoBo) {
                auto &hw=*p.clients[i]; Stopped(hw,p.options.nodes[i]);
                p.Action(i,field,[&](const std::string &file) { hw.Execute(ClearBit(field),file); });
            }
        p.Record("{\"phase\":\"described\",\"metadata_verified\":true,\"cobo_not_ready_observed\":true,\"board_permissions_verified\":false"+
            std::string(",\"actual_loaded_configuration_verified\":false,\"production_ecc_state_committed\":false}"));
        p.state=State::Described;
    } catch (...) {
        p.state=State::Failed;
        try { p.Record("{\"phase\":\"failed\",\"automatic_retry\":false,\"automatic_cleanup\":false,\"hardware_stop_sent\":false,\"production_ecc_state_committed\":false}"); } catch (...) {}
        throw;
    }
}
SystemDescriptionClient::State SystemDescriptionClient::GetState() const { return fImpl->state.load(); }
SystemDescriptionOptions SystemDescriptionClient::Configuration() const { return fImpl->options; }
}
