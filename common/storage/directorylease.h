#ifndef GETDAQ_DIRECTORYLEASE_H
#define GETDAQ_DIRECTORYLEASE_H
// 仅由 Linux/macOS 接收器和同机归档代理使用；不依赖 Qt。
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <stdexcept>
#include <string>
namespace GetDAQ {
class DirectoryLease {
public:
    explicit DirectoryLease(const std::string &directory) {
        fFd = ::open((directory + "/.getdaq-receiver.lock").c_str(), O_CREAT | O_RDWR, 0640);
        if (fFd < 0) throw std::runtime_error("Cannot open receiver/archive directory lock");
        if (::flock(fFd, LOCK_EX | LOCK_NB) != 0) {
            ::close(fFd); fFd = -1;
            throw std::runtime_error("Output directory already used by another receiver or catalog operation");
        }
    }
    ~DirectoryLease() { if (fFd >= 0) ::close(fFd); }
    DirectoryLease(const DirectoryLease &) = delete;
    DirectoryLease &operator=(const DirectoryLease &) = delete;
private:
    int fFd = -1;
};
}
#endif
