#include "receivercatalog.h"
#include "receivercataloginternal.h"
#include "configurationsha256.h"
#include "planio.h"
#include "supervisorio.h"
#include "systemexperimentconfiguration.h"
#include <algorithm>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
using namespace SupervisorIO;
using namespace ReceiverCatalogInternal;

std::set<std::string> ReceiverCatalogInternal::RawNames(const Directory &directory) {
    const int scan=::openat(directory.Descriptor(),".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    DIR *entries=scan<0 ? nullptr : ::fdopendir(scan);
    if (!entries) { if (scan>=0) ::close(scan); throw std::runtime_error("Cannot enumerate catalog source"); }
    std::set<std::string> result; int error=0;
    for (;;) {
        errno=0; const auto entry=::readdir(entries);
        if (!entry) { error=errno; break; }
        const std::string name=entry->d_name;
        if (name.size()>=5 && name.substr(name.size()-5)==".graw") result.insert(name);
    }
    ::closedir(entries); Require(!error,"Catalog source enumeration failed"); return result;
}
namespace ReceiverCatalogInternal {
bool ReceiverBoundary(const JsonValue &seal,const ReceiverSourcePlan &plan,const ManagedReceiverStatus &expected) {
    const bool unused=seal.members.count("source_unused_confirmed") && Boolean(seal.At("source_unused_confirmed"));
    const auto storage=seal.members.find("storage_mode");
    if ((storage==seal.members.end() ? "frame-storage" : String(storage->second))!=plan.storageMode) return false;
    if (unused!=expected.sourceUnusedConfirmed) return false;
    const auto connections=Unsigned(seal.At("connections"));
    if (plan.inputProtocol=="ICE") return !unused && expected.sourceEndConfirmed && connections==1 &&
        !Boolean(seal.At("sender_eof_confirmed")) && Boolean(seal.At("sender_end_confirmed")) && String(seal.At("kind"))=="getdaq.ice-receiver-seal" &&
        String(seal.At("input_protocol"))=="ICE" && String(seal.At("completion_basis"))=="ordered-same-connection-finishStream";
    if (expected.sourceEndConfirmed) return false;
    if (!unused) return Boolean(seal.At("sender_eof_confirmed")) && connections>=1 && (plan.inputProtocol=="FDT" || connections==1);
    return plan.inputProtocol=="FDT" && !Boolean(seal.At("sender_eof_confirmed")) && !connections &&
        !expected.receivedBytes && !expected.frames && !expected.files && Unsigned(seal.At("received_bytes"),true)==0 &&
        seal.At("files").type==JsonValue::Array && seal.At("files").items.empty();
}
void CheckSealFields(const JsonValue &seal,const ReceiverSourcePlan &plan) {
    std::set<std::string> fields={"schema","complete","control_protocol","pid","session_id","sender_id","configuration_sha256","artifact_sha256","output_directory","connections","sender_eof_confirmed","durable","received_bytes","files"};
    if (seal.members.count("source_unused_confirmed")) fields.insert("source_unused_confirmed");
    if (seal.members.count("storage_mode")) fields.insert("storage_mode");
    if (plan.inputProtocol=="ICE") for (const auto &field:{"kind","input_protocol","sender_end_confirmed","completion_basis"}) fields.insert(field);
    ExactSupervisorFields(seal,fields);
}
} // namespace ReceiverCatalogInternal
namespace {
std::string InspectReceiverCatalogAt(const std::string &path,const std::string &logicalPath) {
    Path(path); Path(logicalPath); Directory root(path); const auto request=DecodeReceiverCatalogRequest(ReadFile(path+"/request.json",1024*1024));
    const auto receiptBytes=ReadFile(path+"/catalog-complete.json",65536),manifestBytes=ReadFile(path+"/manifest.json",16*1024*1024);
    const auto receipt=ParseStrictJson(receiptBytes),manifest=CatalogJson(manifestBytes); const auto key=ReceiverCatalogRequestKey(request);
    const auto manifestSchema=Unsigned(manifest.At("schema")); const bool replacement=manifestSchema==2;
    ExactSupervisorFields(receipt,{"schema","kind","complete","request_key","manifest_sha256","operation_id","directory","payload_method","raw_payload_bytes_read","linked_files"});
    Require(Unsigned(receipt.At("schema"))==1 && String(receipt.At("kind"))=="getdaq.receiver-catalog-complete" && Boolean(receipt.At("complete")) &&
        String(receipt.At("request_key"))==key && String(receipt.At("manifest_sha256"))==ConfigurationSha256(manifestBytes) && String(receipt.At("operation_id"))==request.operationId &&
        String(receipt.At("payload_method"))=="hardlink" && Unsigned(receipt.At("raw_payload_bytes_read"),true)==0,"Catalog completion scope/manifest differs");
    if (replacement) ExactSupervisorFields(manifest,{"schema","kind","request_key","plan_sha256","session_id","payload_method","raw_payload_bytes_read","publication_root","publication_target","publication_backup","staged_for_replacement","sources"});
    else ExactSupervisorFields(manifest,{"schema","kind","request_key","plan_sha256","session_id","payload_method","raw_payload_bytes_read","sources"});
    if (replacement) {
        const auto publicationRoot=String(manifest.At("publication_root")); Path(publicationRoot);
        const auto publicationTarget=String(manifest.At("publication_target")); Path(publicationTarget);
        const auto publicationBackup=String(manifest.At("publication_backup")); Path(publicationBackup);
        ReceiverCatalogOptions publication=request; publication.receiver.catalogRoot=publicationRoot;
        Require(publicationTarget==CatalogPublicationTarget(publication) &&
            publicationBackup==CatalogReplacementBackupPath(publicationTarget,request.operationId) &&
            String(receipt.At("directory"))==publicationTarget &&
            (!Boolean(manifest.At("staged_for_replacement")) ||
                (request.stagedForReplacement && logicalPath==CatalogReplacementStagePath(publicationTarget,request.operationId))),
            "Replacement catalog logical publication path differs from its Run identity");
    } else Require(String(receipt.At("directory"))==logicalPath,"Catalog completion path differs");
    Require((manifestSchema==1 || replacement) && String(manifest.At("kind"))=="getdaq.receiver-catalog-manifest" && String(manifest.At("request_key"))==key &&
        String(manifest.At("plan_sha256"))==request.stopped.planSha256 && String(manifest.At("session_id"))==request.stopped.sessionId &&
        String(manifest.At("payload_method"))=="hardlink" && Unsigned(manifest.At("raw_payload_bytes_read"),true)==0 && manifest.At("sources").type==JsonValue::Array &&
        manifest.At("sources").items.size()>=request.stopped.receivers.size() && (replacement || manifest.At("sources").items.size()==request.stopped.receivers.size()),"Catalog manifest session/source coverage differs");
    std::map<std::string,const ManagedReceiverStatus *> expectedReceivers;
    for (const auto &receiver:request.stopped.receivers) expectedReceivers.emplace(receiver.id,&receiver);
    std::uint64_t files=0; std::set<std::string> ids,seenCurrent;
    for (std::size_t i=0;i<manifest.At("sources").items.size();++i) {
        const auto &source=manifest.At("sources").items[i];
        if (replacement) ExactSupervisorFields(source,{"id","stored_directory","original_directory","seal_sha256","frames","files","retained_files"});
        else ExactSupervisorFields(source,{"id","stored_directory","original_directory","seal_sha256","frames","files"});
        const auto id=String(source.At("id")); const auto expectedIt=expectedReceivers.find(id);
        const bool current=expectedIt!=expectedReceivers.end();
        Require(ids.insert(id).second && (current || replacement),"Catalog source is not bound to this Run request");
        const auto stored=String(source.At("stored_directory"));
        Require(stored=="source_"+std::to_string(i) && source.At("files").type==JsonValue::Array,
            "Catalog source storage mapping differs");
        const ManagedReceiverStatus *expected=current ? expectedIt->second : nullptr;
        const auto sourceFrames=Unsigned(source.At("frames"),true);
        if (current) {
            Require(seenCurrent.insert(id).second && String(source.At("original_directory"))==expected->outputDirectory &&
                sourceFrames==expected->frames && source.At("files").items.size()==expected->files,
                "Catalog current source mapping/counters differ");
        } else {
            Require(source.At("files").items.empty(),"Retained-only source cannot claim current-session files");
        }
        Directory directory(path+'/'+stored); const auto seal=ReadFile(directory.Path()+"/receiver-seal.json",16*1024*1024);
        Require(ConfigurationSha256(seal)==String(source.At("seal_sha256")),"Catalog copied receiver seal changed");
        if (current) {
            const auto parsedSeal=CatalogJson(seal);
            const auto plan=std::find_if(request.receiver.sources.begin(),request.receiver.sources.end(),[&](const ReceiverSourcePlan &p) { return p.id==expected->id; });
            Require(plan!=request.receiver.sources.end(),"Catalog copied source absent from plan");
            CheckSealFields(parsedSeal,*plan);
            Require(Unsigned(parsedSeal.At("schema"))==1 && Unsigned(parsedSeal.At("control_protocol"))==1 && Unsigned(parsedSeal.At("pid"))==std::uint64_t(expected->pid) &&
                String(parsedSeal.At("configuration_sha256"))==request.receiver.configurationSha256 && String(parsedSeal.At("artifact_sha256"))==request.receiver.artifactSha256 &&
                String(parsedSeal.At("output_directory"))==expected->outputDirectory && String(parsedSeal.At("session_id"))==expected->sessionId && String(parsedSeal.At("sender_id"))==expected->id && Boolean(parsedSeal.At("complete")) &&
                Boolean(parsedSeal.At("durable")) && ReceiverBoundary(parsedSeal,*plan,*expected) && Unsigned(parsedSeal.At("received_bytes"),true)==expected->receivedBytes,"Catalog copied seal scope differs");
        } else {
            Require(replacement && !source.At("retained_files").items.empty(),"Retained-only source has no retained raw files");
        }
        std::set<std::string> names; std::uint64_t currentBytes=0;
        for (const auto &file:source.At("files").items) {
            ExactSupervisorFields(file,{"name","bytes","device","inode"}); const auto name=String(file.At("name")); const auto bytes=Unsigned(file.At("bytes"),true); struct stat info;
            Require(name.size()>5 && name.size()<=255 && name.substr(name.size()-5)==".graw" && name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-:")==std::string::npos &&
                names.insert(name).second && ++files<=65536 && !::fstatat(directory.Descriptor(),name.c_str(),&info,AT_SYMLINK_NOFOLLOW) && S_ISREG(info.st_mode) &&
                info.st_size>=0 && std::uint64_t(info.st_size)==bytes && std::uint64_t(info.st_dev)==Unsigned(file.At("device"),true) &&
                std::uint64_t(info.st_ino)==Unsigned(file.At("inode"),true) && currentBytes<=UINT64_MAX-bytes,"Catalog archived file identity/size differs");
            currentBytes+=bytes;
        }
        if (current) Require(currentBytes==expected->receivedBytes,"Catalog current source bytes differ from stopped/seal proof");
        else Require(!currentBytes,"Retained-only source has current-session bytes");
        if (replacement) {
            Require(source.At("retained_files").type==JsonValue::Array,"Replacement retained file list is malformed");
            for (const auto &file:source.At("retained_files").items) {
                ExactSupervisorFields(file,{"name","bytes","device","inode","origin_catalog","origin_receipt_sha256","origin_source_id","origin_stored_directory"});
                const auto name=String(file.At("name")); const auto bytes=Unsigned(file.At("bytes"),true); const auto origin=String(file.At("origin_catalog"));
                const auto originStored=String(file.At("origin_stored_directory")); Digest(String(file.At("origin_receipt_sha256"))); Path(origin);
                struct stat info;
                Require(name.size()>5 && name.size()<=255 && name.substr(name.size()-5)==".graw" && name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-:")==std::string::npos &&
                    names.insert(name).second && ++files<=65536 && String(file.At("origin_source_id"))==id &&
                    originStored.size()>7 && originStored.compare(0,7,"source_")==0 && originStored.find_first_of("/\\")==std::string::npos &&
                    origin!=path && !::fstatat(directory.Descriptor(),name.c_str(),&info,AT_SYMLINK_NOFOLLOW) && S_ISREG(info.st_mode) && info.st_size>=0 &&
                    std::uint64_t(info.st_size)==bytes && std::uint64_t(info.st_dev)==Unsigned(file.At("device"),true) &&
                    std::uint64_t(info.st_ino)==Unsigned(file.At("inode"),true),"Catalog retained file identity/provenance differs");
            }
        }
        Require(RawNames(directory)==names,"Catalog archived file set differs"); directory.Check();
    }
    Require(seenCurrent.size()==request.stopped.receivers.size(),"Catalog omits a current-session receiver source");
    Require(files==Unsigned(receipt.At("linked_files"),true),"Catalog final linked file count differs"); root.Check(); return receiptBytes;
}
} // namespace

std::string InspectReceiverCatalog(const std::string &path) { return InspectReceiverCatalogAt(path,path); }
std::string ReceiverCatalogInternal::InspectCatalogAt(const std::string &path,const std::string &logicalPath) {
    return InspectReceiverCatalogAt(path,logicalPath);
}
} // namespace GetDAQ
