#include "eccserviceprocesssupervisor_p.h"

#include "configurationsha256.h"
#include "eccprotocol.h"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <poll.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace GetDAQ {
namespace EccServiceProcessSupervisorInternal {
std::string ReadCredential(const std::string &path)
{
    CanonicalRegularFile(path, false);
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) throw std::runtime_error("Cannot open ECC service credential");
    struct stat info;
    if (::fstat(fd, &info) || !S_ISREG(info.st_mode) || info.st_uid != ::geteuid() ||
        (info.st_mode & 0077) != 0 || info.st_size < 64 || info.st_size > 65) {
        ::close(fd);
        throw std::runtime_error("ECC service credential must be private, owned and exactly 64 hex digits");
    }
    std::string value(static_cast<std::size_t>(info.st_size), '\0');
    std::size_t offset = 0;
    while (offset < value.size()) {
        const ssize_t count = ::read(fd, &value[offset], value.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ::close(fd); throw std::runtime_error("Cannot read ECC service credential"); }
        offset += static_cast<std::size_t>(count);
    }
    ::close(fd);
    if (value.size() == 65 && value.back() == '\n') value.pop_back();
    if (!IsDigest(value)) throw std::runtime_error("ECC service credential is malformed");
    return value;
}

void WriteAll(int fd, const char *data, std::size_t size)
{
    std::size_t offset = 0;
    while (offset < size) {
        const ssize_t written = ::send(fd, data + offset, size - offset, MSG_NOSIGNAL);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) throw std::runtime_error("ECC shutdown request write failed");
        offset += static_cast<std::size_t>(written);
    }
}

std::string ReadHttpResponse(int fd, unsigned int timeoutMs)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    std::string response;
    char buffer[4096];
    std::size_t expected = std::string::npos;
    while (response.size() < 65536) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) throw std::runtime_error("ECC shutdown response timed out; outcome is unknown");
        const auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
        struct pollfd descriptor = {fd, POLLIN, 0};
        int ready;
        do { ready = ::poll(&descriptor, 1, static_cast<int>(remain)); } while (ready < 0 && errno == EINTR);
        if (ready == 0) throw std::runtime_error("ECC shutdown response timed out; outcome is unknown");
        if (ready < 0 || (descriptor.revents & (POLLERR | POLLNVAL)))
            throw std::runtime_error("ECC shutdown response socket failed; outcome is unknown");
        const ssize_t count = ::recv(fd, buffer, sizeof(buffer), 0);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) throw std::runtime_error("ECC shutdown response read failed; outcome is unknown");
        if (count == 0) break;
        response.append(buffer, static_cast<std::size_t>(count));
        const std::size_t split = response.find("\r\n\r\n");
        if (split != std::string::npos && expected == std::string::npos) {
            const std::size_t lengthHeader = response.find("\r\nContent-Length:");
            if (lengthHeader == std::string::npos || lengthHeader > split)
                throw std::runtime_error("ECC shutdown response has no bounded content length");
            std::size_t begin = lengthHeader + 17;
            while (begin < split && response[begin] == ' ') ++begin;
            std::size_t end = begin;
            while (end < split && response[end] >= '0' && response[end] <= '9') ++end;
            if (end == begin || (end < split && response[end] != '\r'))
                throw std::runtime_error("ECC shutdown response content length is invalid");
            std::size_t bodyLength = 0;
            for (std::size_t i = begin; i < end; ++i) {
                const unsigned int digit = static_cast<unsigned int>(response[i] - '0');
                if (bodyLength > (8192U - digit) / 10U)
                    throw std::runtime_error("ECC shutdown response body exceeds its limit");
                bodyLength = bodyLength * 10U + digit;
            }
            expected = split + 4 + bodyLength;
        }
        if (expected != std::string::npos && response.size() >= expected) {
            if (response.size() != expected)
                throw std::runtime_error("ECC shutdown response contains trailing bytes");
            return response;
        }
    }
    throw std::runtime_error("ECC shutdown response is incomplete or exceeds its limit");
}

std::string ConnectAddress(const std::string &address)
{
    in_addr parsed = {};
    if (::inet_pton(AF_INET, address.c_str(), &parsed) != 1)
        throw std::invalid_argument("ECC shutdown requires a literal IPv4 address");
    if (address == "0.0.0.0") return "127.0.0.1";
    return address;
}

std::string RequestBytes(const std::string &instance, const std::string &snapshot,
                         const std::string &handoff)
{
    return "{\"schema\":1,\"kind\":\"getdaq.ecc-service-shutdown\",\"instance_id\":" +
        EncodeJsonString(instance) + ",\"snapshot_sha256\":" + EncodeJsonString(snapshot) +
        ",\"handoff_operation_id\":" + EncodeJsonString(handoff) + "}\n";
}

}

using namespace EccServiceProcessSupervisorInternal;

EccServiceProcessExitEvidence EccServiceProcessSupervisor::ShutdownAndWait(
    const std::string &address, unsigned int port, const std::string &credentialFile,
    const std::string &instanceId, const std::string &snapshotSha256,
    const std::string &handoffOperationId, const std::string &retirementOperationId,
    unsigned int timeoutMs,
    const std::string &executionId, const std::string &executionSnapshotSha256,
    unsigned int pointIndex, const std::string &runtimeIdentitySha256)
{
    Require(fImpl->pid > 0 && !fImpl->exitObserved && fImpl->Running(),
            "ECC shutdown requires the exact managed child to be running");
    Require(port > 0 && port <= 65535 && timeoutMs >= 100 && timeoutMs <= 60000,
            "Invalid ECC shutdown endpoint or timeout");
    Require(IsDigest(instanceId) && IsDigest(snapshotSha256) && IsOperationId(handoffOperationId),
            "ECC shutdown requires fixed instance, snapshot and handoff operation identities");
    Require(IsOperationId(retirementOperationId),
            "ECC shutdown requires the durable scan retirement operation identity");
    Require(IsOperationId(executionId) && IsDigest(executionSnapshotSha256) &&
            pointIndex < 256 && IsDigest(runtimeIdentitySha256),
            "ECC shutdown requires a fixed scan journal and runtime identity binding");
    const std::string host = ConnectAddress(address);
    const std::string token = ReadCredential(credentialFile);
    const std::string body = RequestBytes(instanceId, snapshotSha256, handoffOperationId);

    const int socketFd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (socketFd < 0) throw std::runtime_error("Cannot create ECC shutdown socket");
    struct timeval timeout = {static_cast<time_t>(timeoutMs / 1000U),
                              static_cast<suseconds_t>((timeoutMs % 1000U) * 1000U)};
    (void)::setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)::setsockopt(socketFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    sockaddr_in endpoint = {};
    endpoint.sin_family = AF_INET;
    endpoint.sin_port = htons(static_cast<unsigned short>(port));
    if (::inet_pton(AF_INET, host.c_str(), &endpoint.sin_addr) != 1) {
        ::close(socketFd); throw std::invalid_argument("Invalid ECC shutdown IPv4 endpoint");
    }
    if (::connect(socketFd, reinterpret_cast<sockaddr *>(&endpoint), sizeof(endpoint))) {
        ::close(socketFd); throw std::runtime_error("Cannot connect to managed ECC service for shutdown");
    }
    const std::string request = "POST /v1/shutdown HTTP/1.1\r\nHost: " + host + ":" +
        std::to_string(port) + "\r\nAuthorization: Bearer " + token +
        "\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " +
        std::to_string(body.size()) + "\r\n\r\n" + body;
    std::string response;
    try { WriteAll(socketFd, request.data(), request.size()); response = ReadHttpResponse(socketFd, timeoutMs); }
    catch (...) { ::close(socketFd); throw; }
    ::close(socketFd);

    const std::size_t lineEnd = response.find("\r\n");
    if (lineEnd == std::string::npos || response.compare(0, lineEnd, "HTTP/1.1 200 OK") != 0)
        throw std::runtime_error("ECC service did not accept authenticated shutdown; preserve process and journal evidence");
    const std::size_t split = response.find("\r\n\r\n");
    if (split == std::string::npos || response.substr(split + 4) !=
        "{\"schema\":1,\"kind\":\"getdaq.ecc-service-shutdown-accepted\",\"instance_id\":" +
        EncodeJsonString(instanceId) + ",\"snapshot_sha256\":" + EncodeJsonString(snapshotSha256) +
        ",\"handoff_operation_id\":" + EncodeJsonString(handoffOperationId) + "}\n")
        throw std::runtime_error("ECC shutdown response does not match the exact request; outcome is unknown");
    const std::string responseBody = response.substr(split + 4);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        int status = 0;
        const pid_t waited = ::waitpid(fImpl->pid, &status, WNOHANG);
        if (waited == fImpl->pid) { fImpl->exitObserved = true; fImpl->waitStatus = status; break; }
        if (waited < 0 && errno != EINTR) throw std::runtime_error("Cannot wait for managed ECC service process");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!fImpl->exitObserved)
        throw std::runtime_error("ECC shutdown was accepted but managed process exit timed out; preserve unknown retirement");
    if (!WIFEXITED(fImpl->waitStatus) || WEXITSTATUS(fImpl->waitStatus) != 0)
        throw std::runtime_error("ECC shutdown was accepted but managed service did not exit normally with status 0");

    return EccServiceProcessExitEvidence(
        static_cast<int>(fImpl->pid), 0, fImpl->configurationSha256,
        instanceId, snapshotSha256, handoffOperationId, retirementOperationId,
        ConfigurationSha256(body), ConfigurationSha256(responseBody),
        executionId, executionSnapshotSha256, pointIndex, runtimeIdentitySha256);
}


} // namespace GetDAQ
