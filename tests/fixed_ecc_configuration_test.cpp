#include "fixedeccconfiguration.h"
#include "ecccontroljobs.h"
#include "native_receiver_fixture.h"
#include "configurationpublication.h"
#include "configurationselection.h"
#include "eccconfigurationselector.h"
#include "fixedscanruntimeidentity.h"
#include "scanpointdeployment.h"
#include "../apps/datasupervisor/planio.h"
#include "../apps/datasupervisor/supervisorcommon.h"
#include "effective_record_fixture.h"
#include "../common/experiment/pulseramplitudescan.h"
#include "../common/experiment/scanexecutionpolicy.h"
#include "../common/experiment/scanexecutionsnapshot.h"
#include "../common/experiment/systemscanplan.h"
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
namespace {
using namespace GetDAQReceiverFixture;
std::string Reference(const std::string &path) { return "{\"path\":"+EncodeJsonString(path)+",\"sha256\":"+EncodeJsonString(GetDAQ::ConfigurationSha256(IO::ReadFile(path,64*1024*1024)))+"}"; }
void Write(const std::string &path,const std::string &bytes) { IO::WriteNew(path,bytes); }
std::string ShellQuote(const std::string &value) {
    std::string result="'";
    for (char c:value) { if (c=='\'') result+="'\\''"; else result+=c; }
    return result+"'";
}
std::string Publication(const std::string &directory,const std::string &artifactBytes) {
    IO::CreateDirectory(directory); const auto artifact=GetDAQ::DecodeSystemExperimentArtifact(artifactBytes);
    GetDAQ::ConfigurationPublication p; p.rootSource=artifact.sources[0].path; p.configurationSha256=artifact.configurationSha256;
    p.effectiveSha256=std::string(64,'a'); p.snapshotSha256=std::string(64,'b');
    const auto add=[&](const std::string &name,const std::string &bytes) {
        GetDAQ::PublicationFile f; f.stored=name; f.bytes=bytes.size(); f.sha256=GetDAQ::ConfigurationSha256(bytes);
        Write(directory+'/'+name,bytes); Require(::chmod((directory+'/'+name).c_str(),0400)==0,"Set publication read-only fixture"); return f;
    };
    p.artifact=add("experiment.json",artifactBytes); p.profile=add("profile.json","actual-receiver-test-profile");
    GetDAQ::PublicationSource source; source.sourcePath=p.rootSource; source.file=add("source-000.xcfg",p.rootSource); p.sources.push_back(source);
    const auto snapshot=GetDAQTest::EmptyEffectiveSnapshot(p.rootSource,source.file.sha256,source.file.bytes);
    const auto record=GetDAQ::ParseStrictJson(snapshot); p.effectiveSha256=record.At("effective_sha256").text; p.snapshotSha256=record.At("snapshot_sha256").text;
    p.effective=add("effective.json",snapshot);
    add("publication.json",GetDAQ::EncodeConfigurationPublication(p)); return directory+"/publication.json";
}
EccControlRequest Request(EccControlJobs &jobs,const std::string &command,const std::string &id) {
    const auto identity=ParseStrictJson(jobs.IdentityJson()); EccControlRequest r; r.operationId=id; r.command=command; r.instanceId=IO::String(identity.At("instance_id"));
    r.snapshotSha256=IO::String(identity.At("snapshot_sha256")); r.clientId="test-console"; r.clientEpoch=std::string(64,'a'); return r;
}
EccControlOperation Terminal(EccControlJobs &jobs,const std::string &id) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    for (;;) { const auto op=jobs.Operation(id); if (op.phase=="complete" || op.phase=="failed" || op.phase=="unknown") return op;
        Require(std::chrono::steady_clock::now()<deadline,"Fixed ECC command test deadline"); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
}
}
int main(int argc,char **argv) {
    Ice::CommunicatorPtr communicator;
    try {
        Require(argc==4,"Pass CoBo/MuTaNT descriptions and NEW_EVIDENCE_DIR"); const std::string root=argv[3]; IO::CreateDirectory(root);
        const auto cobo=LoadHardwareDescription(argv[1]),mutant=LoadHardwareDescription(argv[2]); communicator=Ice::initialize();
        auto adapter=communicator->createObjectAdapterWithEndpoints("FixedInputFixture","tcp -h 127.0.0.1 -p 0"); Rig r(communicator,adapter,cobo,mutant,1); r.options.mutants.clear();
        r.senders.erase(std::remove_if(r.senders.begin(),r.senders.end(),[](const DataSenderOptions &s) { return s.senderId[0]!='C'; }),r.senders.end());
        const auto original=Options(r,root); const auto artifactPath=root+"/experiment.json",descriptionPath=root+"/cobo.json",topologyPath=root+"/topology.json",credentialPath=root+"/credential",configPath=root+"/service.json";
        Write(artifactPath,original.experimentArtifactBytes); Write(descriptionPath,IO::ReadFile(argv[1],8*1024*1024)); Write(credentialPath,IO::RandomSession()+"\n");
        std::string topology="{\"schema_version\":1,\"kind\":\"getdaq.alignment-topology\",\"encoding\":\"1.1\",\"timeout_ms\":3000,\"mutants\":[],\"cobos\":[",policies="[",senders="[";
        for (const auto &c:r.options.cobos) {
            if (topology.back()!='[') topology+=',';
            topology+="{\"id\":"+EncodeJsonString(c.id)+",\"expected_node_name\":"+EncodeJsonString(c.prepare.expectedNodeName)+",\"node_proxy\":"+EncodeJsonString(c.prepare.nodeProxy)+
                ",\"link_proxy\":"+EncodeJsonString(c.linkProxy)+",\"enabled\":"+(c.enabled ? "true" : "false")+",\"hardware_description_file\":"+EncodeJsonString(descriptionPath)+"}";
            if (policies.back()!='[') policies+=',';
            policies+="{\"id\":"+EncodeJsonString(c.id)+",\"callback_endpoints\":"+EncodeJsonString(c.prepare.callbackEndpoints)+",\"published_endpoints\":"+EncodeJsonString(c.prepare.publishedEndpoints)+
                ",\"max_pending_alarms\":"+std::to_string(c.prepare.maxPendingAlarms)+",\"max_alarm_audit_bytes\":"+std::to_string(c.prepare.maxAlarmAuditBytes)+"}";
        }
        topology+="]}\n"; policies+=']'; Write(topologyPath,topology);
        for (const auto &s:r.senders) {
            if (senders.back()!='[') senders+=',';
            senders+="{\"id\":"+EncodeJsonString(s.senderId)+",\"control_proxy\":"+EncodeJsonString(s.controlProxy)+",\"receiver_address\":"+EncodeJsonString(s.receiverAddress)+
                ",\"circular_buffer_mask\":"+std::to_string(s.circularBufferMask)+",\"always_flush\":"+(s.alwaysFlush ? "true" : "false")+",\"exclusive_stopped_owner_confirmed\":true,\"encoding_minor\":1,\"timeout_ms\":3000}";
        }
        senders+=']';
        auto bytes="{\"schema\":1,\"kind\":\"getdaq.ecc-service\",\"service_id\":\"test-ecc\",\"client_id\":\"test-console\",\"bind\":\"127.0.0.1\",\"port\":24000,\"credential_file\":"+EncodeJsonString(credentialPath)+
            ",\"audit_root\":"+EncodeJsonString(root+"/controller")+",\"deployment_id\":\"fixed-input-test\",\"experiment_artifact\":"+Reference(artifactPath)+",\"topology\":"+Reference(topologyPath)+
            ",\"hardware_descriptions\":["+Reference(descriptionPath)+"],\"description_policy\":\"use-existing\",\"current_topology\":null,\"exclusive_owner_confirmed\":true,\"data_processes_stopped\":true,\"cobo_policies\":"+policies+",\"senders\":"+senders+
            ",\"remote_receivers\":{\"controller_id\":\"test-ecc\",\"command_timeout_ms\":1000,\"poll_interval_ms\":20,\"endpoints\":[{\"agent_id\":\"offline-node\",\"plan_sha256\":"+EncodeJsonString(std::string(64,'b'))+
            ",\"address\":\"127.0.0.1\",\"port\":24001,\"credential_file\":"+EncodeJsonString(credentialPath)+",\"timeout_ms\":100}]}}\n";
        DataSupervisorOptions receiverOptions;
        receiverOptions.auditRoot=root+"/data-control"; receiverOptions.deploymentId="fixed-scan-template";
        receiverOptions.receiverExecutable="/opt/getdaq/getdaq-receiver";
        receiverOptions.configurationSha256=DecodeSystemExperimentArtifact(original.experimentArtifactBytes).configurationSha256;
        receiverOptions.artifactSha256=ConfigurationSha256(original.experimentArtifactBytes);
        for (const auto &sender:r.senders) {
            ReceiverSourcePlan source; source.id=sender.senderId;
            source.receiverAddress=sender.receiverAddress; source.inputProtocol=sender.protocol;
            source.bindAddress="0.0.0.0"; source.outputRoot=root+"/receiver-data-"+sender.senderId;
            const auto colon=source.receiverAddress.find(':');
            source.dataPort=static_cast<unsigned int>(std::stoul(source.receiverAddress.substr(colon+1)));
            source.statusPort=static_cast<unsigned int>(30000+receiverOptions.sources.size());
            source.exportPort=0; receiverOptions.sources.push_back(source);
        }
        const std::string receiverPlan=SupervisorIO::EncodeOptions(receiverOptions,std::string(64,'e'));
        const std::string receiverPlanPath=root+"/receiver-template.json";
        Write(receiverPlanPath,receiverPlan);
        const std::string oldDigest=EncodeJsonString(std::string(64,'b'));
        const auto digestPosition=bytes.find(oldDigest);
        Require(digestPosition!=std::string::npos,"Find receiver plan digest in fixed service fixture");
        bytes.replace(digestPosition,oldDigest.size(),EncodeJsonString(ConfigurationSha256(receiverPlan)));
        Write(configPath,bytes);
        {
            SystemExperimentArtifact baseline = DecodeSystemExperimentArtifact(original.experimentArtifactBytes);
            auto cobo = std::find_if(baseline.settings.cobos.begin(), baseline.settings.cobos.end(),
                [](const SystemExperimentCoBo &entry) { return entry.id == "C0"; });
            Require(cobo != baseline.settings.cobos.end(), "Scan fixture selected CoBo exists");
            const auto amplitude = cobo->configure.asad.generator.find("amplitudeStart");
            Require(amplitude != cobo->configure.asad.generator.end(), "Scan fixture amplitude is present");
            int startMv = std::atoi(amplitude->second.c_str());
            if (startMv >= 1000) {
                startMv = 999;
                cobo->configure.asad.generator["amplitudeStart"] = "999";
                baseline = DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(baseline));
            }
            const int stopMv = startMv + 1;
            const auto scan = BuildPulserAmplitudeScan(baseline, "C0", startMv, stopMv, 1);
            std::map<std::string, std::string> pointFiles;
            std::ostringstream plan;
            plan << "{\"schema_version\":1,\"kind\":\"getdaq.pulser-amplitude-scan\","
                 << "\"target_cobo\":\"C0\",\"start_mv\":" << startMv << ",\"stop_mv\":" << stopMv
                 << ",\"step_mv\":1,\"hardware_loaded_verified\":false,"
                 << "\"requires_full_configure_per_point\":true,\"acquisition_event_count_unspecified\":true,\"points\":[";
            for (std::size_t i = 0; i < scan.size(); ++i) {
                const std::string name = "point-" + std::to_string(i) + ".json";
                const std::string artifactBytes = EncodeSystemExperimentArtifact(scan[i].artifact);
                pointFiles[name] = artifactBytes;
                if (i) plan << ',';
                plan << "{\"index\":" << i << ",\"voltage_mv\":" << scan[i].voltageMv
                     << ",\"artifact\":" << EncodeJsonString(name)
                     << ",\"configuration_sha256\":" << EncodeJsonString(scan[i].artifact.configurationSha256)
                     << ",\"artifact_sha256\":" << EncodeJsonString(ConfigurationSha256(artifactBytes)) << "}";
            }
            plan << "]}";
            const std::string planBytes = plan.str();
            const auto loader = [&pointFiles](const std::string &name) { return pointFiles.at(name); };
            const auto summary = ValidateSystemScanPlan(planBytes, loader);
            const auto policy = ScanExecutionPolicy::ForEventCount(planBytes, summary, 5, 1000);
            const auto snapshot = ScanExecutionSnapshot::Create("fixed-scan-identity", planBytes, policy.Encode(), loader);

            const std::string inputDirectory=root+"/scan-deploy-input";
            IO::CreateDirectory(inputDirectory);
            Write(inputDirectory+"/scan-manifest.json",planBytes);
            for(const auto &point:pointFiles)Write(inputDirectory+"/"+point.first,point.second);
            const std::string policyPath=root+"/scan-policy.json",snapshotPath=root+"/scan-snapshot.json";
            Write(policyPath,policy.Encode());Write(snapshotPath,snapshot.Encode());

            const auto bundle=CreateScanPointDeployments(snapshot,configPath,
                {ScanReceiverPlanTemplate{"offline-node",receiverPlanPath,root+"/remote-plans"}},
                root+"/scan-deployment-bundle",loader);
            Require(bundle.points.size()==snapshot.PointCount() && bundle.manifestFile==root+"/scan-deployment-bundle/manifest.json",
                    "Offline scan deployment bundle covers every frozen point");
            std::set<std::string> deploymentBindings;
            for (std::size_t i=0;i<bundle.points.size();++i) {
                const auto &point=bundle.points[i];
                Require(point.index==i && point.configurationSha256==snapshot.Points()[i].configurationSha256 &&
                    point.artifactSha256==snapshot.Points()[i].artifactSha256 && point.receiverPlans.size()==1,
                    "Scan deployment point differs from frozen snapshot");
                const FixedEccConfiguration deployed(point.serviceFile);
                const auto deployedOptions=deployed.ControllerOptions();
                const auto deployedArtifact=DecodeSystemExperimentArtifact(deployedOptions.experimentArtifactBytes);
                Require(deployedArtifact.configurationSha256==point.configurationSha256 &&
                    deployedArtifact.artifactSha256==point.artifactSha256 && deployed.ReceiverOptions().endpoints[0].planSha256==point.receiverPlans[0].planSha256,
                    "Generated ECC deployment does not bind point artifact and receiver plan");
                const auto plan=DecodeSupervisorPlan(IO::ReadFile(point.receiverPlans[0].planFile,1024*1024));
                Require(plan.options.configurationSha256==point.configurationSha256 && plan.options.artifactSha256==point.artifactSha256 &&
                    plan.options.auditRoot!=receiverOptions.auditRoot && point.receiverPlans[0].targetPlanFile==root+"/remote-plans/scan-fixed-scan-identity-point-"+
                    (i==0 ? "000" : "001")+".json" && point.controllerAuditRoot.find(".scan-fixed-scan-identity/point-")!=std::string::npos &&
                    point.requiredControllerAuditParentDirectory==root+"/controller.scan-fixed-scan-identity",
                    "Generated receiver plan is frozen to its point and target path");
                deploymentBindings.insert(deployed.BindingSha256());
            }
            Require(deploymentBindings.size()==snapshot.PointCount(),"Each scan point receives a distinct fixed ECC deployment binding");
            const auto reverified=VerifyScanPointDeploymentBundle(snapshot,root+"/scan-deployment-bundle");
            Require(reverified.points.size()==snapshot.PointCount() &&
                reverified.executionId==snapshot.ExecutionId() &&
                reverified.points[1].serviceFile==bundle.points[1].serviceFile &&
                reverified.points[1].receiverPlans[0].planSha256==bundle.points[1].receiverPlans[0].planSha256,
                "Read-only bundle verifier did not recover the exact frozen per-point deployments");
            const std::string verifiedReceiverPlan=reverified.points[0].receiverPlans[0].planFile;
            const std::string originalReceiverPlan=IO::ReadFile(verifiedReceiverPlan,1024*1024);
            const auto replaceSealedFixture=[&](const std::string &contents) {
                Require(::chmod(verifiedReceiverPlan.c_str(),0600)==0,"Open sealed receiver-plan fixture");
                { std::ofstream output(verifiedReceiverPlan.c_str(),std::ios::binary|std::ios::trunc);
                  output.write(contents.data(),static_cast<std::streamsize>(contents.size()));
                  Require(output.good(),"Rewrite sealed receiver-plan fixture"); }
                Require(::chmod(verifiedReceiverPlan.c_str(),0400)==0,"Reseal receiver-plan fixture");
            };
            replaceSealedFixture(originalReceiverPlan+" ");
            Rejects([&] {VerifyScanPointDeploymentBundle(snapshot,root+"/scan-deployment-bundle");},
                "Sealed scan receiver plan digest mismatch");
            replaceSealedFixture(originalReceiverPlan);
            Require(VerifyScanPointDeploymentBundle(snapshot,root+"/scan-deployment-bundle").points.size()==snapshot.PointCount(),
                "Restored receiver-plan fixture did not verify after the tamper test");

            const std::string cliBundle=root+"/scan-deployment-cli";
            const std::string cliCommand=ShellQuote(GETDAQ_SCAN_DEPLOYMENT_TOOL)+" create "+
                ShellQuote(inputDirectory)+" "+ShellQuote(policyPath)+" "+ShellQuote(snapshotPath)+" "+
                ShellQuote(configPath)+" "+ShellQuote(cliBundle)+" "+ShellQuote(receiverPlanPath)+" "+
                ShellQuote(root+"/remote-plans");
            const std::string cliResultPath=root+"/scan-deployment-cli-result.json";
            Require(std::system((cliCommand+" > "+ShellQuote(cliResultPath)).c_str())==0,
                "Scan deployment CLI creates the reviewed bundle");
            const auto cliManifest=ParseStrictJson(IO::ReadFile(cliBundle+"/manifest.json",1024*1024));
            const auto cliResult=ParseStrictJson(IO::ReadFile(cliResultPath,4096));
            Require(IO::String(cliManifest.At("kind"))=="getdaq.scan-deployment-bundle" &&
                cliManifest.At("points").items.size()==snapshot.PointCount() &&
                IO::Boolean(cliResult.At("created")) &&
                !IO::Boolean(cliResult.At("remote_files_installed")) &&
                !IO::Boolean(cliResult.At("service_started")) &&
                !IO::Boolean(cliResult.At("hardware_commands_sent")),
                "Scan deployment CLI publishes every point from the validated source snapshot");
            Require(std::system((cliCommand+" >/dev/null 2>&1").c_str())!=0,
                "Scan deployment CLI refuses to replace an existing bundle");
            const std::string verifyCommand=ShellQuote(GETDAQ_SCAN_DEPLOYMENT_TOOL)+" verify "+
                ShellQuote(inputDirectory)+" "+ShellQuote(policyPath)+" "+ShellQuote(snapshotPath)+" "+ShellQuote(cliBundle);
            const std::string verifyResultPath=root+"/scan-deployment-verify-result.json";
            Require(std::system((verifyCommand+" > "+ShellQuote(verifyResultPath)).c_str())==0,
                "Scan deployment CLI verifies the sealed bundle against the frozen source snapshot");
            const auto verifyResult=ParseStrictJson(IO::ReadFile(verifyResultPath,4096));
            Require(IO::Boolean(verifyResult.At("verified")) &&
                verifyResult.At("point_count").text==std::to_string(snapshot.PointCount()) &&
                !IO::Boolean(verifyResult.At("services_started")) &&
                !IO::Boolean(verifyResult.At("hardware_commands_sent")),
                "Scan deployment verification reports an unexpected side effect or point count");
            const std::string cliBundleManifest=cliBundle+"/manifest.json";
            std::string tamperedManifest=IO::ReadFile(cliBundleManifest,1024*1024);
            const std::string frozenSnapshotSha=GetDAQ::ConfigurationSha256(snapshot.Encode());
            const auto snapshotAt=tamperedManifest.find(frozenSnapshotSha);
            Require(snapshotAt!=std::string::npos,"Find snapshot identity in generated deployment manifest");
            tamperedManifest.replace(snapshotAt,frozenSnapshotSha.size(),std::string(64,'0'));
            Require(::chmod(cliBundleManifest.c_str(),0600)==0,"Open deployment manifest for tamper fixture");
            { std::ofstream output(cliBundleManifest.c_str(),std::ios::binary|std::ios::trunc);
              output.write(tamperedManifest.data(),static_cast<std::streamsize>(tamperedManifest.size()));
              Require(output.good(),"Write tampered deployment manifest fixture"); }
            Require(::chmod(cliBundleManifest.c_str(),0400)==0,"Reseal tampered manifest fixture");
            Require(std::system((verifyCommand+" >/dev/null 2>&1").c_str())!=0,
                "Scan deployment CLI accepted a sealed manifest bound to a different snapshot");

            const std::string pointArtifactPath = root + "/point-zero-experiment.json";
            Write(pointArtifactPath, pointFiles.at(snapshot.Points()[0].artifact));
            const std::string pointConfigPath = root + "/point-zero-service.json";
            std::string pointConfigBytes = bytes;
            const std::string oldReference = Reference(artifactPath);
            const auto oldReferencePosition = pointConfigBytes.find(oldReference);
            Require(oldReferencePosition != std::string::npos, "Find fixed artifact reference for scan fixture");
            pointConfigBytes.replace(oldReferencePosition, oldReference.size(), Reference(pointArtifactPath));
            const auto auditPosition = pointConfigBytes.find(root + "/controller");
            Require(auditPosition != std::string::npos, "Find fixed audit root for scan fixture");
            pointConfigBytes.replace(auditPosition, (root + "/controller").size(), root + "/point-zero-controller");
            Write(pointConfigPath, pointConfigBytes);
            FixedEccConfiguration pointDeployment(pointConfigPath);
            const NativeEccOptions controller = pointDeployment.ControllerOptions();
            const RemoteReceiverOptions receiverOptions = pointDeployment.ReceiverOptions();
            const std::string dataBinding = RemoteReceiverBindingSha256(receiverOptions);
            const NativeEccPlan nativePlan = PlanNativeEccConfiguration(controller, dataBinding);
            const std::string instance(64, 'a'), nodeInstance(64, 'b'), epoch(64, 'c');
            const std::string nodePlan = receiverOptions.endpoints[0].planSha256;
            const std::string identity = "{\"schema\":1,\"service\":\"getdaq-ecc-server\",\"service_id\":\"" + pointDeployment.ServiceId() +
                "\",\"client_id\":\"" + pointDeployment.ClientId() + "\",\"instance_id\":\"" + instance +
                "\",\"snapshot_sha256\":\"" + nativePlan.snapshotSha256 + "\",\"input_binding_sha256\":\"" + pointDeployment.BindingSha256() +
                "\",\"configuration_sha256\":\"" + snapshot.Points()[0].configurationSha256 + "\",\"artifact_sha256\":\"" + snapshot.Points()[0].artifactSha256 +
                "\",\"data_binding_sha256\":\"" + dataBinding + "\"}";
            const std::string node = "{\"agent_id\":\"" + receiverOptions.endpoints[0].agentId + "\",\"instance_id\":\"" + nodeInstance +
                "\",\"plan_sha256\":\"" + nodePlan + "\",\"lifecycle\":{\"schema\":1,\"state\":\"fresh\","
                "\"needs_reconciliation\":false,\"plan_sha256\":\"" + nodePlan + "\",\"session_id\":\"\",\"message\":\"\",\"receivers\":[]}}";
            const std::string status = "{\"schema\":1,\"service\":\"getdaq-ecc-server\",\"instance_id\":\"" + instance +
                "\",\"snapshot_sha256\":\"" + nativePlan.snapshotSha256 + "\",\"operation_active\":false,\"operation_id\":\"\","
                "\"client_epoch\":\"\",\"control_needs_reconciliation\":false,\"message\":\"\","
                "\"controller\":{\"state\":1,\"transition\":0,\"error\":0,\"outcome_unknown\":false,"
                "\"needs_reconciliation\":false,\"operation_id\":\"\",\"message\":\"\"},\"data\":{\"needs_reconciliation\":false,"
                "\"controller_scope_sha256\":\"" + nativePlan.dataControllerScopeSha256 + "\",\"controller_epoch\":\"" + epoch +
                "\",\"message\":\"\",\"nodes\":[" + node + "]}}";
            const auto expected = BuildFixedScanRuntimeIdentityExpectation(snapshot, 0, pointDeployment, identity, status);
            Require(expected.serviceId == pointDeployment.ServiceId() && expected.clientId == pointDeployment.ClientId() &&
                    expected.snapshotSha256 == nativePlan.snapshotSha256 && expected.dataNodes.size() == 1 &&
                    expected.dataNodes[0].agentId == receiverOptions.endpoints[0].agentId,
                    "Fixed scan identity expectation did not derive deployment anchors");
            ScanRuntimePointIdentityRequest pointRequest;
            pointRequest.executionSnapshotSha256 = ConfigurationSha256(snapshot.Encode());
            pointRequest.pointIndex = 0;
            pointRequest.configurationSha256 = snapshot.Points()[0].configurationSha256;
            pointRequest.artifactSha256 = snapshot.Points()[0].artifactSha256;
            const auto serviceExpected = BuildFixedScanRuntimeIdentityExpectation(
                DecodeScanRuntimePointIdentityRequest(
                    EncodeScanRuntimePointIdentityRequest(pointRequest)),
                pointDeployment, identity, status);
            Require(EncodeScanRuntimeIdentityExpectation(serviceExpected) ==
                    EncodeScanRuntimeIdentityExpectation(expected),
                    "Service point-request identity differs from frozen-snapshot identity");
            pointRequest.artifactSha256 = std::string(64, 'f');
            Rejects([&] { BuildFixedScanRuntimeIdentityExpectation(
                pointRequest, pointDeployment, identity, status); },
                "Fixed ECC artifact/configuration differs from the frozen scan point");
            Rejects([&] { BuildFixedScanRuntimeIdentityExpectation(snapshot, 1, pointDeployment, identity, status); },
                    "frozen scan point");
            const std::string wrongClient = std::string(identity).replace(
                identity.find(pointDeployment.ClientId()), pointDeployment.ClientId().size(), "attacker");
            Rejects([&] { BuildFixedScanRuntimeIdentityExpectation(snapshot, 0, pointDeployment, wrongClient, status); },
                    "fixed point deployment");
            const std::string wrongScope = std::string(status).replace(
                status.find(nativePlan.dataControllerScopeSha256), nativePlan.dataControllerScopeSha256.size(), std::string(64, 'd'));
            Rejects([&] { BuildFixedScanRuntimeIdentityExpectation(snapshot, 0, pointDeployment, identity, wrongScope); },
                    "fixed point snapshot or data scope");
            FixedEccConfiguration offline(pointConfigPath, FixedEccConfiguration::OfflineEvidenceOnly());
            Rejects([&] { BuildFixedScanRuntimeIdentityExpectation(snapshot, 0, offline, identity, status); },
                    "validated control deployment inputs");
        }
        Require(!FixedEccConfiguration(configPath).AlarmStopEnabled(),"Legacy configuration unexpectedly enabled automatic alarm stop");
        Require(!FixedEccConfiguration(configPath).ReceiverStopEnabled(),"Legacy configuration enabled receiver failure stop");
        const auto receiverPolicy=[&](const std::string &json,unsigned int number) {
            auto modified=bytes; modified.insert(modified.size()-2,",\"receiver_stop_policy\":"+json);
            const auto file=root+"/receiver-policy-"+std::to_string(number)+".json"; Write(file,modified); return file;
        };
        {
            FixedEccConfiguration checked(receiverPolicy("{\"enabled\":true,\"poll_interval_ms\":100}",0));
            Require(checked.ReceiverStopEnabled() && checked.ReceiverPollIntervalMs()==100,"Receiver policy not bound");
        }
        Rejects([&] { FixedEccConfiguration bad(receiverPolicy("{\"enabled\":true,\"poll_interval_ms\":99}",1)); },"100..5000");
        Rejects([&] { FixedEccConfiguration bad(receiverPolicy("{\"enabled\":true,\"poll_interval_ms\":5001}",2)); },"100..5000");
        Rejects([&] { FixedEccConfiguration bad(receiverPolicy("{\"enabled\":\"true\",\"poll_interval_ms\":100}",3)); },"boolean");
        Rejects([&] { FixedEccConfiguration bad(receiverPolicy("{\"enabled\":true,\"poll_interval_ms\":100,\"retry\":true}",4)); },"fields");
        const auto policy=[&](const std::string &json,unsigned int number) {
            auto modified=bytes; modified.insert(modified.size()-2,",\"alarm_stop_policy\":"+json);
            const auto file=root+"/alarm-policy-"+std::to_string(number)+".json"; Write(file,modified); return file;
        };
        {
            FixedEccConfiguration checked(policy("{\"enabled\":true,\"poll_interval_ms\":25}",0));
            Require(checked.AlarmStopEnabled() && checked.AlarmPollIntervalMs()==25,"Reviewed automatic alarm policy not bound");
            struct stat st; Require(::lstat((root+"/controller").c_str(),&st) && errno==ENOENT,"Alarm policy offline check created ECC root");
        }
        Rejects([&] { FixedEccConfiguration bad(policy("{\"enabled\":true,\"poll_interval_ms\":24}",1)); },"25..5000");
        Rejects([&] { FixedEccConfiguration bad(policy("{\"enabled\":true,\"poll_interval_ms\":5001}",2)); },"25..5000");
        Rejects([&] { FixedEccConfiguration bad(policy("{\"enabled\":\"true\",\"poll_interval_ms\":100}",3)); },"boolean");
        Rejects([&] { FixedEccConfiguration bad(policy("{\"enabled\":true,\"poll_interval_ms\":100,\"retry\":true}",4)); },"fields");
        {
            r.options.cobos[0].prepare.settings.monitoring.subscribeAlarms=false;
            const auto unobserved=Options(r,root); r.options.cobos[0].prepare.settings.monitoring.subscribeAlarms=true;
            const auto unobservedPath=root+"/unobserved-experiment.json"; Write(unobservedPath,unobserved.experimentArtifactBytes);
            auto modified=bytes; const auto reference=Reference(artifactPath); const auto at=modified.find(reference);
            Require(at!=std::string::npos,"Find experiment binding for missing-subscription test"); modified.replace(at,reference.size(),Reference(unobservedPath));
            modified.insert(modified.size()-2,",\"alarm_stop_policy\":{\"enabled\":true,\"poll_interval_ms\":100}");
            const auto file=root+"/alarm-policy-unobserved.json"; Write(file,modified);
            Rejects([&] { FixedEccConfiguration bad(file); },"subscription for every active CoBo");
        }
        {
            FixedEccConfiguration config(configPath); config.Check();
            struct stat st; Require(::lstat((root+"/controller").c_str(),&st) && errno==ENOENT,"Offline fixed input check created ECC root");
            Require(config.ControllerOptions().deploymentSourceSha256==config.BindingSha256(),"Parent source binding missing");
            const auto ownershipConfig=root+"/service-ownership.json";
            Write(ownershipConfig,bytes.substr(0,bytes.find_last_of('}'))+",\"host_control_ownership\":{\"directory\":"+EncodeJsonString(root+"/ownership")+"}}");
            FixedEccConfiguration ownershipInput(ownershipConfig); ownershipInput.Check();
            Require(ownershipInput.ControllerOptions().hostControlOwnershipRoot==root+"/ownership","Ownership directory missing from fixed input");
            Require(ownershipInput.BindingSha256()!=config.BindingSha256(),"Ownership policy did not change fixed input binding");
            Require(::lstat((root+"/ownership").c_str(),&st)!=0 && errno==ENOENT,"Offline input check created ownership storage");
            EccControlJobs jobs(config); const auto request=Request(jobs,"start","illegal-start"); jobs.Submit(request); const auto failed=Terminal(jobs,"illegal-start");
            Rejects([&] { jobs.Submit(Request(jobs,"stop","internal-alarm-spoof")); },"Reserved");
            Require(!IO::Boolean(ParseStrictJson(jobs.AlarmsJson()).At("enabled")),"Disabled alarm endpoint is not cached/disabled");
            Require(failed.phase=="failed" && ParseStrictJson(failed.resultJson).At("state").text=="1","Illegal transition fabricated Running");
            Require(jobs.Submit(request).phase=="failed","Duplicate failed command replayed");
            auto changed=request; changed.command="prepare"; Rejects([&] { jobs.Submit(changed); },"different request key");
            auto other=Request(jobs,"describe","other-owner"); other.clientEpoch=std::string(64,'c'); Rejects([&] { jobs.Submit(other); },"epoch differs");
            const auto originalSnapshot=IO::ReadFile(root+"/controller/snapshot.json",1024*1024); Require(ParseStrictJson(originalSnapshot).At("deployment_source_sha256").text==config.BindingSha256(),"Native snapshot source digest missing");
            { std::fstream file(topologyPath,std::ios::in|std::ios::out|std::ios::binary); file.seekp(0); file.put('!'); file.flush(); Require(bool(file),"Inject fixed input corruption"); }
            Require(IO::ReadFile(topologyPath,1024*1024)!=topology,"Fixed topology corruption fixture did not change bytes");
            const auto rejectedInput=jobs.Submit(Request(jobs,"describe","changed-input"));
            Require(rejectedInput.phase=="failed" && rejectedInput.message.find("not executed")!=std::string::npos,
                    "Changed fixed topology did not durably reject the requested command");
            { std::ofstream file(topologyPath,std::ios::binary|std::ios::trunc); file<<topology; }
            Rejects([&] { config.Check(); },"latched"); Require(IO::Boolean(ParseStrictJson(jobs.StatusJson()).At("control_needs_reconciliation")),"Input restoration cleared network gate");
        }
        {
            FixedEccConfiguration config(configPath); EccControlJobs jobs(config); const auto identity=ParseStrictJson(jobs.IdentityJson());
            Require(jobs.Operation("illegal-start").phase=="failed" && IO::Boolean(ParseStrictJson(jobs.StatusJson()).At("control_needs_reconciliation")),"Reopen lost history/cleared gate");
            auto request=Request(jobs,"describe","after-reopen"); Rejects([&] { jobs.Submit(request); },"requires reconciliation");
        }
        {
            const auto directory=root+"/publication",manifest=Publication(directory,original.experimentArtifactBytes),packageArtifact=directory+"/experiment.json",publishedConfig=root+"/published-service.json";
            auto modified=bytes; const auto previous=Reference(artifactPath); const auto at=modified.find(previous); Require(at!=std::string::npos,"Find publication artifact field");
            modified.replace(at,previous.size(),Reference(packageArtifact));
            const auto audit=modified.find(root+"/controller"); Require(audit!=std::string::npos,"Find publication audit root"); modified.replace(audit,(root+"/controller").size(),root+"/published-controller");
            modified.insert(modified.size()-2,",\"configuration_publication\":"+Reference(manifest)); Write(publishedConfig,modified);
            {
                FixedEccConfiguration config(publishedConfig); config.Check(); Require(config.ControllerOptions().configurationPublicationSha256==GetDAQ::ConfigurationSha256(IO::ReadFile(manifest,1024*1024)),"Publication digest not bound");
                EccControlJobs jobs(config); const auto snapshot=ParseStrictJson(IO::ReadFile(root+"/published-controller/snapshot.json",1024*1024));
                Require(snapshot.At("configuration_publication_sha256").text==config.ControllerOptions().configurationPublicationSha256,"Parent snapshot publication missing");
                Require(!IO::Boolean(snapshot.At("actual_loaded_configuration_verified")),"Publication fabricated hardware readback");
                const auto source=directory+"/source-000.xcfg"; Require(::chmod(source.c_str(),0600)==0,"Inject writable publication source");
                const auto rejectedPublication = jobs.Submit(Request(jobs,"describe","writable-publication"));
                Require(rejectedPublication.phase=="failed" && rejectedPublication.message.find("not executed")!=std::string::npos,
                        "Writable publication input did not durably reject the requested command");
                Require(::chmod(source.c_str(),0400)==0,"Restore publication permissions"); Rejects([&] { config.Check(); },"latched");
            }
            {
                FixedEccConfiguration config(publishedConfig); const auto source=directory+"/source-000.xcfg";
                Require(::chmod(source.c_str(),0600)==0,"Allow publication content fault injection");
                { std::ofstream file(source,std::ios::binary|std::ios::trunc); file<<std::string(std::string("actual-receiver-test-settings").size(),'x'); }
                Require(::chmod(source.c_str(),0400)==0,"Restore read-only source after mutation"); Rejects([&] { config.Check(); },"changed");
                { Require(::chmod(source.c_str(),0600)==0,"Allow source restoration"); std::ofstream file(source,std::ios::binary|std::ios::trunc); file<<"actual-receiver-test-settings"; }
                Require(::chmod(source.c_str(),0400)==0,"Restore source mode"); Rejects([&] { config.Check(); },"latched");
            }
            {
                FixedEccConfiguration config(publishedConfig); const auto displaced=root+"/publication-displaced";
                Require(::rename(directory.c_str(),displaced.c_str())==0,"Replace publication directory fixture"); IO::CreateDirectory(directory);
                Rejects([&] { config.Check(); },"directory"); Require(::rmdir(directory.c_str())==0 && ::rename(displaced.c_str(),directory.c_str())==0,"Restore publication directory fixture");
                Rejects([&] { config.Check(); },"latched");
            }
            {
                auto wrong=modified; const auto bound=Reference(packageArtifact),external=Reference(artifactPath); const auto pos=wrong.find(bound); Require(pos!=std::string::npos,"Find wrong artifact path injection");
                wrong.replace(pos,bound.size(),external); const auto wrongPath=root+"/publication-wrong-artifact.json"; Write(wrongPath,wrong);
                Rejects([&] { FixedEccConfiguration bad(wrongPath); },"bound publication artifact");
            }
        }
        {
            const auto directory=root+"/publication",manifest=directory+"/publication.json";
            const auto manifestBytes=IO::ReadFile(manifest,1024*1024);const auto package=GetDAQ::DecodeConfigurationPublication(manifestBytes);
            GetDAQ::ConfigurationRegistration registration;registration.repositoryId=std::string(64,'9');registration.publicationSha256=GetDAQ::ConfigurationSha256(manifestBytes);
            registration.configurationSha256=package.configurationSha256;registration.artifactSha256=package.artifact.sha256;
            registration.effectiveSha256=package.effectiveSha256;registration.snapshotSha256=package.snapshotSha256;registration.manifestFile=manifest;
            const auto receipt=GetDAQ::EncodeConfigurationRegistration(registration),receiptPath=directory+"/registration.json";
            Write(receiptPath,receipt);Require(::chmod(receiptPath.c_str(),0400)==0,"Seal fixture registration");
            const auto selectionPath=root+"/selected.json",selectionBytes=GetDAQ::EncodeConfigurationSelection(GetDAQ::SelectConfigurationRegistration(receipt));
            Write(selectionPath,selectionBytes);Require(::chmod(selectionPath.c_str(),0400)==0,"Seal reviewed selection");
            const auto originalHistory=IO::ReadFile(root+"/controller/snapshot.json",1024*1024);
            const auto retained=ParseStrictJson(GetDAQ::SelectEccConfiguration(configPath,selectionPath,root+"/retained-history-deployment"));
            FixedEccConfiguration retainedConfig(IO::String(retained.At("configuration_file")));
            Require(retainedConfig.ControllerOptions().auditRoot==root+"/controller","Selection silently changed prior audit root");
            Rejects([&] {EccControlJobs previous(retainedConfig);},"immutable");
            Require(IO::ReadFile(root+"/controller/snapshot.json",1024*1024)==originalHistory,"Selection replaced prior history");
            auto firstBoot=bytes;const auto previousRoot=root+"/controller",newRoot=root+"/selected-controller";
            const auto rootAt=firstBoot.find(previousRoot);Require(rootAt!=std::string::npos,"Find first-boot audit template");
            firstBoot.replace(rootAt,previousRoot.size(),newRoot);const auto firstBootTemplate=root+"/first-boot-template.json";Write(firstBootTemplate,firstBoot);
            const auto result=ParseStrictJson(GetDAQ::SelectEccConfiguration(firstBootTemplate,selectionPath,root+"/selected-deployment"));
            const auto selectedFile=IO::String(result.At("configuration_file"));FixedEccConfiguration selected(selectedFile);selected.Check();
            const auto options=selected.ControllerOptions();Require(options.configurationRepositoryId==registration.repositoryId && options.configurationRegistrationSha256==GetDAQ::ConfigurationSha256(receipt),"Selected repository binding missing");
            struct stat st;Require(::lstat(newRoot.c_str(),&st) && errno==ENOENT,"Offline selection created controller");
            Require(!IO::Boolean(result.At("hardware_connected")) && !IO::Boolean(result.At("hardware_loaded_verified")) && !IO::Boolean(result.At("ecc_state_changed")),"Selection fabricated execution");
            Rejects([&] {GetDAQ::SelectEccConfiguration(firstBootTemplate,selectionPath,root+"/selected-deployment");},"create");
            {
                EccControlJobs jobs(selected);const auto cache=ParseStrictJson(jobs.ConfigurationJson());Require(cache.At("selection").At("repository_id").text==registration.repositoryId && !IO::Boolean(cache.At("hardware_loaded_verified")),"ECC configuration cache differs");
                const auto snapshot=ParseStrictJson(IO::ReadFile(newRoot+"/snapshot.json",1024*1024));
                Require(snapshot.At("configuration_selection_sha256").text==GetDAQ::ConfigurationSha256(selectionBytes) && snapshot.At("configuration_registration_sha256").text==GetDAQ::ConfigurationSha256(receipt),"Selection not bound to parent snapshot");
            }
            Require(::chmod(receiptPath.c_str(),0600)==0,"Inject writable registration");Rejects([&] {selected.Check();},"changed");
            Require(::chmod(receiptPath.c_str(),0400)==0,"Restore registration permissions");Rejects([&] {selected.Check();},"latched");
            auto wrong=registration;wrong.repositoryId=std::string(64,'a');const auto wrongSelection=root+"/wrong-selection.json";
            auto bad=GetDAQ::SelectConfigurationRegistration(GetDAQ::EncodeConfigurationRegistration(wrong));bad.registrationSha256=GetDAQ::ConfigurationSha256(receipt);
            Write(wrongSelection,GetDAQ::EncodeConfigurationSelection(bad));Require(::chmod(wrongSelection.c_str(),0400)==0,"Seal wrong selection");
            Rejects([&] {GetDAQ::SelectEccConfiguration(configPath,wrongSelection,root+"/wrong-deployment");},"binding differs");
            Require(::lstat((root+"/wrong-deployment/service.json").c_str(),&st) && errno==ENOENT,"Rejected selection published startup config");
        }
        adapter->destroy(); communicator->destroy(); std::cout<<"PASS fixed input/publication/selection binding, offline deployment, cached metadata, strict transition/dedup/owner, restoration latch and reopen gate; no hardware executed\n";
    } catch (const std::exception &e) { std::cerr<<"FAIL "<<e.what()<<'\n'; if (communicator) try { communicator->destroy(); } catch (...) {} return 1; }
}
