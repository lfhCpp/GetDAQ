#include "receivercatalog.h"
#include "receivercataloginternal.h"
#include "receivercatalog_scananalysis_p.h"
#include "receivercatalog_scanpoint_p.h"
#include "receivercatalog_scansource_p.h"
#include "peakingtimescan.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include "planio.h"
#include "hitpattern.h"
#include "mfmstream.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <istream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>

namespace GetDAQ {
using namespace SupervisorIO;
using namespace ReceiverCatalogInternal;
using namespace ReceiverCatalogScanAnalysisInternal;


std::string AnalyzeThresholdScan(const std::string &scanPath,const std::string &bindingPath,
                                unsigned int coboNumber,const std::string &outputPath) {
    ValidateScanAnalysisPaths(scanPath,bindingPath,outputPath);
    Require(coboNumber<=255,"MFM CoBo number must be in 0..255");
    Directory scan(scanPath),bindings(bindingPath);
    const auto manifestBytes=ReadFile(scanPath+"/scan-manifest.json",1024*1024);
    const auto scanManifest=ParseStrictJson(manifestBytes);
    Require(String(scanManifest.At("kind"))=="getdaq.aget-threshold-scan" &&
        !Boolean(scanManifest.At("hardware_loaded_verified")) && Boolean(scanManifest.At("requires_full_configure_per_point")),
        "Analysis requires an offline AGET threshold scan plan");
    const auto targetCobo=String(scanManifest.At("target_cobo"));
    const unsigned asad=static_cast<unsigned>(Unsigned(scanManifest.At("target_asad")));
    const unsigned aget=static_cast<unsigned>(Unsigned(scanManifest.At("target_aget")));
    Require(asad<=3 && aget<=3,"Threshold scan target AsAd/AGET is invalid");
    const auto &points=scanManifest.At("points");
    Require(points.type==JsonValue::Array && !points.items.empty() && points.items.size()<=128,
        "Invalid AGET threshold scan point list");
    const std::int64_t startCode=ManifestInteger(scanManifest.At("start_code"));
    const std::int64_t stopCode=ManifestInteger(scanManifest.At("stop_code"));
    const std::int64_t stepCode=ManifestInteger(scanManifest.At("step_code"));
    Require(startCode>=0 && stopCode<=127 && startCode<=stopCode && stepCode>0,
        "Threshold scan range must be ascending within 0..127 with a positive step");
    const std::int64_t thresholdSpan=stopCode-startCode;
    Require(thresholdSpan%stepCode==0,"Threshold scan step must land exactly on the stop code");
    const std::int64_t thresholdCount=thresholdSpan/stepCode+1;
    Require(thresholdCount==static_cast<std::int64_t>(points.items.size()),
        "Threshold point count differs from its declared code range");

    std::set<std::string> runs,sessions;
    std::string rows="[";
    for (unsigned int i=0;i<points.items.size();++i) {
        const auto &point=points.items[i];
        Require(Unsigned(point.At("index"))==i,"Threshold scan point indexes must be contiguous and ordered");
        const auto threshold=Unsigned(point.At("threshold_code"));
        Require(threshold<=127 && threshold==static_cast<std::uint64_t>(startCode+static_cast<std::int64_t>(i)*stepCode),
            "Threshold point code differs from the declared scan range");
        const auto boundPoint=LoadBoundScanPointInput(scanPath,bindingPath,i,point,targetCobo,
            outputPath,"Threshold","Threshold point artifact does not contain its target CoBo");
        const auto &binding=boundPoint.binding;
        const auto &catalogPath=boundPoint.catalogPath;
        const auto &artifact=boundPoint.artifact;
        const auto *selectedCoBo=&artifact.settings.cobos[boundPoint.selectedCoboIndex];
        const CoBoAgetChipSettings *selectedChip=nullptr;
        for (const auto &chip:selectedCoBo->configure.readout.aget.chips)
            if (chip.asadId==asad && chip.agetId==aget) { selectedChip=&chip; break; }
        Require(selectedChip,"Threshold point artifact does not contain its target AGET");
        const auto coarseSetting=selectedChip->settings.globals.find("Reg1/GlobalThresholdValue");
        Require(coarseSetting!=selectedChip->settings.globals.end() && !coarseSetting->second.empty(),
            "Threshold point artifact is missing its global coarse threshold");
        unsigned int coarse=0;
        for (char digit:coarseSetting->second) {
            Require(digit>='0' && digit<='7' && coarse<=7,"Threshold point artifact has an invalid coarse threshold");
            coarse=coarse*10u+static_cast<unsigned int>(digit-'0');
        }
        Require(coarse<=7 && !selectedChip->settings.channels.empty(),"Threshold point artifact has no channel thresholds");
        const unsigned int fine=selectedChip->settings.channels.front().threshold;
        Require(fine<=15,"Threshold point artifact has an invalid fine threshold");
        for (const auto &channel:selectedChip->settings.channels)
            Require(channel.threshold==fine,"Threshold point artifact has inconsistent channel threshold codes");
        Require(coarse*16u+fine==threshold,"Manifest threshold code differs from the bound point artifact");
        const auto runKey=String(binding.At("experiment"))+"/"+std::to_string(Unsigned(binding.At("run_number")));
        Require(runs.insert(runKey).second && sessions.insert(String(binding.At("session_id"))).second,
            "Threshold points must have distinct Run numbers and receiver sessions");

        HitPatternCounts total{};
        const auto filesRead=ForEachBoundScanCatalogRawFile(catalogPath,targetCobo,[&](int descriptor) {
            DescriptorInputBuffer streamBuffer(descriptor); std::istream stream(&streamBuffer);
            const auto part=CountHitPatternFrames(stream,coboNumber,asad,aget,false);
            MergeHitPatternCounts(&total,part);
        });
        Require(filesRead>0 && total.frames>0,"No selected CoBo/AsAd/AGET frames were found in the target catalog source");
        if (i) rows+=',';
        rows+="{\"index\":"+std::to_string(i)+",\"threshold_code\":"+std::to_string(threshold)+
            ",\"configuration_sha256\":"+EncodeJsonString(String(point.At("configuration_sha256")))+
            ",\"artifact_sha256\":"+EncodeJsonString(String(point.At("artifact_sha256")))+
            ",\"experiment\":"+EncodeJsonString(String(binding.At("experiment")))+
            ",\"run_number\":"+std::to_string(Unsigned(binding.At("run_number")))+
            ",\"session_id\":"+EncodeJsonString(String(binding.At("session_id")))+
            ",\"catalog_receipt_sha256\":"+EncodeJsonString(String(binding.At("catalog_receipt_sha256")))+
            +",\"files_read\":"+std::to_string(filesRead)+",\"matching_frames\":"+EncodeJsonString(std::to_string(total.frames))+
            +",\"channel_frames\":[";
        for (std::size_t channel=0;channel<total.channelFrames.size();++channel) {
            if (channel) rows+=',';
            rows+=EncodeJsonString(std::to_string(total.channelFrames[channel]));
        }
        rows+="]}";
    }
    rows+=']';
    Require(ReadFile(scanPath+"/scan-manifest.json",1024*1024)==manifestBytes,"Scan plan changed during threshold analysis");
    const std::string report="{\"schema_version\":1,\"kind\":\"getdaq.threshold-hit-pattern-analysis\",\"scan_kind\":\"getdaq.aget-threshold-scan\",\"target_cobo\":"+
        EncodeJsonString(targetCobo)+",\"mfm_cobo_number\":"+std::to_string(coboNumber)+",\"target_asad\":"+std::to_string(asad)+
        ",\"target_aget\":"+std::to_string(aget)+",\"scan_manifest_sha256\":"+EncodeJsonString(ConfigurationSha256(manifestBytes))+
        +",\"complete\":true,\"mapping_verified\":true,\"hardware_loaded_verified\":false,\"interpretation\":\"MFM header hit-pattern counts only; no physical threshold conclusion\",\"points\":"+rows+"}\n";
    PublishScanAnalysisReport(outputPath,report,"threshold","Threshold analysis");
    scan.Check(); bindings.Check();
    return report;
}

} // namespace GetDAQ
