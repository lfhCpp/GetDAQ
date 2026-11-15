#include "systemdescription.h"
#include <iostream>
#include <map>
#include <stdexcept>
namespace {
void Usage() {
    std::cerr<<"Usage: getdaq-system-describe validate-topology FILE POLICY [--current-topology FILE]\n"
        "       getdaq-system-describe check-system FILE POLICY --exclusive-stopped [--current-topology FILE]\n"
        "       getdaq-system-describe describe-system FILE POLICY NEW_AUDIT_DIRECTORY --exclusive-stopped [--current-topology FILE]\n"
        "POLICY: use-existing | create-empty | replace-existing (no implicit default).\n"
        "Describe clears all CoBo initDone/mutantMode after full metadata checks; no Prepare or ECC state commit.\n"
        "Replacement destroys metadata only; caller must own hardware and stop all board/sender/receiver DAQ first.\n";
}
GetDAQ::DescriptionPolicy Policy(const std::string &s) {
    using P=GetDAQ::DescriptionPolicy;
    if (s=="use-existing") return P::UseExisting;
    if (s=="create-empty") return P::CreateEmpty;
    if (s=="replace-existing") return P::ReplaceExisting;
    throw std::invalid_argument("Explicit description policy must be use-existing, create-empty or replace-existing");
}
}
int main(int argc,char **argv) {
    using namespace GetDAQ;
    if (argc==2 && std::string(argv[1])=="--help") { Usage(); return 0; }
    if (argc<4) { Usage(); return 2; }
    const std::string command=argv[1];
    const bool offline=command=="validate-topology",check=command=="check-system",describe=command=="describe-system";
    if ((!offline && !check && !describe) || (describe && argc<6)) { Usage(); return 2; }
    try {
        const auto policy=Policy(argv[3]); bool exclusive=false; std::string current;
        for (int i=describe ? 5 : 4;i<argc;++i) {
            const std::string option=argv[i];
            if (option=="--exclusive-stopped" && !exclusive) exclusive=true;
            else if (option=="--current-topology" && current.empty() && i+1<argc) current=argv[++i];
            else throw std::invalid_argument("Unknown, repeated or incomplete description option");
        }
        if (offline && exclusive) throw std::invalid_argument("Offline validation does not accept an ownership declaration");
        if (!offline && !exclusive) throw std::invalid_argument("Check/Describe requires --exclusive-stopped; verify actual ownership and stopped data processes first");
        if (!current.empty() && policy!=DescriptionPolicy::ReplaceExisting) throw std::invalid_argument("Current topology is only valid for explicit replacement");
        auto topology=LoadSystemAlignmentOptions(argv[2],true);
        // Offline只校验格式/计划，不把这两个内部检查条件输出为实际授权或硬件停止。
        topology.exclusiveOwnerConfirmed=topology.daqStoppedConfirmed=true;
        auto options=BuildSystemDescriptionOptions(topology,policy);
        if (!current.empty()) {
            auto old=LoadSystemAlignmentOptions(current,true); old.exclusiveOwnerConfirmed=old.daqStoppedConfirmed=true;
            const auto previous=BuildSystemDescriptionOptions(old,DescriptionPolicy::UseExisting);
            if (previous.nodes.size()!=options.nodes.size()) throw std::invalid_argument("Current/desired topology counts differ");
            std::map<std::string,const SystemDescriptionNode *> byId;
            const auto key=[](const SystemDescriptionNode &n) { return std::to_string(unsigned(n.role))+':'+n.id; };
            for (const auto &n:previous.nodes) byId.emplace(key(n),&n);
            for (auto &n:options.nodes) {
                const auto found=byId.find(key(n));
                if (found==byId.end() || found->second->nodeProxy!=n.nodeProxy)
                    throw std::invalid_argument("Current/desired topology IDs or exact node proxies differ");
                n.currentDescription=found->second->description; byId.erase(found);
            }
            if (!byId.empty()) throw std::invalid_argument("Incomplete current description binding");
            ValidateSystemDescriptionOptions(options);
        }
        if (offline) {
            std::cout<<"{\"valid\":true,\"nodes\":"<<options.nodes.size()<<",\"hardware_connected\":false,\"ownership_declared\":false,\"hardware_stopped_verified\":false}\n";
            return 0;
        }
        SystemDescriptionClient client(options);
        if (check) {
            client.Validate();
            std::cout<<"{\"checked\":true,\"nodes\":"<<options.nodes.size()<<",\"mutating_commands_sent\":false,\"register_write_sent\":false,\"control_lock_verified\":false,\"sender_stopped_verified\":false,\"empty_node_hardware_stopped_verified\":false}\n";
        } else {
            client.Describe(argv[4]);
            std::cout<<"{\"phase\":\"described\",\"nodes\":"<<options.nodes.size()<<",\"audit_directory\":"<<JsonString(argv[4])
                <<",\"control_lock_verified\":false,\"actual_loaded_configuration_verified\":false,\"production_ecc_state_committed\":false}\n";
        }
        return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
}
