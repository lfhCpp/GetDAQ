#include "nativeeccplancompiler_p.h"
#include "../../../common/text/utf8.h"
#include "systemacquisition.h"
#include "systemexperimentbinding.h"
#include "hardwareaudit.h"
#include "configurationsha256.h"
#include "strictjson.h"
#include <map>
#include <stdexcept>
#include <string>

namespace GetDAQ {
namespace {
void Text(const std::string &s,std::size_t maximum) {
    if (s.empty() || s.size()>maximum) throw std::invalid_argument("Invalid bounded native ECC text");
    ValidateJsonUtf8(s);
    for (unsigned char c:s) if (c<32 || c==127) throw std::invalid_argument("Control byte in native ECC text");
}
SystemAlignmentOptions Topology(const SystemPreparationOptions &o) {
    SystemAlignmentOptions a; a.encodingMinor=o.encodingMinor; a.timeoutMs=o.timeoutMs;
    a.exclusiveOwnerConfirmed=o.exclusiveOwnerConfirmed; a.daqStoppedConfirmed=o.dataProcessesStopped;
    for (const auto &c:o.cobos) {
        AlignmentCoBoEndpoint e; e.id=c.id; e.nodeProxy=c.prepare.nodeProxy; e.linkProxy=c.linkProxy;
        e.expectedNodeName=c.prepare.expectedNodeName; e.description=c.prepare.description; e.enabled=c.enabled; a.cobos.push_back(e);
    }
    for (const auto &m:o.mutants) a.mutants.push_back(m.endpoint);
    return a;
}
void BindDescriptions(const SystemDescriptionOptions &d,const SystemPreparationOptions &o) {
    ValidateSystemDescriptionOptions(d);
    const auto expected=BuildSystemDescriptionOptions(Topology(o),DescriptionPolicy::UseExisting);
    if (d.encodingMinor!=o.encodingMinor || d.timeoutMs!=o.timeoutMs || d.nodes.size()!=expected.nodes.size())
        throw std::invalid_argument("Native ECC description/preparation topology size/encoding/timeout differs");
    std::map<std::string,const SystemDescriptionNode *> nodes;
    for (const auto &n:d.nodes) nodes.emplace(std::to_string(int(n.role))+':'+n.id,&n);
    for (const auto &e:expected.nodes) {
        const auto found=nodes.find(std::to_string(int(e.role))+':'+e.id);
        if (found==nodes.end() || found->second->nodeProxy!=e.nodeProxy || HardwarePolicyJson(found->second->description)!=HardwarePolicyJson(e.description))
            throw std::invalid_argument("Native ECC description/preparation ID/role/proxy/name/policy differs");
        nodes.erase(found);
    }
    if (!nodes.empty()) throw std::invalid_argument("Extra native ECC description target");
}
std::vector<DataSenderOptions> BindSenders(const SystemPreparationOptions &o,const std::vector<DataSenderOptions> &input) {
    ValidateDataSenderSet(input); std::map<std::string,DataSenderOptions> byId;
    for (const auto &s:input) {
        if (!s.exclusiveStoppedOwnerConfirmed) throw std::invalid_argument("Native ECC sender requires explicit exclusive stopped owner declaration");
        byId.emplace(s.senderId,s);
    }
    std::vector<DataSenderOptions> ordered;
    const auto add=[&](const std::string &id,unsigned int mask) {
        const auto p=byId.find(id);
        if (p==byId.end() || p->second.circularBufferMask!=mask || p->second.encodingMinor!=o.encodingMinor)
            throw std::invalid_argument("Native ECC sender ID/activity/mask/encoding differs from artifact");
        ordered.push_back(p->second); byId.erase(p);
    };
    for (const auto &c:o.cobos) if (c.enabled) add(c.id,CompileCoBoConfigure(c.configure).readout.aget.asadMask);
    for (const auto &m:o.mutants) if (m.settings.readoutEnabled) add(m.endpoint.id,1);
    if (!byId.empty()) throw std::invalid_argument("Extra native ECC sender");
    return ordered;
}
std::string Snapshot(const NativeEccOptions &input,const SystemPreparationOptions &o,const std::vector<DataSenderOptions> &senders,const std::string &dataBinding) {
    std::string s="{\"schema_version\":1,\"kind\":\"getdaq.native-ecc-binding\",\"deployment_id\":"+JsonString(input.deploymentId)+
        ",\"configuration_sha256\":"+JsonString(o.configurationSha256)+",\"artifact_sha256\":"+JsonString(o.artifactSha256)+
        ",\"data_lifecycle_binding_sha256\":"+JsonString(dataBinding)+
        ",\"encoding_minor\":"+std::to_string(o.encodingMinor)+",\"timeout_ms\":"+std::to_string(o.timeoutMs)+
        ",\"exclusive_owner_declared\":true,\"data_stopped_declared\":true,\"control_lock_verified\":false,\"actual_loaded_configuration_verified\":false,\"description\":[";
    for (const auto &n:input.description.nodes) {
        if (s.back()!='[') s+=',';
        s+="{\"role\":"+std::to_string(int(n.role))+",\"id\":"+JsonString(n.id)+",\"proxy\":"+JsonString(n.nodeProxy)+
            ",\"policy\":"+std::to_string(int(n.policy))+",\"desired\":"+HardwarePolicyJson(n.description);
        if (n.policy==DescriptionPolicy::ReplaceExisting) s+=",\"current\":"+HardwarePolicyJson(n.currentDescription);
        s+='}';
    }
    s+=']';
    if (!input.deploymentSourceSha256.empty()) s+=",\"deployment_source_sha256\":"+JsonString(input.deploymentSourceSha256);
    s+=",\"cobos\":[";
    for (const auto &c:o.cobos) {
        if (s.back()!='[') s+=',';
        s+="{\"id\":"+JsonString(c.id)+",\"enabled\":"+(c.enabled ? "true" : "false")+",\"link_proxy\":"+JsonString(c.linkProxy)+
            ",\"callback_endpoints\":"+JsonString(c.prepare.callbackEndpoints)+",\"published_endpoints\":"+JsonString(c.prepare.publishedEndpoints)+
            ",\"max_pending_alarms\":"+std::to_string(c.prepare.maxPendingAlarms)+",\"max_alarm_audit_bytes\":"+std::to_string(c.prepare.maxAlarmAuditBytes)+"}";
    }
    s+="],\"mutants\":[";
    for (const auto &m:o.mutants) {
        if (s.back()!='[') s+=',';
        s+="{\"id\":"+JsonString(m.endpoint.id)+",\"alignment_proxy\":"+JsonString(m.endpoint.alignmentProxy)+",\"master\":"+(m.endpoint.master ? "true" : "false")+"}";
    }
    s+="],\"senders\":[";
    for (const auto &sender:senders) {
        if (s.back()!='[') s+=',';
        s+="{\"id\":"+JsonString(sender.senderId)+",\"proxy\":"+JsonString(sender.controlProxy)+",\"receiver_address\":"+JsonString(sender.receiverAddress)+
            ",\"mask\":"+std::to_string(sender.circularBufferMask)+",\"always_flush\":"+(sender.alwaysFlush ? "true" : "false")+
            ",\"exclusive_stopped_owner_declared\":true,\"encoding_minor\":"+std::to_string(sender.encodingMinor)+",\"timeout_ms\":"+std::to_string(sender.timeoutMs);
        if (sender.protocol!="TCP") s+=",\"protocol\":"+JsonString(sender.protocol);
        s+='}';
    }
    s+=']';
    if (!input.configurationPublicationSha256.empty()) s+=",\"configuration_publication_sha256\":"+JsonString(input.configurationPublicationSha256);
    if (!input.configurationSelectionSha256.empty() || !input.configurationRepositoryId.empty() || !input.configurationRegistrationSha256.empty()) {
        for (const auto &digest:{input.configurationSelectionSha256,input.configurationRepositoryId,input.configurationRegistrationSha256,input.configurationPublicationSha256})
            if (digest.size()!=64 || digest.find_first_not_of("0123456789abcdef")!=std::string::npos) throw std::invalid_argument("Native ECC selection binding requires complete SHA-256 fields");
        s+=",\"configuration_selection_sha256\":"+JsonString(input.configurationSelectionSha256)+",\"configuration_repository_id\":"+JsonString(input.configurationRepositoryId)+
            ",\"configuration_registration_sha256\":"+JsonString(input.configurationRegistrationSha256);
    }
    if (!input.hostControlOwnershipRoot.empty()) s+=",\"host_control_ownership_root\":"+JsonString(input.hostControlOwnershipRoot);
    s+="}\n";
    if (s.size()>64*1024*1024) throw std::invalid_argument("Native ECC snapshot exceeds 64 MiB");
    ParseStrictJson(s); return s;
}
}
CompiledNativeEccPlan CompileNativeEccPlan(const NativeEccOptions &o,const std::string &binding) {
    CompiledNativeEccPlan result;
    auto &bound=result.bound;auto &senders=result.senders;auto &dataBinding=result.dataBinding;
    auto &snapshot=result.plan.snapshotJson;auto &snapshotHash=result.plan.snapshotSha256;
    Text(o.auditRoot,3500); Text(o.deploymentId,128);
    if (!o.deploymentSourceSha256.empty() && (o.deploymentSourceSha256.size()!=64 || o.deploymentSourceSha256.find_first_not_of("0123456789abcdef")!=std::string::npos))
        throw std::invalid_argument("Native ECC deployment source binding must be lowercase SHA-256");
    if (!o.configurationPublicationSha256.empty() && (o.configurationPublicationSha256.size()!=64 || o.configurationPublicationSha256.find_first_not_of("0123456789abcdef")!=std::string::npos))
        throw std::invalid_argument("Native ECC publication binding must be lowercase SHA-256");
    if (o.auditRoot[0]!='/' || o.auditRoot.back()=='/' || o.auditRoot.find("//")!=std::string::npos ||
        o.auditRoot.find("/./")!=std::string::npos || o.auditRoot.find("/../")!=std::string::npos ||
        o.auditRoot.size()<2 || o.auditRoot.substr(o.auditRoot.size()-2)=="/." || (o.auditRoot.size()>=3 && o.auditRoot.substr(o.auditRoot.size()-3)=="/.."))
        throw std::invalid_argument("Native ECC root requires an absolute normalized directory path");
    const auto artifact=DecodeSystemExperimentArtifact(o.experimentArtifactBytes);
    bound=BindSystemExperiment(artifact,o.endpoints);
    if (!bound.exclusiveOwnerConfirmed || !bound.dataProcessesStopped) throw std::invalid_argument("Native ECC requires exclusive owner and stopped data declarations");
    SystemPreparationClient check(bound); bound=check.Configuration(); // 纯构造检查并保留实际执行的规范化参数。
    BindDescriptions(o.description,bound); senders=BindSenders(bound,o.senders);
    dataBinding=binding;
    if (dataBinding.size()!=64 || dataBinding.find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::invalid_argument("Native ECC requires an immutable data lifecycle SHA-256 binding");
    snapshot=Snapshot(o,bound,senders,dataBinding); snapshotHash=GetDAQ::ConfigurationSha256(snapshot);
    result.plan.dataControllerScopeSha256=GetDAQ::ConfigurationSha256("getdaq.data-controller:1\n"+snapshotHash+'\n'+o.auditRoot);
    return result;
}
NativeEccPlan PlanNativeEccConfiguration(const NativeEccOptions &options,const std::string &binding) {
    auto compiled=CompileNativeEccPlan(options,binding);
    return std::move(compiled.plan);
}
}
