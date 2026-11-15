#ifndef GETDAQ_FIXED_ECC_FIXTURE_H
#define GETDAQ_FIXED_ECC_FIXTURE_H
#include "fixedeccconfiguration.h"
#include "ecccontroljobs.h"
#include "native_receiver_fixture.h"
#include "configurationpublication.h"
#include "configurationselection.h"
#include "effective_record_fixture.h"
namespace GetDAQFixedEccFixture {
using namespace GetDAQReceiverFixture;
inline std::string Reference(const std::string &path) {
    return "{\"path\":"+EncodeJsonString(path)+",\"sha256\":"+EncodeJsonString(GetDAQ::ConfigurationSha256(IO::ReadFile(path,64*1024*1024)))+"}";
}
// 合成已审阅包用于实际Qt/原生Run联调；不将空有效树当真实XML编译结果。
inline std::string ReviewedConfiguration(const std::string &root,const std::string &baseFile,const std::string &artifactBytes) {
    const auto directory=root+"/reviewed-package";IO::CreateDirectory(directory);const auto a=DecodeSystemExperimentArtifact(artifactBytes);
    Require(a.sources.size()==1 && a.sources[0].sha256==GetDAQ::ConfigurationSha256(a.sources[0].path),"Synthetic publication source fixture differs");
    ConfigurationPublication p;p.rootSource=a.sources[0].path;p.configurationSha256=a.configurationSha256;
    const auto file=[&](const std::string &name,const std::string &bytes) {
        PublicationFile f;f.stored=name;f.bytes=bytes.size();f.sha256=GetDAQ::ConfigurationSha256(bytes);IO::WriteNew(directory+'/'+name,bytes);
        Require(!::chmod((directory+'/'+name).c_str(),0400),"Seal reviewed fixture role");return f;
    };
    p.artifact=file("experiment.json",artifactBytes);p.profile=file("profile.json","actual-receiver-test-profile");
    Require(p.profile.sha256==a.profileSha256,"Synthetic publication profile differs");PublicationSource s;s.sourcePath=p.rootSource;s.file=file("source-000.xcfg",p.rootSource);p.sources.push_back(s);
    const auto snapshot=GetDAQTest::EmptyEffectiveSnapshot(p.rootSource,s.file.sha256,s.file.bytes);const auto record=ParseStrictJson(snapshot);
    p.effectiveSha256=record.At("effective_sha256").text;p.snapshotSha256=record.At("snapshot_sha256").text;p.effective=file("effective.json",snapshot);
    const auto manifestBytes=EncodeConfigurationPublication(p);file("publication.json",manifestBytes);
    ConfigurationRegistration r;r.repositoryId=std::string(64,'a');r.publicationSha256=GetDAQ::ConfigurationSha256(manifestBytes);r.configurationSha256=p.configurationSha256;r.artifactSha256=p.artifact.sha256;
    r.effectiveSha256=p.effectiveSha256;r.snapshotSha256=p.snapshotSha256;r.manifestFile=directory+"/publication.json";const auto receipt=EncodeConfigurationRegistration(r);file("registration.json",receipt);
    const auto selected=EncodeConfigurationSelection(SelectConfigurationRegistration(receipt));const auto choice=root+"/reviewed-selection.json";IO::WriteNew(choice,selected);Require(!::chmod(choice.c_str(),0400),"Seal reviewed fixture selection");
    auto base=IO::ReadFile(baseFile,1024*1024);const auto original=Reference(root+"/experiment.json");const auto at=base.find(original);Require(at!=std::string::npos,"Find fixture artifact reference");
    base.replace(at,original.size(),Reference(directory+"/experiment.json"));base.insert(base.size()-2,",\"configuration_publication\":"+Reference(r.manifestFile)+",\"configuration_selection\":"+Reference(choice));
    const auto output=root+"/reviewed-service.json";IO::WriteNew(output,base);return output;
}
// 仅生成完整本机夹具输入；不调用硬件，不代替生产配置编译器。
inline std::string WriteConfiguration(const std::string &root,const std::string &description,const NativeEccOptions &options,
                                     const RemoteReceiverOptions &remote,unsigned int port,const std::string &credential,bool automaticAlarmStop=false,bool receiverFailureStop=false) {
    Require(options.endpoints.mutants.empty(),"This fixed service fixture exercises standalone CoBo mode");
    const auto artifact=root+"/experiment.json",hardware=root+"/cobo.json",topologyPath=root+"/topology.json",config=root+"/service.json";
    IO::WriteNew(artifact,options.experimentArtifactBytes); IO::WriteNew(hardware,IO::ReadFile(description,8*1024*1024));
    std::string topology="{\"schema_version\":1,\"kind\":\"getdaq.alignment-topology\",\"encoding\":"+EncodeJsonString(options.endpoints.encodingMinor ? "1.1" : "1.0")+
        ",\"timeout_ms\":"+std::to_string(options.endpoints.timeoutMs)+",\"mutants\":[],\"cobos\":[",policies="[",senders="[",peers="[";
    for (const auto &c:options.endpoints.cobos) {
        if (topology.back()!='[') topology+=',';
        topology+="{\"id\":"+EncodeJsonString(c.id)+",\"expected_node_name\":"+EncodeJsonString(c.prepare.expectedNodeName)+",\"node_proxy\":"+EncodeJsonString(c.prepare.nodeProxy)+
            ",\"link_proxy\":"+EncodeJsonString(c.linkProxy)+",\"enabled\":"+(c.enabled ? "true" : "false")+",\"hardware_description_file\":"+EncodeJsonString(hardware)+"}";
        if (policies.back()!='[') policies+=',';
        policies+="{\"id\":"+EncodeJsonString(c.id)+",\"callback_endpoints\":"+EncodeJsonString(c.prepare.callbackEndpoints)+",\"published_endpoints\":"+EncodeJsonString(c.prepare.publishedEndpoints)+
            ",\"max_pending_alarms\":"+std::to_string(c.prepare.maxPendingAlarms)+",\"max_alarm_audit_bytes\":"+std::to_string(c.prepare.maxAlarmAuditBytes)+"}";
    }
    topology+="]}\n"; policies+=']'; IO::WriteNew(topologyPath,topology);
    for (const auto &s:options.senders) {
        if (senders.back()!='[') senders+=',';
        senders+="{\"id\":"+EncodeJsonString(s.senderId)+",\"control_proxy\":"+EncodeJsonString(s.controlProxy)+",\"receiver_address\":"+EncodeJsonString(s.receiverAddress)+
            ",\"circular_buffer_mask\":"+std::to_string(s.circularBufferMask)+",\"always_flush\":"+(s.alwaysFlush ? "true" : "false")+
            ",\"exclusive_stopped_owner_confirmed\":true,\"encoding_minor\":"+std::to_string(s.encodingMinor)+",\"timeout_ms\":"+std::to_string(s.timeoutMs);
        if(s.protocol!="TCP") senders+=",\"protocol\":"+EncodeJsonString(s.protocol);
        senders+='}';
    }
    senders+=']';
    for (const auto &e:remote.endpoints) {
        if (peers.back()!='[') peers+=',';
        peers+="{\"agent_id\":"+EncodeJsonString(e.agentId)+",\"plan_sha256\":"+EncodeJsonString(e.planSha256)+",\"address\":"+EncodeJsonString(e.control.address)+
            ",\"port\":"+std::to_string(e.control.port)+",\"credential_file\":"+EncodeJsonString(e.control.credentialFile)+",\"timeout_ms\":"+std::to_string(e.control.timeoutMs)+"}";
    }
    peers+=']';
    auto bytes="{\"schema\":1,\"kind\":\"getdaq.ecc-service\",\"service_id\":\"test-ecc\",\"client_id\":\"test-console\",\"bind\":\"127.0.0.1\",\"port\":"+std::to_string(port)+",\"credential_file\":"+EncodeJsonString(credential)+
        ",\"audit_root\":"+EncodeJsonString(options.auditRoot)+",\"deployment_id\":"+EncodeJsonString(options.deploymentId)+",\"experiment_artifact\":"+Reference(artifact)+",\"topology\":"+Reference(topologyPath)+
        ",\"hardware_descriptions\":["+Reference(hardware)+"],\"description_policy\":\"use-existing\",\"current_topology\":null,\"exclusive_owner_confirmed\":true,\"data_processes_stopped\":true,\"cobo_policies\":"+policies+",\"senders\":"+senders+
        ",\"remote_receivers\":{\"controller_id\":"+EncodeJsonString(remote.controllerId)+",\"command_timeout_ms\":"+std::to_string(remote.commandTimeoutMs)+",\"poll_interval_ms\":"+std::to_string(remote.pollIntervalMs)+",\"endpoints\":"+peers+"}}\n";
    if (automaticAlarmStop) bytes.insert(bytes.size()-2,",\"alarm_stop_policy\":{\"enabled\":true,\"poll_interval_ms\":25}");
    if (receiverFailureStop) bytes.insert(bytes.size()-2,",\"receiver_stop_policy\":{\"enabled\":true,\"poll_interval_ms\":100}");
    IO::WriteNew(config,bytes); return config;
}
}
#endif
