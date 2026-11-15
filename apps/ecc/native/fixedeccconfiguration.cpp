#include "fixedeccconfiguration.h"
#include "fixedeccconfiguration_inputs_p.h"
#include "configurationsha256.h"
#include "planio.h"
#include "supervisorio.h"
#include "systemexperimentbinding.h"
#include "configurationpublication.h"
#include "configurationselection.h"
#include <arpa/inet.h>
#include <atomic>
#include <map>
#include <set>
#include <stdexcept>
namespace GetDAQ {
namespace {
using namespace SupervisorIO;
DescriptionPolicy Policy(const std::string &name) {
    if (name=="use-existing") return DescriptionPolicy::UseExisting;
    if (name=="create-empty") return DescriptionPolicy::CreateEmpty;
    if (name=="replace-existing") return DescriptionPolicy::ReplaceExisting;
    throw std::invalid_argument("Fixed ECC requires explicit description policy");
}
void Array(const JsonValue &value,std::size_t maximum) {
    if (value.type!=JsonValue::Array || value.items.size()>maximum) throw std::invalid_argument("Invalid bounded fixed ECC array");
}
}
class FixedEccConfiguration::Impl {
public:
    NativeEccOptions controller;
    RemoteReceiverOptions receiver;
    std::string binding,serviceId,clientId,bindAddress,credentialFile;
    unsigned int port=0;
    bool alarmStopEnabled=false;
    const bool controlAccessValidated;
    unsigned int alarmPollIntervalMs=100;
    bool receiverStopEnabled=false;
    unsigned int receiverPollIntervalMs=1000;
    FixedEccConfigurationInputs inputs;
    std::set<std::string> descriptions,usedDescriptions;
    std::unique_ptr<SupervisorIO::Directory> publicationDirectory;
    std::string selectionJson="null";
    mutable std::atomic<bool> fault{false};
    const FixedEccConfigurationInputs::File &Reference(const JsonValue &v,std::size_t maximum) {
        ExactSupervisorFields(v,{"path","sha256"}); const auto path=String(v.At("path")),sha=String(v.At("sha256")); Digest(sha); return inputs.Add(path,maximum,sha);
    }
    explicit Impl(const std::string &configuration,bool control) : controlAccessValidated(control) {
        const auto v=ParseStrictJson(inputs.Add(configuration,1024*1024).bytes);
        auto fields=std::set<std::string>{"schema","kind","service_id","client_id","bind","port","credential_file","audit_root","deployment_id",
            "experiment_artifact","topology","hardware_descriptions","description_policy","current_topology","exclusive_owner_confirmed","data_processes_stopped",
            "cobo_policies","senders","remote_receivers"};
        if (v.members.count("alarm_stop_policy")) fields.insert("alarm_stop_policy");
        if (v.members.count("host_control_ownership")) fields.insert("host_control_ownership");
        if (v.members.count("receiver_stop_policy")) fields.insert("receiver_stop_policy");
        if (v.members.count("configuration_publication")) { fields.insert("configuration_publication"); inputs.SetTotalLimit(256*1024*1024); }
        if (v.members.count("configuration_selection")) {
            fields.insert("configuration_selection");
            if (!v.members.count("configuration_publication")) throw std::invalid_argument("ECC selection requires configuration publication");
        }
        if(v.members.count("deployment_origin"))fields.insert("deployment_origin");
        ExactSupervisorFields(v,fields);
        if (v.members.count("receiver_stop_policy")) {
            const auto &policy=v.At("receiver_stop_policy"); ExactSupervisorFields(policy,{"enabled","poll_interval_ms"});
            receiverStopEnabled=Boolean(policy.At("enabled")); receiverPollIntervalMs=SupervisorUnsigned32(policy.At("poll_interval_ms"));
            if (receiverPollIntervalMs<100 || receiverPollIntervalMs>5000)
                throw std::invalid_argument("Receiver poll interval requires 100..5000 milliseconds");
        }
        if (v.members.count("alarm_stop_policy")) {
            const auto &policy=v.At("alarm_stop_policy"); ExactSupervisorFields(policy,{"enabled","poll_interval_ms"});
            alarmStopEnabled=Boolean(policy.At("enabled")); alarmPollIntervalMs=SupervisorUnsigned32(policy.At("poll_interval_ms"));
            if (alarmPollIntervalMs<25 || alarmPollIntervalMs>5000) throw std::invalid_argument("Alarm poll interval requires 25..5000 milliseconds");
        }
        if (Unsigned(v.At("schema"))!=1 || String(v.At("kind"))!="getdaq.ecc-service") throw std::invalid_argument("Unsupported fixed ECC service configuration");
        serviceId=String(v.At("service_id")); clientId=String(v.At("client_id")); Text(serviceId,128); Text(clientId,128);
        bindAddress=String(v.At("bind")); port=SupervisorUnsigned32(v.At("port")); credentialFile=String(v.At("credential_file")); Path(credentialFile);
        in_addr a={}; char canonical[INET_ADDRSTRLEN];
        if (!port || port>65535 || ::inet_pton(AF_INET,bindAddress.c_str(),&a)!=1 || !::inet_ntop(AF_INET,&a,canonical,sizeof(canonical)) || bindAddress!=canonical)
            throw std::invalid_argument("Fixed ECC service requires canonical IPv4/port");
        controller.auditRoot=String(v.At("audit_root")); Path(controller.auditRoot);
        if (controller.auditRoot.size()>3300) throw std::invalid_argument("ECC root leaves insufficient operation path space");
        controller.deploymentId=String(v.At("deployment_id")); Text(controller.deploymentId,128);
        if (v.members.count("host_control_ownership")) {
            const auto &ownership=v.At("host_control_ownership"); ExactSupervisorFields(ownership,{"directory"});
            controller.hostControlOwnershipRoot=String(ownership.At("directory")); Path(controller.hostControlOwnershipRoot);
        }
        const bool owner=Boolean(v.At("exclusive_owner_confirmed")),stopped=Boolean(v.At("data_processes_stopped"));
        if (!owner || !stopped) throw std::invalid_argument("Fixed ECC startup requires explicit owner/stopped declarations; not hardware verification");
        controller.experimentArtifactBytes=Reference(v.At("experiment_artifact"),64*1024*1024).bytes;
        if(v.members.count("deployment_origin")) {
            if(v.members.count("configuration_selection") || v.members.count("configuration_publication"))throw std::invalid_argument("Distributed replica origin is distinct from legacy publication selection");
            const auto &originFile=Reference(v.At("deployment_origin"),1024*1024);inputs.RequireReadOnly(originFile.path);
            const auto origin=ParseStrictJson(originFile.bytes);ExactSupervisorFields(origin,{"schema","kind","repository_id","release_id","node_id","group_id","manifest","files"});
            if(Unsigned(origin.At("schema"))!=1 || String(origin.At("kind"))!="getdaq.distributed-replica")throw std::invalid_argument("Unsupported distributed replica origin");
            Digest(String(origin.At("repository_id")));Digest(String(origin.At("release_id")));Text(String(origin.At("node_id")),96);Text(String(origin.At("group_id")),96);
            const auto &manifestFile=Reference(origin.At("manifest"),1024*1024);inputs.RequireReadOnly(manifestFile.path);
            if(manifestFile.digest!=String(origin.At("release_id")))throw std::invalid_argument("Distributed manifest differs from release identity");
            const auto manifest=ParseStrictJson(manifestFile.bytes);ExactSupervisorFields(manifest,{"schema","kind","project_id","hosts","groups","files"});
            if(Unsigned(manifest.At("schema"))!=1 || String(manifest.At("kind"))!="getdaq.distributed-release")throw std::invalid_argument("Unsupported distributed release manifest");
            Array(manifest.At("hosts"),64);Array(manifest.At("groups"),64);Array(manifest.At("files"),512);Array(origin.At("files"),512);
            bool assigned=false;for(const auto &host:manifest.At("hosts").items)if(String(host.At("id"))==String(origin.At("node_id")))assigned=true;
            if(!assigned)throw std::invalid_argument("Replica node is not assigned to release");
            std::map<std::string,const JsonValue *> portable;for(const auto &file:manifest.At("files").items)if(!portable.emplace(String(file.At("id")),&file).second)throw std::invalid_argument("Duplicate replica file identity");
            std::map<std::string,std::string> mappings;const auto parent=manifestFile.path.substr(0,manifestFile.path.find_last_of('/'));
            for(const auto &file:origin.At("files").items){ExactSupervisorFields(file,{"id","path","sha256"});const auto id=String(file.At("id"));const auto found=portable.find(id);if(found==portable.end() || id.empty() || id.find('/')!=std::string::npos || id=="." || id==".." || !mappings.emplace(id,String(file.At("path"))).second || String(file.At("path"))!=parent+'/'+id || String(file.At("sha256"))!=String(found->second->At("sha256")))throw std::invalid_argument("Replica file mapping differs from manifest");const auto &bound=inputs.Add(String(file.At("path")),16*1024*1024,String(file.At("sha256")));inputs.RequireReadOnly(bound.path);if(bound.bytes.size()!=Unsigned(found->second->At("bytes")))throw std::invalid_argument("Replica file length differs");}
            if(mappings.size()!=portable.size())throw std::invalid_argument("Replica omits published files");
            const JsonValue *group=nullptr;for(const auto &candidate:manifest.At("groups").items)if(String(candidate.At("id"))==String(origin.At("group_id"))){if(group)throw std::invalid_argument("Duplicate replica group");group=&candidate;}
            if(!group || String(group->At("ecc").At("node_id"))!=String(origin.At("node_id")) || ConfigurationSha256(controller.experimentArtifactBytes)!=String(group->At("artifact_sha256")) || mappings[String(group->At("artifact_file"))]!=String(v.At("experiment_artifact").At("path")))throw std::invalid_argument("ECC artifact/host differs from assigned replica group");
            const auto compiled=DecodeSystemExperimentArtifact(controller.experimentArtifactBytes);
            for(const auto &source:compiled.sources){const std::string prefix="bundle://";if(source.path.compare(0,prefix.size(),prefix)!=0 || !mappings.count(source.path.substr(prefix.size())) || inputs.At(mappings.at(source.path.substr(prefix.size()))).digest!=source.sha256)throw std::invalid_argument("Portable experiment source differs from replica");}
            if(!mappings.count(String(group->At("profile_file"))) || inputs.At(mappings.at(String(group->At("profile_file")))).digest!=compiled.profileSha256)throw std::invalid_argument("Portable experiment profile differs from replica");
        }
        if (v.members.count("configuration_publication")) {
            const auto &manifest=Reference(v.At("configuration_publication"),1024*1024);
            inputs.RequireReadOnly(manifest.path);
            const auto separator=manifest.path.find_last_of('/'); const auto directory=manifest.path.substr(0,separator);
            if (manifest.path.substr(separator+1)!="publication.json") throw std::invalid_argument("Publication manifest must be publication.json");
            publicationDirectory.reset(new SupervisorIO::Directory(directory));
            const auto publication=DecodeConfigurationPublication(manifest.bytes);
            const auto artifact=VerifyConfigurationPublication(publication,[&](const PublicationFile &file) {
                const auto &input=inputs.Add(directory+'/'+file.stored,static_cast<std::size_t>(file.bytes),file.sha256);
                inputs.RequireReadOnly(input.path);
                return input.bytes;
            });
            const auto &expected=v.At("experiment_artifact");
            if (String(expected.At("path"))!=directory+'/'+publication.artifact.stored || controller.experimentArtifactBytes!=inputs.At(directory+'/'+publication.artifact.stored).bytes ||
                artifact.configurationSha256!=publication.configurationSha256) throw std::invalid_argument("Fixed ECC artifact must be the bound publication artifact");
            controller.configurationPublicationSha256=manifest.digest;
            if (v.members.count("configuration_selection")) {
                const auto &selected=Reference(v.At("configuration_selection"),65536);inputs.RequireReadOnly(selected.path);
                const auto selection=DecodeConfigurationSelection(selected.bytes);
                const auto &receipt=inputs.Add(directory+"/registration.json",65536,selection.registrationSha256);inputs.RequireReadOnly(receipt.path);
                VerifyConfigurationSelection(selection,receipt.bytes,manifest.path,manifest.bytes,publication);
                controller.configurationSelectionSha256=selected.digest;
                controller.configurationRepositoryId=selection.registration.repositoryId;
                controller.configurationRegistrationSha256=receipt.digest;
                selectionJson=EncodeConfigurationSelection(selection);
            }
        }
        const auto artifact=DecodeSystemExperimentArtifact(controller.experimentArtifactBytes);
        Array(v.At("hardware_descriptions"),512);
        for (const auto &entry:v.At("hardware_descriptions").items) {
            const auto &input=Reference(entry,8*1024*1024);
            if (!descriptions.insert(input.path).second) throw std::invalid_argument("Duplicate fixed hardware description reference");
        }
        const auto reader=[&](const std::string &path) {
            if (!descriptions.count(path)) throw std::invalid_argument("Topology references unregistered hardware description file");
            usedDescriptions.insert(path); return DecodeHardwareDescription(inputs.At(path).bytes);
        };
        auto topology=DecodeSystemAlignmentOptions(Reference(v.At("topology"),1024*1024).bytes,reader,true);
        topology.exclusiveOwnerConfirmed=owner; topology.daqStoppedConfirmed=stopped;
        controller.endpoints=BindSystemExperiment(artifact,topology);
        Array(v.At("cobo_policies"),192); std::map<std::string,const JsonValue *> policies;
        for (const auto &policy:v.At("cobo_policies").items) {
            ExactSupervisorFields(policy,{"id","callback_endpoints","published_endpoints","max_pending_alarms","max_alarm_audit_bytes"});
            if (!policies.emplace(String(policy.At("id")),&policy).second) throw std::invalid_argument("Duplicate fixed CoBo policy ID");
        }
        for (auto &c:controller.endpoints.cobos) {
            const auto found=policies.find(c.id); if (found==policies.end()) throw std::invalid_argument("CoBo callback/audit policy missing");
            const auto &policy=*found->second; c.prepare.callbackEndpoints=String(policy.At("callback_endpoints")); c.prepare.publishedEndpoints=String(policy.At("published_endpoints"));
            c.prepare.maxPendingAlarms=SupervisorUnsigned32(policy.At("max_pending_alarms")); c.prepare.maxAlarmAuditBytes=Unsigned(policy.At("max_alarm_audit_bytes")); policies.erase(found);
        }
        if (!policies.empty()) throw std::invalid_argument("Extra fixed CoBo callback/audit policies");
        const auto policy=Policy(String(v.At("description_policy"))); controller.description=BuildSystemDescriptionOptions(topology,policy);
        if (policy==DescriptionPolicy::ReplaceExisting) {
            if (v.At("current_topology").type==JsonValue::Null) throw std::invalid_argument("Description replacement needs explicit current topology");
            auto current=DecodeSystemAlignmentOptions(Reference(v.At("current_topology"),1024*1024).bytes,reader,true);
            current.exclusiveOwnerConfirmed=owner; current.daqStoppedConfirmed=stopped; const auto previous=BuildSystemDescriptionOptions(current,DescriptionPolicy::UseExisting);
            std::map<std::string,const SystemDescriptionNode *> nodes;
            const auto key=[](const SystemDescriptionNode &node) { return std::to_string(int(node.role))+':'+node.id; };
            for (const auto &node:previous.nodes) nodes.emplace(key(node),&node);
            for (auto &node:controller.description.nodes) {
                const auto found=nodes.find(key(node)); if (found==nodes.end() || found->second->nodeProxy!=node.nodeProxy) throw std::invalid_argument("Current/desired description target differs");
                node.currentDescription=found->second->description; nodes.erase(found);
            }
            if (!nodes.empty()) throw std::invalid_argument("Extra current description target");
            ValidateSystemDescriptionOptions(controller.description);
        } else if (v.At("current_topology").type!=JsonValue::Null) throw std::invalid_argument("Current topology only allowed for explicit replacement");
        if (usedDescriptions!=descriptions) throw std::invalid_argument("Fixed hardware description table has unused entries");
        Array(v.At("senders"),195);
        for (const auto &sender:v.At("senders").items) {
            std::set<std::string> fields={"id","control_proxy","receiver_address","circular_buffer_mask","always_flush","exclusive_stopped_owner_confirmed","encoding_minor","timeout_ms"};
            if (sender.type==JsonValue::Object && sender.members.count("protocol")) fields.insert("protocol");
            ExactSupervisorFields(sender,fields);
            DataSenderOptions s; s.senderId=String(sender.At("id")); s.controlProxy=String(sender.At("control_proxy")); s.receiverAddress=String(sender.At("receiver_address"));
            s.circularBufferMask=SupervisorUnsigned32(sender.At("circular_buffer_mask")); s.alwaysFlush=Boolean(sender.At("always_flush")); s.exclusiveStoppedOwnerConfirmed=Boolean(sender.At("exclusive_stopped_owner_confirmed"));
            if (fields.count("protocol")) s.protocol=String(sender.At("protocol"));
            const auto minor=SupervisorUnsigned32(sender.At("encoding_minor"));
            if (minor>1) throw std::invalid_argument("Fixed sender encoding must be 0 or 1");
            s.encodingMinor=int(minor); const auto timeout=SupervisorUnsigned32(sender.At("timeout_ms"));
            if (timeout>60000) throw std::invalid_argument("Fixed sender timeout outside bounds");
            s.timeoutMs=int(timeout); controller.senders.push_back(s);
        }
        ValidateDataSenderSet(controller.senders);
        const auto &remote=v.At("remote_receivers"); ExactSupervisorFields(remote,{"controller_id","command_timeout_ms","poll_interval_ms","endpoints"});
        receiver.controllerId=String(remote.At("controller_id")); receiver.commandTimeoutMs=SupervisorUnsigned32(remote.At("command_timeout_ms")); receiver.pollIntervalMs=SupervisorUnsigned32(remote.At("poll_interval_ms"));
        Array(remote.At("endpoints"),195);
        for (const auto &entry:remote.At("endpoints").items) {
            ExactSupervisorFields(entry,{"agent_id","plan_sha256","address","port","credential_file","timeout_ms"}); RemoteReceiverEndpoint endpoint;
            endpoint.agentId=String(entry.At("agent_id")); endpoint.planSha256=String(entry.At("plan_sha256")); endpoint.control.address=String(entry.At("address")); endpoint.control.port=SupervisorUnsigned32(entry.At("port"));
            endpoint.control.credentialFile=String(entry.At("credential_file")); endpoint.control.timeoutMs=SupervisorUnsigned32(entry.At("timeout_ms")); receiver.endpoints.push_back(endpoint);
        }
        SystemPreparationClient preparation(controller.endpoints); // 纯构造验证完整已绑定参数/策略。
        if (alarmStopEnabled) for (const auto &c:controller.endpoints.cobos) if (c.enabled && !c.prepare.settings.monitoring.subscribeAlarms)
            throw std::invalid_argument("Automatic alarm stop requires owned alarm subscription for every active CoBo");
        if(control) {
            RemoteReceiverManager data(receiver); // 只检查本地选项及凭据，无远端命令。
            SupervisorCredential credential(credentialFile); credential.Check();
        } else RemoteReceiverBindingSha256(receiver); // Offline declarations only; no credential reads.
        binding=inputs.BindingSha256(); controller.deploymentSourceSha256=binding; Check();
    }
    void Check() const {
        if (fault) throw std::runtime_error("Fixed ECC configuration fault is latched; explicit reconciliation required");
        try { if (publicationDirectory) publicationDirectory->Check(); inputs.Check(); }
        catch (...) { fault=true; throw; }
    }
};
FixedEccConfiguration::FixedEccConfiguration(const std::string &file) : fImpl(new Impl(file,true)) {}
FixedEccConfiguration::FixedEccConfiguration(const std::string &file,OfflineEvidenceOnly) : fImpl(new Impl(file,false)) {}
FixedEccConfiguration::~FixedEccConfiguration() {}
void FixedEccConfiguration::Check() const { fImpl->Check(); }
bool FixedEccConfiguration::ControlAccessValidated() const {return fImpl->controlAccessValidated;}
std::string FixedEccConfiguration::BindingSha256() const { return fImpl->binding; }
NativeEccOptions FixedEccConfiguration::ControllerOptions() const { return fImpl->controller; }
RemoteReceiverOptions FixedEccConfiguration::ReceiverOptions() const { return fImpl->receiver; }
std::string FixedEccConfiguration::ServiceId() const { return fImpl->serviceId; }
std::string FixedEccConfiguration::ClientId() const { return fImpl->clientId; }
std::string FixedEccConfiguration::BindAddress() const { return fImpl->bindAddress; }
std::string FixedEccConfiguration::CredentialFile() const { return fImpl->credentialFile; }
unsigned int FixedEccConfiguration::Port() const { return fImpl->port; }
bool FixedEccConfiguration::AlarmStopEnabled() const { return fImpl->alarmStopEnabled; }
std::string FixedEccConfiguration::SelectionJson() const { return fImpl->selectionJson; }
unsigned int FixedEccConfiguration::AlarmPollIntervalMs() const { return fImpl->alarmPollIntervalMs; }
bool FixedEccConfiguration::ReceiverStopEnabled() const { return fImpl->receiverStopEnabled; }
unsigned int FixedEccConfiguration::ReceiverPollIntervalMs() const { return fImpl->receiverPollIntervalMs; }
}
