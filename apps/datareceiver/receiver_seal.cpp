#include "receiver.h"
#include "receiver_internal.h"
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
void Receiver::SaveSeal()
{
    if (fConfig.cycleControlStdin && (!fCycleFinish || fCycleControlLost)) throw std::runtime_error("Cycle seal requires the private finish request; controller loss or signal is not completion");
    // 文件枚举和大小验证不读取原始事件。锁仍由本进程持有，退出后代理才能归档。
    std::ostringstream files; uint64_t total = 0; bool first = true;
    DIR *dir = ::opendir(fConfig.output.c_str());
    if (!dir) throw std::runtime_error("Cannot enumerate files for receiver seal");
    try {
        while (dirent *entry = ::readdir(dir)) {
            const std::string name = entry->d_name;
            if (name.size() < 5 || name.substr(name.size() - 5) != ".graw") continue;
            struct stat info;
            if (::lstat((fConfig.output + "/" + name).c_str(), &info) || !S_ISREG(info.st_mode) || info.st_size < 0)
                throw std::runtime_error("Non-regular or unreadable raw file in output directory");
            total += uint64_t(info.st_size);
            if (!first) files << ',';
            first = false;
            files << "{\"name\":" << ReceiverInternal::QuoteJson(name) << ",\"bytes\":" << ReceiverInternal::QuoteJson(std::to_string(info.st_size)) << '}';
        }
    } catch (...) { ::closedir(dir); throw; }
    ::closedir(dir);
    if (total != fSynced || fSynced != fReceived.load() - fControlBytes.load() || !fSealed)
        throw std::runtime_error("Cannot seal: files differ from received bytes (use an empty incoming directory)");
    if (fCycleUnused && (fConnections || fEofs || fReceived || fFrames || fFiles || !first))
        throw std::runtime_error("Cannot confirm unused source: a connection, event or raw file exists");
    const std::string body = "{\"schema\":1,\"complete\":true,\"control_protocol\":1,\"pid\":"+std::to_string(::getpid())+
        ",\"session_id\":"+ReceiverInternal::QuoteJson(fConfig.sessionId)+",\"sender_id\":"+ReceiverInternal::QuoteJson(fConfig.senderId)+
        ",\"configuration_sha256\":"+ReceiverInternal::QuoteJson(fConfig.configurationSha256)+",\"artifact_sha256\":"+ReceiverInternal::QuoteJson(fConfig.artifactSha256)+
        ",\"output_directory\":"+ReceiverInternal::QuoteJson(fConfig.output)+",\"connections\":"+std::to_string(fConnections.load())+
        ",\"sender_eof_confirmed\":"+std::string(fConnections>0 && fEofs==fConnections ? "true" : "false")+
        (fConfig.storageMode != "frame-storage" ? ",\"storage_mode\":"+ReceiverInternal::QuoteJson(fConfig.storageMode) : "")+
        (fCycleUnused ? ",\"source_unused_confirmed\":true" : "")+
        ",\"durable\":" + std::string(fConfig.durable ? "true" : "false")
        + ",\"received_bytes\":" + ReceiverInternal::QuoteJson(std::to_string(total)) + ",\"files\":[" + files.str() + "]}\n";
    const std::string path = fConfig.output + "/.getdaq-receiver-seal.json";
    const std::string temp = path + ".tmp";
    const int fd = ::open(temp.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0640);
    if (fd < 0) throw std::runtime_error("Cannot create receiver seal");
    std::size_t offset = 0;
    while (offset < body.size()) {
        const ssize_t n = ::write(fd, body.data() + offset, body.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { ::close(fd); throw std::runtime_error("Cannot write receiver seal"); }
        offset += std::size_t(n);
    }
    const int syncError = ::fsync(fd) == 0 ? 0 : errno;
    const int closeError = ::close(fd) == 0 ? 0 : errno;
    if (syncError) throw std::runtime_error("Receiver seal fsync failed: " + std::string(std::strerror(syncError)));
    if (closeError) throw std::runtime_error("Receiver seal close failed: " + std::string(std::strerror(closeError)));
    if (::rename(temp.c_str(), path.c_str())) throw std::runtime_error("Receiver seal rename failed: " + std::string(std::strerror(errno)));
    const int directory = ::open(fConfig.output.c_str(), O_RDONLY);
    if (directory < 0) throw std::runtime_error("Cannot open seal directory");
    const int directoryError = ::fsync(directory) == 0 ? 0 : errno; ::close(directory);
    if (directoryError) { ::unlink(path.c_str()); throw std::runtime_error("Receiver seal directory fsync failed: " + std::string(std::strerror(directoryError))); }
}
}
