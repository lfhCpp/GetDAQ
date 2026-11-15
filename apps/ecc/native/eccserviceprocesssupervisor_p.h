#ifndef GETDAQ_ECCSERVICEPROCESSSUPERVISOR_P_H
#define GETDAQ_ECCSERVICEPROCESSSUPERVISOR_P_H

#include "eccserviceprocesssupervisor.h"

#include <string>
#include <sys/types.h>

namespace GetDAQ {
namespace EccServiceProcessSupervisorInternal {

void Require(bool condition, const char *message);
bool IsDigest(const std::string &value);
bool IsOperationId(const std::string &value);
std::string CanonicalRegularFile(const std::string &path, bool executable);
std::string ReadCredential(const std::string &path);
void WriteAll(int fd, const char *data, std::size_t size);
std::string ReadHttpResponse(int fd, unsigned int timeoutMs);
std::string ConnectAddress(const std::string &address);
std::string RequestBytes(const std::string &instance, const std::string &snapshot,
                         const std::string &handoff);

} // namespace EccServiceProcessSupervisorInternal

class EccServiceProcessSupervisor::Impl {
public:
    pid_t pid = -1;
    std::string executableSha256;
    std::string configurationSha256;
    bool exitObserved = false;
    int waitStatus = 0;

    void ReapIfFinished();
    bool Running() const;
};

} // namespace GetDAQ

#endif
