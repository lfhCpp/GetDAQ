#include "eccserviceprocesssupervisor_p.h"

#include "configurationsha256.h"
#include "eccprotocol.h"
#include "supervisorio.h"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace GetDAQ {
namespace EccServiceProcessSupervisorInternal {

void Require(bool condition, const char *message)
{
    if (!condition) throw std::invalid_argument(message);
}

bool IsDigest(const std::string &value)
{
    return value.size() == 64 &&
        value.find_first_not_of("0123456789abcdef") == std::string::npos;
}

bool IsOperationId(const std::string &value)
{
    if (value.empty() || value.size() > 128) return false;
    for (unsigned char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'))
            return false;
    return value != "." && value != "..";
}

std::string CanonicalRegularFile(const std::string &path, bool executable)
{
    if (path.empty() || path.size() > 3300 || path[0] != '/')
        throw std::invalid_argument("ECC process paths must be absolute and bounded");
    char *resolved = ::realpath(path.c_str(), nullptr);
    if (!resolved) throw std::runtime_error("Cannot resolve ECC process path");
    const std::string canonical(resolved);
    ::free(resolved);
    if (canonical != path) throw std::invalid_argument("ECC process paths must be canonical");
    struct stat info;
    if (::lstat(path.c_str(), &info) || !S_ISREG(info.st_mode) || S_ISLNK(info.st_mode) ||
        (executable && ::access(path.c_str(), X_OK)))
        throw std::invalid_argument(executable ? "ECC service executable is not a regular executable" :
                                               "ECC service configuration is not a regular file");
    return canonical;
}


}

using namespace EccServiceProcessSupervisorInternal;

void EccServiceProcessSupervisor::Impl::ReapIfFinished()
{
    if (pid <= 0 || exitObserved) return;
    int status = 0;
    const pid_t result = ::waitpid(pid, &status, WNOHANG);
    if (result == pid) { exitObserved = true; waitStatus = status; }
    else if (result < 0 && errno != EINTR)
        throw std::runtime_error("Cannot observe managed ECC process exit");
}

bool EccServiceProcessSupervisor::Impl::Running() const
{
    if (pid <= 0 || exitObserved) return false;
    return ::kill(pid, 0) == 0 || errno == EPERM;
}

EccServiceProcessSupervisor::EccServiceProcessSupervisor() : fImpl(new Impl) {}
EccServiceProcessSupervisor::~EccServiceProcessSupervisor() { delete fImpl; }

void EccServiceProcessSupervisor::Start(const std::string &executable,
                                        const std::string &configurationFile)
{
    fImpl->ReapIfFinished();
    Require(fImpl->pid <= 0 || fImpl->exitObserved,
            "A managed ECC service process is already active");
    const std::string program = CanonicalRegularFile(executable, true);
    const std::string configuration = CanonicalRegularFile(configurationFile, false);
    const std::string executableBytes = SupervisorIO::ReadFile(program, 256U * 1024U * 1024U);
    const std::string bytes = SupervisorIO::ReadFile(configuration, 1024U * 1024U);
    int errors[2];
    if (::pipe2(errors, O_CLOEXEC)) throw std::runtime_error("Cannot create ECC process launch check pipe");
    const pid_t child = ::fork();
    if (child < 0) { ::close(errors[0]); ::close(errors[1]); throw std::runtime_error("Cannot start ECC service process"); }
    if (child == 0) {
        ::close(errors[0]);
        char *const args[] = {const_cast<char *>(program.c_str()), const_cast<char *>("--config"),
                              const_cast<char *>(configuration.c_str()), nullptr};
        ::execv(program.c_str(), args);
        const int failure = errno;
        const ssize_t ignored = ::write(errors[1], &failure, sizeof(failure));
        (void)ignored;
        _exit(127);
    }
    ::close(errors[1]);
    int failure = 0;
    std::size_t received = 0;
    while (received < sizeof(failure)) {
        const ssize_t count = ::read(errors[0], reinterpret_cast<char *>(&failure) + received,
                                     sizeof(failure) - received);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        received += static_cast<std::size_t>(count);
    }
    ::close(errors[0]);
    if (received) {
        int status = 0;
        while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {}
        throw std::runtime_error(std::string("Cannot execute ECC service: ") + std::strerror(failure));
    }
    fImpl->pid = child;
    fImpl->executableSha256 = ConfigurationSha256(executableBytes);
    fImpl->configurationSha256 = ConfigurationSha256(bytes);
    fImpl->exitObserved = false;
    fImpl->waitStatus = 0;
}

EccServiceProcessStartEvidence EccServiceProcessSupervisor::StartForScanPoint(
    const std::string &executable, const std::string &configurationFile,
    const std::string &expectedExecutableSha256,
    const std::string &expectedConfigurationSha256,
    const std::string &executionId, const std::string &executionSnapshotSha256,
    unsigned int pointIndex, const std::string &operationId)
{
    Require(IsDigest(expectedExecutableSha256) && IsDigest(expectedConfigurationSha256) &&
            IsOperationId(executionId) && IsDigest(executionSnapshotSha256) &&
            pointIndex < 256 && IsOperationId(operationId),
            "Managed ECC startup requires fixed scan and deployment identities");
    const std::string program = CanonicalRegularFile(executable, true);
    const std::string configuration = CanonicalRegularFile(configurationFile, false);
    Require(ConfigurationSha256(SupervisorIO::ReadFile(program, 256U * 1024U * 1024U)) ==
                expectedExecutableSha256 &&
            ConfigurationSha256(SupervisorIO::ReadFile(configuration, 1024U * 1024U)) ==
                expectedConfigurationSha256,
            "Managed ECC process inputs differ from the reviewed deployment");
    Start(executable, configurationFile);
    fImpl->ReapIfFinished();
    Require(fImpl->executableSha256 == expectedExecutableSha256 &&
            fImpl->configurationSha256 == expectedConfigurationSha256,
            "Managed ECC process inputs differ from the reviewed deployment");
    Require(fImpl->pid > 0 && !fImpl->exitObserved && fImpl->Running(),
            "Managed ECC process exited before startup could be observed");
    return EccServiceProcessStartEvidence(ProcessId(), operationId,
        fImpl->executableSha256, fImpl->configurationSha256, executionId,
        executionSnapshotSha256, pointIndex);
}

bool EccServiceProcessSupervisor::Running() const { return fImpl->Running(); }
int EccServiceProcessSupervisor::ProcessId() const { return fImpl->pid > 0 ? static_cast<int>(fImpl->pid) : 0; }
const std::string &EccServiceProcessSupervisor::ServiceConfigurationSha256() const
{
    return fImpl->configurationSha256;
}
const std::string &EccServiceProcessSupervisor::ExecutableSha256() const
{
    return fImpl->executableSha256;
}


}
