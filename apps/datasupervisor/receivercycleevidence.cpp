#include "receivercycleevidence.h"
#include "supervisorio.h"
#include "planio.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <atomic>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <sys/file.h>
#include <unistd.h>
namespace GetDAQ {
namespace {
using namespace SupervisorIO;
void Require(bool value,const char *message) {if (!value) throw std::runtime_error(message);}
bool StableMetadata(const struct stat &a,const struct stat &b) {
#ifdef __APPLE__
    const bool times=a.st_mtimespec.tv_sec==b.st_mtimespec.tv_sec && a.st_mtimespec.tv_nsec==b.st_mtimespec.tv_nsec &&
        a.st_ctimespec.tv_sec==b.st_ctimespec.tv_sec && a.st_ctimespec.tv_nsec==b.st_ctimespec.tv_nsec;
#else
    const bool times=a.st_mtim.tv_sec==b.st_mtim.tv_sec && a.st_mtim.tv_nsec==b.st_mtim.tv_nsec &&
        a.st_ctim.tv_sec==b.st_ctim.tv_sec && a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;
#endif
    return Same(a,b) && a.st_mode==b.st_mode && a.st_uid==b.st_uid && a.st_size==b.st_size && times;
}
std::set<std::string> RawNames(const std::string &path) {
    Directory directory(path);const int fd=::openat(directory.Descriptor(),".",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    Require(fd>=0,"Cannot inspect original receiver file set");DIR *stream=::fdopendir(fd);
    if (!stream) {::close(fd);throw std::runtime_error("Cannot enumerate original receiver directory");}
    std::set<std::string> result;errno=0;
    while (const auto entry=::readdir(stream)) {
        const std::string name=entry->d_name;
        if (name.size()>=5 && name.substr(name.size()-5)==".graw") result.insert(name);
        if (result.size()>65536) {::closedir(stream);throw std::runtime_error("Original receiver file set exceeds limit");}
        errno=0;
    }
    const auto error=errno;::closedir(stream);Require(!error,"Cannot read complete original receiver file set");directory.Check();return result;
}
std::string RawJson(const std::set<std::string> &names) {
    std::string result="[";for (const auto &name:names) {if (result.size()>1)result+=',';result+=EncodeJsonString(name);}return result+']';
}
std::string Entry(const std::string &path,const std::string &kind) {
    Path(path);struct stat st;Require(!::lstat(path.c_str(),&st),"Cannot bind original receiver evidence");
    const bool raw=path.size()>=5 && path.substr(path.size()-5)==".graw";
    Require(kind=="directory" || (kind=="payload-stat")==raw,"Raw payloads may only use stat evidence, metadata must be hashed");
    Require(st.st_uid==::geteuid() && ((kind=="directory" && S_ISDIR(st.st_mode) && (st.st_mode&0777)==0700) ||
        (kind!="directory" && S_ISREG(st.st_mode) && st.st_size>=0)),"Invalid original receiver evidence type/owner");
    std::string result="{\"path\":"+EncodeJsonString(path)+",\"kind\":"+EncodeJsonString(kind)+
        ",\"device\":"+EncodeJsonString(std::to_string(std::uint64_t(st.st_dev)))+",\"inode\":"+EncodeJsonString(std::to_string(std::uint64_t(st.st_ino)))+
        ",\"mode\":"+EncodeJsonString(std::to_string(st.st_mode&07777));
    if (kind=="directory") result+=",\"raw_names\":"+RawJson(RawNames(path));
    else {
        result+=",\"bytes\":"+EncodeJsonString(std::to_string(std::uint64_t(st.st_size)));
        if (kind=="metadata") result+=",\"sha256\":"+EncodeJsonString(ConfigurationSha256(ReadFile(path,64*1024*1024)));
    }
    return result+'}';
}
}
std::string CaptureReceiverCycleEvidence(const std::string &root,const std::vector<std::string> &metadata,
                                        const std::vector<std::string> &directories) {
    Path(root);Require(!directories.empty() && directories.size()<=195,"Original cycle evidence requires bounded receiver directories");
    std::map<std::string,std::string> entries;
    const auto add=[&](const std::string &path,const std::string &kind) {
        const auto value=Entry(path,kind);const auto found=entries.find(path);
        Require(found==entries.end() || found->second==value,"Conflicting original cycle evidence entries");entries[path]=value;
    };
    add(root,"directory");add(root+"/.supervisor.lock","metadata");
    for (const auto &file:metadata) {add(file.substr(0,file.find_last_of('/')),"directory");add(file,"metadata");}
    for (const auto &directory:directories) {
        add(directory,"directory");add(directory+"/.getdaq-receiver.lock","metadata");
        for (const auto &name:RawNames(directory)) add(directory+'/'+name,"payload-stat");
    }
    std::string result="{\"schema\":1,\"kind\":\"getdaq.receiver-cycle-evidence\",\"deployment_root\":"+EncodeJsonString(root)+",\"entries\":[";
    bool first=true;for (const auto &entry:entries) {if(!first)result+=',';first=false;result+=entry.second;}
    result+="]}\n";Require(result.size()<=16*1024*1024,"Original cycle evidence exceeds metadata budget");return result;
}
class ReceiverCycleEvidence::Impl {
public:
    std::string root;
    struct BoundEntry {std::string path,kind,bytes;struct stat identity;};
    std::vector<BoundEntry> entries;
    mutable std::atomic<bool> faulted{false};
    mutable std::mutex mutex;
    Fd lease;
    explicit Impl(const std::string &bytes) {
        Require(bytes.size()<=16*1024*1024,"Original cycle evidence exceeds limit");
        JsonLimits limits;limits.arrayItems=131072;const auto value=ParseStrictJson(bytes,limits);
        ExactSupervisorFields(value,{"schema","kind","deployment_root","entries"});
        Require(Unsigned(value.At("schema"))==1 && String(value.At("kind"))=="getdaq.receiver-cycle-evidence","Unsupported original cycle evidence");
        root=String(value.At("deployment_root"));Path(root);const auto &list=value.At("entries");
        Require(list.type==JsonValue::Array && !list.items.empty() && list.items.size()<=131072,"Invalid original cycle evidence entries");
        std::set<std::string> names;
        for (const auto &item:list.items) {
            const auto path=String(item.At("path")),kind=String(item.At("kind"));Path(path);
            Require(names.insert(path).second,"Duplicate original cycle evidence path");
            Require(kind=="directory" || kind=="metadata" || kind=="payload-stat","Unsupported original evidence entry kind");
            if (kind=="directory") ExactSupervisorFields(item,{"path","kind","device","inode","mode","raw_names"});
            else if (kind=="metadata") ExactSupervisorFields(item,{"path","kind","device","inode","mode","bytes","sha256"});
            else ExactSupervisorFields(item,{"path","kind","device","inode","mode","bytes"});
            // Recreate the canonical metadata entry; raw payloads are only stat'ed.
            struct stat before,after;Require(!::lstat(path.c_str(),&before),"Cannot inspect original metadata");
            const auto expected=Entry(path,kind);const auto canonical=ParseStrictJson(expected,limits);
            Require(!::lstat(path.c_str(),&after) && (kind!="metadata" || StableMetadata(before,after)),"Original metadata changed while binding");
            Require(canonical.members.size()==item.members.size(),"Original evidence fields differ");
            for (const auto &field:canonical.members) {
                if (field.second.type==JsonValue::Array) {
                    const auto &actual=item.At(field.first);Require(actual.type==JsonValue::Array && actual.items.size()==field.second.items.size(),"Original raw file set changed");
                    for (std::size_t i=0;i<actual.items.size();++i) Require(String(actual.items[i])==String(field.second.items[i]),"Original raw file names changed");
                } else Require(String(item.At(field.first))==String(field.second),"Original evidence identity/content changed");
            }
            BoundEntry entry;entry.path=path;entry.kind=kind;entry.bytes=expected;entry.identity=after;entries.push_back(entry);
        }
        Require(names.count(root) && names.count(root+"/.supervisor.lock") &&
            names.count(root+"/plan.json") && names.count(root+"/supervisor.jsonl"),"Original deployment lease/audit is not bound");
    }
    void CheckLocked() const {
        if (faulted) throw std::runtime_error("Original completed cycle evidence fault remains latched");
        try {
            for (const auto &entry:entries) {
                if (entry.kind=="metadata") {
                    struct stat current;Require(!::lstat(entry.path.c_str(),&current) && StableMetadata(entry.identity,current),"Original completed metadata identity/content timestamps changed");
                } else Require(Entry(entry.path,entry.kind)==entry.bytes,"Original completed cycle identity/file size changed");
            }
        } catch (...) {faulted=true;throw;}
    }
    void CheckReservationLocked() const {
        if (faulted) throw std::runtime_error("Original completed cycle evidence fault remains latched");
        try {
            Require(lease.value>=0,"Original deployment lease is not reserved");
            for (const auto &entry:entries) {
                if (entry.path==root) {
                    struct stat current;Require(!::lstat(root.c_str(),&current) && Same(entry.identity,current) &&
                        current.st_mode==entry.identity.st_mode && current.st_uid==::geteuid(),"Original deployment directory changed");
                } else if (entry.path==root+"/.supervisor.lock" || entry.path==root+"/plan.json" || entry.path==root+"/supervisor.jsonl") {
                    struct stat current;Require(!::lstat(entry.path.c_str(),&current) && StableMetadata(entry.identity,current),"Original deployment audit changed");
                    if (entry.path==root+"/.supervisor.lock") {
                        struct stat held;Require(!::fstat(lease.value,&held) && Same(held,current),"Original deployment lease changed");
                    }
                }
            }
        } catch (...) {faulted=true;throw;}
    }
};
ReceiverCycleEvidence::ReceiverCycleEvidence(const std::string &bytes) : fImpl(new Impl(bytes)) {}
ReceiverCycleEvidence::~ReceiverCycleEvidence() {}
void ReceiverCycleEvidence::Check() const {std::lock_guard<std::mutex> lock(fImpl->mutex);fImpl->CheckLocked();}
void ReceiverCycleEvidence::CheckReservation() const {std::lock_guard<std::mutex> lock(fImpl->mutex);fImpl->CheckReservationLocked();}
void ReceiverCycleEvidence::ReserveRetiredDeployment() {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.mutex);p.CheckLocked();
    if (p.lease.value>=0) throw std::runtime_error("Original deployment lease already reserved");
    Fd lease(::open((p.root+"/.supervisor.lock").c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
    Require(lease.value>=0 && !::flock(lease.value,LOCK_EX|LOCK_NB),"Original data service still holds its deployment lease");
    const std::string suffix="/.getdaq-receiver.lock";
    for (const auto &entry:p.entries) if (entry.path.size()>=suffix.size() && entry.path.substr(entry.path.size()-suffix.size())==suffix) {
        Fd output(::open(entry.path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
        Require(output.value>=0 && !::flock(output.value,LOCK_EX|LOCK_NB),"Original receiver output lock is held during cutover");
    }
    p.CheckLocked();p.lease.value=lease.value;lease.value=-1;
}
bool ReceiverCycleEvidence::Faulted() const {return fImpl->faulted;}
}
