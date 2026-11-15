#include "receivercatalog.h"
#include "receivercataloginternal.h"
#include "configurationsha256.h"
#include "planio.h"
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <set>
#include <sys/file.h>
#include <unistd.h>

namespace GetDAQ {
namespace ReceiverCatalogInternal {
using namespace SupervisorIO;
namespace {
const char *IntentName=".getdaq-catalog-intent.json";
bool Present(int directory,const std::string &name) {
    struct stat info;
    if (!::fstatat(directory,name.c_str(),&info,AT_SYMLINK_NOFOLLOW)) return true;
    Require(errno==ENOENT,"Cannot inspect catalog path"); return false;
}
}

void CheckSourceBinding(SourceBinding &source) {
    source.directory->Check(); struct stat current,held;
    Require(!::fstatat(source.directory->Descriptor(),".getdaq-receiver.lock",&current,AT_SYMLINK_NOFOLLOW) && !::fstat(source.lease->value,&held) &&
        MetadataSame(source.leaseIdentity,current) && Same(current,held),"Catalog source lease identity changed");
    Require(!::fstatat(source.directory->Descriptor(),source.sealName.c_str(),&current,AT_SYMLINK_NOFOLLOW) && MetadataSame(source.sealIdentity,current) &&
        ReadFile(source.directory->Path()+'/'+source.sealName,16*1024*1024)==source.seal,"Catalog source seal identity/content changed");
    std::set<std::string> expected;
    for (const auto &file:source.files) {
        Require(!::fstatat(source.directory->Descriptor(),file.first.c_str(),&current,AT_SYMLINK_NOFOLLOW) && MetadataSame(file.second,current),"Catalog source data metadata changed");
        expected.insert(file.first);
    }
    Require(RawNames(*source.directory)==expected,"Catalog source file set changed");
    if (!source.intent.empty()) Require(!::fstatat(source.directory->Descriptor(),IntentName,&current,AT_SYMLINK_NOFOLLOW) && MetadataSame(source.intentIdentity,current) &&
        ReadFile(source.directory->Path()+'/'+IntentName,1024*1024)==source.intent,"Catalog source intent identity/content changed");
}

void ValidateSourceBindings(const ReceiverCatalogOptions &options,dev_t catalogDevice,
                            std::vector<std::unique_ptr<SourceBinding> > &sources) {
    std::uint64_t entries=0,metadataBytes=0;
    for (const auto &receiver:options.stopped.receivers) {
        const auto plan=std::find_if(options.receiver.sources.begin(),options.receiver.sources.end(),[&](const ReceiverSourcePlan &entry) {return entry.id==receiver.id;});
        Require(plan!=options.receiver.sources.end(),"Catalog receiver is absent from fixed plan");
        std::unique_ptr<SourceBinding> source(new SourceBinding); source->status=&receiver; source->directory.reset(new Directory(receiver.outputDirectory));
        struct stat directoryInfo; Require(!::fstat(source->directory->Descriptor(),&directoryInfo) && directoryInfo.st_dev==catalogDevice,"Catalog requires the same filesystem; use a separate native transfer job for cross-filesystem data");
        Require(!Present(source->directory->Descriptor(),IntentName),"Receiver session already has catalog intent; observe/reconcile original operation");
        source->lease.reset(new Fd(::openat(source->directory->Descriptor(),".getdaq-receiver.lock",O_RDWR|O_NOFOLLOW|O_CLOEXEC)));
        Require(source->lease->value>=0 && !::fstat(source->lease->value,&source->leaseIdentity) && S_ISREG(source->leaseIdentity.st_mode) &&
            source->leaseIdentity.st_uid==::geteuid() && !(source->leaseIdentity.st_mode&0022) && !::flock(source->lease->value,LOCK_EX|LOCK_NB),"Receiver source lock is invalid or still held; do not catalog live data");
        source->sealName=plan->inputProtocol=="ICE" ? ".getdaq-ice-receiver-seal.json" : ".getdaq-receiver-seal.json";
        Require(!::fstatat(source->directory->Descriptor(),source->sealName.c_str(),&source->sealIdentity,AT_SYMLINK_NOFOLLOW) && S_ISREG(source->sealIdentity.st_mode),"Receiver seal missing/nonregular");
        source->seal=ReadFile(receiver.outputDirectory+'/'+source->sealName,16*1024*1024); metadataBytes+=source->seal.size(); Require(metadataBytes<=64*1024*1024,"Catalog source metadata budget exceeded");
        const auto seal=CatalogJson(source->seal);
        CheckSealFields(seal,*plan);
        Require(Unsigned(seal.At("schema"))==1 && Unsigned(seal.At("control_protocol"))==1 && Boolean(seal.At("complete")) && Boolean(seal.At("durable")) &&
            ReceiverBoundary(seal,*plan,receiver) && Unsigned(seal.At("pid"))==std::uint64_t(receiver.pid) &&
            String(seal.At("session_id"))==receiver.sessionId && String(seal.At("sender_id"))==receiver.id && String(seal.At("output_directory"))==receiver.outputDirectory &&
            String(seal.At("configuration_sha256"))==options.receiver.configurationSha256 && String(seal.At("artifact_sha256"))==options.receiver.artifactSha256,"Catalog seal EOF/source/configuration binding differs");
        Require(seal.At("files").type==JsonValue::Array && seal.At("files").items.size()==receiver.files,"Catalog seal file count differs");
        std::uint64_t total=0;
        for (const auto &file:seal.At("files").items) {
            ExactSupervisorFields(file,{"name","bytes"});
            const auto name=String(file.At("name")); const auto bytes=Unsigned(file.At("bytes"),true); struct stat info;
            Require(name.size()>5 && name.size()<=255 && name.substr(name.size()-5)==".graw" && name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-:")==std::string::npos,"Catalog file name is not a single raw file component");
            Require(++entries<=65536 && !::fstatat(source->directory->Descriptor(),name.c_str(),&info,AT_SYMLINK_NOFOLLOW) && S_ISREG(info.st_mode) && info.st_uid==::geteuid() &&
                !(info.st_mode&0022) && info.st_nlink==1 && info.st_size>=0 && std::uint64_t(info.st_size)==bytes && source->files.emplace(name,info).second && total<=UINT64_MAX-bytes,"Catalog raw file identity/type/size/link-count differs");
            total+=bytes;
        }
        Require(total==receiver.receivedBytes && total==Unsigned(seal.At("received_bytes"),true),"Catalog source bytes differ from stopped/seal proof");
        CheckSourceBinding(*source); sources.push_back(std::move(source));
    }
}

} // namespace ReceiverCatalogInternal
} // namespace GetDAQ
