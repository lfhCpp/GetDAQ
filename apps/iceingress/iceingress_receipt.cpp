#include "iceingress.h"
#include "iceingress_helpers_p.h"

#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
void IceIngressRecorder::SaveReceipt()
{
    using IceIngressInternal::Quote;
    using IceIngressInternal::RawName;
    if (!fEnd || fConnections != 1 || !fInputClosed || fStop || !fWriterDone || fQueue.Pending() ||
        fReceived != fAccepted || fAccepted != fSynced)
        throw std::runtime_error("ICE receipt requires actual source end, complete frames and drained durable bytes");
    DIR *directory = ::opendir(fConfig.output.c_str());
    if (!directory) throw std::runtime_error("Cannot enumerate ICE receipt files");
    std::ostringstream files;
    std::uint64_t total = 0, count = 0;
    int scanError = 0;
    try {
        for (;;) {
            errno = 0;
            const auto entry = ::readdir(directory);
            if (!entry) { scanError = errno; break; }
            const std::string name = entry->d_name;
            if (!RawName(name)) continue;
            struct stat info;
            if (::lstat((fConfig.output + '/' + name).c_str(), &info) || !S_ISREG(info.st_mode) || info.st_size < 0 ||
                total > UINT64_MAX - std::uint64_t(info.st_size))
                throw std::runtime_error("ICE receipt raw file type/size differs");
            if (count++) files << ',';
            files << "{\"name\":" << Quote(name) << ",\"bytes\":" << Quote(std::to_string(info.st_size)) << '}';
            total += std::uint64_t(info.st_size);
        }
    } catch (...) {
        ::closedir(directory);
        throw;
    }
    ::closedir(directory);
    if (scanError || total != fSynced || count != fFiles)
        throw std::runtime_error("ICE receipt file set differs from writer counters");
    const std::string body = "{\"schema\":1,\"kind\":\"getdaq.ice-receiver-seal\",\"complete\":true,\"input_protocol\":\"ICE\",\"storage_mode\":" +
        Quote(fConfig.storageMode) + ",\"pid\":" + std::to_string(::getpid()) +
        ",\"control_protocol\":1,\"connections\":" + std::to_string(fConnections) +
        ",\"sender_id\":" + Quote(fConfig.senderId) + ",\"configuration_sha256\":" + Quote(fConfig.configurationSha256) +
        ",\"artifact_sha256\":" + Quote(fConfig.artifactSha256) + ",\"session_id\":" + Quote(fConfig.sessionId) +
        ",\"output_directory\":" + Quote(fConfig.output) +
        ",\"sender_eof_confirmed\":false,\"sender_end_confirmed\":true,\"completion_basis\":\"ordered-same-connection-finishStream\",\"durable\":true,\"received_bytes\":" +
        Quote(std::to_string(total)) + ",\"files\":[" + files.str() + "]}\n";
    const auto path = fConfig.output + "/.getdaq-ice-receiver-seal.json";
    const auto temporary = path + ".tmp";
    const int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0640);
    if (fd < 0) throw std::runtime_error("Cannot create exclusive ICE receipt");
    std::size_t offset = 0;
    while (offset < body.size()) {
        const auto written = ::write(fd, body.data() + offset, body.size() - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) { ::close(fd); throw std::runtime_error("Cannot write ICE receipt"); }
        offset += std::size_t(written);
    }
    const int syncError = ::fsync(fd) == 0 ? 0 : errno;
    const int closeError = ::close(fd) == 0 ? 0 : errno;
    if (syncError || closeError) throw std::runtime_error("Cannot sync/close ICE receipt");
    if (::rename(temporary.c_str(), path.c_str())) throw std::runtime_error("Cannot publish ICE receipt");
    const int directoryFd = ::open(fConfig.output.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (directoryFd < 0) { ::unlink(path.c_str()); throw std::runtime_error("Cannot open ICE receipt directory"); }
    const int error = ::fsync(directoryFd) == 0 ? 0 : errno;
    ::close(directoryFd);
    if (error) { ::unlink(path.c_str()); throw std::runtime_error("Cannot sync ICE receipt directory"); }
    fReceipt = true;
}
}
