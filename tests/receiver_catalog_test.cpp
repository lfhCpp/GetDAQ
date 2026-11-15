#include "receiver_catalog_fixture.h"
#include "peakingtimescan.h"
#include "autogainscan.h"
#include "pulseramplitudescan.h"
#include "peakingtimescan.h"
#include "thresholdscan.h"
#include "cobo_configure_fixture.h"
#include <sys/wait.h>
#include <map>
#include <utility>
using namespace GetDAQCatalogFixture;
namespace {
std::string ReplaceAll(std::string value,const std::string &from,const std::string &to) {
    std::size_t position=0;
    while ((position=value.find(from,position))!=std::string::npos) { value.replace(position,from.size(),to); position+=to.size(); }
    return value;
}
SystemExperimentArtifact ScanArtifact() {
    SystemExperimentArtifact artifact; artifact.profileSha256=ConfigurationSha256("scan-binding-profile");
    ExperimentConfigurationSource source; source.path="scan-binding.xcfg"; source.sha256=ConfigurationSha256("scan-binding-source"); artifact.sources.push_back(source);
    SystemExperimentCoBo cobo; cobo.id="C0"; cobo.configure=GetDAQTest::Configure(1);
    cobo.prepare.inputs.asadMask=cobo.prepare.clock.asadMask=cobo.prepare.monitoring.asadMask=cobo.prepare.calibration.asadMask=1;
    cobo.prepare.clock.allowNominalDifference=true;
    AsAdInputBoardSettings input; input.asadId=0; input.positions[0].active=true; cobo.prepare.inputs.boards.push_back(input);
    AsAdBoardClockSettings clock; clock.asadId=0; clock.delayPs=-5335; cobo.prepare.clock.boards.push_back(clock);
    AsAdMonitoringBoardSettings monitoring; monitoring.asadId=0; monitoring.externalTemperatureLower=-10; cobo.prepare.monitoring.boards.push_back(monitoring);
    AsAdCalibrationSettings calibration; calibration.asadId=0; cobo.prepare.calibration.boards.push_back(calibration); cobo.prepare.calibration.chipVersions[0]=0x202;
    artifact.settings.cobos.push_back(cobo); return DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(artifact));
}
void InterruptedReplacementRecovery(bool candidateAlreadyMoved) {
    Fixture oldFixture;
    { ReceiverCatalog catalog(oldFixture.options); catalog.Execute(); }
    Fixture newFixture(8192,false,false,false,false,'e');
    newFixture.options.operationId=candidateAlreadyMoved ? "catalog-crash-after-publish" : "catalog-crash-after-backup";
    newFixture.options.receiver.catalogRoot=newFixture.root+"/catalog-new"; CreateDirectory(newFixture.options.receiver.catalogRoot);
    newFixture.options.stopped.planSha256=ConfigurationSha256(EncodeOptions(newFixture.options.receiver,newFixture.options.executableSha256));
    newFixture.target=newFixture.options.receiver.catalogRoot+"/Test/run_0000/node-0";
    { ReceiverCatalog catalog(newFixture.options); catalog.Execute(); }
    const auto candidate=oldFixture.root+"/replacement-crash-candidate";
    const auto candidateReceipt=PrepareReceiverCatalogReplacement(oldFixture.target,newFixture.target,candidate);
    const auto oldReceipt=InspectReceiverCatalog(oldFixture.target),newReceipt=InspectReceiverCatalog(newFixture.target);
    const auto requestBytes=ReadFile(newFixture.target+"/request.json",1024*1024);
    const auto request=DecodeReceiverCatalogRequest(requestBytes);
    const auto requestKey=ReceiverCatalogRequestKey(request);
    const auto manifest=ParseStrictJson(ReadFile(candidate+"/manifest.json",16*1024*1024));
    const auto backup=String(manifest.At("publication_backup"));
    const auto slash=oldFixture.target.find_last_of('/');
    const auto parent=oldFixture.target.substr(0,slash),leaf=oldFixture.target.substr(slash+1);
    const auto journal=parent+"/."+leaf+".replacement-"+
        ConfigurationSha256("getdaq.catalog-replacement-transaction:1\n"+request.operationId).substr(0,24)+".txn";
    const auto intent="{\"schema\":1,\"kind\":\"getdaq.catalog-replacement-intent\",\"operation_id\":"+EncodeJsonString(request.operationId)+
        ",\"request_key\":"+EncodeJsonString(requestKey)+",\"target\":"+EncodeJsonString(oldFixture.target)+
        ",\"backup\":"+EncodeJsonString(backup)+",\"candidate\":"+EncodeJsonString(candidate)+
        ",\"old_receipt_sha256\":"+EncodeJsonString(ConfigurationSha256(oldReceipt))+
        ",\"new_receipt_sha256\":"+EncodeJsonString(ConfigurationSha256(newReceipt))+
        ",\"candidate_receipt_sha256\":"+EncodeJsonString(ConfigurationSha256(candidateReceipt))+"}\n";
    CreateDirectory(journal); WriteNew(journal+"/lock",""); WriteNew(journal+"/intent.json",intent);
    Require(!::rename(oldFixture.target.c_str(),backup.c_str()),"Simulate crash after old catalog preservation");
    if (candidateAlreadyMoved) {
        const auto oldMarker="{\"phase\":\"old-preserved\",\"operation_id\":"+EncodeJsonString(request.operationId)+
            ",\"receipt_sha256\":"+EncodeJsonString(ConfigurationSha256(oldReceipt))+"}\n";
        WriteNew(journal+"/old-preserved.json",oldMarker);
        Require(!::rename(candidate.c_str(),oldFixture.target.c_str()),"Simulate crash after candidate publish rename");
    }
    Fd activeTransactionLock(::open((journal+"/lock").c_str(),O_RDWR|O_CLOEXEC));
    Require(activeTransactionLock.value>=0 && !::flock(activeTransactionLock.value,LOCK_EX|LOCK_NB),
        "Hold replacement transaction lock during observer concurrency check");
    Reject([&]{InspectReceiverCatalogReplacement(oldFixture.target,newFixture.target,candidate,request.operationId);});
    Require(Exists(backup+"/catalog-complete.json") &&
        (candidateAlreadyMoved ? InspectReceiverCatalog(oldFixture.target)==candidateReceipt : !Exists(oldFixture.target+"/catalog-complete.json")),
        "Busy observer changed an interrupted replacement path");
    Require(!::flock(activeTransactionLock.value,LOCK_UN),"Release replacement transaction observation lock");
    const auto observed=ParseStrictJson(InspectReceiverCatalogReplacement(oldFixture.target,newFixture.target,candidate,request.operationId));
    Require(String(observed.At("phase"))==(candidateAlreadyMoved ? "published" : "old_preserved") &&
        !Boolean(observed.At("recovery_authorized")) && String(observed.At("raw_payload_bytes_read"))=="0",
        "Read-only replacement observation did not classify the interrupted filesystem phase");
    Require(PublishReceiverCatalogReplacement(oldFixture.target,newFixture.target,candidate)==candidateReceipt &&
        InspectReceiverCatalog(oldFixture.target)==candidateReceipt &&
        ReadFile(backup+"/catalog-complete.json",65536)==oldReceipt,
        "Same-operation publisher did not recover the prepared filesystem phase");
}
void ThresholdCatalogCliSmoke(const std::string &catalogProgram,const std::string &loader=std::string(),
                              const std::string &libraryPath=std::string()) {
    std::string ownedRoot;
    {
        Fixture fixture(80,true); ownedRoot=fixture.root;
        const auto points=BuildThresholdScan(ScanArtifact(),"C0",0,0,0,0,1);
        fixture.options.receiver.configurationSha256=points[0].artifact.configurationSha256;
        fixture.options.receiver.artifactSha256=points[0].artifact.artifactSha256;
        for (const auto &receiver:fixture.options.stopped.receivers) {
            const auto sealPath=receiver.outputDirectory+"/.getdaq-receiver-seal.json";
            auto seal=ReadFile(sealPath,65536); seal=ReplaceAll(seal,std::string(64,'a'),fixture.options.receiver.configurationSha256);
            seal=ReplaceAll(seal,std::string(64,'b'),fixture.options.receiver.artifactSha256);
            Require(!::unlink(sealPath.c_str()),"Replace threshold CLI fixture seal"); WriteNew(sealPath,seal);
        }
        fixture.options.stopped.planSha256=ConfigurationSha256(EncodeOptions(fixture.options.receiver,fixture.options.executableSha256));
        ReceiverCatalog catalog(fixture.options); catalog.Execute();
        const auto scanPath=fixture.root+"/threshold-scan", bindingPath=fixture.root+"/threshold-bindings";
        CreateDirectory(scanPath); CreateDirectory(bindingPath);
        const auto artifact=EncodeSystemExperimentArtifact(points[0].artifact);
        WriteNew(scanPath+"/point-000.json",artifact);
        const auto manifest="{\"schema_version\":1,\"kind\":\"getdaq.aget-threshold-scan\",\"target_cobo\":\"C0\",\"target_asad\":0,\"target_aget\":0,\"start_code\":0,\"stop_code\":0,\"step_code\":1,\"hardware_loaded_verified\":false,\"requires_full_configure_per_point\":true,\"acquisition_event_count_unspecified\":true,\"points\":[{\"index\":0,\"threshold_code\":0,\"artifact\":\"point-000.json\",\"configuration_sha256\":"+
            EncodeJsonString(points[0].artifact.configurationSha256)+",\"artifact_sha256\":"+EncodeJsonString(points[0].artifact.artifactSha256)+"}]}\n";
        WriteNew(scanPath+"/scan-manifest.json",manifest);
        BindScanPointToCatalog(scanPath,0,fixture.target,bindingPath+"/point-000.json");
        const auto output=fixture.root+"/cli-analysis.json";
        const pid_t child=::fork(); Require(child>=0,"Fork threshold catalog CLI smoke test");
        if (!child) {
            if (loader.empty())
                ::execl(catalogProgram.c_str(),catalogProgram.c_str(),"--analyze-threshold-scan",scanPath.c_str(),bindingPath.c_str(),"0",output.c_str(),static_cast<char *>(nullptr));
            else
                ::execl(loader.c_str(),loader.c_str(),"--library-path",libraryPath.c_str(),catalogProgram.c_str(),"--analyze-threshold-scan",scanPath.c_str(),bindingPath.c_str(),"0",output.c_str(),static_cast<char *>(nullptr));
            ::_exit(127);
        }
        int status=0; Require(::waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0,
            "Built getdaq-catalog threshold analysis CLI failed");
        const auto report=ParseStrictJson(ReadFile(output,1024*1024));
        const auto &point=report.At("points").items.at(0);
        Require(String(report.At("kind"))=="getdaq.threshold-hit-pattern-analysis" && Boolean(report.At("mapping_verified")) &&
            !Boolean(report.At("hardware_loaded_verified")) && Unsigned(point.At("matching_frames"),true)==2 &&
            Unsigned(point.At("channel_frames").items.at(4),true)==2,"Built catalog CLI report differs from synthetic MFM input");
        const pid_t repeat=::fork(); Require(repeat>=0,"Fork threshold catalog no-overwrite check");
        if (!repeat) {
            if (loader.empty())
                ::execl(catalogProgram.c_str(),catalogProgram.c_str(),"--analyze-threshold-scan",scanPath.c_str(),bindingPath.c_str(),"0",output.c_str(),static_cast<char *>(nullptr));
            else
                ::execl(loader.c_str(),loader.c_str(),"--library-path",libraryPath.c_str(),catalogProgram.c_str(),"--analyze-threshold-scan",scanPath.c_str(),bindingPath.c_str(),"0",output.c_str(),static_cast<char *>(nullptr));
            ::_exit(127);
        }
        int repeatStatus=0; Require(::waitpid(repeat,&repeatStatus,0)==repeat && WIFEXITED(repeatStatus) && WEXITSTATUS(repeatStatus)==2,
            "Built catalog CLI overwrote an existing threshold report");
    }
    Require(::access(ownedRoot.c_str(),F_OK)!=0,"Threshold CLI smoke test left its owned fixture root");
    std::cout<<"PASS threshold catalog command-line aggregation; synthetic_raw_bytes=320; owned raw cleaned\n";
}
}
int main(int argc,char **argv) {
    try {
        const std::string catalogProgram=argc==2 ? argv[1] : std::string();
        if (argc==3 && std::string(argv[1])=="--threshold-cli-smoke") {
            ThresholdCatalogCliSmoke(argv[2]); return 0;
        }
        if (argc==5 && std::string(argv[1])=="--threshold-cli-smoke-with-loader") {
            ThresholdCatalogCliSmoke(argv[4],argv[2],argv[3]); return 0;
        }
        if (argc>2) throw std::invalid_argument("Unknown receiver-catalog test argument");
        {
            Fixture fixture;auto &source=fixture.options.stopped.receivers[0];
            auto seal=ReadFile(source.outputDirectory+"/.getdaq-receiver-seal.json",65536);
            const auto ending=seal.rfind("]}");Require(ending!=std::string::npos,"Find synthetic seal file array");std::string extra;
            for(unsigned int i=0;i<4097;++i) {
                const auto name="empty-"+std::to_string(i)+".graw";Fd file(::open((source.outputDirectory+'/'+name).c_str(),O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC,0600));Require(file.value>=0,"Create bounded empty file-count fixture");
                extra+=",{\"name\":"+EncodeJsonString(name)+",\"bytes\":\"0\"}";
            }
            seal.insert(ending,extra);Require(!::unlink((source.outputDirectory+"/.getdaq-receiver-seal.json").c_str()),"Replace owned seal fixture");WriteNew(source.outputDirectory+"/.getdaq-receiver-seal.json",seal);source.files+=4097;
            ReceiverCatalog catalog(fixture.options);const auto receipt=catalog.Execute();Require(InspectReceiverCatalog(fixture.target)==receipt,"Declared 65536-file catalog budget was truncated by default JSON array limit");
            std::cout<<"PASS catalog JSON capacity above 4096 entries; bounded empty owned files\n";
        }
        unsigned int checks=0;
        InterruptedReplacementRecovery(false); ++checks;
        InterruptedReplacementRecovery(true); ++checks;
        {
            Fixture fixture; const auto bytes=EncodeReceiverCatalogRequest(fixture.options); const auto decoded=DecodeReceiverCatalogRequest(bytes);
            Require(ReceiverCatalogRequestKey(decoded)==ReceiverCatalogRequestKey(fixture.options),"Canonical catalog key differs");
            auto legacy=fixture.options.receiver; legacy.catalogRoot.clear(); const auto old=EncodeOptions(legacy,fixture.options.executableSha256);
            Require(old.find("catalog_root")==std::string::npos && DecodeSupervisorPlan(old).options.catalogRoot.empty(),"Disabled catalog altered old plan contract");
            Require(DecodeSupervisorPlan(EncodeOptions(fixture.options.receiver,fixture.options.executableSha256)).options.catalogRoot==fixture.options.receiver.catalogRoot,"Catalog root missing from immutable plan"); checks+=3;
            auto extra=bytes; extra.insert(extra.size()-2,",\"extra\":true"); Reject([&] { DecodeReceiverCatalogRequest(extra); }); ++checks;
        }
        {
            Fixture fixture; const auto baseline=ScanArtifact(); const auto points=BuildPeakingTimeScan(baseline,"C0");
            fixture.options.receiver.configurationSha256=points[3].artifact.configurationSha256;
            fixture.options.receiver.artifactSha256=points[3].artifact.artifactSha256;
            for (const auto &receiver:fixture.options.stopped.receivers) {
                const auto sealPath=receiver.outputDirectory+"/.getdaq-receiver-seal.json";
                auto seal=ReadFile(sealPath,65536); seal=ReplaceAll(seal,std::string(64,'a'),fixture.options.receiver.configurationSha256);
                seal=ReplaceAll(seal,std::string(64,'b'),fixture.options.receiver.artifactSha256);
                Require(!::unlink(sealPath.c_str()),"Replace binding fixture seal"); WriteNew(sealPath,seal);
            }
            fixture.options.stopped.planSha256=ConfigurationSha256(EncodeOptions(fixture.options.receiver,fixture.options.executableSha256));
            ReceiverCatalog catalog(fixture.options); catalog.Execute();
            const auto scanPath=fixture.root+"/scan"; CreateDirectory(scanPath);
            std::string entries="[";
            for (std::size_t i=0;i<points.size();++i) {
                if (i) entries+=',';
                const auto name="point-"+(i<10 ? std::string("0") : std::string())+std::to_string(i)+".json";
                const auto bytes=EncodeSystemExperimentArtifact(points[i].artifact); WriteNew(scanPath+'/'+name,bytes);
                entries+="{\"index\":"+std::to_string(i)+",\"artifact\":"+EncodeJsonString(name)+
                    ",\"configuration_sha256\":"+EncodeJsonString(points[i].artifact.configurationSha256)+
                    ",\"artifact_sha256\":"+EncodeJsonString(points[i].artifact.artifactSha256)+"}";
            }
            entries+="]";
            const auto scanManifest="{\"schema_version\":1,\"kind\":\"getdaq.peaking-time-scan\",\"target_cobo\":\"C0\",\"hardware_loaded_verified\":false,\"requires_full_configure_per_point\":true,\"acquisition_event_count_unspecified\":true,\"points\":"+entries+"}\n";
            WriteNew(scanPath+"/scan-manifest.json",scanManifest);
            const auto archiveReceipt=InspectReceiverCatalog(fixture.target);
            const auto bindingPath=fixture.root+"/scan-run-binding.json";
            const auto binding=ParseStrictJson(BindScanPointToCatalog(scanPath,3,fixture.target,bindingPath));
            Require(String(binding.At("kind"))=="getdaq.scan-run-binding" && Unsigned(binding.At("point_index"))==3 &&
                String(binding.At("experiment"))=="Test" && Unsigned(binding.At("run_number"))==0 &&
                String(binding.At("session_id"))==fixture.options.stopped.sessionId && !Boolean(binding.At("hardware_loaded_verified")) &&
                InspectReceiverCatalog(fixture.target)==archiveReceipt,"Scan point association differs or modifies the sealed catalog"); checks+=2;
            Reject([&] { BindScanPointToCatalog(scanPath,0,fixture.target,fixture.root+"/wrong-binding.json"); }); ++checks;
            Reject([&] { BindScanPointToCatalog(scanPath,3,fixture.target,bindingPath); }); ++checks;
            Reject([&] { BindScanPointToCatalog(scanPath,3,fixture.target,"/getdaq-scan-binding-root-output.json"); }); ++checks;

            const auto onePoint=fixture.root+"/one-point-scan", bindingDirectory=fixture.root+"/scan-bindings";
            CreateDirectory(onePoint); CreateDirectory(bindingDirectory);
            const auto pointBytes=EncodeSystemExperimentArtifact(points[3].artifact);
            WriteNew(onePoint+"/point-00.json",pointBytes);
            const auto oneManifest="{\"schema_version\":1,\"kind\":\"getdaq.peaking-time-scan\",\"target_cobo\":\"C0\",\"hardware_loaded_verified\":false,\"requires_full_configure_per_point\":true,\"acquisition_event_count_unspecified\":true,\"points\":[{\"index\":0,\"artifact\":\"point-00.json\",\"configuration_sha256\":"+
                EncodeJsonString(points[3].artifact.configurationSha256)+",\"artifact_sha256\":"+EncodeJsonString(points[3].artifact.artifactSha256)+"}]}\n";
            WriteNew(onePoint+"/scan-manifest.json",oneManifest);
            BindScanPointToCatalog(onePoint,0,fixture.target,bindingDirectory+"/point-000.json");
            const auto indexPath=fixture.root+"/scan-index.json";
            const auto index=ParseStrictJson(SummarizeScanBindings(onePoint,bindingDirectory,indexPath));
            Require(Boolean(index.At("complete")) && !Boolean(index.At("hardware_loaded_verified")) && index.At("points").items.size()==1 &&
                String(index.At("points").items[0].At("session_id"))==fixture.options.stopped.sessionId && InspectReceiverCatalog(fixture.target)==archiveReceipt,
                "Verified scan index differs or modifies immutable archive"); checks+=2;
            Reject([&] { SummarizeScanBindings(onePoint,bindingDirectory,indexPath); }); ++checks;
            auto stale=ReadFile(bindingDirectory+"/point-000.json",65536); stale=ReplaceAll(stale,"\"run_number\":0","\"run_number\":1");
            Require(!::unlink((bindingDirectory+"/point-000.json").c_str()),"Remove scan binding fixture"); WriteNew(bindingDirectory+"/point-000.json",stale);
            Reject([&] { SummarizeScanBindings(onePoint,bindingDirectory,fixture.root+"/stale-index.json"); }); ++checks;
        }
        {
            Fixture fixture(80,false,true);
            const auto points=BuildAutoGainScan(ScanArtifact(),"C0",0,0,0,0,-1000,-1000,1);
            fixture.options.receiver.configurationSha256=points[0].artifact.configurationSha256;
            fixture.options.receiver.artifactSha256=points[0].artifact.artifactSha256;
            for (const auto &receiver:fixture.options.stopped.receivers) {
                const auto sealPath=receiver.outputDirectory+"/.getdaq-receiver-seal.json";
                auto seal=ReadFile(sealPath,65536); seal=ReplaceAll(seal,std::string(64,'a'),fixture.options.receiver.configurationSha256);
                seal=ReplaceAll(seal,std::string(64,'b'),fixture.options.receiver.artifactSha256);
                Require(!::unlink(sealPath.c_str()),"Replace AutoGain binding fixture seal"); WriteNew(sealPath,seal);
            }
            fixture.options.stopped.planSha256=ConfigurationSha256(EncodeOptions(fixture.options.receiver,fixture.options.executableSha256));
            ReceiverCatalog catalog(fixture.options); catalog.Execute();
            const auto scanPath=fixture.root+"/autogain-scan", bindingPath=fixture.root+"/autogain-bindings";
            CreateDirectory(scanPath); CreateDirectory(bindingPath);
            const auto artifact=EncodeSystemExperimentArtifact(points[0].artifact);
            WriteNew(scanPath+"/point-000.json",artifact);
            const auto manifest="{\"schema_version\":1,\"kind\":\"getdaq.auto-gain-scan\",\"target_cobo\":\"C0\",\"target_asad\":0,\"target_aget\":0,\"first_channel\":0,\"last_channel\":0,\"start_mv\":-1000,\"stop_mv\":-1000,\"step_mv\":1,\"legacy_channel_mask_mapping_verified_from_source\":true,\"hardware_loaded_verified\":false,\"requires_full_configure_per_point\":true,\"acquisition_event_count_unspecified\":true,\"gain_result_interpretation_verified\":false,\"points\":[{\"index\":0,\"channel_id\":0,\"voltage_mv\":-1000,\"artifact\":\"point-000.json\",\"configuration_sha256\":"+
                EncodeJsonString(points[0].artifact.configurationSha256)+",\"artifact_sha256\":"+EncodeJsonString(points[0].artifact.artifactSha256)+"}]}\n";
            WriteNew(scanPath+"/scan-manifest.json",manifest);
            const auto archiveReceipt=InspectReceiverCatalog(fixture.target);
            const auto binding=ParseStrictJson(BindScanPointToCatalog(scanPath,0,fixture.target,bindingPath+"/point-000.json"));
            Require(String(binding.At("kind"))=="getdaq.scan-run-binding" &&
                String(binding.At("scan_kind"))=="getdaq.auto-gain-scan" && Unsigned(binding.At("point_index"))==0 &&
                String(binding.At("experiment"))=="Test" && Unsigned(binding.At("run_number"))==0 &&
                !Boolean(binding.At("hardware_loaded_verified")) && InspectReceiverCatalog(fixture.target)==archiveReceipt,
                "AutoGain scan point association differs or modifies the sealed catalog");
            const auto index=ParseStrictJson(SummarizeScanBindings(scanPath,bindingPath,fixture.root+"/autogain-index.json"));
            Require(Boolean(index.At("complete")) && String(index.At("scan_kind"))=="getdaq.auto-gain-scan" &&
                index.At("points").items.size()==1 && !Boolean(index.At("hardware_loaded_verified")) &&
                InspectReceiverCatalog(fixture.target)==archiveReceipt,
                "AutoGain scan summary differs or modifies the sealed catalog");
            const auto response=ParseStrictJson(AnalyzeBoundAutoGainWaveforms(scanPath,bindingPath,0,fixture.root+"/autogain-waveform-response.json"));
            const auto &result=response.At("points").items.at(0);
            Require(String(response.At("kind"))=="getdaq.autogain-waveform-response" && Boolean(response.At("mapping_verified")) &&
                !Boolean(response.At("physical_gain_interpreted")) && Unsigned(result.At("frames_seen"))==2 &&
                Unsigned(result.At("channel_frames"))==2 && Unsigned(result.At("valid_waveforms"))==2 &&
                result.At("mean_peak_adc").text=="100" && result.At("mean_peaking_buckets").text=="2" &&
                result.At("mean_fwhm_buckets").text=="3" && InspectReceiverCatalog(fixture.target)==archiveReceipt,
                "Bound AutoGain waveform metrics differ or modify immutable archive"); checks+=2;
            auto wrongChannel=manifest; wrongChannel=ReplaceAll(wrongChannel,"\"channel_id\":0","\"channel_id\":1");
            Require(!::unlink((scanPath+"/scan-manifest.json").c_str()),"Remove AutoGain manifest before mismatch test");
            WriteNew(scanPath+"/scan-manifest.json",wrongChannel);
            Reject([&] { AnalyzeBoundAutoGainWaveforms(scanPath,bindingPath,0,fixture.root+"/wrong-autogain-analysis.json"); }); ++checks;
            const auto inconsistentScan=fixture.root+"/inconsistent-autogain-scan";
            const auto inconsistentBindings=fixture.root+"/inconsistent-autogain-bindings";
            CreateDirectory(inconsistentScan); CreateDirectory(inconsistentBindings);
            WriteNew(inconsistentScan+"/point-000.json",artifact);
            auto inconsistentManifest=ReplaceAll(manifest,"\"voltage_mv\":-1000","\"voltage_mv\":-999");
            WriteNew(inconsistentScan+"/scan-manifest.json",inconsistentManifest);
            BindScanPointToCatalog(inconsistentScan,0,fixture.target,inconsistentBindings+"/point-000.json");
            const auto inconsistentReport=fixture.root+"/inconsistent-autogain-report.json";
            Reject([&] { AnalyzeBoundAutoGainWaveforms(inconsistentScan,inconsistentBindings,0,inconsistentReport); });
            struct stat absentReport;
            Require(::lstat(inconsistentReport.c_str(),&absentReport) && errno==ENOENT,
                "Inconsistent AutoGain point must not publish a waveform report"); ++checks;
            std::cout<<"PASS AutoGain bound waveform metrics; synthetic_raw_bytes=208; owned raw cleaned\n";
        }
        {
            Fixture fixture(80,false,false,true);
            const auto points=BuildAutoGainScan(ScanArtifact(),"C0",0,0,0,0,-1000,-1000,1);
            fixture.options.receiver.configurationSha256=points[0].artifact.configurationSha256;
            fixture.options.receiver.artifactSha256=points[0].artifact.artifactSha256;
            for (const auto &receiver:fixture.options.stopped.receivers) {
                const auto sealPath=receiver.outputDirectory+"/.getdaq-receiver-seal.json";
                auto seal=ReadFile(sealPath,65536); seal=ReplaceAll(seal,std::string(64,'a'),fixture.options.receiver.configurationSha256);
                seal=ReplaceAll(seal,std::string(64,'b'),fixture.options.receiver.artifactSha256);
                Require(!::unlink(sealPath.c_str()),"Replace compact AutoGain fixture seal"); WriteNew(sealPath,seal);
            }
            fixture.options.stopped.planSha256=ConfigurationSha256(EncodeOptions(fixture.options.receiver,fixture.options.executableSha256));
            ReceiverCatalog catalog(fixture.options); catalog.Execute();
            const auto scanPath=fixture.root+"/compact-autogain-scan", bindingPath=fixture.root+"/compact-autogain-bindings";
            CreateDirectory(scanPath); CreateDirectory(bindingPath);
            const auto artifact=EncodeSystemExperimentArtifact(points[0].artifact); WriteNew(scanPath+"/point-000.json",artifact);
            const auto manifest="{\"schema_version\":1,\"kind\":\"getdaq.auto-gain-scan\",\"target_cobo\":\"C0\",\"target_asad\":0,\"target_aget\":0,\"first_channel\":0,\"last_channel\":0,\"start_mv\":-1000,\"stop_mv\":-1000,\"step_mv\":1,\"legacy_channel_mask_mapping_verified_from_source\":true,\"hardware_loaded_verified\":false,\"requires_full_configure_per_point\":true,\"acquisition_event_count_unspecified\":true,\"gain_result_interpretation_verified\":false,\"points\":[{\"index\":0,\"channel_id\":0,\"voltage_mv\":-1000,\"artifact\":\"point-000.json\",\"configuration_sha256\":"+
                EncodeJsonString(points[0].artifact.configurationSha256)+",\"artifact_sha256\":"+EncodeJsonString(points[0].artifact.artifactSha256)+"}]}\n";
            WriteNew(scanPath+"/scan-manifest.json",manifest);
            BindScanPointToCatalog(scanPath,0,fixture.target,bindingPath+"/point-000.json");
            const auto response=ParseStrictJson(AnalyzeBoundAutoGainWaveforms(scanPath,bindingPath,0,fixture.root+"/compact-autogain-response.json"));
            const auto &result=response.At("points").items.at(0);
            Require(Unsigned(result.At("frames_seen"))==2 && Unsigned(result.At("channel_frames"))==2 &&
                Unsigned(result.At("valid_waveforms"))==2 && result.At("mean_peak_adc").text=="100" &&
                result.At("mean_peaking_buckets").text=="2" && result.At("mean_fwhm_buckets").text=="3",
                "Compact CoBo type-2 bound waveform analysis differs");
            std::cout<<"PASS compact type-2 AutoGain waveform metrics; synthetic_raw_bytes=2228336; owned raw cleaned\n";
            ++checks;
        }
        {
            Fixture fixture(80,false,true);
            const auto points=BuildPulserAmplitudeScan(ScanArtifact(),"C0",-1000,-1000,1);
            fixture.options.receiver.configurationSha256=points[0].artifact.configurationSha256;
            fixture.options.receiver.artifactSha256=points[0].artifact.artifactSha256;
            for (const auto &receiver:fixture.options.stopped.receivers) {
                const auto sealPath=receiver.outputDirectory+"/.getdaq-receiver-seal.json";
                auto seal=ReadFile(sealPath,65536); seal=ReplaceAll(seal,std::string(64,'a'),fixture.options.receiver.configurationSha256);
                seal=ReplaceAll(seal,std::string(64,'b'),fixture.options.receiver.artifactSha256);
                Require(!::unlink(sealPath.c_str()),"Replace Pulser binding fixture seal"); WriteNew(sealPath,seal);
            }
            fixture.options.stopped.planSha256=ConfigurationSha256(EncodeOptions(fixture.options.receiver,fixture.options.executableSha256));
            ReceiverCatalog catalog(fixture.options); catalog.Execute();
            const auto scanPath=fixture.root+"/pulser-scan",bindingPath=fixture.root+"/pulser-bindings";
            CreateDirectory(scanPath); CreateDirectory(bindingPath);
            const auto artifact=EncodeSystemExperimentArtifact(points[0].artifact); WriteNew(scanPath+"/point-000.json",artifact);
            const auto manifest="{\"schema_version\":1,\"kind\":\"getdaq.pulser-amplitude-scan\",\"target_cobo\":\"C0\",\"start_mv\":-1000,\"stop_mv\":-1000,\"step_mv\":1,\"hardware_loaded_verified\":false,\"requires_full_configure_per_point\":true,\"acquisition_event_count_unspecified\":true,\"points\":[{\"index\":0,\"voltage_mv\":-1000,\"artifact\":\"point-000.json\",\"configuration_sha256\":"+
                EncodeJsonString(points[0].artifact.configurationSha256)+",\"artifact_sha256\":"+EncodeJsonString(points[0].artifact.artifactSha256)+"}]}\n";
            WriteNew(scanPath+"/scan-manifest.json",manifest);
            BindScanPointToCatalog(scanPath,0,fixture.target,bindingPath+"/point-000.json");
            const auto response=ParseStrictJson(AnalyzeBoundPulserWaveforms(scanPath,bindingPath,0,0,0,0,fixture.root+"/pulser-response.json"));
            const auto &result=response.At("points").items.at(0);
            Require(String(response.At("kind"))=="getdaq.pulser-waveform-response" &&
                String(response.At("scan_kind"))=="getdaq.pulser-amplitude-scan" && Boolean(response.At("mapping_verified")) &&
                !Boolean(response.At("hardware_loaded_verified")) && !Boolean(response.At("physical_gain_interpreted")) &&
                Unsigned(result.At("frames_seen"))==2 && Unsigned(result.At("channel_frames"))==2 &&
                Unsigned(result.At("valid_waveforms"))==2 && result.At("voltage_mv").text=="-1000" &&
                result.At("mean_peak_adc").text=="100" && result.At("mean_peaking_buckets").text=="2" &&
                result.At("mean_fwhm_buckets").text=="3",
                "Bound Pulser waveform response differs or implies a physical gain judgment"); ++checks;
            Reject([&] { AnalyzeBoundPulserWaveforms(scanPath,bindingPath,0,0,0,68,fixture.root+"/invalid-channel.json"); }); ++checks;
            const auto inconsistentScan=fixture.root+"/inconsistent-pulser-scan";
            const auto inconsistentBindings=fixture.root+"/inconsistent-pulser-bindings";
            CreateDirectory(inconsistentScan); CreateDirectory(inconsistentBindings);
            WriteNew(inconsistentScan+"/point-000.json",artifact);
            auto inconsistentManifest=manifest;
            inconsistentManifest=ReplaceAll(inconsistentManifest,"\"stop_mv\":-1000","\"stop_mv\":-900");
            WriteNew(inconsistentScan+"/scan-manifest.json",inconsistentManifest);
            BindScanPointToCatalog(inconsistentScan,0,fixture.target,inconsistentBindings+"/point-000.json");
            const auto inconsistentReport=fixture.root+"/inconsistent-pulser-report.json";
            Reject([&] { AnalyzeBoundPulserWaveforms(inconsistentScan,inconsistentBindings,0,0,0,0,inconsistentReport); });
            struct stat absentReport;
            Require(::lstat(inconsistentReport.c_str(),&absentReport) && errno==ENOENT,
                "Inconsistent Pulser plan must not publish a waveform report"); ++checks;
            std::cout<<"PASS Pulser Run-bound waveform response; synthetic_raw_bytes=208; owned raw cleaned\n";
        }
        {
            Fixture fixture(80,false,true);
            const auto points=BuildPeakingTimeScan(ScanArtifact(),"C0");
            fixture.options.receiver.configurationSha256=points[0].artifact.configurationSha256;
            fixture.options.receiver.artifactSha256=points[0].artifact.artifactSha256;
            for (const auto &receiver:fixture.options.stopped.receivers) {
                const auto sealPath=receiver.outputDirectory+"/.getdaq-receiver-seal.json";
                auto seal=ReadFile(sealPath,65536); seal=ReplaceAll(seal,std::string(64,'a'),fixture.options.receiver.configurationSha256);
                seal=ReplaceAll(seal,std::string(64,'b'),fixture.options.receiver.artifactSha256);
                Require(!::unlink(sealPath.c_str()),"Replace PeakingTime binding fixture seal"); WriteNew(sealPath,seal);
            }
            fixture.options.stopped.planSha256=ConfigurationSha256(EncodeOptions(fixture.options.receiver,fixture.options.executableSha256));
            ReceiverCatalog catalog(fixture.options); catalog.Execute();
            const auto scanPath=fixture.root+"/peaking-scan",bindingPath=fixture.root+"/peaking-bindings";
            CreateDirectory(scanPath); CreateDirectory(bindingPath);
            const auto artifact=EncodeSystemExperimentArtifact(points[0].artifact); WriteNew(scanPath+"/point-00.json",artifact);
            const auto cobo=std::find_if(points[0].artifact.settings.cobos.begin(),points[0].artifact.settings.cobos.end(),[](const SystemExperimentCoBo &entry){return entry.id=="C0";});
            Require(cobo!=points[0].artifact.settings.cobos.end() && !cobo->configure.readout.aget.chips.empty(),"PeakingTime fixture target chip missing");
            const auto timing=cobo->configure.readout.aget.chips[0].settings.globals.at("Reg1/peackingTime");
            const auto manifest="{\"schema_version\":1,\"kind\":\"getdaq.peaking-time-scan\",\"target_cobo\":\"C0\",\"hardware_loaded_verified\":false,\"requires_full_configure_per_point\":true,\"acquisition_event_count_unspecified\":true,\"points\":[{\"index\":0,\"artifact\":\"point-00.json\",\"configuration_sha256\":"+
                EncodeJsonString(points[0].artifact.configurationSha256)+",\"artifact_sha256\":"+EncodeJsonString(points[0].artifact.artifactSha256)+"}]}\n";
            WriteNew(scanPath+"/scan-manifest.json",manifest);
            BindScanPointToCatalog(scanPath,0,fixture.target,bindingPath+"/point-000.json");
            const auto response=ParseStrictJson(AnalyzeBoundPeakingTimeWaveforms(scanPath,bindingPath,0,0,0,0,fixture.root+"/peaking-response.json"));
            const auto &result=response.At("points").items.at(0);
            Require(String(response.At("kind"))=="getdaq.peaking-time-waveform-response" &&
                String(response.At("scan_kind"))=="getdaq.peaking-time-scan" && Boolean(response.At("mapping_verified")) &&
                !Boolean(response.At("hardware_loaded_verified")) && result.At("peaking_time").text==timing &&
                Unsigned(result.At("valid_waveforms"))==2 && result.At("mean_peak_adc").text=="100" &&
                result.At("mean_peaking_buckets").text=="2" && result.At("mean_fwhm_buckets").text=="3",
                "Bound PeakingTime waveform response differs or implies hardware verification"); ++checks;
            Reject([&] { AnalyzeBoundPeakingTimeWaveforms(scanPath,bindingPath,0,0,0,68,fixture.root+"/invalid-peaking-channel.json"); }); ++checks;
            if (!catalogProgram.empty()) {
                const auto cliOutput=fixture.root+"/peaking-cli-response.json";
                const pid_t child=::fork(); Require(child>=0,"Fork PeakingTime catalog CLI test");
                if (!child) {
                    ::execl(catalogProgram.c_str(),catalogProgram.c_str(),"--analyze-peaking-scan",scanPath.c_str(),bindingPath.c_str(),
                        "0","0","0","0",cliOutput.c_str(),static_cast<char *>(nullptr));
                    ::_exit(127);
                }
                int status=0; Require(::waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0,
                    "getdaq-catalog PeakingTime scan CLI failed");
                const auto cliReportBytes=ReadFile(cliOutput,1024*1024);
                const auto cliReport=ParseStrictJson(cliReportBytes);
                Require(String(cliReport.At("kind"))=="getdaq.peaking-time-waveform-response" &&
                    cliReport.At("points").items.at(0).At("peaking_time").text==timing &&
                    cliReport.At("points").items.at(0).At("mean_peak_adc").text=="100",
                    "getdaq-catalog PeakingTime CLI report differs from bound artifact analysis"); ++checks;
                const pid_t repeat=::fork(); Require(repeat>=0,"Fork PeakingTime CLI no-overwrite test");
                if (!repeat) {
                    ::execl(catalogProgram.c_str(),catalogProgram.c_str(),"--analyze-peaking-scan",scanPath.c_str(),bindingPath.c_str(),
                        "0","0","0","0",cliOutput.c_str(),static_cast<char *>(nullptr));
                    ::_exit(127);
                }
                int repeatStatus=0; Require(::waitpid(repeat,&repeatStatus,0)==repeat && WIFEXITED(repeatStatus) && WEXITSTATUS(repeatStatus)==2 &&
                    ReadFile(cliOutput,1024*1024)==cliReportBytes,"PeakingTime CLI overwrote its existing report"); ++checks;
                const auto invalidOutput=fixture.root+"/peaking-cli-invalid-channel.json";
                const pid_t invalid=::fork(); Require(invalid>=0,"Fork PeakingTime CLI invalid-channel test");
                if (!invalid) {
                    ::execl(catalogProgram.c_str(),catalogProgram.c_str(),"--analyze-peaking-scan",scanPath.c_str(),bindingPath.c_str(),
                        "0","0","0","68",invalidOutput.c_str(),static_cast<char *>(nullptr));
                    ::_exit(127);
                }
                int invalidStatus=0; Require(::waitpid(invalid,&invalidStatus,0)==invalid && WIFEXITED(invalidStatus) &&
                    WEXITSTATUS(invalidStatus)==2 && !Exists(invalidOutput),"PeakingTime CLI accepted an invalid channel or published a report"); ++checks;
            }
            std::cout<<"PASS PeakingTime Run-bound waveform response; synthetic_raw_bytes=208; owned raw cleaned\n";
        }
        {
            Fixture fixture(80,false,false,false,true);
            const auto points=BuildAutoGainScan(ScanArtifact(),"C0",0,0,0,0,-1000,-1000,1);
            fixture.options.receiver.configurationSha256=points[0].artifact.configurationSha256;
            fixture.options.receiver.artifactSha256=points[0].artifact.artifactSha256;
            for (const auto &receiver:fixture.options.stopped.receivers) {
                const auto sealPath=receiver.outputDirectory+"/.getdaq-receiver-seal.json";
                auto seal=ReadFile(sealPath,65536); seal=ReplaceAll(seal,std::string(64,'a'),fixture.options.receiver.configurationSha256);
                seal=ReplaceAll(seal,std::string(64,'b'),fixture.options.receiver.artifactSha256);
                Require(!::unlink(sealPath.c_str()),"Replace layered AutoGain fixture seal"); WriteNew(sealPath,seal);
            }
            fixture.options.stopped.planSha256=ConfigurationSha256(EncodeOptions(fixture.options.receiver,fixture.options.executableSha256));
            ReceiverCatalog catalog(fixture.options); catalog.Execute();
            const auto scanPath=fixture.root+"/layered-autogain-scan", bindingPath=fixture.root+"/layered-autogain-bindings";
            CreateDirectory(scanPath); CreateDirectory(bindingPath);
            const auto artifact=EncodeSystemExperimentArtifact(points[0].artifact); WriteNew(scanPath+"/point-000.json",artifact);
            const auto manifest="{\"schema_version\":1,\"kind\":\"getdaq.auto-gain-scan\",\"target_cobo\":\"C0\",\"target_asad\":0,\"target_aget\":0,\"first_channel\":0,\"last_channel\":0,\"start_mv\":-1000,\"stop_mv\":-1000,\"step_mv\":1,\"legacy_channel_mask_mapping_verified_from_source\":true,\"hardware_loaded_verified\":false,\"requires_full_configure_per_point\":true,\"acquisition_event_count_unspecified\":true,\"gain_result_interpretation_verified\":false,\"points\":[{\"index\":0,\"channel_id\":0,\"voltage_mv\":-1000,\"artifact\":\"point-000.json\",\"configuration_sha256\":"+
                EncodeJsonString(points[0].artifact.configurationSha256)+",\"artifact_sha256\":"+EncodeJsonString(points[0].artifact.artifactSha256)+"}]}\n";
            WriteNew(scanPath+"/scan-manifest.json",manifest);
            BindScanPointToCatalog(scanPath,0,fixture.target,bindingPath+"/point-000.json");
            const auto response=ParseStrictJson(AnalyzeBoundAutoGainWaveforms(scanPath,bindingPath,0,fixture.root+"/layered-autogain-response.json"));
            const auto &result=response.At("points").items.at(0);
            Require(Unsigned(result.At("frames_seen"))==2 && Unsigned(result.At("channel_frames"))==2 &&
                Unsigned(result.At("valid_waveforms"))==2 && result.At("mean_peak_adc").text=="100" &&
                result.At("mean_peaking_buckets").text=="2" && result.At("mean_fwhm_buckets").text=="3",
                "Layered MFM bound waveform analysis differs");
            std::cout<<"PASS layered MFM AutoGain waveform metrics; synthetic_raw_bytes=272; owned raw cleaned\n";
            ++checks;
        }
        {
            Fixture fixture(80,true);
            const auto points=BuildThresholdScan(ScanArtifact(),"C0",0,0,0,0,1);
            fixture.options.receiver.configurationSha256=points[0].artifact.configurationSha256;
            fixture.options.receiver.artifactSha256=points[0].artifact.artifactSha256;
            for (const auto &receiver:fixture.options.stopped.receivers) {
                const auto sealPath=receiver.outputDirectory+"/.getdaq-receiver-seal.json";
                auto seal=ReadFile(sealPath,65536); seal=ReplaceAll(seal,std::string(64,'a'),fixture.options.receiver.configurationSha256);
                seal=ReplaceAll(seal,std::string(64,'b'),fixture.options.receiver.artifactSha256);
                Require(!::unlink(sealPath.c_str()),"Replace threshold fixture seal"); WriteNew(sealPath,seal);
            }
            fixture.options.stopped.planSha256=ConfigurationSha256(EncodeOptions(fixture.options.receiver,fixture.options.executableSha256));
            ReceiverCatalog catalog(fixture.options); catalog.Execute();
            const auto scanPath=fixture.root+"/threshold-scan", bindingPath=fixture.root+"/threshold-bindings";
            CreateDirectory(scanPath); CreateDirectory(bindingPath);
            const auto artifact=EncodeSystemExperimentArtifact(points[0].artifact);
            WriteNew(scanPath+"/point-000.json",artifact);
            const auto manifest="{\"schema_version\":1,\"kind\":\"getdaq.aget-threshold-scan\",\"target_cobo\":\"C0\",\"target_asad\":0,\"target_aget\":0,\"start_code\":0,\"stop_code\":0,\"step_code\":1,\"hardware_loaded_verified\":false,\"requires_full_configure_per_point\":true,\"acquisition_event_count_unspecified\":true,\"points\":[{\"index\":0,\"threshold_code\":0,\"artifact\":\"point-000.json\",\"configuration_sha256\":"+
                EncodeJsonString(points[0].artifact.configurationSha256)+",\"artifact_sha256\":"+EncodeJsonString(points[0].artifact.artifactSha256)+"}]}\n";
            WriteNew(scanPath+"/scan-manifest.json",manifest);
            BindScanPointToCatalog(scanPath,0,fixture.target,bindingPath+"/point-000.json");
            const auto report=ParseStrictJson(AnalyzeThresholdScan(scanPath,bindingPath,0,fixture.root+"/threshold-analysis.json"));
            const auto &result=report.At("points").items.at(0);
            Require(String(report.At("kind"))=="getdaq.threshold-hit-pattern-analysis" && Boolean(report.At("mapping_verified")) &&
                !Boolean(report.At("hardware_loaded_verified")) && Unsigned(result.At("threshold_code"))==0 &&
                Unsigned(result.At("matching_frames"),true)==2 && result.At("channel_frames").items.size()==68 &&
                Unsigned(result.At("channel_frames").items[4],true)==2 && InspectReceiverCatalog(fixture.target).size()>0,
                "Bound threshold scan did not produce expected streamed hit-pattern counts");
            Reject([&] { AnalyzeThresholdScan(scanPath,bindingPath,0,fixture.root+"/threshold-analysis.json"); });
            Reject([&] { AnalyzeThresholdScan(scanPath,bindingPath,1,fixture.root+"/wrong-coordinator.json"); });
            auto inconsistentRange=ReplaceAll(manifest,"\"stop_code\":0","\"stop_code\":8");
            Require(inconsistentRange!=manifest && !::unlink((scanPath+"/scan-manifest.json").c_str()),"Prepare inconsistent threshold range");
            WriteNew(scanPath+"/scan-manifest.json",inconsistentRange);
            Require(!::unlink((bindingPath+"/point-000.json").c_str()),"Remove old binding for inconsistent threshold range");
            BindScanPointToCatalog(scanPath,0,fixture.target,bindingPath+"/point-000.json");
            const auto inconsistentOutput=fixture.root+"/inconsistent-threshold-range.json";
            Reject([&] { AnalyzeThresholdScan(scanPath,bindingPath,0,inconsistentOutput); });
            Require(!Exists(inconsistentOutput),"Inconsistent threshold range published an analysis report");
            Require(!::unlink((scanPath+"/scan-manifest.json").c_str()),"Restore threshold range manifest");
            WriteNew(scanPath+"/scan-manifest.json",manifest);
            Require(!::unlink((bindingPath+"/point-000.json").c_str()),"Remove inconsistent-range binding");
            BindScanPointToCatalog(scanPath,0,fixture.target,bindingPath+"/point-000.json");
            auto mislabeled=manifest; mislabeled=ReplaceAll(mislabeled,"\"threshold_code\":0","\"threshold_code\":1");
            Require(mislabeled!=manifest && !::unlink((scanPath+"/scan-manifest.json").c_str()),"Prepare mismatched threshold-code manifest");
            WriteNew(scanPath+"/scan-manifest.json",mislabeled);
            Require(!::unlink((bindingPath+"/point-000.json").c_str()),"Remove old threshold binding for relabel test");
            BindScanPointToCatalog(scanPath,0,fixture.target,bindingPath+"/point-000.json");
            Reject([&] { AnalyzeThresholdScan(scanPath,bindingPath,0,fixture.root+"/mislabeled-threshold.json"); });
            Require(!Exists(fixture.root+"/mislabeled-threshold.json"),"Mismatched threshold label published an analysis report");
            Require(!::unlink((scanPath+"/scan-manifest.json").c_str()),"Restore threshold scan manifest"); WriteNew(scanPath+"/scan-manifest.json",manifest);
            Require(!::unlink((bindingPath+"/point-000.json").c_str()),"Remove relabeled scan binding");
            BindScanPointToCatalog(scanPath,0,fixture.target,bindingPath+"/point-000.json");
            Fd truncated(::open((fixture.target+"/source_0/data0.graw").c_str(),O_RDWR|O_CLOEXEC));
            Require(truncated.value>=0 && !::ftruncate(truncated.value,79),"Prepare truncated bound raw rejection");
            Reject([&] { AnalyzeThresholdScan(scanPath,bindingPath,0,fixture.root+"/truncated-catalog-analysis.json"); });
            Require(!Exists(fixture.root+"/truncated-catalog-analysis.json"),"Corrupted catalog published an analysis report");
            std::cout<<"PASS threshold scan binding to sealed catalogs and bounded streamed MFM hit-pattern aggregation\n";
            ++checks;
        }
        {
            Fixture fixture(512ULL*1024*1024); ReceiverCatalog catalog(fixture.options); catalog.Validate();
            Require(!Exists(fixture.target) && !Exists(fixture.options.stopped.receivers[0].outputDirectory+"/.getdaq-catalog-intent.json"),"Read-only validation created target/intent"); ++checks;
            const auto receipt=catalog.Execute(); Require(InspectReceiverCatalog(fixture.target)==receipt,"Catalog independent inspection differs"); ++checks;
            for (unsigned int i=0;i<2;++i) for (unsigned int j=0;j<2;++j) {
                struct stat source,target;
                Require(!::lstat((fixture.options.stopped.receivers[i].outputDirectory+"/data"+std::to_string(j)+".graw").c_str(),&source) &&
                    !::lstat((fixture.target+"/source_"+std::to_string(i)+"/data"+std::to_string(j)+".graw").c_str(),&target) && Same(source,target) &&
                    source.st_nlink==2 && source.st_size==target.st_size && source.st_blocks==target.st_blocks,"Catalog copied/moved raw payload or lost original inode"); ++checks;
            }
            Reject([&] { catalog.Execute(); }); ++checks;
            ReceiverCatalog duplicate(fixture.options); Reject([&] { duplicate.Execute(); }); ++checks;
            Fd changed(::open((fixture.target+"/source_0/data0.graw").c_str(),O_RDWR|O_CLOEXEC)); Require(changed.value>=0 && !::ftruncate(changed.value,1),"Mutate archived fixture");
            Reject([&] { InspectReceiverCatalog(fixture.target); }); ++checks;
        }
        {
            Fixture oldFixture;
            const auto &oldSource=oldFixture.options.stopped.receivers[0];
            const auto oldPath=oldSource.outputDirectory+"/data1.graw";
            const auto retainedPath=oldSource.outputDirectory+"/legacy-only.graw";
            Require(!::rename(oldPath.c_str(),retainedPath.c_str()),"Rename owned old-only raw fixture");
            const auto sealPath=oldSource.outputDirectory+"/.getdaq-receiver-seal.json";
            auto seal=ReadFile(sealPath,65536);seal=ReplaceAll(seal,"data1.graw","legacy-only.graw");
            Require(!::unlink(sealPath.c_str()),"Replace owned old catalog seal fixture");WriteNew(sealPath,seal);
            {ReceiverCatalog oldCatalog(oldFixture.options);oldCatalog.Execute();}

            Fixture newFixture(8192,false,false,false,false,'e');
            newFixture.options.operationId="catalog-new";
            newFixture.options.receiver.sources.pop_back();
            newFixture.options.stopped.receivers.pop_back();
            newFixture.options.receiver.catalogRoot=newFixture.root+"/catalog-new";CreateDirectory(newFixture.options.receiver.catalogRoot);
            newFixture.options.stopped.planSha256=ConfigurationSha256(EncodeOptions(newFixture.options.receiver,newFixture.options.executableSha256));
            newFixture.target=newFixture.options.receiver.catalogRoot+"/Test/run_0000/node-0";
            {ReceiverCatalog newCatalog(newFixture.options);newCatalog.Execute();}
            const auto plan=PlanReceiverCatalogReplacement(oldFixture.target,newFixture.target);
            Require(plan.size()==5,"Verified catalog merge plan has the wrong raw count");
            std::map<std::string,std::map<std::string,bool>> selected;
            for(const auto &entry:plan) Require(selected[entry.sourceId].insert(std::make_pair(entry.fileName,entry.fromNewRun)).second,
                "Verified catalog merge plan repeats a source filename");
            Require(selected["C0"]["data0.graw"] && selected["C0"]["data1.graw"] &&
                !selected["C0"]["legacy-only.graw"] && !selected["C1"]["data0.graw"] && !selected["C1"]["data1.graw"],
                "Verified catalog merge plan did not prefer new same-name raw and retain the old noncollision");
            const auto oldReceipt=InspectReceiverCatalog(oldFixture.target),newReceipt=InspectReceiverCatalog(newFixture.target);
            for (const auto &entry:plan) {
                const auto &catalog=entry.fromNewRun ? newFixture.target : oldFixture.target;
                const auto &receipt=entry.fromNewRun ? newReceipt : oldReceipt;
                const auto expectedStored="source_"+entry.sourceId.substr(1);
                struct stat raw;
                Require(entry.sourceCatalogDirectory==catalog && entry.sourceStoredDirectory==expectedStored &&
                    entry.sourceCatalogReceiptSha256==ConfigurationSha256(receipt) &&
                    !::lstat((catalog+'/'+entry.sourceStoredDirectory+'/'+entry.fileName).c_str(),&raw) && raw.st_size>=0 &&
                    entry.bytes==std::uint64_t(raw.st_size) && entry.device==std::uint64_t(raw.st_dev) && entry.inode==std::uint64_t(raw.st_ino),
                    "Verified replacement plan lost the chosen catalog file identity/provenance");
            }
            const auto candidate=oldFixture.root+"/replacement-candidate";
            const auto candidateReceipt=PrepareReceiverCatalogReplacement(oldFixture.target,newFixture.target,candidate);
            Require(InspectReceiverCatalog(candidate)==candidateReceipt,"Prepared merged catalog candidate failed independent inspection");
            const auto preparedObservation=ParseStrictJson(InspectReceiverCatalogReplacement(oldFixture.target,newFixture.target,candidate,newFixture.options.operationId));
            Require(String(preparedObservation.At("phase"))=="prepared" && !Boolean(preparedObservation.At("recovery_authorized")) &&
                String(preparedObservation.At("raw_payload_bytes_read"))=="0" && InspectReceiverCatalog(oldFixture.target)==oldReceipt,
                "Read-only replacement inspection did not classify the prepared state or changed the old catalog");
            const auto candidatePlan=PlanReceiverCatalogReplacement(oldFixture.target,candidate);
            Require(candidatePlan.size()==5,"A verified replacement candidate did not expose its full logical file set");
            const auto chainedCandidate=oldFixture.root+"/replacement-candidate-chained";
            const auto chainedReceipt=PrepareReceiverCatalogReplacement(oldFixture.target,candidate,chainedCandidate);
            const auto chainedManifest=ParseStrictJson(ReadFile(chainedCandidate+"/manifest.json",16*1024*1024));
            Require(InspectReceiverCatalog(chainedCandidate)==chainedReceipt &&
                chainedManifest.At("sources").items.size()==2 &&
                chainedManifest.At("sources").items[0].At("files").items.size()==2 &&
                chainedManifest.At("sources").items[0].At("retained_files").items.size()==1 &&
                chainedManifest.At("sources").items[1].At("files").items.empty() &&
                chainedManifest.At("sources").items[1].At("retained_files").items.size()==2,
                "Chained replacement confused current-session files with schema-2 retained files"); ++checks;
            struct stat selectedNew,selectedOld;
            Require(!::lstat((candidate+"/source_0/data0.graw").c_str(),&selectedNew) &&
                !::lstat((newFixture.target+"/source_0/data0.graw").c_str(),&selectedOld) &&
                selectedNew.st_dev==selectedOld.st_dev && selectedNew.st_ino==selectedOld.st_ino &&
                !::lstat((candidate+"/source_0/legacy-only.graw").c_str(),&selectedNew) &&
                !::lstat((oldFixture.target+"/source_0/legacy-only.graw").c_str(),&selectedOld) &&
                selectedNew.st_dev==selectedOld.st_dev && selectedNew.st_ino==selectedOld.st_ino &&
                !::lstat((candidate+"/source_1/data1.graw").c_str(),&selectedNew) &&
                !::lstat((oldFixture.target+"/source_1/data1.graw").c_str(),&selectedOld) &&
                selectedNew.st_dev==selectedOld.st_dev && selectedNew.st_ino==selectedOld.st_ino,
                "Prepared candidate did not hardlink the new collision winner and retained old-only raw");
            Reject([&]{PrepareReceiverCatalogReplacement(oldFixture.target,newFixture.target,candidate);});
            Reject([&]{PlanReceiverCatalogReplacement(oldFixture.target,oldFixture.target);});
            Fixture wrongFixture(8192,false,false,false,false,'f');
            wrongFixture.options.runNumber=1;wrongFixture.options.operationId="catalog-wrong-run";
            wrongFixture.options.receiver.catalogRoot=wrongFixture.root+"/catalog-wrong-run";CreateDirectory(wrongFixture.options.receiver.catalogRoot);
            wrongFixture.options.stopped.planSha256=ConfigurationSha256(EncodeOptions(wrongFixture.options.receiver,wrongFixture.options.executableSha256));
            wrongFixture.target=wrongFixture.options.receiver.catalogRoot+"/Test/run_0001/node-0";
            {ReceiverCatalog wrongRunCatalog(wrongFixture.options);wrongRunCatalog.Execute();}
            Reject([&]{PlanReceiverCatalogReplacement(oldFixture.target,wrongFixture.target);});
            Require(InspectReceiverCatalog(oldFixture.target).size()>0 && InspectReceiverCatalog(newFixture.target).size()>0 &&
                InspectReceiverCatalog(candidate)==candidateReceipt,
                "Read-only replacement planning modified a complete catalog");checks+=5;
            if (!catalogProgram.empty()) {
                const auto cliCandidate=oldFixture.root+"/replacement-candidate-cli";
                const pid_t child=::fork(); Require(child>=0,"Fork replacement candidate CLI");
                if (!child) { ::execl(catalogProgram.c_str(),catalogProgram.c_str(),"--prepare-catalog-replacement",
                    oldFixture.target.c_str(),newFixture.target.c_str(),cliCandidate.c_str(),static_cast<char *>(nullptr)); ::_exit(127); }
                int status=0; Require(::waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0 &&
                    !InspectReceiverCatalog(cliCandidate).empty(),"getdaq-catalog replacement candidate command failed"); ++checks;
            }
            const auto candidateManifest=ParseStrictJson(ReadFile(candidate+"/manifest.json",16*1024*1024));
            const auto backup=String(candidateManifest.At("publication_backup"));
            Require(String(ParseStrictJson(ReadFile(candidate+"/catalog-complete.json",65536)).At("directory"))==oldFixture.target &&
                String(candidateManifest.At("publication_target"))==oldFixture.target &&
                String(candidateManifest.At("sources").items[0].At("retained_files").items[0].At("origin_catalog"))==backup,
                "Replacement candidate is not bound to its final path and retained-origin backup");
            const auto published=PublishReceiverCatalogReplacement(oldFixture.target,newFixture.target,candidate);
            Require(published==candidateReceipt && InspectReceiverCatalog(oldFixture.target)==candidateReceipt &&
                ReadFile(backup+"/catalog-complete.json",65536)==oldReceipt &&
                Exists(backup+"/source_0/data0.graw") && Exists(backup+"/source_0/legacy-only.graw"),
                "Published catalog differs or the complete old catalog/raw backup was not preserved"); ++checks;
            const auto publishedObservation=ParseStrictJson(InspectReceiverCatalogReplacement(oldFixture.target,newFixture.target,candidate,newFixture.options.operationId));
            Require(String(publishedObservation.At("phase"))=="published" &&
                String(publishedObservation.At("published_receipt_sha256"))==ConfigurationSha256(candidateReceipt),
                "Read-only replacement inspection did not classify the published state"); ++checks;
            const auto supersededSessionRaw=oldFixture.options.stopped.receivers[0].outputDirectory+"/data0.graw";
            const auto retainedSessionRaw=oldFixture.options.stopped.receivers[0].outputDirectory+"/legacy-only.graw";
            const auto finalized=FinalizeReceiverCatalogReplacement(oldFixture.target,newFixture.options.operationId);
            const auto finalization=ParseStrictJson(finalized);
            Require(String(finalization.At("kind"))=="getdaq.catalog-replacement-cleanup-complete" &&
                Unsigned(finalization.At("superseded_files"))==1 && Unsigned(finalization.At("removed_link_bytes"),true)==8192 &&
                InspectReceiverCatalog(oldFixture.target)==candidateReceipt && !Exists(backup+"/source_0/data0.graw") && !Exists(supersededSessionRaw) &&
                Exists(backup+"/source_0/legacy-only.graw") && Exists(retainedSessionRaw) &&
                Exists(backup+"/source_1/data0.graw") && Exists(backup+"/source_1/data1.graw"),
                "Finalization did not remove only exact same-source/name old raw links and preserve the other old files");
            Require(FinalizeReceiverCatalogReplacement(oldFixture.target,newFixture.options.operationId)==finalized,
                "Same-operation replacement raw cleanup was not idempotent"); ++checks;
            const auto finalizedObservation=ParseStrictJson(InspectReceiverCatalogReplacement(oldFixture.target,newFixture.target,candidate,newFixture.options.operationId));
            Require(String(finalizedObservation.At("phase"))=="finalized" &&
                String(finalizedObservation.At("raw_payload_bytes_read"))=="0" && !Boolean(finalizedObservation.At("recovery_authorized")),
                "Read-only replacement inspection did not classify the completed cleanup"); ++checks;
            if (!catalogProgram.empty()) {
                int outputPipe[2];Require(!::pipe(outputPipe),"Create replacement inspection CLI output pipe");
                const pid_t inspect=::fork(); Require(inspect>=0,"Fork replacement inspection CLI");
                if (!inspect) {
                    ::close(outputPipe[0]);if(::dup2(outputPipe[1],STDOUT_FILENO)<0)::_exit(126);::close(outputPipe[1]);
                    ::execl(catalogProgram.c_str(),catalogProgram.c_str(),"--inspect-catalog-replacement",
                        oldFixture.target.c_str(),newFixture.target.c_str(),candidate.c_str(),newFixture.options.operationId.c_str(),static_cast<char *>(nullptr)); ::_exit(127);
                }
                ::close(outputPipe[1]);std::string observationBytes;char observationChunk[4096];ssize_t observationCount=0;
                while((observationCount=::read(outputPipe[0],observationChunk,sizeof(observationChunk)))>0) {
                    observationBytes.append(observationChunk,static_cast<std::size_t>(observationCount));
                    Require(observationBytes.size()<=65536,"Replacement inspection CLI output exceeds its metadata budget");
                }
                ::close(outputPipe[0]);int inspectStatus=0;
                Require(observationCount==0 && ::waitpid(inspect,&inspectStatus,0)==inspect && WIFEXITED(inspectStatus) && WEXITSTATUS(inspectStatus)==0,
                    "getdaq-catalog replacement inspection command failed");
                const auto cliObservation=ParseStrictJson(observationBytes);
                Require(String(cliObservation.At("kind"))=="getdaq.catalog-replacement-observation" &&
                    String(cliObservation.At("operation_id"))==newFixture.options.operationId &&
                    String(cliObservation.At("phase"))=="finalized" &&
                    String(cliObservation.At("raw_payload_bytes_read"))=="0" && !Boolean(cliObservation.At("recovery_authorized")),
                    "getdaq-catalog inspection CLI output differs from the read-only API contract"); ++checks;
                const pid_t child=::fork(); Require(child>=0,"Fork replacement publish CLI");
                if (!child) { ::execl(catalogProgram.c_str(),catalogProgram.c_str(),"--publish-catalog-replacement",
                    oldFixture.target.c_str(),newFixture.target.c_str(),candidate.c_str(),static_cast<char *>(nullptr)); ::_exit(127); }
                int status=0; Require(::waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0 &&
                    InspectReceiverCatalog(oldFixture.target)==candidateReceipt,"getdaq-catalog publish retry command failed"); ++checks;
            }
            Require(PublishReceiverCatalogReplacement(oldFixture.target,newFixture.target,candidate)==candidateReceipt &&
                InspectReceiverCatalog(oldFixture.target)==candidateReceipt,
                "Same-operation catalog publication retry did not observe the completed transaction"); ++checks;
        }
        {
            Fixture oldFixture;
            { ReceiverCatalog oldCatalog(oldFixture.options); oldCatalog.Execute(); }
            const auto oldReceipt=InspectReceiverCatalog(oldFixture.target);
            Fixture newFixture(8192,false,false,false,false,'f');
            newFixture.options.operationId="catalog-stage-new";
            newFixture.options.receiver.catalogRoot=oldFixture.options.receiver.catalogRoot;
            newFixture.options.stopped.planSha256=ConfigurationSha256(EncodeOptions(newFixture.options.receiver,newFixture.options.executableSha256));
            newFixture.options.stagedForReplacement=true;
            const auto stage=ReceiverCatalogReplacementStagePath(newFixture.options);
            newFixture.target=stage;
            { ReceiverCatalog staged(newFixture.options); staged.Validate();
              const auto stagedReceipt=staged.Execute();
              Require(InspectReceiverCatalog(stage)==stagedReceipt &&
                  String(ParseStrictJson(stagedReceipt).At("directory"))==oldFixture.target &&
                  Boolean(ParseStrictJson(ReadFile(stage+"/manifest.json",16*1024*1024)).At("staged_for_replacement")) &&
                  InspectReceiverCatalog(oldFixture.target)==oldReceipt,
                  "Replacement stage was not independently complete/bound or modified the old catalog"); }
            const auto candidate=newFixture.root+"/replacement-candidate";
            const auto candidateReceipt=PrepareReceiverCatalogReplacement(oldFixture.target,stage,candidate);
            Require(InspectReceiverCatalog(candidate)==candidateReceipt && InspectReceiverCatalog(oldFixture.target)==oldReceipt,
                "Staged replacement candidate was not prepared without changing the old Run");
            const auto published=PublishReceiverCatalogReplacement(oldFixture.target,stage,candidate);
            Require(published==candidateReceipt && InspectReceiverCatalog(oldFixture.target)==candidateReceipt &&
                InspectReceiverCatalog(ReceiverCatalogReplacementStagePath(newFixture.options))==InspectReceiverCatalog(stage),
                "Staged catalog replacement did not publish the verified candidate");
            ++checks;
        }
        {
            Fixture fixture; Fd active(::open((fixture.options.stopped.receivers[1].outputDirectory+"/.getdaq-receiver.lock").c_str(),O_RDWR|O_CLOEXEC));
            Require(active.value>=0 && !::flock(active.value,LOCK_EX|LOCK_NB),"Hold live receiver fixture lease");
            ReceiverCatalog catalog(fixture.options); Reject([&] { catalog.Validate(); }); Require(!Exists(fixture.target),"Live receiver precheck created archive"); ++checks;
        }
        {
            Fixture fixture; auto changed=fixture.options; changed.stopped.receivers[1].sessionId=std::string(64,'e'); Reject([&] { ReceiverCatalog catalog(changed); }); ++checks;
            changed=fixture.options; changed.stopped.receivers[1].sealVerified=false; Reject([&] { ReceiverCatalog catalog(changed); }); ++checks;
            changed=fixture.options; changed.stopped.receivers[1].outputDirectory="/tmp/wrong-session"; Reject([&] { ReceiverCatalog catalog(changed); }); ++checks;
            changed=fixture.options; changed.experiment="../outside"; Reject([&] { ReceiverCatalog catalog(changed); }); ++checks;
            changed=fixture.options; changed.receiver.catalogRoot=fixture.root+"/another"; Reject([&] { ReceiverCatalog catalog(changed); }); ++checks;
        }
        {
            Fixture fixture; ReceiverCatalog catalog(fixture.options); catalog.Validate();
            const auto file=fixture.options.stopped.receivers[0].outputDirectory+"/data0.graw";
            Fd changed(::open(file.c_str(),O_RDWR|O_CLOEXEC)); Require(changed.value>=0 && !::ftruncate(changed.value,1),"Corrupt catalog source metadata");
            Reject([&] { catalog.Validate(); }); Require(!::ftruncate(changed.value,8192),"Restore catalog fixture"); Reject([&] { catalog.Validate(); }); checks+=2;
        }
        {
            Fixture fixture;
            ReceiverCatalog interrupted(fixture.options,[](const std::string &stage,const std::string &,std::uint64_t count) { if (stage=="linked" && count==1) throw std::runtime_error("Injected audit failure after first link"); });
            Reject([&] { interrupted.Execute(); }); Require(Exists(fixture.target+"/source_0/data0.graw") && !Exists(fixture.target+"/catalog-complete.json"),"Partial catalog falsely marked complete/removed evidence"); checks+=2;
            auto other=fixture.options; other.operationId="another"; other.runNumber=1;
            ReceiverCatalog replay(other); Reject([&] { replay.Execute(); }); Require(!Exists(fixture.options.receiver.catalogRoot+"/Test/run_0001/node-0"),"New ID replayed partial session"); ++checks;
        }
        {
            Fixture fixture; const auto file=fixture.options.stopped.receivers[1].outputDirectory+"/data0.graw";
            Require(!::unlink(file.c_str()) && !::symlink("/dev/null",file.c_str()),"Prepare raw symlink rejection");
            ReceiverCatalog catalog(fixture.options); Reject([&] { catalog.Execute(); }); Require(!Exists(fixture.target),"Malformed second source allowed first archive mutation"); ++checks;
        }
        {
            Fixture fixture; ReceiverCatalog catalog(fixture.options,[&](const std::string &stage,const std::string &,std::uint64_t) {
                if (stage=="before-complete") {
                    Fd raw(::open((fixture.options.stopped.receivers[0].outputDirectory+"/data0.graw").c_str(),O_RDWR|O_CLOEXEC));
                    Require(raw.value>=0 && !::ftruncate(raw.value,1),"Inject late raw metadata change");
                }
            });
            Reject([&] { catalog.Execute(); }); Require(!Exists(fixture.target+"/catalog-complete.json"),"Late changed data falsely completed"); ++checks;
        }
        {
            Fixture fixture; ReceiverCatalog catalog(fixture.options,[&](const std::string &stage,const std::string &,std::uint64_t count) {
                if (stage=="linked" && count==1) {
                    const auto lock=fixture.options.receiver.catalogRoot+"/.getdaq-catalog.lock";
                    Require(!::rename(lock.c_str(),(lock+".old").c_str()),"Replace catalog lease fixture"); WriteNew(lock,"");
                }
            });
            Reject([&] { catalog.Execute(); }); Require(!Exists(fixture.target+"/catalog-complete.json"),"Replaced root lease falsely completed"); ++checks;
        }
        std::cout<<"PASS "<<checks<<" native catalog checks; sparse 512 MiB files keep device/inode/blocks, original paths and no raw payload reads; metadata fixtures only, no actual receiver/hardware/network\n";
    } catch (const std::exception &error) { std::cerr<<"FAIL catalog: "<<error.what()<<'\n'; return 1; }
}
