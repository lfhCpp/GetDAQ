#include "catalogtransfer.h"
#include "catalogtransfer_internal.h"
#include "receivercatalog.h"
#include "supervisorio.h"
#include "configurationsha256.h"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <memory>
#include <pthread.h>
#include <signal.h>
#include <set>
#include <stdexcept>
#include <sys/file.h>
#ifdef __linux__
#include <linux/fs.h>
#include <sys/syscall.h>
#endif
#ifdef __APPLE__
#include <stdio.h>
#include <sys/stdio.h>
#endif
#include <unistd.h>

namespace GetDAQ {
namespace CatalogTransferInternal {
using namespace SupervisorIO;
void Require(bool value,const char *message) {if(!value)throw std::runtime_error(message);}
JsonValue Json(const std::string &bytes) {JsonLimits limit;limit.arrayItems=65536;return ParseStrictJson(bytes,limit);}
std::string Quote(std::uint64_t value) {return EncodeJsonString(std::to_string(value));}
void RawName(const std::string &name) {
    Require(name.size()>5 && name.size()<=255 && name.substr(name.size()-5)==".graw" &&
        name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-:")==std::string::npos,"Invalid transfer file component");
}
bool Unchanged(const struct stat &a,const struct stat &b) {
    if(!Same(a,b) || a.st_mode!=b.st_mode || a.st_uid!=b.st_uid || a.st_size!=b.st_size || a.st_nlink!=b.st_nlink)return false;
#ifdef __APPLE__
    return a.st_mtimespec.tv_sec==b.st_mtimespec.tv_sec && a.st_mtimespec.tv_nsec==b.st_mtimespec.tv_nsec && a.st_ctimespec.tv_sec==b.st_ctimespec.tv_sec && a.st_ctimespec.tv_nsec==b.st_ctimespec.tv_nsec;
#else
    return a.st_mtim.tv_sec==b.st_mtim.tv_sec && a.st_mtim.tv_nsec==b.st_mtim.tv_nsec && a.st_ctim.tv_sec==b.st_ctim.tv_sec && a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;
#endif
}
struct stat Regular(int directory,const std::string &name) {
    struct stat info;Require(!::fstatat(directory,name.c_str(),&info,AT_SYMLINK_NOFOLLOW) && S_ISREG(info.st_mode) && info.st_uid==::geteuid() && !(info.st_mode&0022) && info.st_size>=0,"Transfer file is missing/nonregular/writable by other users");return info;
}
void Sync(const Directory &directory) {directory.Check();Require(!::fsync(directory.Descriptor()),"Transfer directory sync failed");directory.Check();}
std::unique_ptr<Fd> Lease(const Directory &directory,const std::string &name,bool create) {
    std::unique_ptr<Fd> fd(new Fd(::openat(directory.Descriptor(),name.c_str(),O_RDWR|O_NOFOLLOW|O_CLOEXEC|(create ? O_CREAT : 0),0600)));
    struct stat info;Require(fd->value>=0 && !::fstat(fd->value,&info) && S_ISREG(info.st_mode) && info.st_uid==::geteuid() &&
        (name==".getdaq-transfer.lock" ? (info.st_mode&0777)==0600 : !(info.st_mode&0022)) && !::flock(fd->value,LOCK_EX|LOCK_NB),"Transfer lease invalid or held by another process");return fd;
}
void CheckLease(const Directory &directory,const std::string &name,const Fd &fd) {
    directory.Check();const auto current=Regular(directory.Descriptor(),name);struct stat held;
    Require(!::fstat(fd.value,&held) && Same(current,held) && (name==".getdaq-transfer.lock" ? (current.st_mode&0777)==0600 : !(current.st_mode&0022)),"Transfer lease replaced");
}
std::set<std::string> Names(const Directory &directory,bool rawOnly) {
    const int fd=::openat(directory.Descriptor(),".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);DIR *scan=fd<0 ? nullptr : ::fdopendir(fd);Require(scan,"Cannot enumerate transfer source");
    std::set<std::string> result;int error=0;
    for(;;) {errno=0;const auto entry=::readdir(scan);if(!entry){error=errno;break;}const std::string name=entry->d_name;if(name!="." && name!=".." && (!rawOnly || (name.size()>=5 && name.substr(name.size()-5)==".graw")))result.insert(name);}
    ::closedir(scan);Require(!error,"Transfer enumeration failed");return result;
}
void CheckTargetTree(const Directory &root,std::size_t sources) {
    std::set<std::string> allowed={".getdaq-transfer.lock","transfer-intent.json","resume-source.json","source-request.json","source-manifest.json","source-catalog-complete.json","transfer-manifest.json","transfer-complete.json",".transfer-complete.pending"};
    for(std::size_t i=0;i<sources;++i)allowed.insert("source_"+std::to_string(i));
    for(const auto &name:Names(root,false))Require(allowed.count(name),"Unexpected entry in copied dataset; preserve it, no continuation");
    for(std::size_t i=0;i<sources;++i) {
        const auto name="source_"+std::to_string(i);struct stat info;
        if(::fstatat(root.Descriptor(),name.c_str(),&info,AT_SYMLINK_NOFOLLOW)){Require(errno==ENOENT,"Inspect copied source directory");continue;}
        Directory directory(root.Path()+'/'+name);
        for(const auto &file:Names(directory,false))if(file!="receiver-seal.json")RawName(file);
    }
}
std::string Identity(const struct stat &s) {
#ifdef __APPLE__
    const auto mt=s.st_mtimespec,ct=s.st_ctimespec;
#else
    const auto mt=s.st_mtim,ct=s.st_ctim;
#endif
    return Quote(s.st_dev)+','+Quote(s.st_ino)+','+Quote(s.st_size)+','+Quote(s.st_mode)+','+Quote(s.st_uid)+','+Quote(s.st_nlink)+','+EncodeJsonString(std::to_string(mt.tv_sec))+','+Quote(mt.tv_nsec)+','+EncodeJsonString(std::to_string(ct.tv_sec))+','+Quote(ct.tv_nsec);
}
std::string SourceCheckpoint(const Input &input,const Directory &target) {
    struct stat left,right;Require(!::fstat(input.root.Descriptor(),&left) && !::fstat(target.Descriptor(),&right),"Cannot bind resume roots");
    std::string files="[";
    for(const auto &file:input.files) {
        if(files.back()!='[')files+=',';
        files+="["+EncodeJsonString("source_"+std::to_string(file.source)+'/'+file.name)+','+Identity(file.info)+']';
        Require(files.size()<=32*1024*1024,"Resume source metadata exceeds budget");
    }
    return "{\"schema\":1,\"kind\":\"getdaq.catalog-transfer-source-checkpoint\",\"source_root\":["+Quote(left.st_dev)+','+Quote(left.st_ino)+"],\"target_root\":["+Quote(right.st_dev)+','+Quote(right.st_ino)+"],\"request_sha256\":"+EncodeJsonString(ConfigurationSha256(input.request))+",\"manifest_sha256\":"+EncodeJsonString(ConfigurationSha256(input.manifest))+",\"receipt_sha256\":"+EncodeJsonString(ConfigurationSha256(input.receipt))+",\"files\":"+files+"]}\n";
}
std::string StreamSourceCheckpoint(const Input &input,const std::string &transport) {
    struct stat root;Require(!::fstat(input.root.Descriptor(),&root),"Cannot inspect SSH transfer source root");
    std::string files="[";
    for(const auto &file:input.files) {
        if(files.size()>1)files+=',';
        files+="["+EncodeJsonString("source_"+std::to_string(file.source)+'/'+file.name)+','+Identity(file.info)+"]";
        Require(files.size()<=32*1024*1024,"SSH source identity checkpoint exceeds limit");
    }
    const auto kind=transport=="openssh-stdio-v1" ? "getdaq.catalog-ssh-source-checkpoint" : "getdaq.catalog-tcp-source-checkpoint";
    return "{\"schema\":1,\"kind\":"+EncodeJsonString(kind)+",\"source_root\":["+Quote(root.st_dev)+','+Quote(root.st_ino)+
        "],\"request_sha256\":"+EncodeJsonString(ConfigurationSha256(input.request))+",\"manifest_sha256\":"+EncodeJsonString(ConfigurationSha256(input.manifest))+
        ",\"receipt_sha256\":"+EncodeJsonString(ConfigurationSha256(input.receipt))+",\"files\":"+files+"]}\n";
}
void ReadPrefix(int fd,std::uint64_t bytes,Sha256Stream &hash) {
    Require(::lseek(fd,0,SEEK_SET)==0,"Cannot rewind file for SSH resume check");
    std::array<unsigned char,65536> buffer;std::uint64_t done=0;
    while(done<bytes) {
        const auto wanted=std::size_t(std::min<std::uint64_t>(buffer.size(),bytes-done));
        const auto count=::read(fd,buffer.data(),wanted);
        if(count<0 && errno==EINTR)continue;
        Require(count>0,"File ended before checked SSH resume prefix");
        hash.Update(buffer.data(),std::size_t(count));done+=std::uint64_t(count);
    }
}
void Metadata(const std::string &path,const std::string &bytes,bool resume,std::size_t maximum) {
    struct stat info;
    if(resume && !::lstat(path.c_str(),&info)) {Require(S_ISREG(info.st_mode) && info.st_uid==::geteuid() && !(info.st_mode&0022) && ReadFile(path,maximum)==bytes,"Resume metadata differs; never overwrite");return;}
    if(resume)Require(errno==ENOENT,"Cannot inspect resume metadata");
    WriteNew(path,bytes);
}
std::map<std::string,struct stat> CheckPrefixes(const Input &input,const std::vector<std::unique_ptr<Directory>> &directories) {
    std::map<std::string,struct stat> result;
    for(std::size_t i=0;i<directories.size();++i) {
        auto expected=Names(*input.directories[i]);const auto actual=Names(*directories[i]);
        for(const auto &name:actual)Require(expected.count(name),"Resume target contains an unexpected raw file");
    }
    std::array<unsigned char,65536> source,target;
    for(const auto &file:input.files) {
        auto &directory=*directories[file.source];struct stat info;
        if(::fstatat(directory.Descriptor(),file.name.c_str(),&info,AT_SYMLINK_NOFOLLOW)){Require(errno==ENOENT,"Inspect partial raw file");continue;}
        info=Regular(directory.Descriptor(),file.name);Require(info.st_nlink==1 && info.st_size<=file.info.st_size,"Partial target is aliased or exceeds original size");
        Fd src(::openat(input.directories[file.source]->Descriptor(),file.name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC)),dst(::openat(directory.Descriptor(),file.name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
        Require(src.value>=0 && dst.value>=0,"Cannot inspect resume raw prefix");
        struct stat sourceHeld,targetHeld;
        Require(!::fstat(src.value,&sourceHeld) && Unchanged(file.info,sourceHeld) && !::fstat(dst.value,&targetHeld) && Unchanged(info,targetHeld),"Resume prefix descriptors differ from bound files");
        std::uint64_t offset=0;
        while(offset<std::uint64_t(info.st_size)) {
            const auto wanted=std::min<std::uint64_t>(source.size(),std::uint64_t(info.st_size)-offset);
            std::size_t count=0;
            while(count<wanted) {
                const auto a=::pread(src.value,source.data()+count,wanted-count,off_t(offset+count));if(a<0 && errno==EINTR)continue;Require(a>0,"Resume source prefix read failed");
                std::size_t received=0;while(received<std::size_t(a)){const auto b=::pread(dst.value,target.data()+count+received,std::size_t(a)-received,off_t(offset+count+received));if(b<0 && errno==EINTR)continue;Require(b>0,"Resume target prefix read failed");received+=b;}
                Require(std::equal(source.begin()+count,source.begin()+count+a,target.begin()+count),"Partial target bytes differ from original source; no append performed");count+=a;
            }
            offset+=wanted;
        }
        struct stat held;Require(!::fstat(src.value,&sourceHeld) && Unchanged(file.info,sourceHeld) && !::fstat(dst.value,&held) && Unchanged(info,held) && Unchanged(info,Regular(directory.Descriptor(),file.name)),"Source/partial target changed during preflight");
        result.emplace("source_"+std::to_string(file.source)+'/'+file.name,info);
    }
    input.Check();return result;
}


std::string RemoteDestination(const std::string &root,const std::string &key) {return root+"/getdaq-transfer-"+key.substr(0,32);}
void PublishDirectoryNoReplace(const std::string &source,const std::string &destination) {
#ifdef __APPLE__
    if(::renamex_np(source.c_str(),destination.c_str(),RENAME_EXCL))
        throw std::runtime_error(std::string("Cannot exclusively publish remote archive directory: ")+std::strerror(errno));
#elif defined(__linux__) && defined(SYS_renameat2)
    if(::syscall(SYS_renameat2,AT_FDCWD,source.c_str(),AT_FDCWD,destination.c_str(),RENAME_NOREPLACE))
        throw std::runtime_error(std::string("Cannot exclusively publish remote archive directory: ")+std::strerror(errno));
#else
    (void)source;(void)destination;
    throw std::runtime_error("Atomic no-overwrite archive publication is unavailable on this platform");
#endif
}
BlockPipeSignal::BlockPipeSignal() : fWasPending(false) {
    ::sigemptyset(&fSignalSet);Require(!::sigaddset(&fSignalSet,SIGPIPE),"Cannot prepare archive peer signal guard");
    const int blocked=::pthread_sigmask(SIG_BLOCK,&fSignalSet,&fPreviousMask);
    if(blocked)throw std::runtime_error("Cannot block archive peer signal: pthread error "+std::to_string(blocked));
    sigset_t pending;
    if(::sigpending(&pending)) {::pthread_sigmask(SIG_SETMASK,&fPreviousMask,nullptr);throw std::runtime_error("Cannot inspect pending archive peer signal");}
    fWasPending=::sigismember(&pending,SIGPIPE)==1;
}
BlockPipeSignal::~BlockPipeSignal() {
    if(!fWasPending) {
        sigset_t pending;
        if(!::sigpending(&pending) && ::sigismember(&pending,SIGPIPE)==1) {
            int received=0;int result;
            do {result=::sigwait(&fSignalSet,&received);} while(result==EINTR);
        }
    }
    ::pthread_sigmask(SIG_SETMASK,&fPreviousMask,nullptr);
}
}
using namespace CatalogTransferInternal;
std::string CopyReceiverCatalog(const std::string &source,const std::string &destination,CatalogTransferProgress progress,const std::string &expected) {return CatalogTransferInternal::Transfer(source,destination,progress,expected,false);}
std::string ResumeReceiverCatalog(const std::string &source,const std::string &destination,CatalogTransferProgress progress,const std::string &expected) {return CatalogTransferInternal::Transfer(source,destination,progress,expected,true);}
std::string VerifyTransferredCatalog(const std::string &destination) {Directory root(destination);auto lease=Lease(root,".getdaq-transfer.lock",false);const auto result=Verify(destination,true);CheckLease(root,".getdaq-transfer.lock",*lease);return result;}


}
