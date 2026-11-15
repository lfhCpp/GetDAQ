#include "receivercatalog.h"
#include "receivercataloginternal.h"
#include "configurationsha256.h"
#include "supervisorio.h"
#include "planio.h"
#include <map>
#include <set>

namespace GetDAQ {
using namespace ReceiverCatalogInternal;
using namespace SupervisorIO;

std::vector<CatalogReplacementEntry> PlanReceiverCatalogReplacement(const std::string &oldPath,const std::string &newPath) {
    Require(oldPath!=newPath,"Replacement catalogs must be distinct revisions");
    const auto oldReceipt=InspectReceiverCatalog(oldPath);
    const auto newReceipt=InspectReceiverCatalog(newPath);
    const auto oldRequestBytes=ReadFile(oldPath+"/request.json",1024*1024);
    const auto newRequestBytes=ReadFile(newPath+"/request.json",1024*1024);
    const auto oldManifestBytes=ReadFile(oldPath+"/manifest.json",16*1024*1024);
    const auto newManifestBytes=ReadFile(newPath+"/manifest.json",16*1024*1024);
    const auto oldRequest=DecodeReceiverCatalogRequest(oldRequestBytes);
    const auto newRequest=DecodeReceiverCatalogRequest(newRequestBytes);
    Require(oldRequest.experiment==newRequest.experiment && oldRequest.runNumber==newRequest.runNumber &&
        oldRequest.receiver.deploymentId==newRequest.receiver.deploymentId,
        "Replacement catalogs must identify the same experiment/Run/deployment");
    Require(oldRequest.stopped.sessionId!=newRequest.stopped.sessionId && oldRequest.operationId!=newRequest.operationId,
        "Replacement requires a distinct receiver session and operation identity");
    typedef std::map<std::pair<std::string,std::string>,CatalogReplacementEntry> FileIdentities;
    FileIdentities oldIdentities,newIdentities;
    const auto filesBySource=[](const std::string &manifestBytes,const std::string &catalogPath,
                                const std::string &receiptBytes,FileIdentities &identities) {
        std::map<std::string,std::set<std::string>> result;
        const auto manifest=CatalogJson(manifestBytes);
        Require(manifest.At("sources").type==JsonValue::Array,"Replacement catalog sources are malformed");
        for (const auto &source:manifest.At("sources").items) {
            const auto sourceId=String(source.At("id"));
            auto inserted=result.emplace(sourceId,std::set<std::string>()).first;
            Require(source.At("files").type==JsonValue::Array,"Replacement catalog files are malformed");
            const auto addFile=[&](const JsonValue &file,bool currentSessionFile) {
                const auto name=String(file.At("name"));
                Require(inserted->second.insert(name).second,"Replacement catalog repeats a source filename");
                const auto fields=file.members.count("origin_catalog") ?
                    std::set<std::string>{"name","bytes","device","inode","origin_catalog","origin_receipt_sha256","origin_source_id","origin_stored_directory"} :
                    std::set<std::string>{"name","bytes","device","inode"};
                ExactSupervisorFields(file,fields);
                CatalogReplacementEntry identity; identity.sourceId=sourceId; identity.fileName=name;
                identity.currentSessionFile=currentSessionFile;
                identity.sourceCatalogDirectory=catalogPath; identity.sourceStoredDirectory=String(source.At("stored_directory"));
                identity.sourceCatalogReceiptSha256=ConfigurationSha256(receiptBytes);
                identity.bytes=Unsigned(file.At("bytes"),true); identity.device=Unsigned(file.At("device"),true); identity.inode=Unsigned(file.At("inode"),true);
                Require(identities.emplace(std::make_pair(sourceId,name),identity).second,"Replacement catalog repeats a source file identity");
            };
            for (const auto &file:source.At("files").items) addFile(file,true);
            if (manifest.At("schema").type==JsonValue::Number && Unsigned(manifest.At("schema"))==2) {
                Require(source.At("retained_files").type==JsonValue::Array,"Replacement catalog retained file list is malformed");
                for (const auto &file:source.At("retained_files").items) addFile(file,false);
            }
        }
        return result;
    };
    auto plan=BuildCatalogReplacementPlan(filesBySource(oldManifestBytes,oldPath,oldReceipt,oldIdentities),
        filesBySource(newManifestBytes,newPath,newReceipt,newIdentities));
    for (auto &entry:plan) {
        const bool fromNewRun=entry.fromNewRun;
        const auto &identities=fromNewRun ? newIdentities : oldIdentities;
        const auto found=identities.find(std::make_pair(entry.sourceId,entry.fileName));
        Require(found!=identities.end(),"Replacement plan has no verified source file identity");
        entry=found->second; entry.fromNewRun=fromNewRun;
    }
    Require(ReadFile(oldPath+"/request.json",1024*1024)==oldRequestBytes &&
        ReadFile(newPath+"/request.json",1024*1024)==newRequestBytes &&
        ReadFile(oldPath+"/manifest.json",16*1024*1024)==oldManifestBytes &&
        ReadFile(newPath+"/manifest.json",16*1024*1024)==newManifestBytes &&
        InspectReceiverCatalog(oldPath)==oldReceipt && InspectReceiverCatalog(newPath)==newReceipt,
        "Replacement catalog changed while the read-only plan was being built");
    return plan;
}

} // namespace GetDAQ
