#include "receivercatalog.h"
#include "receivercataloginternal.h"
#include "receivercatalog_scananalysis_p.h"
#include "receivercatalog_scanpoint_p.h"
#include "receivercatalog_scansource_p.h"
#include "receivercatalog_scanwaveformmetrics_p.h"
#include "peakingtimescan.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include "planio.h"
#include "hitpattern.h"
#include "mfmstream.h"
#include <algorithm>
#include <cstdlib>
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


static std::string AnalyzeBoundScanWaveforms(const std::string &scanPath,const std::string &bindingPath,
                                             unsigned int coboNumber,unsigned int requestedAsad,
                                             unsigned int requestedAget,unsigned int requestedChannel,
                                             bool autoGain,bool pulserScan,const std::string &outputPath) {
    ValidateScanAnalysisPaths(scanPath,bindingPath,outputPath);
    Require(coboNumber<=255,"MFM CoBo number must be in 0..255");
    Directory scan(scanPath),bindings(bindingPath);
    const auto manifestBytes=ReadFile(scanPath+"/scan-manifest.json",1024*1024);
    const auto scanManifest=ParseStrictJson(manifestBytes);
    const std::string scanKind=String(scanManifest.At("kind"));
    const std::string requiredKind=autoGain ? "getdaq.auto-gain-scan" : pulserScan ? "getdaq.pulser-amplitude-scan" : "getdaq.peaking-time-scan";
    Require(scanKind==requiredKind &&
        !Boolean(scanManifest.At("hardware_loaded_verified")) && Boolean(scanManifest.At("requires_full_configure_per_point")) &&
        (!autoGain || !Boolean(scanManifest.At("gain_result_interpretation_verified"))),
        autoGain ? "Analysis requires an offline AutoGain configuration plan" : pulserScan ?
            "Analysis requires an offline Pulser amplitude configuration plan" : "Analysis requires an offline PeakingTime configuration plan");
    const auto targetCobo=String(scanManifest.At("target_cobo"));
    const unsigned asad=autoGain ? static_cast<unsigned>(Unsigned(scanManifest.At("target_asad"))) : requestedAsad;
    const unsigned aget=autoGain ? static_cast<unsigned>(Unsigned(scanManifest.At("target_aget"))) : requestedAget;
    Require(asad<=3 && aget<=3 && requestedChannel<=67,"Waveform target AsAd/AGET/channel is invalid");
    const auto &points=scanManifest.At("points");
    Require(points.type==JsonValue::Array && !points.items.empty() && points.items.size()<=256,"Invalid AutoGain scan point list");
    std::int64_t startMv=0,stopMv=0,stepMv=0,voltageCount=0;
    std::vector<unsigned int> expectedChannels;
    if (autoGain || pulserScan) {
        startMv=ManifestInteger(scanManifest.At("start_mv"));
        stopMv=ManifestInteger(scanManifest.At("stop_mv"));
        stepMv=ManifestInteger(scanManifest.At("step_mv"));
        Require(startMv>=-1000 && stopMv<=1000 && startMv<=stopMv && stepMv>0,
            "Scan voltage range must be ascending within -1000..1000 mV with a positive step");
        const std::int64_t span=stopMv-startMv;
        Require(span%stepMv==0,"Scan voltage step must land exactly on the stop voltage");
        voltageCount=span/stepMv+1;
        if (autoGain) {
            const std::uint64_t first=Unsigned(scanManifest.At("first_channel"));
            const std::uint64_t last=Unsigned(scanManifest.At("last_channel"));
            Require(first<=67 && last<=67 && first<=last,"AutoGain channel range is outside 0..67");
            const unsigned int lastReg3=last<33 ? static_cast<unsigned int>(last) : 33u;
            for (int channel=static_cast<int>(lastReg3);channel>=static_cast<int>(first);--channel)
                expectedChannels.push_back(static_cast<unsigned int>(channel));
            const unsigned int firstReg4=first>34 ? static_cast<unsigned int>(first) : 34u;
            for (unsigned int channel=firstReg4;channel<=last;++channel) expectedChannels.push_back(channel);
            Require(expectedChannels.size()==last-first+1,"AutoGain manifest channel range contains an unsupported channel mapping");
            Require(voltageCount*static_cast<std::int64_t>(expectedChannels.size())==static_cast<std::int64_t>(points.items.size()),
                "AutoGain point count differs from its channel and voltage ranges");
        } else {
            Require(voltageCount==static_cast<std::int64_t>(points.items.size()),
                "Pulser point count differs from its declared voltage range");
        }
    }
    std::set<std::string> runs,sessions;
    std::string rows="[";
    for (unsigned int i=0;i<points.items.size();++i) {
        const auto &point=points.items[i];
        Require(Unsigned(point.At("index"))==i,"Waveform scan point indexes must be contiguous and ordered");
        if (!autoGain && !pulserScan) Require(i<16,"PeakingTime scan point index is outside 0..15");
        const unsigned channel=autoGain ? static_cast<unsigned>(Unsigned(point.At("channel_id"))) : requestedChannel;
        const std::int64_t voltage=(!autoGain && !pulserScan) ? 0 : [&]() {
            return ManifestInteger(point.At("voltage_mv"));
        }();
        Require(channel<=67,"Waveform point channel is outside 0..67");
        if (autoGain) {
            const std::size_t channelIndex=static_cast<std::size_t>(i/voltageCount);
            const std::int64_t voltageIndex=static_cast<std::int64_t>(i)%voltageCount;
            Require(channelIndex<expectedChannels.size() && channel==expectedChannels[channelIndex] &&
                voltage==startMv+voltageIndex*stepMv,
                "AutoGain point ordering or values differ from the declared channel/voltage ranges");
        } else if (pulserScan) {
            Require(voltage==startMv+static_cast<std::int64_t>(i)*stepMv,
                "Pulser point voltages differ from the declared voltage range");
        }
        const auto boundPoint=LoadBoundScanPointInput(scanPath,bindingPath,i,point,targetCobo,
            outputPath,"AutoGain","AutoGain artifact omits its target CoBo");
        const auto &binding=boundPoint.binding;
        const auto &catalogPath=boundPoint.catalogPath;
        const auto &artifact=boundPoint.artifact;
        const auto artifactName=String(point.At("artifact"));
        Require(artifactName.size()>=10 && artifactName.compare(0,6,"point-")==0 && artifactName.substr(artifactName.size()-5)==".json" &&
            artifactName.find_first_of("/\\")==std::string::npos && artifactName.find("..") == std::string::npos,
            "AutoGain point artifact must be a single local file name");
        const auto *selectedCoBo=&artifact.settings.cobos[boundPoint.selectedCoboIndex];
        std::string peakingTime;
        if (!autoGain && pulserScan) {
            const auto voltageSetting=selectedCoBo->configure.asad.generator.find("amplitudeStart");
            Require(voltageSetting!=selectedCoBo->configure.asad.generator.end() && SignedIntegerText(voltageSetting->second)==voltage,
                "Pulser manifest voltage differs from its bound configuration artifact");
        }
        const CoBoAgetChipSettings *selectedChip=nullptr;
        for (const auto &chip:selectedCoBo->configure.readout.aget.chips)
            if (chip.asadId==asad && chip.agetId==aget) { selectedChip=&chip; break; }
        Require(selectedChip,"AutoGain artifact omits its target AGET");
        if (!autoGain && !pulserScan) {
            const auto setting=selectedChip->settings.globals.find("Reg1/peackingTime");
            Require(setting!=selectedChip->settings.globals.end(),"PeakingTime artifact omits the selected chip timing value");
            const auto value=SignedIntegerText(setting->second);
            Require(value==PeakingTimeForScanIndex(selectedChip->settings.chipVersion,i),
                "PeakingTime point index differs from the selected chip's configuration table");
            peakingTime=std::to_string(value);
        }
        bool channelSelected=false,channelActive=false;
        for (const auto &entry:selectedChip->settings.channels) {
            if (entry.id==channel) { channelSelected=entry.selectedForTest; channelActive=entry.active; }
            else if (autoGain) Require(!entry.selectedForTest,"AutoGain artifact selects more than its manifest channel");
        }
        Require(autoGain ? channelSelected : channelActive,
            autoGain ? "AutoGain manifest channel is not selected by its bound artifact" : "Waveform response channel is not active in its bound artifact");
        const auto runKey=String(binding.At("experiment"))+"/"+std::to_string(Unsigned(binding.At("run_number")));
        Require(runs.insert(runKey).second && sessions.insert(String(binding.At("session_id"))).second,
            "AutoGain points must have distinct Run numbers and receiver sessions");
        BoundScanWaveformAccumulator waveform(coboNumber,asad,aget,channel);
        const auto filesRead=ForEachBoundScanCatalogRawFile(catalogPath,targetCobo,[&](int descriptor) {
            DescriptorInputBuffer streamBuffer(descriptor); std::istream stream(&streamBuffer);
            MfmStream mfm(32u*1024u*1024u,[&](const std::uint8_t *frame,const FrameInfo &info) {
                waveform.ProcessFrame(frame,info);
            });
            std::vector<std::uint8_t> buffer(64u*1024u);
            while (stream) { stream.read(reinterpret_cast<char *>(buffer.data()),std::streamsize(buffer.size()));
                const auto count=stream.gcount(); if (count>0) mfm.Feed(buffer.data(),static_cast<std::size_t>(count)); }
            Require(!stream.bad(),"Bound AutoGain raw file read failed"); mfm.Finish();
        });
        const auto &waveformMetrics=waveform.Metrics();
        Require(filesRead>0 && waveformMetrics.frames>0,"No selected CoBo/AsAd frames found in bound AutoGain catalog source");
        double meanPeak=waveformMetrics.meanPeak;
        double meanPeaking=waveformMetrics.meanPeaking;
        double meanFwhm=waveformMetrics.meanFwhm;
        if (waveformMetrics.valid) {
            meanPeak/=waveformMetrics.valid; meanPeaking/=waveformMetrics.valid; meanFwhm/=waveformMetrics.valid;
        }
        if (i) rows+=',';
        rows+="{\"index\":"+std::to_string(i)+",\"channel_id\":"+std::to_string(channel)+
            (autoGain || pulserScan ? ",\"voltage_mv\":"+std::to_string(voltage) : ",\"peaking_time\":"+peakingTime)+
            ",\"configuration_sha256\":"+EncodeJsonString(String(point.At("configuration_sha256")))+
            ",\"artifact_sha256\":"+EncodeJsonString(String(point.At("artifact_sha256")))+
            ",\"experiment\":"+EncodeJsonString(String(binding.At("experiment")))+",\"run_number\":"+std::to_string(Unsigned(binding.At("run_number")))+
            ",\"session_id\":"+EncodeJsonString(String(binding.At("session_id")))+
            ",\"catalog_receipt_sha256\":"+EncodeJsonString(String(binding.At("catalog_receipt_sha256")))+
            ",\"files_read\":"+std::to_string(filesRead)+",\"frames_seen\":"+std::to_string(waveformMetrics.frames)+
            ",\"channel_frames\":"+std::to_string(waveformMetrics.channelFrames)+",\"channel_missing\":"+std::to_string(waveformMetrics.missing)+
            ",\"invalid_waveforms\":"+std::to_string(waveformMetrics.invalid)+",\"valid_waveforms\":"+std::to_string(waveformMetrics.valid)+
            ",\"mean_peak_adc\":"+Decimal(meanPeak)+",\"mean_peaking_buckets\":"+Decimal(meanPeaking)+
            ",\"mean_fwhm_buckets\":"+Decimal(meanFwhm)+"}";
    }
    rows+=']';
    Require(ReadFile(scanPath+"/scan-manifest.json",1024*1024)==manifestBytes,"AutoGain plan changed during waveform analysis");
    const std::string reportKind=autoGain ? "getdaq.autogain-waveform-response" : pulserScan ? "getdaq.pulser-waveform-response" : "getdaq.peaking-time-waveform-response";
    const std::string report="{\"schema_version\":1,\"kind\":"+EncodeJsonString(reportKind)
        +",\"scan_kind\":"+EncodeJsonString(scanKind)+",\"target_cobo\":"+
        EncodeJsonString(targetCobo)+",\"mfm_cobo_number\":"+std::to_string(coboNumber)+",\"target_asad\":"+std::to_string(asad)+
        ",\"target_aget\":"+std::to_string(aget)+",\"scan_manifest_sha256\":"+EncodeJsonString(ConfigurationSha256(manifestBytes))+
        ",\"complete\":true,\"mapping_verified\":true,\"hardware_loaded_verified\":false,\"physical_gain_interpreted\":false,\"interpretation\":"+
        EncodeJsonString(autoGain ? "Archived waveform response metrics only; not a physical gain result or pass/fail judgment" :
            pulserScan ? "Archived waveform response metrics only; channel selection was supplied by the operator; no physical gain or pass/fail judgment" :
            "Archived waveform response metrics only; channel selection was supplied by the operator; no pass/fail judgment")+
        ",\"points\":"+rows+"}\n";
    PublishScanAnalysisReport(outputPath,report,"AutoGain","AutoGain report");
    scan.Check(); bindings.Check(); return report;
}

std::string AnalyzeBoundAutoGainWaveforms(const std::string &scanPath,const std::string &bindingPath,
                                          unsigned int coboNumber,const std::string &outputPath) {
    return AnalyzeBoundScanWaveforms(scanPath,bindingPath,coboNumber,0,0,0,true,false,outputPath);
}

std::string AnalyzeBoundPulserWaveforms(const std::string &scanPath,const std::string &bindingPath,
                                        unsigned int coboNumber,unsigned int asad,unsigned int aget,
                                        unsigned int channel,const std::string &outputPath) {
    return AnalyzeBoundScanWaveforms(scanPath,bindingPath,coboNumber,asad,aget,channel,false,true,outputPath);
}

std::string AnalyzeBoundPeakingTimeWaveforms(const std::string &scanPath,const std::string &bindingPath,
                                             unsigned int coboNumber,unsigned int asad,unsigned int aget,
                                             unsigned int channel,const std::string &outputPath) {
    return AnalyzeBoundScanWaveforms(scanPath,bindingPath,coboNumber,asad,aget,channel,false,false,outputPath);
}

} // namespace GetDAQ
