#include "receivercatalog.h"
#include "receivercataloginternal.h"
#include "receivercatalogreplacement_p.h"
#include "controlprotocol.h"
#include "configurationsha256.h"
#include "planio.h"
#include "supervisorio.h"
#include "systemexperimentconfiguration.h"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__linux__)
#include <linux/fs.h>
#include <sys/syscall.h>
#elif defined(__APPLE__)
#include <stdio.h>
#endif

namespace GetDAQ {
using namespace SupervisorIO;
using namespace ReceiverCatalogInternal;
using namespace ReceiverCatalogReplacementInternal;

std::string PrepareReceiverCatalogReplacement(const std::string &oldPath,const std::string &newPath,
                                               const std::string &candidatePath) {
    Path(oldPath); Path(newPath); Path(candidatePath);
    const auto overlaps=[](const std::string &a,const std::string &b) {
        return a==b || a.compare(0,b.size()+1,b+'/')==0 || b.compare(0,a.size()+1,a+'/')==0;
    };
    Require(!overlaps(candidatePath,oldPath) && !overlaps(candidatePath,newPath),
        "Replacement candidate must be separate from both input catalogs");
    struct stat existing; Require(::lstat(candidatePath.c_str(),&existing) && errno==ENOENT,
        "Replacement candidate already exists; preserve it for inspection");
    const auto oldReceipt=InspectReceiverCatalog(oldPath),newReceipt=InspectReceiverCatalog(newPath);
    const auto plan=PlanReceiverCatalogReplacement(oldPath,newPath);
    Directory oldCatalogRoot(oldPath),newCatalogRoot(newPath);
    const auto slash=candidatePath.find_last_of('/'); Require(slash!=std::string::npos && slash+1<candidatePath.size(),
        "Replacement candidate must be a child of an existing directory");
    const auto parentPath=slash ? candidatePath.substr(0,slash) : std::string("/");
    Directory candidateParent(parentPath);
    const auto newRequestBytes=ReadFile(newPath+"/request.json",1024*1024);
    const auto newManifestBytes=ReadFile(newPath+"/manifest.json",16*1024*1024);
    const auto oldManifestBytes=ReadFile(oldPath+"/manifest.json",16*1024*1024);
    const auto oldReceiptValue=ParseStrictJson(oldReceipt),newReceiptValue=ParseStrictJson(newReceipt);
    Require(ConfigurationSha256(oldManifestBytes)==String(oldReceiptValue.At("manifest_sha256")) &&
        ConfigurationSha256(newManifestBytes)==String(newReceiptValue.At("manifest_sha256")) &&
        DecodeReceiverCatalogRequest(newRequestBytes).operationId==String(newReceiptValue.At("operation_id")) &&
        ReceiverCatalogRequestKey(DecodeReceiverCatalogRequest(newRequestBytes))==String(newReceiptValue.At("request_key")),
        "Replacement input metadata changed before candidate preparation");
    const auto newManifest=CatalogJson(newManifestBytes),oldManifest=CatalogJson(oldManifestBytes);
    const auto sourceMap=[](const JsonValue &manifest) {
        std::map<std::string,JsonValue> result;
        for (const auto &source:manifest.At("sources").items)
            Require(result.emplace(String(source.At("id")),source).second,"Replacement input repeats a source ID");
        return result;
    };
    const auto newSources=sourceMap(newManifest),oldSources=sourceMap(oldManifest);
    std::set<std::string> sourceIds;
    for (const auto &source:newSources) sourceIds.insert(source.first);
    for (const auto &source:oldSources) sourceIds.insert(source.first);

    CreateDirectory(candidatePath); Directory candidate(candidatePath);
    struct stat oldRootInfo,newRootInfo,candidateParentInfo,candidateInfo;
    Require(!::fstat(oldCatalogRoot.Descriptor(),&oldRootInfo) && !::fstat(newCatalogRoot.Descriptor(),&newRootInfo) &&
        !::fstat(candidateParent.Descriptor(),&candidateParentInfo) && !::fstat(candidate.Descriptor(),&candidateInfo) &&
        oldRootInfo.st_dev==newRootInfo.st_dev && oldRootInfo.st_dev==candidateParentInfo.st_dev && oldRootInfo.st_dev==candidateInfo.st_dev,
        "Old/new catalogs and replacement candidate parent must share a filesystem");
    SyncDirectory(candidateParent);
    const auto request=DecodeReceiverCatalogRequest(newRequestBytes); const auto key=ReceiverCatalogRequestKey(request);
    WriteNew(candidatePath+"/request.json",newRequestBytes);
    const auto oldRequest=DecodeReceiverCatalogRequest(ReadFile(oldPath+"/request.json",1024*1024));
    const auto publicationTarget=CatalogPublicationTarget(oldRequest);
    const auto publicationBackup=CatalogReplacementBackupPath(publicationTarget,request.operationId);
    Require(oldPath==publicationTarget,"Old catalog is not at its configured Run publication path");
    struct stat backupInfo; Require(::lstat(publicationBackup.c_str(),&backupInfo) && errno==ENOENT,
        "Replacement backup path already exists; preserve it for inspection");
    Require(candidatePath!=publicationBackup,"Replacement candidate and backup paths must be distinct");
    std::string manifest="{\"schema\":2,\"kind\":\"getdaq.receiver-catalog-manifest\",\"request_key\":"+EncodeJsonString(key)+
        ",\"plan_sha256\":"+EncodeJsonString(request.stopped.planSha256)+",\"session_id\":"+EncodeJsonString(request.stopped.sessionId)+
        ",\"payload_method\":\"hardlink\",\"raw_payload_bytes_read\":\"0\",\"publication_root\":"+
        EncodeJsonString(oldRequest.receiver.catalogRoot)+",\"publication_target\":"+EncodeJsonString(publicationTarget)+
        ",\"publication_backup\":"+EncodeJsonString(publicationBackup)+",\"staged_for_replacement\":false,\"sources\":[";
    std::uint64_t linked=0; std::size_t sourceIndex=0;
    for (const auto &sourceId:sourceIds) {
        const auto newIt=newSources.find(sourceId),oldIt=oldSources.find(sourceId);
        std::vector<const CatalogReplacementEntry *> currentEntries,retainedEntries;
        for (const auto &entry:plan) if (entry.sourceId==sourceId)
            (entry.fromNewRun && entry.currentSessionFile ? currentEntries : retainedEntries).push_back(&entry);
        const bool current=newIt!=newSources.end();
        if (!current && retainedEntries.empty()) continue;
        const auto &baseCatalog=current ? newPath : oldPath;
        const auto &baseSource=current ? newIt->second : oldIt->second;
        const auto stored=String(baseSource.At("stored_directory"));
        const auto seal=ReadFile(baseCatalog+'/'+stored+"/receiver-seal.json",16*1024*1024);
        Require(ConfigurationSha256(seal)==String(baseSource.At("seal_sha256")),"Replacement source seal changed");
        const auto destinationName="source_"+std::to_string(sourceIndex++);
        CreateDirectory(candidatePath+'/'+destinationName); Directory destination(candidatePath+'/'+destinationName);
        WriteNew(destination.Path()+"/receiver-seal.json",seal);
        std::string currentJson="[";
        const auto linkSelected=[&](const CatalogReplacementEntry &entry) {
            Require(entry.sourceId==sourceId && entry.bytes<=std::uint64_t(std::numeric_limits<off_t>::max()),
                "Replacement plan entry scope/size is invalid");
            Directory originCatalog(entry.sourceCatalogDirectory);
            Directory origin(originCatalog.Path()+'/'+entry.sourceStoredDirectory);
            struct stat before,after,sourceAfter; const auto &name=entry.fileName;
            Require(!::fstatat(origin.Descriptor(),name.c_str(),&before,AT_SYMLINK_NOFOLLOW) && S_ISREG(before.st_mode) &&
                before.st_size>=0 && std::uint64_t(before.st_size)==entry.bytes && std::uint64_t(before.st_dev)==entry.device &&
                std::uint64_t(before.st_ino)==entry.inode,"Replacement source raw identity changed before linking");
            struct stat rootInfo,sourceInfo;
            Require(!::fstat(candidate.Descriptor(),&rootInfo) && !::fstat(origin.Descriptor(),&sourceInfo) &&
                rootInfo.st_dev==sourceInfo.st_dev,"Replacement requires old/new catalogs and candidate on one filesystem");
            Require(!::linkat(origin.Descriptor(),name.c_str(),destination.Descriptor(),name.c_str(),0),
                "Cannot exclusively link replacement raw into candidate");
            Require(!::fstatat(destination.Descriptor(),name.c_str(),&after,AT_SYMLINK_NOFOLLOW) &&
                !::fstatat(origin.Descriptor(),name.c_str(),&sourceAfter,AT_SYMLINK_NOFOLLOW) && S_ISREG(after.st_mode) &&
                std::uint64_t(after.st_dev)==entry.device && std::uint64_t(after.st_ino)==entry.inode &&
                std::uint64_t(after.st_size)==entry.bytes && Same(before,sourceAfter),
                "Replacement hardlink identity differs from its verified source");
            ++linked; Require(linked<=65536,"Replacement candidate exceeds the catalog file limit");
            origin.Check(); destination.Check();
        };
        for (const auto *entry:currentEntries) {
            linkSelected(*entry);
            if (currentJson.size()>1) currentJson+=',';
            currentJson+="{\"name\":"+EncodeJsonString(entry->fileName)+",\"bytes\":"+EncodeJsonString(std::to_string(entry->bytes))+
                ",\"device\":"+EncodeJsonString(std::to_string(entry->device))+",\"inode\":"+EncodeJsonString(std::to_string(entry->inode))+"}";
        }
        currentJson+=']';
        std::string retainedJson="[";
        for (const auto *entry:retainedEntries) {
            linkSelected(*entry);
            if (retainedJson.size()>1) retainedJson+=',';
            retainedJson+="{\"name\":"+EncodeJsonString(entry->fileName)+",\"bytes\":"+EncodeJsonString(std::to_string(entry->bytes))+
                ",\"device\":"+EncodeJsonString(std::to_string(entry->device))+",\"inode\":"+EncodeJsonString(std::to_string(entry->inode))+
                ",\"origin_catalog\":"+EncodeJsonString(entry->sourceCatalogDirectory==oldPath ? publicationBackup : entry->sourceCatalogDirectory)+",\"origin_receipt_sha256\":"+
                EncodeJsonString(entry->sourceCatalogReceiptSha256)+",\"origin_source_id\":"+EncodeJsonString(entry->sourceId)+
                ",\"origin_stored_directory\":"+EncodeJsonString(entry->sourceStoredDirectory)+"}";
        }
        retainedJson+=']'; SyncDirectory(destination);
        if (manifest.back()!='[') manifest+=',';
        manifest+="{\"id\":"+EncodeJsonString(sourceId)+",\"stored_directory\":"+EncodeJsonString(destinationName)+
            ",\"original_directory\":"+EncodeJsonString(String(baseSource.At("original_directory")))+
            ",\"seal_sha256\":"+EncodeJsonString(ConfigurationSha256(seal))+
            ",\"frames\":"+EncodeJsonString(std::to_string(Unsigned(baseSource.At("frames"),true)))+
            ",\"files\":"+currentJson+",\"retained_files\":"+retainedJson+"}";
        Require(manifest.size()<=16*1024*1024,"Replacement catalog manifest exceeds 16 MiB");
    }
    manifest+="]}\n"; Require(manifest.size()<=16*1024*1024,"Replacement catalog manifest exceeds 16 MiB");
    WriteNew(candidatePath+"/manifest.json",manifest); SyncDirectory(candidate);
    Require(InspectReceiverCatalog(oldPath)==oldReceipt && InspectReceiverCatalog(newPath)==newReceipt,
        "Replacement input catalog changed while candidate was being prepared");
    const auto completion="{\"schema\":1,\"kind\":\"getdaq.receiver-catalog-complete\",\"complete\":true,\"request_key\":"+
        EncodeJsonString(key)+",\"manifest_sha256\":"+EncodeJsonString(ConfigurationSha256(manifest))+
        ",\"operation_id\":"+EncodeJsonString(request.operationId)+",\"directory\":"+EncodeJsonString(publicationTarget)+
        ",\"payload_method\":\"hardlink\",\"raw_payload_bytes_read\":\"0\",\"linked_files\":"+
        EncodeJsonString(std::to_string(linked))+"}\n";
    WriteNew(candidatePath+"/.complete.pending",completion);
    Require(!::linkat(candidate.Descriptor(),".complete.pending",candidate.Descriptor(),"catalog-complete.json",0),
        "Cannot exclusively publish replacement candidate completion");
    SyncDirectory(candidate); Require(!::unlinkat(candidate.Descriptor(),".complete.pending",0),"Cannot remove replacement completion staging link"); SyncDirectory(candidate);
    SyncDirectory(candidateParent);
    Require(InspectReceiverCatalog(candidatePath)==completion,"Prepared replacement candidate failed independent inspection");
    return completion;
}


} // namespace GetDAQ
