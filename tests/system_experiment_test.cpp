#include "systemexperimentconfiguration.h"
#include "systemscanplan.h"
#include "scanexecutionpolicy.h"
#include "scanexecutionsnapshot.h"
#include "strictjson.h"
#include "configurationsha256.h"
#include "configurationpublication.h"
#include "peakingtimescan.h"
#include "pulseramplitudescan.h"
#include "thresholdscan.h"
#include "autogainscan.h"
#include "agetconfiguration.h"
#include "effective_record_fixture.h"
#include "cobo_configure_fixture.h"
#include <iostream>
#include <functional>
#include <map>
#include <sstream>
#include <vector>
#include <stdexcept>
namespace {
void Require(bool condition,const char *why) { if (!condition) throw std::runtime_error(why); }
template<class F> void Rejects(F action) { try { action(); } catch (const std::exception &) { return; } throw std::runtime_error("Malformed system configuration accepted"); }
std::string Replace(std::string s,const std::string &from,const std::string &to) {
    const auto pos=s.find(from); Require(pos!=std::string::npos,"Missing replacement target"); return s.replace(pos,from.size(),to);
}
struct ScanPlanPoint { std::string fields; GetDAQ::SystemExperimentArtifact artifact; };
void CheckExecutionSnapshot(
    const std::string &planBytes, const std::string &policyBytes,
    const std::function<std::string(const std::string &)> &loader,
    unsigned int pointCount)
{
    using namespace GetDAQ;
    const ScanExecutionSnapshot snapshot = ScanExecutionSnapshot::Create(
        "scan-test-001", planBytes, policyBytes, loader);
    const ScanExecutionSnapshot restored = ScanExecutionSnapshot::Decode(
        snapshot.Encode(), planBytes, policyBytes, loader);
    Require(restored.ExecutionId() == "scan-test-001" &&
            restored.SourcePlanSha256() == ConfigurationSha256(planBytes) &&
            restored.SourcePolicySha256() == ConfigurationSha256(policyBytes) &&
            restored.PointCount() == pointCount &&
            restored.Points().front().artifactSha256 == snapshot.Points().front().artifactSha256,
            "Scan execution snapshot did not preserve the immutable plan/policy identity");
    Rejects([&] { ScanExecutionSnapshot::Create("../unsafe", planBytes, policyBytes, loader); });
    Rejects([&] { ScanExecutionSnapshot::Decode(
        Replace(snapshot.Encode(), snapshot.Points().front().artifactSha256, std::string(64, '0')),
        planBytes, policyBytes, loader); });
    Rejects([&] { ScanExecutionSnapshot::Decode(
        snapshot.Encode(), planBytes, policyBytes + " ", loader); });
}
void ValidateScanPlan(const std::string &kind,const std::string &metadata,
                      const std::vector<ScanPlanPoint> &points)
{
    std::map<std::string,std::string> files;
    std::ostringstream manifest;
    manifest << "{\"schema_version\":1,\"kind\":\"" << kind
             << "\",\"target_cobo\":\"C0\",\"hardware_loaded_verified\":false,"
                "\"requires_full_configure_per_point\":true,\"acquisition_event_count_unspecified\":true,";
    manifest << metadata << "\"points\":[";
    for (std::size_t i=0;i<points.size();++i) {
        const std::string name="point-"+std::to_string(i)+".json";
        const std::string bytes=GetDAQ::EncodeSystemExperimentArtifact(points[i].artifact);
        files[name]=bytes;
        if (i) manifest << ',';
        manifest << "{\"index\":" << i << ',' << points[i].fields
                 << "\"artifact\":\"" << name << "\",\"configuration_sha256\":\""
                 << points[i].artifact.configurationSha256 << "\",\"artifact_sha256\":\""
                 << GetDAQ::ConfigurationSha256(bytes) << "\"}";
    }
    manifest << "]}";
    const auto loader=[&](const std::string &name) { return files.at(name); };
    const auto summary=GetDAQ::ValidateSystemScanPlan(manifest.str(),loader);
    Require(summary.kind==kind && summary.targetCoBo=="C0" && summary.pointCount==points.size(),
            "Qt-free scan-plan validator returned the wrong summary");
    if (kind=="getdaq.aget-threshold-scan") {
        Rejects([&] { GetDAQ::ScanExecutionPolicy::ForEventCount(manifest.str(),summary,10,1000); });
        Rejects([&] { GetDAQ::ScanExecutionPolicy::Decode(
            "{\"schema_version\":1,\"kind\":\"getdaq.scan-execution-policy\",\"source_plan_sha256\":\""+
                GetDAQ::ConfigurationSha256(manifest.str())+"\",\"scan_kind\":\"getdaq.aget-threshold-scan\","
                "\"target_cobo\":\"C0\",\"point_count\":"+std::to_string(points.size())+","
                "\"stopping_strategy\":\"timed_windows\",\"windows_per_point\":3,\"window_duration_ms\":1000}",
            manifest.str(),summary); });
    } else {
        const auto policy=GetDAQ::ScanExecutionPolicy::ForEventCount(manifest.str(),summary,100000,3600000);
        const auto restored=GetDAQ::ScanExecutionPolicy::Decode(policy.Encode(),manifest.str(),summary);
        Require(restored.Strategy()==GetDAQ::ScanStopStrategy::EventCount &&
                restored.EventsPerPoint()==100000 && restored.MaximumDurationMsPerPoint()==3600000 &&
                restored.PointCount()==points.size(),
                "Event-count execution policy did not roundtrip");
        Rejects([&] { GetDAQ::ScanExecutionPolicy::ForEventCount(manifest.str(),summary,0,1000); });
        Rejects([&] { GetDAQ::ScanExecutionPolicy::ForEventCount(manifest.str(),summary,1000000001U,1000); });
        Rejects([&] { GetDAQ::ScanExecutionPolicy::ForEventCount(manifest.str(),summary,1000,0); });
        Rejects([&] { GetDAQ::ScanExecutionPolicy::Decode(
            policy.Encode(),manifest.str()+" ",summary); });
        CheckExecutionSnapshot(manifest.str(), policy.Encode(), loader,
                               static_cast<unsigned int>(points.size()));
    }
    Rejects([&] {
        GetDAQ::SystemScanPlanSummary wrongPlan=summary;
        wrongPlan.kind="wrong-kind";
        GetDAQ::ScanExecutionPolicy::ForEventCount(manifest.str(),wrongPlan,10,1000);
    });
    const std::string digest=points.front().artifact.configurationSha256;
    const std::string wrongDigest(64,'0');
    if (digest!=wrongDigest)
        Rejects([&] { GetDAQ::ValidateSystemScanPlan(Replace(manifest.str(),digest,wrongDigest),loader); });
    Rejects([&] { GetDAQ::ValidateSystemScanPlan(
        Replace(manifest.str(),"\"points\":[","\"unexpected\":0,\"points\":["),loader); });
    Rejects([&] { GetDAQ::ValidateSystemScanPlan(
        Replace(manifest.str(),"\"index\":0,","\"index\":0,\"unexpected\":0,"),loader); });
}
GetDAQ::SystemExperimentArtifact Artifact() {
    using namespace GetDAQ;
    SystemExperimentArtifact a; a.profileSha256=ConfigurationSha256("reviewed-profile");
    ExperimentConfigurationSource source; source.path="实验\"\\.xcfg"; source.sha256=ConfigurationSha256("original-xcfg"); a.sources.push_back(source);
    SystemExperimentCoBo c; c.id="C0"; c.configure=GetDAQTest::Configure(9);
    c.prepare.inputs.asadMask=c.prepare.clock.asadMask=c.prepare.monitoring.asadMask=c.prepare.calibration.asadMask=9;
    c.prepare.clock.allowNominalDifference=true; c.prepare.clock.boards.clear();
    for (unsigned int id:{0,3}) {
        AsAdInputBoardSettings b; b.asadId=id; b.positions[0].active=true; c.prepare.inputs.boards.push_back(b);
        AsAdBoardClockSettings clock; clock.asadId=id; clock.delayPs=-5335; c.prepare.clock.boards.push_back(clock);
        AsAdMonitoringBoardSettings monitor; monitor.asadId=id; monitor.externalTemperatureLower=-10; c.prepare.monitoring.boards.push_back(monitor);
        AsAdCalibrationSettings calibration; calibration.asadId=id; c.prepare.calibration.boards.push_back(calibration);
        c.prepare.calibration.chipVersions[4*id]=0x202;
    }
    c.configure.readout.aget.allowReg2BankResetDifference=true;
    a.settings.cobos.push_back(c);
    SystemExperimentCoBo disabled; disabled.id="C1"; disabled.enabled=false; a.settings.cobos.push_back(disabled);
    SystemExperimentMutant m; m.id="M0"; m.settings.timestamp10ns=(UINT64_C(1)<<48)-1; m.settings.eventNumber=UINT32_MAX;
    m.settings.twoProtonCounterCommand=false; m.settings.inspection[3]=255; a.settings.mutants.push_back(m);
    return a;
}
}
int main() {
    try {
        using namespace GetDAQ;
        Require(ConfigurationSha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","Empty SHA-256 vector");
        Require(ConfigurationSha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","ABC SHA-256 vector");
        Require(ConfigurationSha256(std::string(1000000,'a'))=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0","Million-A SHA-256 vector");
        auto a=Artifact(); const auto bytes=EncodeSystemExperimentArtifact(a); const auto loaded=DecodeSystemExperimentArtifact(bytes);
        {
            ConfigurationPublication p; p.rootSource=a.sources[0].path; p.configurationSha256=loaded.configurationSha256;
            p.effectiveSha256=std::string(64,'a'); p.snapshotSha256=std::string(64,'b');
            std::map<std::string,std::string> files;
            const auto file=[&](const std::string &name,const std::string &data) { PublicationFile f; f.stored=name; f.bytes=data.size(); f.sha256=ConfigurationSha256(data); files[name]=data; return f; };
            p.artifact=file("experiment.json",bytes); p.profile=file("profile.json","reviewed-profile");
            PublicationSource source; source.sourcePath=p.rootSource; source.file=file("source-000.xcfg","original-xcfg"); p.sources.push_back(source);
            const auto snapshot=GetDAQTest::EmptyEffectiveSnapshot(p.rootSource,source.file.sha256,source.file.bytes);
            const auto snapshotRecord=GetDAQ::ParseStrictJson(snapshot);
            p.effectiveSha256=snapshotRecord.At("effective_sha256").text; p.snapshotSha256=snapshotRecord.At("snapshot_sha256").text;
            p.effective=file("effective.json",snapshot);
            const auto manifest=EncodeConfigurationPublication(p); const auto publication=DecodeConfigurationPublication(manifest);
            const auto read=[&](const PublicationFile &f) { return files.at(f.stored); };
            Require(VerifyConfigurationPublication(publication,read).artifactSha256==loaded.artifactSha256,"Publication artifact differs");
            Require(EncodeConfigurationPublication(publication)==manifest,"Publication codec does not roundtrip");
            auto changed=p; changed.sources[0].file.stored="../source.xcfg"; Rejects([&] { EncodeConfigurationPublication(changed); });
            changed=p; changed.configurationSha256=std::string(64,'0'); Rejects([&] { VerifyConfigurationPublication(changed,read); });
            changed=p; changed.sources[0].file.sha256=std::string(64,'0'); Rejects([&] { VerifyConfigurationPublication(changed,read); });
            changed=p; changed.sources[0].file.bytes=8*1024*1024+1; Rejects([&] { EncodeConfigurationPublication(changed); });
            changed=p; changed.sources.clear();
            for (std::size_t i=0;i<256;++i) {
                auto entry=source; entry.sourcePath=std::string(4050,'a')+PublicationSourceName(i); entry.file.stored=PublicationSourceName(i); changed.sources.push_back(entry);
            }
            changed.rootSource=changed.sources[0].sourcePath; Rejects([&] { EncodeConfigurationPublication(changed); });
            Rejects([&] { DecodeConfigurationPublication(Replace(manifest,"\"schema_version\":1","\"schema_version\":1,\"schema_version\":1")); });
            files["profile.json"]="changed-profile"; Rejects([&] { VerifyConfigurationPublication(p,read); }); files["profile.json"]="reviewed-profile";
            files.erase("source-000.xcfg"); Rejects([&] { VerifyConfigurationPublication(p,read); });
        }
        Require(EncodeSystemExperimentArtifact(loaded)==bytes,"Complete experiment codec did not roundtrip");
        {
            auto baseline = Artifact();
            auto target = baseline.settings.cobos[0];
            baseline.settings.cobos[0].id = "C1";
            baseline.settings.cobos[0].enabled = false;
            target.id = "C0";
            baseline.settings.cobos[1] = target;
            baseline = DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(baseline));
            const auto scan = BuildPulserAmplitudeScan(baseline,"C0",-1000,0,500);
            Require(scan.size()==3,"Pulser scan point count mismatch");
            const int expectedVoltages[] = {-1000,-500,0};
            std::vector<ScanPlanPoint> planPoints;
            for (unsigned int i=0;i<scan.size();++i) {
                Require(scan[i].index==i && scan[i].voltageMv==expectedVoltages[i],"Pulser scan order mismatch");
                auto expected=baseline.settings;
                expected.cobos[1].configure.asad.generator["amplitudeStart"]=std::to_string(expectedVoltages[i]);
                Require(EncodeSystemExperimentSettings(scan[i].artifact.settings)==EncodeSystemExperimentSettings(expected),
                        "Pulser scan changed non-target settings");
                Require(scan[i].artifact.configurationSha256==SystemExperimentDigest(expected),"Pulser scan digest is stale");
                if (i) Require(scan[i].artifact.configurationSha256!=scan[i-1].artifact.configurationSha256,
                               "Pulser scan points have identical digests");
                planPoints.push_back({"\"voltage_mv\":"+std::to_string(scan[i].voltageMv)+",",scan[i].artifact});
            }
            ValidateScanPlan("getdaq.pulser-amplitude-scan",
                "\"start_mv\":-1000,\"stop_mv\":0,\"step_mv\":500,",planPoints);
            Rejects([&] { BuildPulserAmplitudeScan(baseline,"missing",-1000,0,500); });
            Rejects([&] { BuildPulserAmplitudeScan(baseline,"C1",-1000,0,500); });
            Rejects([&] { BuildPulserAmplitudeScan(baseline,"C0",-500,0,500); });
            Rejects([&] { BuildPulserAmplitudeScan(baseline,"C0",-1000,0,300); });
            Rejects([&] { BuildPulserAmplitudeScan(baseline,"C0",0,-1000,500); });
            Rejects([&] { BuildPulserAmplitudeScan(baseline,"C0",-1000,1000,1); });
            Rejects([&] { BuildPulserAmplitudeScan(baseline,"C0",-1001,0,1); });
        }
        {
            auto baseline = Artifact();
            auto target = baseline.settings.cobos[0];
            baseline.settings.cobos[0].id = "C1";
            baseline.settings.cobos[0].enabled = false;
            target.id = "C0";
            baseline.settings.cobos[1] = target;
            baseline = DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(baseline));
            const auto scan = BuildThresholdScan(baseline,"C0",0,0,0,24,8);
            const unsigned int expectedCodes[] = {0,8,16,24};
            std::vector<ScanPlanPoint> planPoints;
            Require(scan.size()==4,"Threshold scan point count mismatch");
            for (unsigned int i=0;i<scan.size();++i) {
                Require(scan[i].index==i && scan[i].thresholdCode==expectedCodes[i],"Threshold scan order mismatch");
                auto expected=baseline.settings;
                auto &chip=expected.cobos[1].configure.readout.aget.chips[0];
                chip.settings.globals["Reg1/GlobalThresholdValue"]=std::to_string(expectedCodes[i]/16U);
                for (auto &channel:chip.settings.channels) channel.threshold=expectedCodes[i]%16U;
                Require(EncodeSystemExperimentSettings(scan[i].artifact.settings)==EncodeSystemExperimentSettings(expected),
                        "Threshold scan changed settings outside the explicitly selected AGET threshold");
                Require(scan[i].artifact.configurationSha256==SystemExperimentDigest(expected),"Threshold scan digest is stale");
                if (i) Require(scan[i].artifact.configurationSha256!=scan[i-1].artifact.configurationSha256,
                               "Threshold scan points have identical digests");
                Require(DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(scan[i].artifact)).configurationSha256==
                        scan[i].artifact.configurationSha256,"Threshold scan artifact failed roundtrip");
                planPoints.push_back({"\"threshold_code\":"+std::to_string(scan[i].thresholdCode)+",",scan[i].artifact});
            }
            ValidateScanPlan("getdaq.aget-threshold-scan",
                "\"target_asad\":0,\"target_aget\":0,\"start_code\":0,\"stop_code\":24,\"step_code\":8,",planPoints);
            Rejects([&] { BuildThresholdScan(baseline,"missing",0,0,0,8,8); });
            Rejects([&] { BuildThresholdScan(baseline,"C1",0,0,0,8,8); });
            Rejects([&] { BuildThresholdScan(baseline,"C0",0,3,0,8,8); });
            Rejects([&] { BuildThresholdScan(baseline,"C0",0,0,16,16,1); });
            Rejects([&] { BuildThresholdScan(baseline,"C0",0,0,0,24,7); });
            Rejects([&] { BuildThresholdScan(baseline,"C0",0,0,0,127,0); });
            Rejects([&] { BuildThresholdScan(baseline,"C0",0,0,24,8,8); });
            Rejects([&] { BuildThresholdScan(baseline,"C0",0,0,128,128,1); });
        }
        {
            auto baseline=Artifact();
            auto target=baseline.settings.cobos[0];
            baseline.settings.cobos[0].id="C1"; baseline.settings.cobos[0].enabled=false;
            target.id="C0"; baseline.settings.cobos[1]=target;
            baseline=DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(baseline));
            const auto scan=BuildAutoGainScan(baseline,"C0",0,0,0,1,-1000,-500,500);
            const unsigned int expectedChannels[]={1,1,0,0};
            const int expectedVoltages[]={-1000,-500,-1000,-500};
            std::vector<ScanPlanPoint> planPoints;
            Require(scan.size()==4,"AutoGain scan point count mismatch");
            for(unsigned int i=0;i<scan.size();++i) {
                Require(scan[i].index==i && scan[i].channelId==expectedChannels[i] &&
                        scan[i].voltageMv==expectedVoltages[i],"AutoGain channel/amplitude ordering mismatch");
                auto expected=baseline.settings;
                auto &cobo=expected.cobos[1];
                cobo.configure.asad.generator["amplitudeStart"]=std::to_string(expectedVoltages[i]);
                auto &chip=cobo.configure.readout.aget.chips[0].settings;
                for(auto &channel:chip.channels) channel.selectedForTest=channel.id==expectedChannels[i];
                Require(EncodeSystemExperimentSettings(scan[i].artifact.settings)==EncodeSystemExperimentSettings(expected),
                        "AutoGain point changed unrelated system settings or selected multiple test channels");
                Require(scan[i].artifact.configurationSha256==SystemExperimentDigest(expected),
                        "AutoGain configuration digest is stale");
                if(i) Require(scan[i].artifact.configurationSha256!=scan[i-1].artifact.configurationSha256,
                              "AutoGain points have identical digests");
                planPoints.push_back({"\"channel_id\":"+std::to_string(scan[i].channelId)+
                    ",\"voltage_mv\":"+std::to_string(scan[i].voltageMv)+",",scan[i].artifact});
            }
            ValidateScanPlan("getdaq.auto-gain-scan",
                "\"target_asad\":0,\"target_aget\":0,\"first_channel\":0,\"last_channel\":1,"
                "\"start_mv\":-1000,\"stop_mv\":-500,\"step_mv\":500,"
                "\"channel_order\":\"aget-reg3-bit-ascending-then-reg4-bit-ascending\","
                "\"legacy_channel_mask_mapping_verified_from_source\":true,"
                "\"gain_result_interpretation_verified\":false,",planPoints);
            const auto allChannels=BuildAutoGainScan(baseline,"C0",0,0,0,67,-1000,-1000,1);
            Require(allChannels.size()==68 && allChannels.front().channelId==33 &&
                    allChannels[33].channelId==0 && allChannels[34].channelId==34 &&
                    allChannels.back().channelId==67,
                    "AutoGain did not preserve legacy Reg3/Reg4 one-hot channel order");
            for (const auto &point:allChannels) {
                const auto &chip=point.artifact.settings.cobos[1].configure.readout.aget.chips[0].settings;
                const auto image=CompileAget(chip);
                const std::uint64_t reg3=point.channelId<=33 ? (UINT64_C(1)<<(33-point.channelId)) : 0;
                const std::uint64_t reg4=point.channelId>=34 ? (UINT64_C(1)<<(point.channelId-34)) : 0;
                Require(image.registers[2].value==reg3 && image.registers[3].value==reg4,
                        "AutoGain channel ID does not map to the legacy one-hot AGET register");
            }
            Rejects([&] { BuildAutoGainScan(baseline,"missing",0,0,0,1,-1000,-500,500); });
            Rejects([&] { BuildAutoGainScan(baseline,"C0",0,3,0,1,-1000,-500,500); });
            Rejects([&] { BuildAutoGainScan(baseline,"C0",0,0,0,67,-1000,1000,1); });
            Rejects([&] { BuildAutoGainScan(baseline,"C0",0,0,1,0,-1000,-500,500); });
            Rejects([&] { BuildAutoGainScan(baseline,"C0",0,0,0,68,-1000,-500,500); });
            Rejects([&] { BuildAutoGainScan(baseline,"C0",0,0,0,1,-500,-500,500); });
            Rejects([&] { BuildAutoGainScan(baseline,"C0",0,0,0,1,-1000,0,300); });
            Rejects([&] { BuildAutoGainScan(baseline,"C0",0,0,0,1,-1000,-500,0); });
            Rejects([&] { BuildAutoGainScan(baseline,"C0",0,0,0,1,-1001,-500,500); });
            auto inactive=baseline;
            inactive.settings.cobos[1].configure.readout.aget.chips[0].settings.channels[1].active=false;
            Rejects([&] { BuildAutoGainScan(inactive,"C0",0,0,0,1,-1000,-500,500); });
        }
        {
            auto baseline = Artifact();
            // Place the selected C0 after another configured entry. The scan
            // must resolve by ID, never by the first vector position.
            auto target = baseline.settings.cobos[0];
            baseline.settings.cobos[0].id = "C1";
            baseline.settings.cobos[0].enabled = false;
            target.id = "C0";
            baseline.settings.cobos[1] = target;
            auto &co = baseline.settings.cobos[1];
            co.configure.readout.aget.chips[1].settings.chipVersion = 0x203;
            co.configure.readout.aget.chips[1].settings.globals["Reg1/peackingTime"] = "68";
            co.prepare.calibration.chipVersions[12] = 0x203;
            baseline = DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(baseline));
            const auto scan = BuildPeakingTimeScan(baseline,"C0");
            std::vector<ScanPlanPoint> planPoints;
            const unsigned int agetTimes[16] = {70,117,232,280,334,383,502,541,568,632,721,760,831,870,976,1014};
            const unsigned int astreTimes[16] = {68,116,180,225,343,442,541,641,739,836,943,1039,2034,3985,5770,7700};
            Require(scan.size()==16,"Peaking-time plan does not contain all sixteen settings");
            for (unsigned int i=0;i<16;++i) {
                Require(scan[i].index==i,"Peaking-time scan order changed");
                planPoints.push_back({"",scan[i].artifact});
                const auto &chips=scan[i].artifact.settings.cobos[1].configure.readout.aget.chips;
                Require(chips[0].settings.globals.at("Reg1/peackingTime")==std::to_string(agetTimes[i]),"AGET peaking table mismatch");
                Require(chips[1].settings.globals.at("Reg1/peackingTime")==std::to_string(astreTimes[i]),"ASTRE peaking table mismatch");
                auto expected=baseline.settings;
                for (auto &chip:expected.cobos[1].configure.readout.aget.chips)
                    chip.settings.globals["Reg1/peackingTime"]=
                        std::to_string(chip.settings.chipVersion==0x203 ? astreTimes[i] : agetTimes[i]);
                Require(!scan[i].artifact.settings.cobos[0].enabled &&
                        EncodeSystemExperimentSettings(scan[i].artifact.settings)==EncodeSystemExperimentSettings(expected),
                        "Scan changed the non-target CoBo or fields outside the selected scan parameters");
                Require(scan[i].artifact.sources[0].sha256==baseline.sources[0].sha256,"Scan changed source provenance");
                Require(scan[i].artifact.configurationSha256==SystemExperimentDigest(scan[i].artifact.settings),"Scan configuration digest is stale");
                if (i) Require(scan[i].artifact.configurationSha256!=scan[i-1].artifact.configurationSha256,"Adjacent scan points have the same execution digest");
                Require(DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(scan[i].artifact)).configurationSha256==scan[i].artifact.configurationSha256,"Scan artifact failed roundtrip");
            }
            ValidateScanPlan("getdaq.peaking-time-scan","",planPoints);
            auto invalid = baseline; invalid.settings.cobos[1].enabled=false;
            Rejects([&] { BuildPeakingTimeScan(invalid,"C0"); });
            Rejects([&] { BuildPeakingTimeScan(baseline,"missing"); });
            invalid = baseline; invalid.settings.cobos[1].configure.readout.aget.chips[0].settings.globals["Reg1/peackingTime"]="71";
            Rejects([&] { BuildPeakingTimeScan(invalid,"C0"); });
        }
        Require(loaded.configurationSha256==SystemExperimentDigest(a.settings) && loaded.artifactSha256==ConfigurationSha256(bytes),"Wrong digest scope");
        Require(loaded.settings.mutants[0].settings.timestamp10ns==(UINT64_C(1)<<48)-1 && loaded.settings.mutants[0].settings.eventNumber==UINT32_MAX,"Counter precision lost");
        Require(loaded.settings.cobos[0].prepare.clock.boards[1].delayPs==-5335 && loaded.settings.cobos[0].prepare.monitoring.boards[0].externalTemperatureLower==-10,"Signed settings lost");
        Require(loaded.sources[0].path==a.sources[0].path && loaded.settings.cobos[0].configure.readout.aget.allowReg2BankResetDifference && !loaded.settings.mutants[0].settings.twoProtonCounterCommand,"Sources/compatibility policies lost");
        auto changed=a; changed.settings.cobos[0].prepare.manualReadDataDelay=20;
        Require(SystemExperimentDigest(changed.settings)!=loaded.configurationSha256,"Execution parameter changed without version change");
        changed=a; changed.sources[0].path="different-location.xcfg";
        const auto moved=DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(changed));
        Require(moved.configurationSha256==loaded.configurationSha256 && moved.artifactSha256!=loaded.artifactSha256,"Provenance relocation changed execution digest or lost byte digest");
        for (const auto &s:std::vector<std::string>{"", "{} {}", "[1,]", "{\"a\":1,\"a\":2}", "{\"a\":1,\"\\u0061\":2}", "01", "+1", "1.", "1e", "NaN", "\"\\ud800\"", "\"\\udc00\"", "\"\\ud800\\u0041\"", "\"\\x41\"", "true false", std::string("\"\xc0\xaf\"",4)})
            Rejects([&] { ParseStrictJson(s); });
        Require(ParseStrictJson("\"\\ud83d\\ude00\"").text=="\xf0\x9f\x98\x80","Unicode surrogate pair");
        Rejects([] { ParseStrictJson(std::string(65,'[')+"0"+std::string(65,']')); });
        Rejects([] { ParseStrictJson("\""+std::string(4097,'a')+"\""); });
        Rejects([&] { DecodeSystemExperimentArtifact(Replace(bytes,"\"schema_version\":1","\"schema_version\":2")); });
        Rejects([&] { DecodeSystemExperimentArtifact(Replace(bytes,"\"manualReadDataDelay\":\"14\"","\"manualReadDataDelay\":\"15\"")); });
        Rejects([&] { DecodeSystemExperimentArtifact(Replace(bytes,"\"enabled\":true","\"enabled\":\"true\"")); });
        Rejects([&] { DecodeSystemExperimentArtifact(Replace(bytes,"\"manualReadDataDelay\":\"14\"","\"manualReadDataDelay\":14")); });
        Rejects([&] { DecodeSystemExperimentArtifact(Replace(bytes,"\"manualReadDataDelay\":\"14\"","\"manualReadDataDelay\":\"014\"")); });
        Rejects([&] { DecodeSystemExperimentArtifact(Replace(bytes,"\"manualReadDataDelay\":\"14\"","\"manualReadDataDelay\":\"4294967296\"")); });
        Rejects([&] { DecodeSystemExperimentArtifact(Replace(bytes,"\"adcDoubleDataRate\":true","\"adcDoubleDataRate\":true,\"unexpected\":true")); });
        Rejects([&] { DecodeSystemExperimentArtifact(Replace(bytes,"\"inspection\":[\"0\",\"0\",\"0\",\"255\"]","\"inspection\":[\"0\"]")); });
        Rejects([&] { DecodeSystemExperimentArtifact(Replace(bytes,"\"settleAttempts\":\"10\",","")); });
        Rejects([&] { DecodeSystemExperimentArtifact(Replace(bytes,"\"phaseDegrees\":\"270\"","\"phaseDegrees\":\"-2147483649\"")); });
        Rejects([&] { DecodeSystemExperimentArtifact(Replace(bytes,"\"writingMHz\":\"100\"","\"writingMHz\":\"1e999\"")); });
        changed=a; changed.settings.cobos[0].prepare.inputs.asadMask=1; Rejects([&] { EncodeSystemExperimentArtifact(changed); });
        changed=a; changed.settings.mutants[0].settings.gmcShelfMask=1; Rejects([&] { EncodeSystemExperimentArtifact(changed); });
        changed=a; changed.settings.cobos[1].id="C0"; Rejects([&] { EncodeSystemExperimentArtifact(changed); });
        changed=a; changed.settings.cobos[0].prepare.calibration.chipVersions[0]=0x201; Rejects([&] { EncodeSystemExperimentArtifact(changed); });
        std::cout<<"PASS complete experiment codec/compatibility precision, digest scopes, bounded strict JSON and malformed inputs\n"; return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
}
