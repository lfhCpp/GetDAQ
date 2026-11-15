#ifndef GETDAQ_HOSTCONTROLOWNERSHIP_H
#define GETDAQ_HOSTCONTROLOWNERSHIP_H
#include <memory>
#include <string>
#include <vector>
namespace GetDAQ {
// Lifetime reservation shared by cooperating ECC processes on ONE POSIX host.
// This is not a board-side or cross-host lock. Never unlink reservation files.
class HostControlOwnership {
public:
    HostControlOwnership(const std::string &directory, const std::vector<std::string> &boardAddresses,
                         const std::string &deploymentId, const std::string &snapshotSha256);
    ~HostControlOwnership();
    HostControlOwnership(const HostControlOwnership &) = delete;
    HostControlOwnership &operator=(const HostControlOwnership &) = delete;
    void Check() const; // Identity/content changes latch a fault; restoring files does not recover.
    bool Faulted() const;
    const std::vector<std::string> &Resources() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
