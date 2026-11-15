#include "nativeeccfilesystemproof_p.h"

#include "configurationsha256.h"
#include "strictjson.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

namespace GetDAQ {
namespace NativeEccFilesystemProof {

bool SameIdentity(const struct stat &a,const struct stat &b) {
    return a.st_dev==b.st_dev && a.st_ino==b.st_ino;
}

Directory::Directory(const std::string &path) : fPath(path) {
    fFd=::open(fPath.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (fFd<0) throw std::runtime_error("Cannot open native ECC directory without following symlinks");
    if (::fstat(fFd,&fIdentity)!=0) {
        ::close(fFd); fFd=-1;
        throw std::runtime_error("Cannot inspect native ECC directory");
    }
    try { Check(); } catch (...) { ::close(fFd); fFd=-1; throw; }
}

Directory::~Directory() { if (fFd>=0) ::close(fFd); }

void Directory::Check() const {
    struct stat current,held;
    if (::lstat(fPath.c_str(),&current)!=0 || ::fstat(fFd,&held)!=0 || !S_ISDIR(current.st_mode) ||
        !SameIdentity(fIdentity,current) || !SameIdentity(fIdentity,held) ||
        current.st_uid!=::geteuid() || (current.st_mode&0777)!=0700)
        throw std::runtime_error("Native ECC directory identity/owner/permissions changed; reconciliation required");
}

void WriteNewFile(const std::string &path,const std::string &bytes) {
    const int fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if (fd<0) throw std::runtime_error("Cannot exclusively create native ECC immutable file");
    try {
        std::size_t sent=0;
        while (sent<bytes.size()) {
            const auto n=::write(fd,bytes.data()+sent,bytes.size()-sent);
            if (n<0 && errno==EINTR) continue;
            if (n<=0) throw std::runtime_error("Cannot write native ECC immutable file");
            sent+=std::size_t(n);
        }
        if (::fsync(fd)!=0) throw std::runtime_error("Cannot fsync native ECC immutable file");
        const auto parent=path.substr(0,path.find_last_of('/'));
        const int directory=::open(parent.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        const bool synced=directory>=0 && ::fsync(directory)==0;
        if (directory>=0) ::close(directory);
        if (!synced) throw std::runtime_error("Cannot fsync native ECC immutable file directory");
    } catch (...) { ::close(fd); throw; }
    ::close(fd);
}

ImmutableFile::ImmutableFile(const std::string &path,const std::string &bytes)
    : fPath(path),fExpected(bytes) {
    fFd=::open(fPath.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if (fFd<0) throw std::runtime_error("Cannot open native ECC immutable snapshot/artifact");
    if (::fstat(fFd,&fIdentity)!=0) {
        ::close(fFd); fFd=-1;
        throw std::runtime_error("Cannot inspect native ECC immutable file");
    }
    try { Check(); } catch (...) { ::close(fFd); fFd=-1; throw; }
}

ImmutableFile::~ImmutableFile() { if (fFd>=0) ::close(fFd); }

bool ImmutableFile::IsPath(const std::string &value) const { return fPath==value; }

void ImmutableFile::AdvanceVerifiedAppend(const std::string &bytes) {
    if (bytes.size()<fExpected.size() || bytes.compare(0,fExpected.size(),fExpected)!=0)
        throw std::runtime_error("Native ECC retained audit prefix changed during authorized append");
    struct stat current,held;
    if (::lstat(fPath.c_str(),&current) || ::fstat(fFd,&held) ||
        !SameIdentity(fIdentity,current) || !SameIdentity(fIdentity,held))
        throw std::runtime_error("Native ECC retained audit identity changed during authorized append");
    std::string previous=fExpected; fExpected=bytes;
    try { Check(); } catch (...) { fExpected.swap(previous); throw; }
}

void ImmutableFile::Check() const {
    struct stat current,held;
    if (::lstat(fPath.c_str(),&current)!=0 || ::fstat(fFd,&held)!=0 || !S_ISREG(current.st_mode) ||
        !SameIdentity(fIdentity,current) || !SameIdentity(fIdentity,held) || current.st_uid!=::geteuid() ||
        (current.st_mode&0777)!=0600 || current.st_size<0 ||
        std::uint64_t(current.st_size)!=fExpected.size() || held.st_size!=current.st_size)
        throw std::runtime_error("Native ECC immutable file identity/size/permissions changed");
    char buffer[65536]; std::size_t offset=0;
    while (offset<fExpected.size()) {
        const auto wanted=std::min<std::size_t>(sizeof(buffer),fExpected.size()-offset);
        const auto n=::pread(fFd,buffer,wanted,off_t(offset));
        if (n<0 && errno==EINTR) continue;
        if (n<=0 || fExpected.compare(offset,std::size_t(n),buffer,std::size_t(n))!=0)
            throw std::runtime_error("Native ECC immutable snapshot/artifact content differs");
        offset+=std::size_t(n);
    }
    if (::lstat(fPath.c_str(),&current)!=0 || !SameIdentity(fIdentity,current) || current.st_size!=held.st_size)
        throw std::runtime_error("Native ECC immutable file changed during validation");
}

std::string ReadFailedDescriptionAudit(const std::string &path,const struct stat &identity,
                                      const std::string &chain) {
    const int fd=::open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if (fd<0) throw std::runtime_error("Cannot bind failed Describe audit");
    struct Close { int fd; ~Close() { ::close(fd); } } close{fd};
    struct stat before,after,current;
    if (::fstat(fd,&before) || !SameIdentity(before,identity) || !S_ISREG(before.st_mode) ||
        before.st_uid!=::geteuid() || (before.st_mode&0777)!=0600 || before.st_nlink!=1 ||
        before.st_size<1 || before.st_size>8*1024*1024)
        throw std::runtime_error("Failed Describe audit identity or size changed");
    std::string bytes(static_cast<std::size_t>(before.st_size),'\0'); std::size_t offset=0;
    while (offset<bytes.size()) {
        const auto n=::read(fd,&bytes[offset],bytes.size()-offset);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) throw std::runtime_error("Cannot read complete failed Describe audit");
        offset+=std::size_t(n);
    }
    if (::fstat(fd,&after) || ::lstat(path.c_str(),&current) || !SameIdentity(before,after) ||
        !SameIdentity(before,current) || before.st_size!=after.st_size || before.st_size!=current.st_size)
        throw std::runtime_error("Failed Describe audit changed while binding");
    auto observed=ConfigurationSha256("getdaq.hardware-audit-chain:1\n"); offset=0;
    while (offset<bytes.size()) {
        const auto end=bytes.find('\n',offset);
        if (end==std::string::npos) throw std::runtime_error("Incomplete failed Describe audit record");
        ParseStrictJson(bytes.substr(offset,end-offset));
        observed=ConfigurationSha256(observed+'\n'+bytes.substr(offset,end+1-offset)); offset=end+1;
    }
    if (observed!=chain) throw std::runtime_error("Failed Describe audit differs from retained writer chain");
    return bytes;
}

} // namespace NativeEccFilesystemProof
} // namespace GetDAQ
