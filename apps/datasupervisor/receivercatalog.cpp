#include "receivercatalog.h"
#include "receivercataloginternal.h"
#include "controlprotocol.h"
#include "supervisorio.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include "planio.h"
#include <algorithm>
#include <stdexcept>
#include <cerrno>
#include <fcntl.h>
#include <map>
#include <set>
#include <sstream>
#include <iomanip>
#include <limits>
#include <sys/file.h>
#include <unistd.h>

namespace GetDAQ {
namespace {
using namespace SupervisorIO;
using namespace ReceiverCatalogInternal;
const char *IntentName=".getdaq-catalog-intent.json";
void Sync(const Directory &directory) { directory.Check(); Require(!::fsync(directory.Descriptor()),"Cannot sync catalog directory"); directory.Check(); }
bool PathPresent(const std::string &path) {
    struct stat info;
    if (!::lstat(path.c_str(),&info)) return true;
    Require(errno==ENOENT,"Cannot inspect replacement transaction path"); return false;
}
void ExistingOrNew(const std::string &path) {
    struct stat info;
    if (::lstat(path.c_str(),&info)) { Require(errno==ENOENT,"Cannot inspect catalog parent"); CreateDirectory(path); }
    Directory check(path); check.Check();
}

}

class ReceiverCatalog::Impl {
public:
    typedef SourceBinding Source;
    ReceiverCatalogOptions options;
    Progress progress;
    std::string request,key,target;
    std::unique_ptr<Directory> root;
    std::vector<std::unique_ptr<Source>> sources;
    bool validated=false,executed=false,fault=false;
    Impl(const ReceiverCatalogOptions &o,Progress p) : options(o),progress(p),request(EncodeReceiverCatalogRequest(o)),key(ReceiverCatalogRequestKey(o)) {
        std::ostringstream run; run<<"run_"<<std::setw(4)<<std::setfill('0')<<o.runNumber;
        const auto officialTarget=o.receiver.catalogRoot+'/'+o.experiment+'/'+run.str()+'/'+o.receiver.deploymentId;
        target=o.stagedForReplacement ? CatalogReplacementStagePath(officialTarget,o.operationId) : officialTarget;
        Require(request.size()<=1024*1024,"Catalog request exceeds 1 MiB");
    }
    void Notify(const std::string &stage,const std::string &id="",std::uint64_t count=0) { if (progress) progress(stage,id,count); }
    void Check(Source &source) { CheckSourceBinding(source); }
    void Validate() {
        Require(!fault && !executed,"Catalog outcome/fault is latched; no replay");
        if (validated) { root->Check(); for (auto &source:sources) Check(*source); return; }
        root.reset(new Directory(options.receiver.catalogRoot)); struct stat rootInfo; Require(!::fstat(root->Descriptor(),&rootInfo),"Cannot inspect catalog root");
        struct stat targetInfo; Require(::lstat(target.c_str(),&targetInfo) && errno==ENOENT,
            options.stagedForReplacement ? "Replacement stage already exists; preserve it for inspection" : "Catalog target exists; preserve it");
        if (options.stagedForReplacement) {
            const auto officialTarget=CatalogPublicationTarget(options);
            const auto oldReceipt=InspectReceiverCatalog(officialTarget);
            const auto oldRequest=DecodeReceiverCatalogRequest(ReadFile(officialTarget+"/request.json",1024*1024));
            Require(oldRequest.experiment==options.experiment && oldRequest.runNumber==options.runNumber &&
                oldRequest.receiver.deploymentId==options.receiver.deploymentId &&
                oldRequest.receiver.catalogRoot==options.receiver.catalogRoot && !oldReceipt.empty() &&
                !PathPresent(CatalogReplacementBackupPath(officialTarget,options.operationId)),
                "Replacement staging requires the matching complete old Run catalog and an unused backup identity");
        }
        ValidateSourceBindings(options,rootInfo.st_dev,sources);
        root->Check(); validated=true; Notify("validated");
    }
    std::string Execute() {
        Validate(); executed=true;
        Fd lease(::openat(root->Descriptor(),".getdaq-catalog.lock",O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600)); struct stat info;
        Require(lease.value>=0 && !::fstat(lease.value,&info) && S_ISREG(info.st_mode) && info.st_uid==::geteuid() && (info.st_mode&0777)==0600 &&
            !::flock(lease.value,LOCK_EX|LOCK_NB),"Catalog root has another writer/invalid lease"); Sync(*root);
        const auto checkLease=[&] {
            root->Check(); struct stat current,held;
            Require(!::fstatat(root->Descriptor(),".getdaq-catalog.lock",&current,AT_SYMLINK_NOFOLLOW) && !::fstat(lease.value,&held) &&
                MetadataSame(info,current) && Same(current,held),"Catalog root lease identity changed; preserve evidence");
        };
        for (auto &source:sources) Check(*source);
        ExistingOrNew(options.receiver.catalogRoot+'/'+options.experiment);
        const auto run=target.substr(0,target.find_last_of('/')); ExistingOrNew(run); CreateDirectory(target); Directory destination(target);
        WriteNew(target+"/request.json",request); Notify("intent");
        for (auto &source:sources) {
            source->intent="{\"schema\":1,\"kind\":\"getdaq.catalog-source-intent\",\"request_key\":"+EncodeJsonString(key)+",\"operation_id\":"+EncodeJsonString(options.operationId)+",\"destination\":"+EncodeJsonString(target)+"}\n";
            WriteNew(source->directory->Path()+'/'+IntentName,source->intent);
            Require(!::fstatat(source->directory->Descriptor(),IntentName,&source->intentIdentity,AT_SYMLINK_NOFOLLOW),"Cannot bind catalog source intent identity"); Check(*source);
        }
        std::string manifest="{\"schema\":"+std::string(options.stagedForReplacement ? "2" : "1")+",\"kind\":\"getdaq.receiver-catalog-manifest\",\"request_key\":"+EncodeJsonString(key)+",\"plan_sha256\":"+EncodeJsonString(options.stopped.planSha256)+
            ",\"session_id\":"+EncodeJsonString(options.stopped.sessionId)+",\"payload_method\":\"hardlink\",\"raw_payload_bytes_read\":\"0\""+
            (options.stagedForReplacement ? ",\"publication_root\":"+EncodeJsonString(options.receiver.catalogRoot)+",\"publication_target\":"+EncodeJsonString(CatalogPublicationTarget(options))+
                ",\"publication_backup\":"+EncodeJsonString(CatalogReplacementBackupPath(CatalogPublicationTarget(options),options.operationId))+",\"staged_for_replacement\":true" : "")+
            ",\"sources\":[";
        std::uint64_t links=0;
        for (std::size_t i=0;i<sources.size();++i) {
            auto &source=*sources[i]; Check(source); const auto stored="source_"+std::to_string(i); CreateDirectory(target+'/'+stored); Directory output(target+'/'+stored);
            WriteNew(output.Path()+"/receiver-seal.json",source.seal);
            std::string files="[";
            for (auto &file:source.files) {
                struct stat before,after;
                Require(!::fstatat(source.directory->Descriptor(),file.first.c_str(),&before,AT_SYMLINK_NOFOLLOW) && MetadataSame(file.second,before),"Source changed before catalog link");
                Require(!::linkat(source.directory->Descriptor(),file.first.c_str(),output.Descriptor(),file.first.c_str(),0),"Cannot exclusively create catalog hardlink; preserve partial catalog");
                Require(!::fstatat(output.Descriptor(),file.first.c_str(),&after,AT_SYMLINK_NOFOLLOW) && MetadataSame(before,after,true) && after.st_nlink==before.st_nlink+1,"Catalog link differs from bound source inode");
                Fd linked(::openat(output.Descriptor(),file.first.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC)); struct stat held;
                Require(linked.value>=0 && !::fstat(linked.value,&held) && MetadataSame(after,held) && !::fsync(linked.value),"Cannot bind/sync catalog linked inode metadata");
                file.second=after; ++links; checkLease(); destination.Check(); output.Check(); Notify("linked",source.status->id,links);
                if (files.back()!='[') files+=',';
                files+="{\"name\":"+EncodeJsonString(file.first)+",\"bytes\":"+EncodeJsonString(std::to_string(after.st_size))+",\"device\":"+EncodeJsonString(std::to_string(after.st_dev))+",\"inode\":"+EncodeJsonString(std::to_string(after.st_ino))+"}";
            }
            files+=']'; Sync(output); Check(source);
            if (manifest.back()!='[') manifest+=',';
            manifest+="{\"id\":"+EncodeJsonString(source.status->id)+",\"stored_directory\":"+EncodeJsonString(stored)+",\"original_directory\":"+EncodeJsonString(source.directory->Path())+
                ",\"seal_sha256\":"+EncodeJsonString(ConfigurationSha256(source.seal))+",\"frames\":"+EncodeJsonString(std::to_string(source.status->frames))+",\"files\":"+files+
                (options.stagedForReplacement ? ",\"retained_files\":[]" : "")+"}";
            Require(manifest.size()<=16*1024*1024,"Catalog manifest exceeds 16 MiB");
        }
        manifest+="]}\n"; WriteNew(target+"/manifest.json",manifest); Sync(destination);
        for (auto &source:sources) Check(*source);
        Notify("before-complete","",links);
        checkLease(); destination.Check(); for (auto &source:sources) Check(*source);
        const auto result="{\"schema\":1,\"kind\":\"getdaq.receiver-catalog-complete\",\"complete\":true,\"request_key\":"+EncodeJsonString(key)+",\"manifest_sha256\":"+EncodeJsonString(ConfigurationSha256(manifest))+
            ",\"operation_id\":"+EncodeJsonString(options.operationId)+",\"directory\":"+EncodeJsonString(options.stagedForReplacement ? CatalogPublicationTarget(options) : target)+",\"payload_method\":\"hardlink\",\"raw_payload_bytes_read\":\"0\",\"linked_files\":"+EncodeJsonString(std::to_string(links))+"}\n";
        WriteNew(target+"/.complete.pending",result);
        Require(!::linkat(destination.Descriptor(),".complete.pending",destination.Descriptor(),"catalog-complete.json",0),"Cannot exclusively publish catalog completion");
        Sync(destination); Require(!::unlinkat(destination.Descriptor(),".complete.pending",0),"Cannot remove completion staging link"); Sync(destination); Sync(*root);
        Notify("complete","",links); return result;
    }
};
ReceiverCatalog::ReceiverCatalog(const ReceiverCatalogOptions &o,Progress progress) : fImpl(new Impl(o,progress)) {}
ReceiverCatalog::~ReceiverCatalog() {}
void ReceiverCatalog::Validate() { try { fImpl->Validate(); } catch (...) { fImpl->fault=true; throw; } }
std::string ReceiverCatalog::Execute() { try { return fImpl->Execute(); } catch (...) { fImpl->fault=true; throw; } }
}
