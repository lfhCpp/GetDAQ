#include "receivercatalog.h"
#include "receivercataloginternal.h"
#include "receivercatalog_scananalysis_p.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include "planio.h"
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
using namespace SupervisorIO;
using namespace ReceiverCatalogInternal;

std::string BuildScanPointBinding(const std::string &scanPath,unsigned int pointIndex,const std::string &catalogPath) {
    Path(scanPath); Path(catalogPath);
    Require(scanPath!=catalogPath && scanPath.compare(0,catalogPath.size()+1,catalogPath+'/')!=0 &&
        catalogPath.compare(0,scanPath.size()+1,scanPath+'/')!=0,"Scan plan and catalog directories must be separate");
    Directory scan(scanPath);
    const auto manifestBytes=ReadFile(scanPath+"/scan-manifest.json",1024*1024);
    const auto manifest=ParseStrictJson(manifestBytes);
    Require(manifest.type==JsonValue::Object && Unsigned(manifest.At("schema_version"))==1,"Unsupported scan manifest");
    const auto kind=String(manifest.At("kind"));
    Require((kind=="getdaq.peaking-time-scan" || kind=="getdaq.pulser-amplitude-scan" ||
        kind=="getdaq.aget-threshold-scan" || kind=="getdaq.auto-gain-scan") &&
        manifest.At("hardware_loaded_verified").type==JsonValue::Boolean && !Boolean(manifest.At("hardware_loaded_verified")) &&
        manifest.At("requires_full_configure_per_point").type==JsonValue::Boolean && Boolean(manifest.At("requires_full_configure_per_point")),
        "Scan manifest is not a supported offline configuration plan");
    const auto &points=manifest.At("points"); Require(points.type==JsonValue::Array && !points.items.empty() && points.items.size()<=256,"Invalid scan point list");
    const JsonValue *selected=nullptr;
    for (unsigned int i=0;i<points.items.size();++i) {
        const auto &entry=points.items[i];
        Require(entry.type==JsonValue::Object && Unsigned(entry.At("index"))==i,"Scan point indexes must be contiguous and ordered");
        if (i==pointIndex) selected=&entry;
    }
    Require(selected!=nullptr,"Requested scan point is outside the plan");
    const auto artifactName=String(selected->At("artifact"));
    Require(artifactName.size()>=10 && artifactName.compare(0,6,"point-")==0 && artifactName.substr(artifactName.size()-5)==".json" &&
        artifactName.find_first_of("/\\")==std::string::npos && artifactName.find("..") == std::string::npos,
        "Scan point artifact must be a single local file name");
    const auto artifactBytes=ReadFile(scanPath+'/'+artifactName,8*1024*1024);
    Require(ConfigurationSha256(artifactBytes)==String(selected->At("artifact_sha256")),"Scan point artifact SHA differs from manifest");
    const auto artifact=DecodeSystemExperimentArtifact(artifactBytes);
    Require(artifact.configurationSha256==String(selected->At("configuration_sha256")) &&
        artifact.artifactSha256==String(selected->At("artifact_sha256")),"Scan point artifact digest differs from manifest");

    const auto catalogReceiptBytes=InspectReceiverCatalog(catalogPath);
    const auto requestBytes=ReadFile(catalogPath+"/request.json",1024*1024);
    const auto request=ParseStrictJson(requestBytes);
    const auto plan=request.At("plan");
    Require(String(request.At("kind"))=="getdaq.receiver-catalog-request" &&
        String(plan.At("configuration_sha256"))==artifact.configurationSha256 && String(plan.At("artifact_sha256"))==artifact.artifactSha256,
        "Completed catalog was recorded for a different scan configuration artifact");
    const auto run=Unsigned(request.At("run_number"));
    Require(run<unsigned(std::numeric_limits<int>::max()),"Catalog Run number is outside supported range");
    const auto stopped=request.At("stopped");
    const auto session=String(stopped.At("session_id")); Digest(session);
    const auto catalogReceipt=ParseStrictJson(catalogReceiptBytes);
    const auto catalogManifest=ReadFile(catalogPath+"/manifest.json",16*1024*1024);
    Require(String(catalogReceipt.At("manifest_sha256"))==ConfigurationSha256(catalogManifest),"Catalog manifest changed during binding");
    JsonLimits catalogLimits; catalogLimits.stringBytes=1024*1024; catalogLimits.arrayItems=200000;
    const auto catalogManifestValue=ParseStrictJson(catalogManifest,catalogLimits);
    Require(String(catalogManifestValue.At("request_key"))==String(catalogReceipt.At("request_key")) &&
        String(catalogManifestValue.At("session_id"))==session,"Catalog completion receipt and request session differ");
    scan.Check();
    const std::string binding="{\"schema\":1,\"kind\":\"getdaq.scan-run-binding\",\"scan_kind\":"+EncodeJsonString(kind)+
        ",\"target_cobo\":"+EncodeJsonString(String(manifest.At("target_cobo")))+
        ",\"point_index\":"+std::to_string(pointIndex)+",\"point_artifact\":"+EncodeJsonString(artifactName)+
        ",\"configuration_sha256\":"+EncodeJsonString(artifact.configurationSha256)+",\"artifact_sha256\":"+EncodeJsonString(artifact.artifactSha256)+
        ",\"scan_manifest_sha256\":"+EncodeJsonString(ConfigurationSha256(manifestBytes))+
        ",\"experiment\":"+EncodeJsonString(String(request.At("experiment")))+",\"run_number\":"+std::to_string(run)+
        ",\"session_id\":"+EncodeJsonString(session)+",\"catalog_directory\":"+EncodeJsonString(catalogPath)+
        ",\"catalog_request_key\":"+EncodeJsonString(String(catalogReceipt.At("request_key")))+
        ",\"catalog_manifest_sha256\":"+EncodeJsonString(String(catalogReceipt.At("manifest_sha256")))
        +",\"catalog_receipt_sha256\":"+EncodeJsonString(ConfigurationSha256(catalogReceiptBytes))
        +",\"hardware_loaded_verified\":false}\n";
    Require(ReadFile(scanPath+"/scan-manifest.json",1024*1024)==manifestBytes &&
        ReadFile(scanPath+'/'+artifactName,8*1024*1024)==artifactBytes &&
        ReadFile(catalogPath+"/request.json",1024*1024)==requestBytes &&
        ReadFile(catalogPath+"/manifest.json",16*1024*1024)==catalogManifest &&
        InspectReceiverCatalog(catalogPath)==catalogReceiptBytes,"Scan plan or completed catalog changed while binding");
    scan.Check();
    return binding;
}

std::string BindScanPointToCatalog(const std::string &scanPath,unsigned int pointIndex,const std::string &catalogPath,const std::string &outputPath) {
    Path(outputPath);
    Require(outputPath.compare(0,scanPath.size()+1,scanPath+'/')!=0 && outputPath.compare(0,catalogPath.size()+1,catalogPath+'/')!=0,
        "Binding output must be outside the immutable scan plan and catalog");
    const auto binding=BuildScanPointBinding(scanPath,pointIndex,catalogPath);
    const auto slash=outputPath.find_last_of('/'); Require(slash>0 && slash+1<outputPath.size(),"Binding output must be inside an existing directory");
    struct stat existing; Require(::lstat(outputPath.c_str(),&existing) && errno==ENOENT,"Binding output already exists; preserve it");
    const auto pending=outputPath+".pending"; Path(pending); WriteNew(pending,binding);
    if (::link(pending.c_str(),outputPath.c_str())) throw std::runtime_error("Cannot exclusively publish scan/Run binding");
    const auto parentPath=outputPath.substr(0,slash);
    Fd parentFd(::open(parentPath.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));
    if (parentFd.value<0 || ::fsync(parentFd.value)) throw std::runtime_error("Cannot sync published scan/Run binding");
    if (::unlink(pending.c_str()) || ::fsync(parentFd.value)) throw std::runtime_error("Binding published but pending-link cleanup/sync failed; inspect both files");
    return binding;
}

std::string SummarizeScanBindings(const std::string &scanPath,const std::string &bindingPath,const std::string &outputPath) {
    Path(scanPath); Path(bindingPath); Path(outputPath);
    Require(scanPath!=bindingPath && scanPath.compare(0,bindingPath.size()+1,bindingPath+'/')!=0 &&
        bindingPath.compare(0,scanPath.size()+1,scanPath+'/')!=0,"Scan plan and binding directory must be separate");
    Require(outputPath.compare(0,scanPath.size()+1,scanPath+'/')!=0 && outputPath.compare(0,bindingPath.size()+1,bindingPath+'/')!=0,
        "Summary output must be outside the immutable plan and binding directory");
    Directory scan(scanPath), bindings(bindingPath);
    const auto manifestBytes=ReadFile(scanPath+"/scan-manifest.json",1024*1024);
    const auto manifest=ParseStrictJson(manifestBytes);
    const auto &points=manifest.At("points");
    Require(points.type==JsonValue::Array && !points.items.empty() && points.items.size()<=256,"Invalid scan point list");
    std::set<std::string> runs,sessions;
    std::string rows="[";
    for (unsigned int i=0;i<points.items.size();++i) {
        const auto &point=points.items[i]; Require(Unsigned(point.At("index"))==i,"Scan point indexes must be contiguous and ordered");
        const std::string filename="point-"+(i<100 ? (i<10 ? std::string("00") : std::string("0")) : std::string())+std::to_string(i)+".json";
        const std::string path=bindingPath+'/'+filename;
        const auto actual=ReadFile(path,65536);
        const auto binding=ParseStrictJson(actual);
        const std::string catalogPath=String(binding.At("catalog_directory"));
        Require(bindingPath!=catalogPath && bindingPath.compare(0,catalogPath.size()+1,catalogPath+'/')!=0 &&
            catalogPath.compare(0,bindingPath.size()+1,bindingPath+'/')!=0,"Binding directory and immutable catalog must be separate");
        Require(outputPath.compare(0,catalogPath.size()+1,catalogPath+'/')!=0,"Summary output must be outside every immutable catalog");
        const auto expected=BuildScanPointBinding(scanPath,i,catalogPath);
        if (actual!=expected) throw std::runtime_error("Scan binding is missing, stale, or does not match its plan and completed catalog: "+filename);
        const std::string runKey=String(binding.At("experiment"))+"/"+std::to_string(Unsigned(binding.At("run_number")));
        Require(runs.insert(runKey).second && sessions.insert(String(binding.At("session_id"))).second,
            "Scan points must have distinct Run numbers and receiver sessions");
        if (i) rows+=',';
        rows+="{\"index\":"+std::to_string(i)+",\"point_artifact\":"+EncodeJsonString(String(binding.At("point_artifact")))+
            ",\"configuration_sha256\":"+EncodeJsonString(String(binding.At("configuration_sha256")))+
            ",\"artifact_sha256\":"+EncodeJsonString(String(binding.At("artifact_sha256")))+
            ",\"experiment\":"+EncodeJsonString(String(binding.At("experiment")))+
            ",\"run_number\":"+std::to_string(Unsigned(binding.At("run_number")))+
            ",\"session_id\":"+EncodeJsonString(String(binding.At("session_id")))+
            ",\"catalog_directory\":"+EncodeJsonString(String(binding.At("catalog_directory")))+
            ",\"catalog_request_key\":"+EncodeJsonString(String(binding.At("catalog_request_key")))+
            ",\"catalog_manifest_sha256\":"+EncodeJsonString(String(binding.At("catalog_manifest_sha256")))+
            ",\"catalog_receipt_sha256\":"+EncodeJsonString(String(binding.At("catalog_receipt_sha256")))+
            ",\"hardware_loaded_verified\":false}";
    }
    rows+="]";
    Require(ReadFile(scanPath+"/scan-manifest.json",1024*1024)==manifestBytes,"Scan plan changed while building index");
    for (unsigned int i=0;i<points.items.size();++i) {
        const std::string filename="point-"+(i<100 ? (i<10 ? std::string("00") : std::string("0")) : std::string())+std::to_string(i)+".json";
        const auto bindingBytes=ReadFile(bindingPath+'/'+filename,65536);
        const auto binding=ParseStrictJson(bindingBytes);
        if (bindingBytes!=BuildScanPointBinding(scanPath,i,String(binding.At("catalog_directory"))))
            throw std::runtime_error("Scan binding changed while building index: "+filename);
    }
    const std::string report="{\"schema_version\":1,\"kind\":\"getdaq.scan-run-index\",\"scan_kind\":"+
        EncodeJsonString(String(manifest.At("kind")))+",\"target_cobo\":"+EncodeJsonString(String(manifest.At("target_cobo")))+
        ",\"scan_manifest_sha256\":"+EncodeJsonString(ConfigurationSha256(manifestBytes))+
        ",\"complete\":true,\"hardware_loaded_verified\":false,\"points\":"+rows+"}\n";
    const auto slash=outputPath.find_last_of('/'); Require(slash>0 && slash+1<outputPath.size(),"Summary output must be inside an existing directory");
    struct stat existing; Require(::lstat(outputPath.c_str(),&existing) && errno==ENOENT,"Summary output already exists; preserve it");
    const auto pending=outputPath+".pending"; WriteNew(pending,report);
    if (::link(pending.c_str(),outputPath.c_str())) throw std::runtime_error("Cannot exclusively publish scan index");
    Fd parentFd(::open(outputPath.substr(0,slash).c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));
    if (parentFd.value<0 || ::fsync(parentFd.value)) throw std::runtime_error("Cannot sync published scan index");
    if (::unlink(pending.c_str()) || ::fsync(parentFd.value)) throw std::runtime_error("Scan index published but pending-link cleanup/sync failed");
    scan.Check(); bindings.Check();
    return report;
}


} // namespace GetDAQ
