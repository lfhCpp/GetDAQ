#include "repositorycredential.h"
#include "configurationsha256.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <stdexcept>
namespace GetDAQ {
namespace {
void Need(bool ok) { if (!ok) throw std::runtime_error("Repository credential identity/content/permissions invalid"); }
std::string Read(int fd) {
    Need(::lseek(fd,0,SEEK_SET)==0);char data[67];std::size_t total=0;
    while (total<sizeof(data)) { const auto n=::read(fd,data+total,sizeof(data)-total);if (n<0 && errno==EINTR) continue;Need(n>=0);if (!n) break;total+=std::size_t(n); }
    Need(total==64 || (total==65 && data[64]=='\n'));return std::string(data,total);
}
}
class RepositoryCredential::Impl {
public:
    int fd=-1;std::string path,bytes,token;struct stat identity;mutable bool fault=false;
    explicit Impl(const std::string &p):path(p) {
        fd=::open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);Need(fd>=0);
        try { Need(::fstat(fd,&identity)==0);bytes=Read(fd);token=bytes.substr(0,64);Need(token.find_first_not_of("0123456789abcdef")==std::string::npos);Check(); }
        catch (...) {::close(fd);fd=-1;throw;}
    }
    ~Impl() {if (fd>=0) ::close(fd);}
    void Check() const {
        if (fault) throw std::runtime_error("Repository credential fault is latched");
        try {
            struct stat current,held;Need(::lstat(path.c_str(),&current)==0 && ::fstat(fd,&held)==0 && S_ISREG(current.st_mode) && current.st_uid==::geteuid() && (current.st_mode&0777)==0600 &&
                current.st_dev==identity.st_dev && current.st_ino==identity.st_ino && current.st_dev==held.st_dev && current.st_ino==held.st_ino && Read(fd)==bytes);
        } catch (...) {fault=true;throw;}
    }
};
RepositoryCredential::RepositoryCredential(const std::string &path):fImpl(new Impl(path)) {}
RepositoryCredential::~RepositoryCredential() {}
bool RepositoryCredential::Authorize(const std::string &header) const {
    fImpl->Check();if (header.size()!=71 || header.compare(0,7,"Bearer ")) return false;
    unsigned different=0;for (std::size_t i=0;i<64;++i) different|=static_cast<unsigned char>(header[7+i])^static_cast<unsigned char>(fImpl->token[i]);return different==0;
}
void RepositoryCredential::Generate(const std::string &path) {
    int random=::open("/dev/urandom",O_RDONLY|O_CLOEXEC);Need(random>=0);char bytes[32];std::size_t offset=0;
    while (offset<sizeof(bytes)) { const auto n=::read(random,bytes+offset,sizeof(bytes)-offset);if (n<0 && errno==EINTR) continue;if (n<=0) {::close(random);Need(false);}offset+=std::size_t(n); }
    ::close(random);const auto token=ConfigurationSha256(std::string(bytes,sizeof(bytes)))+'\n';const int fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);Need(fd>=0);offset=0;
    try {
        while (offset<token.size()) {const auto n=::write(fd,token.data()+offset,token.size()-offset);if (n<0 && errno==EINTR) continue;Need(n>0);offset+=std::size_t(n);}
        Need(::fsync(fd)==0);::close(fd);
    } catch (...) {::close(fd);throw;}
}
}
