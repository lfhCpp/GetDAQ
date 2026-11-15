#include "hostcontrolownership.h"
#include "configurationsha256.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
namespace GetDAQ {
namespace {
struct Fd {
    int value;
    explicit Fd(int fd) : value(fd) { if (value<0) throw std::runtime_error("Cannot open host ownership storage"); }
    ~Fd() { ::close(value); }
    Fd(const Fd &)=delete;
    Fd &operator=(const Fd &)=delete;
};
bool Same(const struct stat &a,const struct stat &b) { return a.st_dev==b.st_dev && a.st_ino==b.st_ino; }
void DirectoryIdentity(int fd,const std::string &path,const struct stat &expected) {
    struct stat held,current;
    if (::fstat(fd,&held) || ::lstat(path.c_str(),&current) || !Same(held,expected) || !Same(current,expected) ||
        !S_ISDIR(current.st_mode) || current.st_uid!=::geteuid() || (current.st_mode&0777)!=0700)
        throw std::runtime_error("Host ownership directory identity or permissions changed");
}
void FileIdentity(int root,int fd,const std::string &name,const struct stat &expected) {
    struct stat held,current;
    if (::fstat(fd,&held) || ::fstatat(root,name.c_str(),&current,AT_SYMLINK_NOFOLLOW) ||
        !Same(held,expected) || !Same(current,expected) || !S_ISREG(current.st_mode) || current.st_nlink!=1 ||
        current.st_uid!=::geteuid() || (current.st_mode&0777)!=0600)
        throw std::runtime_error("Host ownership reservation identity or permissions changed");
}
std::string Address(const std::string &value) {
    in_addr address={}; char canonical[INET_ADDRSTRLEN];
    if (::inet_pton(AF_INET,value.c_str(),&address)!=1 || !::inet_ntop(AF_INET,&address,canonical,sizeof(canonical)) || value!=canonical)
        throw std::invalid_argument("Host ownership requires canonical literal IPv4 board addresses");
    const auto host=ntohl(address.s_addr);
    if (!host || host==0xffffffffU || (host>>24)==0 || (host>>24)>=224)
        throw std::invalid_argument("Host ownership requires unicast board addresses");
    return value;
}
}
class HostControlOwnership::Impl {
public:
    struct Reservation {
        Fd fd;
        std::string name,bytes;
        struct stat identity;
        Reservation(int root,const std::string &resource,const std::string &owner) :
            fd(::openat(root,(ConfigurationSha256("getdaq.host-board:1\n"+resource)+".lock").c_str(),
                        O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600)),
            name(ConfigurationSha256("getdaq.host-board:1\n"+resource)+".lock"),
            bytes("getdaq.host-control-ownership:1\n"+owner+"\n"+std::to_string(::getpid())+"\n"+resource+"\n") {
            if (::fstat(fd.value,&identity)) throw std::runtime_error("Cannot inspect ownership reservation");
            FileIdentity(root,fd.value,name,identity);
            if (::flock(fd.value,LOCK_EX|LOCK_NB)) throw std::runtime_error("Board is already reserved by another host ECC: "+resource);
            FileIdentity(root,fd.value,name,identity);
            if (::ftruncate(fd.value,0)) throw std::runtime_error("Cannot record ownership reservation");
            std::size_t offset=0;
            while (offset<bytes.size()) {
                const auto written=::pwrite(fd.value,bytes.data()+offset,bytes.size()-offset,offset);
                if (written<0 && errno==EINTR) continue;
                if (written<=0) throw std::runtime_error("Cannot record ownership reservation");
                offset+=std::size_t(written);
            }
            if (::fsync(fd.value)) throw std::runtime_error("Cannot sync ownership reservation");
        }
        void Check(int root) const {
            FileIdentity(root,fd.value,name,identity);
            struct stat current;
            if (::fstat(fd.value,&current) || current.st_size!=static_cast<off_t>(bytes.size()))
                throw std::runtime_error("Ownership reservation content changed");
            std::string actual(bytes.size(),'\0'); std::size_t offset=0;
            while (offset<actual.size()) {
                const auto count=::pread(fd.value,&actual[offset],actual.size()-offset,offset);
                if (count<0 && errno==EINTR) continue;
                if (count<=0) throw std::runtime_error("Cannot check ownership reservation");
                offset+=std::size_t(count);
            }
            if (actual!=bytes) throw std::runtime_error("Ownership reservation content changed");
        }
    };
    std::string directory;
    Fd root;
    struct stat identity;
    std::vector<std::string> resources;
    std::vector<std::unique_ptr<Reservation>> reservations;
    mutable std::atomic<bool> faulted{false};
    Impl(const std::string &path,const std::vector<std::string> &addresses,const std::string &deployment,const std::string &snapshot) :
        directory(path),root(::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC)) {
        if (path.size()>3300 || path.empty() || path[0]!='/' || path.find('\0')!=std::string::npos)
            throw std::invalid_argument("Invalid ownership directory path");
        char *resolved=::realpath(path.c_str(),nullptr);
        const bool canonical=resolved && path==resolved; ::free(resolved);
        if (!canonical) throw std::invalid_argument("Ownership directory must already exist at a canonical path");
        if (deployment.empty() || deployment.size()>128 || snapshot.size()!=64 || snapshot.find_first_not_of("0123456789abcdef")!=std::string::npos)
            throw std::invalid_argument("Invalid ownership deployment/snapshot binding");
        if (::fstat(root.value,&identity)) throw std::runtime_error("Cannot inspect ownership directory");
        DirectoryIdentity(root.value,directory,identity);
        if (addresses.empty() || addresses.size()>256) throw std::invalid_argument("Ownership resource count outside limit");
        for (const auto &address:addresses) resources.push_back(Address(address));
        std::sort(resources.begin(),resources.end()); resources.erase(std::unique(resources.begin(),resources.end()),resources.end());
        const auto owner=ConfigurationSha256("getdaq.host-owner:1\n"+deployment+'\n'+snapshot);
        for (const auto &resource:resources) reservations.emplace_back(new Reservation(root.value,resource,owner));
        if (::fsync(root.value)) throw std::runtime_error("Cannot sync ownership directory");
        Check();
    }
    void Check() const {
        if (faulted) throw std::runtime_error("Host ownership fault is latched; explicit reconciliation required");
        try {
            DirectoryIdentity(root.value,directory,identity);
            for (const auto &reservation:reservations) reservation->Check(root.value);
        } catch (...) { faulted=true; throw; }
    }
};
HostControlOwnership::HostControlOwnership(const std::string &directory,const std::vector<std::string> &resources,
                                         const std::string &deployment,const std::string &snapshot) :
    fImpl(new Impl(directory,resources,deployment,snapshot)) {}
HostControlOwnership::~HostControlOwnership() {}
void HostControlOwnership::Check() const { fImpl->Check(); }
bool HostControlOwnership::Faulted() const { return fImpl->faulted; }
const std::vector<std::string> &HostControlOwnership::Resources() const { return fImpl->resources; }
}
