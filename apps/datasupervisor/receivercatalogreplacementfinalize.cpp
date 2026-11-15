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

std::string FinalizeReceiverCatalogReplacement(const std::string &target,const std::string &operationId) {
    Path(target); SupervisorOperationId(operationId);
    const auto receipt=InspectReceiverCatalog(target);
    const auto request=DecodeReceiverCatalogRequest(ReadFile(target+"/request.json",1024*1024));
    const auto receiptSha=ConfigurationSha256(receipt);
    const auto manifestBytes=ReadFile(target+"/manifest.json",16*1024*1024);
    const auto manifest=CatalogJson(manifestBytes);
    Require(request.operationId==operationId && Unsigned(manifest.At("schema"))==2 && String(manifest.At("publication_target"))==target,
        "Replacement finalization target differs from the published manifest");
    const auto backup=String(manifest.At("publication_backup")); Path(backup);
    const auto journalPath=ReplacementTransactionPath(target,operationId);
    Directory journal(journalPath);
    Fd lock(::openat(journal.Descriptor(),"lock",O_RDWR|O_NOFOLLOW|O_CLOEXEC)); struct stat lockInfo;
    Require(lock.value>=0 && !::fstat(lock.value,&lockInfo) && S_ISREG(lockInfo.st_mode) && lockInfo.st_uid==::geteuid() &&
        (lockInfo.st_mode&0777)==0600 && lockInfo.st_nlink==1 && !::flock(lock.value,LOCK_EX|LOCK_NB),
        "Replacement transaction is busy or its finalization lock is unsafe");
    const auto transactionBytes=ReadFile(journalPath+"/intent.json",65536); const auto transaction=ParseStrictJson(transactionBytes);
    ExactSupervisorFields(transaction,{"schema","kind","operation_id","request_key","target","backup","candidate",
        "old_receipt_sha256","new_receipt_sha256","candidate_receipt_sha256"});
    const auto candidateSha=String(transaction.At("candidate_receipt_sha256"));
    const auto oldSha=String(transaction.At("old_receipt_sha256")); Digest(candidateSha); Digest(oldSha);
    Require(Unsigned(transaction.At("schema"))==1 && String(transaction.At("kind"))=="getdaq.catalog-replacement-intent" &&
        String(transaction.At("operation_id"))==operationId && String(transaction.At("target"))==target &&
        String(transaction.At("backup"))==backup && String(transaction.At("request_key"))==ReceiverCatalogRequestKey(request) &&
        receiptSha==candidateSha && MarkerPresent(journal,"old-preserved.json",
            "{\"phase\":\"old-preserved\",\"operation_id\":"+EncodeJsonString(operationId)+",\"receipt_sha256\":"+EncodeJsonString(oldSha)+"}\n") &&
        MarkerPresent(journal,"candidate-published.json",
            "{\"phase\":\"candidate-published\",\"operation_id\":"+EncodeJsonString(operationId)+",\"receipt_sha256\":"+EncodeJsonString(candidateSha)+"}\n") &&
        MarkerPresent(journal,"candidate-verified.json",
            "{\"phase\":\"candidate-verified\",\"operation_id\":"+EncodeJsonString(operationId)+",\"receipt_sha256\":"+EncodeJsonString(candidateSha)+"}\n"),
        "Replacement publication is not fully verified or its finalization identity differs");

    const auto cleanupPath=journalPath+"/cleanup-intent.json";
    if (!PathPresent(cleanupPath)) {
        Require(ConfigurationSha256(InspectCatalogAt(backup,target))==oldSha,
            "Original catalog backup changed before finalization; preserve all raw files");
        const auto oldManifest=CatalogJson(ReadFile(backup+"/manifest.json",16*1024*1024));
        JsonLimits limits; limits.stringBytes=1024*1024; limits.arrayItems=200000;
        const auto newManifest=ParseStrictJson(manifestBytes,limits);
        std::map<std::string,const JsonValue *> newSources;
        for (const auto &source:newManifest.At("sources").items)
            Require(newSources.emplace(String(source.At("id")),&source).second,"Published catalog repeats a source during finalization");
        std::string files="["; std::size_t count=0; std::uint64_t bytes=0;
        for (const auto &source:oldManifest.At("sources").items) {
            const auto id=String(source.At("id")); const auto found=newSources.find(id);
            if (found==newSources.end()) continue;
            const auto &newSource=*found->second;
            std::map<std::string,const JsonValue *> current;
            for (const auto &file:newSource.At("files").items)
                Require(current.emplace(String(file.At("name")),&file).second,"Published catalog repeats a current raw name");
            const auto oldStored=String(source.At("stored_directory"));
            const auto oldSession=String(source.At("original_directory"));
            const auto add=[&](const JsonValue &file,bool hasSessionLink) {
                const auto name=String(file.At("name")); const auto replacement=current.find(name);
                if (replacement==current.end()) return;
                const auto nbytes=Unsigned(file.At("bytes"),true),device=Unsigned(file.At("device"),true),inode=Unsigned(file.At("inode"),true);
                const auto newDevice=Unsigned(replacement->second->At("device"),true),newInode=Unsigned(replacement->second->At("inode"),true);
                struct stat oldCatalogFile,newCatalogFile,sessionFile;
                Require(!::lstat((backup+'/'+oldStored+'/'+name).c_str(),&oldCatalogFile) && S_ISREG(oldCatalogFile.st_mode) &&
                    oldCatalogFile.st_size>=0 && std::uint64_t(oldCatalogFile.st_dev)==device && std::uint64_t(oldCatalogFile.st_ino)==inode &&
                    std::uint64_t(oldCatalogFile.st_size)==nbytes &&
                    !::lstat((target+'/'+String(newSource.At("stored_directory"))+'/'+name).c_str(),&newCatalogFile) && S_ISREG(newCatalogFile.st_mode) &&
                    std::uint64_t(newCatalogFile.st_dev)==newDevice && std::uint64_t(newCatalogFile.st_ino)==newInode &&
                    std::uint64_t(newCatalogFile.st_size)==Unsigned(replacement->second->At("bytes"),true),
                    "Same-name replacement file identity changed before finalization");
                if (hasSessionLink) {
                    Require(!::lstat((oldSession+'/'+name).c_str(),&sessionFile) && S_ISREG(sessionFile.st_mode) &&
                        sessionFile.st_dev==oldCatalogFile.st_dev && sessionFile.st_ino==oldCatalogFile.st_ino && sessionFile.st_size==oldCatalogFile.st_size,
                        "Superseded session raw is no longer the catalog-bound inode");
                }
                if (bytes>UINT64_MAX-nbytes) throw std::runtime_error("Superseded raw byte total overflows");
                bytes+=nbytes; ++count;
                if (files.size()>1) files+=',';
                files+="{\"source_id\":"+EncodeJsonString(id)+",\"name\":"+EncodeJsonString(name)+
                    ",\"bytes\":"+EncodeJsonString(std::to_string(nbytes))+",\"device\":"+EncodeJsonString(std::to_string(device))+
                    ",\"inode\":"+EncodeJsonString(std::to_string(inode))+",\"old_stored_directory\":"+EncodeJsonString(oldStored)+
                    ",\"old_session_directory\":"+EncodeJsonString(hasSessionLink ? oldSession : std::string())+
                    ",\"new_stored_directory\":"+EncodeJsonString(String(newSource.At("stored_directory")))+
                    ",\"new_device\":"+EncodeJsonString(std::to_string(newDevice))+",\"new_inode\":"+EncodeJsonString(std::to_string(newInode))+"}";
            };
            for (const auto &file:source.At("files").items) add(file,true);
            if (source.members.count("retained_files"))
                for (const auto &file:source.At("retained_files").items) add(file,false);
        }
        files+=']';
        const auto cleanup="{\"schema\":1,\"kind\":\"getdaq.catalog-replacement-cleanup-intent\",\"operation_id\":"+EncodeJsonString(operationId)+
            ",\"target\":"+EncodeJsonString(target)+",\"backup\":"+EncodeJsonString(backup)+
            ",\"old_receipt_sha256\":"+EncodeJsonString(oldSha)+",\"published_receipt_sha256\":"+EncodeJsonString(candidateSha)+
            ",\"files\":"+files+"}\n";
        Require(cleanup.size()<=16*1024*1024,"Replacement cleanup intent exceeds metadata budget");
        WriteNew(cleanupPath,cleanup); SyncDirectory(journal);
    }

    const auto cleanupBytes=ReadFile(cleanupPath,16*1024*1024); const auto cleanup=ParseStrictJson(cleanupBytes,JsonLimits());
    ExactSupervisorFields(cleanup,{"schema","kind","operation_id","target","backup","old_receipt_sha256","published_receipt_sha256","files"});
    Require(Unsigned(cleanup.At("schema"))==1 && String(cleanup.At("kind"))=="getdaq.catalog-replacement-cleanup-intent" &&
        String(cleanup.At("operation_id"))==operationId && String(cleanup.At("target"))==target && String(cleanup.At("backup"))==backup &&
        String(cleanup.At("old_receipt_sha256"))==oldSha && String(cleanup.At("published_receipt_sha256"))==candidateSha &&
        ConfigurationSha256(InspectReceiverCatalog(target))==candidateSha && cleanup.At("files").type==JsonValue::Array,
        "Replacement cleanup intent no longer matches the committed catalog");
    std::uint64_t removedBytes=0;
    for (const auto &file:cleanup.At("files").items) {
        ExactSupervisorFields(file,{"source_id","name","bytes","device","inode","old_stored_directory","old_session_directory","new_stored_directory","new_device","new_inode"});
        const auto name=String(file.At("name")),source=String(file.At("source_id"));
        const auto rawBytes=Unsigned(file.At("bytes"),true),device=Unsigned(file.At("device"),true),inode=Unsigned(file.At("inode"),true);
        const auto rawPath=target+'/'+String(file.At("new_stored_directory"))+'/'+name; struct stat current;
        Require(!::lstat(rawPath.c_str(),&current) && S_ISREG(current.st_mode) && std::uint64_t(current.st_dev)==Unsigned(file.At("new_device"),true) &&
            std::uint64_t(current.st_ino)==Unsigned(file.At("new_inode"),true) && std::uint64_t(current.st_size)==rawBytes,
            "Published replacement raw changed during old-name cleanup");
        Directory oldDir(backup+'/'+String(file.At("old_stored_directory")));
        std::unique_ptr<Directory> session; std::unique_ptr<Fd> sessionLease;
        const auto sessionPath=String(file.At("old_session_directory"));
        if (!sessionPath.empty()) {
            session.reset(new Directory(sessionPath));
            struct stat directoryInfo; Require(!::fstat(session->Descriptor(),&directoryInfo) && directoryInfo.st_uid==::geteuid() && !(directoryInfo.st_mode&0022),
                "Superseded receiver session directory is not privately owned");
            sessionLease.reset(new Fd(::openat(session->Descriptor(),".getdaq-receiver.lock",O_RDWR|O_NOFOLLOW|O_CLOEXEC)));
            struct stat leaseInfo;
            Require(sessionLease->value>=0 && !::fstat(sessionLease->value,&leaseInfo) && S_ISREG(leaseInfo.st_mode) && leaseInfo.st_uid==::geteuid() &&
                !(leaseInfo.st_mode&0022) && !::flock(sessionLease->value,LOCK_EX|LOCK_NB),
                "Superseded receiver session is active or its lock is unsafe");
        }
        const auto inspect=[&](const Directory &directory,bool &present,struct stat &info) {
            if (!::fstatat(directory.Descriptor(),name.c_str(),&info,AT_SYMLINK_NOFOLLOW)) {
                present=true;
                Require(S_ISREG(info.st_mode) && info.st_uid==::geteuid() && info.st_size>=0 &&
                    std::uint64_t(info.st_dev)==device && std::uint64_t(info.st_ino)==inode && std::uint64_t(info.st_size)==rawBytes,
                    "Superseded raw path was replaced or changed; preserve it for inspection");
                return;
            }
            Require(errno==ENOENT,"Cannot inspect superseded raw during finalization"); present=false;
        };
        bool inBackup=false,inSession=false; struct stat backupInfo={},sessionInfo={}; inspect(oldDir,inBackup,backupInfo);
        if (!sessionPath.empty()) inspect(*session,inSession,sessionInfo);
        if (inBackup && inSession)
            Require(backupInfo.st_dev==sessionInfo.st_dev && backupInfo.st_ino==sessionInfo.st_ino && backupInfo.st_nlink>=2,
                "Old catalog/session links no longer identify the same superseded raw");
        else if (inBackup) Require(backupInfo.st_nlink>=1,"Old backup raw has an invalid link count");
        else if (inSession) Require(sessionInfo.st_nlink>=1,"Old session raw has an invalid link count");
        if (inBackup) { Require(!::unlinkat(oldDir.Descriptor(),name.c_str(),0),"Cannot remove superseded raw from old catalog backup"); SyncDirectory(oldDir); }
        if (inSession) { Require(!::unlinkat(session->Descriptor(),name.c_str(),0),"Cannot remove superseded raw from old receiver session"); SyncDirectory(*session); }
        bool remains=false; struct stat after={}; inspect(oldDir,remains,after); Require(!remains,"Superseded raw remains in old catalog backup");
        if (!sessionPath.empty()) { inspect(*session,remains,after); Require(!remains,"Superseded raw remains in old receiver session"); }
        const auto fileSha=ConfigurationSha256("getdaq.catalog-replacement-cleanup-file:1\n"+source+'\n'+name+'\n'+std::to_string(device)+'\n'+std::to_string(inode));
        EnsureMarker(journal,"cleanup-"+fileSha+".done","{\"schema\":1,\"operation_id\":"+EncodeJsonString(operationId)+
            ",\"source_id\":"+EncodeJsonString(source)+",\"name\":"+EncodeJsonString(name)+",\"device\":"+EncodeJsonString(std::to_string(device))+
            ",\"inode\":"+EncodeJsonString(std::to_string(inode))+"}\n");
        if (removedBytes<=UINT64_MAX-rawBytes) removedBytes+=rawBytes;
        else throw std::runtime_error("Finalized raw byte total overflows");
    }
    const auto complete="{\"schema\":1,\"kind\":\"getdaq.catalog-replacement-cleanup-complete\",\"operation_id\":"+EncodeJsonString(operationId)+
        ",\"old_receipt_sha256\":"+EncodeJsonString(oldSha)+",\"published_receipt_sha256\":"+EncodeJsonString(candidateSha)+
        ",\"superseded_files\":"+std::to_string(cleanup.At("files").items.size())+",\"removed_link_bytes\":"+EncodeJsonString(std::to_string(removedBytes))+
        ",\"raw_payload_bytes_read\":\"0\"}\n";
    EnsureMarker(journal,"cleanup-complete.json",complete); SyncDirectory(journal);
    return complete;
}


} // namespace GetDAQ
