#include "credential.h"
#include "supervisorio.h"
#include <atomic>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
namespace GetDAQ {
class SupervisorCredential::Impl {
public:
    std::string path,bytes,token;
    SupervisorIO::Fd descriptor;
    struct stat identity;
    mutable std::atomic<bool> fault{false};
};
SupervisorCredential::SupervisorCredential(const std::string &path) : fImpl(new Impl) {
    fImpl->path=path;
    SupervisorIO::Path(path); fImpl->descriptor.value=::open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if (fImpl->descriptor.value<0 || ::fstat(fImpl->descriptor.value,&fImpl->identity)) throw std::runtime_error("Cannot open supervisor credential file");
    fImpl->bytes=SupervisorIO::ReadFile(path,66); fImpl->token=fImpl->bytes;
    if (!fImpl->token.empty() && fImpl->token.back()=='\n') { fImpl->token.pop_back(); if (!fImpl->token.empty() && fImpl->token.back()=='\r') fImpl->token.pop_back(); }
    SupervisorIO::Digest(fImpl->token); Check();
}
SupervisorCredential::~SupervisorCredential() = default;
void SupervisorCredential::Check() const {
    const auto &p=*fImpl;
    if (p.fault) throw std::runtime_error("Supervisor credential fault is latched; restart requires reconciliation");
    try {
    struct stat st,held;
    if (::lstat(p.path.c_str(),&st) || ::fstat(p.descriptor.value,&held) || !S_ISREG(st.st_mode) || !SupervisorIO::Same(st,p.identity) || !SupervisorIO::Same(st,held) ||
        st.st_uid!=::geteuid() || (st.st_mode&0777)!=0600 || SupervisorIO::ReadFile(p.path,66)!=p.bytes) throw std::runtime_error("Supervisor credential identity/content/permissions changed");
    } catch (...) { p.fault=true; throw; }
}
bool SupervisorCredential::Authorize(const std::string &header) const {
    Check(); if (header.size()!=71 || header.compare(0,7,"Bearer ")) return false;
    unsigned int different=0; for (std::size_t i=0;i<fImpl->token.size();++i) different|=static_cast<unsigned char>(fImpl->token[i])^static_cast<unsigned char>(header[i+7]); return different==0;
}
std::string SupervisorCredential::Token() const { Check(); return fImpl->token; }
}
