#include "hardwareaudit.h"
#include "configurationsha256.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
namespace GetDAQ {
void CreateHardwareAuditDirectory(const std::string &path)
{
    if (path.empty() || path.size() > 4096 || path.back() == '/') throw std::invalid_argument("Invalid bounded audit directory");
    for (unsigned char c : path) if (c < 32 || c == 127) throw std::invalid_argument("Control byte in audit directory");
    if (::mkdir(path.c_str(),0700) != 0) throw std::runtime_error("Cannot exclusively create audit directory: "+std::string(std::strerror(errno)));
    const auto slash = path.find_last_of('/');
    const auto parent = slash == std::string::npos ? std::string(".") : slash == 0 ? std::string("/") : path.substr(0,slash);
    const int fd = ::open(parent.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    const bool synced = fd >= 0 && ::fsync(fd) == 0;
    if (fd >= 0) ::close(fd);
    if (!synced) throw std::runtime_error("Cannot sync audit directory parent; no hardware changes sent");
}
HardwareAudit::HardwareAudit(const std::string &path)
{
    fChainSha256=ConfigurationSha256("getdaq.hardware-audit-chain:1\n");
    if (path.empty() || path.find('\0') != std::string::npos) throw std::invalid_argument("Invalid audit path");
    fFd = ::open(path.c_str(),O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,0600);
    if (fFd < 0) throw std::runtime_error("Cannot exclusively create audit: " + std::string(std::strerror(errno)));
    const auto slash = path.find_last_of('/');
    const std::string parent = slash == std::string::npos ? "." : slash == 0 ? "/" : path.substr(0,slash);
    const int directory = ::open(parent.c_str(),O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    const bool ok = directory >= 0 && ::fsync(directory) == 0;
    if (directory >= 0) ::close(directory);
    if (!ok) { ::close(fFd); fFd = -1; throw std::runtime_error("Cannot sync audit directory; no write sent"); }
}
HardwareAudit::~HardwareAudit() { if (fFd >= 0) ::close(fFd); }
void HardwareAudit::Append(const std::string &record)
{
    const std::string line = record + "\n"; std::size_t sent = 0;
    while (sent < line.size()) {
        const ssize_t bytes = ::write(fFd,line.data()+sent,line.size()-sent);
        if (bytes < 0 && errno == EINTR) continue;
        if (bytes <= 0) throw std::runtime_error("Cannot write hardware audit");
        sent += static_cast<std::size_t>(bytes); fBytes += std::uint64_t(bytes);
    }
    if (::fsync(fFd) != 0) throw std::runtime_error("Cannot sync hardware audit");
    fChainSha256=ConfigurationSha256(fChainSha256+'\n'+line);
}
}
