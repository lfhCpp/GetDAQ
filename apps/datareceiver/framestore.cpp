#include "framestore.h"
#include "rawfile.h"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

namespace GetDAQ {
namespace {
void WriteAll(int fd, const uint8_t *data, std::size_t size)
{
    while (size) {
        const ssize_t n = ::write(fd, data, size);
        if (n < 0 && errno == EINTR) continue;
        if (n == 0) throw std::runtime_error("File write returned zero bytes");
        if (n < 0) throw std::runtime_error(std::string("File write failed: ") + std::strerror(errno));
        data += n; size -= std::size_t(n);
    }
}
std::string GetTimestamp()
{
    const auto usec = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (usec < 0) throw std::runtime_error("Cannot name GET files before the Unix epoch");
    return FrameStore::FormatGetTimestamp(uint64_t(usec));
}
}
std::string FrameStore::FormatGetTimestamp(uint64_t usec)
{
    if (usec / 1000000 > uint64_t(std::numeric_limits<time_t>::max())) throw std::invalid_argument("GET timestamp exceeds time_t range");
    const time_t seconds = time_t(usec / 1000000); struct tm local;
    if (!::localtime_r(&seconds, &local)) throw std::runtime_error("Cannot convert GET filename timestamp");
    char stamp[32];
    if (!::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", &local)) throw std::runtime_error("GET filename timestamp exceeds bounds");
    std::ostringstream out; out << stamp << '.' << std::setfill('0') << std::setw(3) << (usec % 1000000 / 1000);
    return out.str();
}
FrameStore::FrameStore(const std::string &directory, uint64_t rotationBytes, bool durable, const std::string &naming,
                       uint64_t minimumFreeBytes)
    : fDirectory(directory), fNaming(naming), fRotation(rotationBytes), fMinimumFreeBytes(minimumFreeBytes), fDurable(durable)
{
    if (naming != "native" && naming != "get") throw std::invalid_argument("File naming must be native or get");
    struct stat s;
    if (::stat(directory.c_str(), &s) != 0 || !S_ISDIR(s.st_mode)) throw std::runtime_error("Output directory must already exist: " + directory);
    CheckFreeSpace(0);
    fPrefix = naming == "get" ? GetTimestamp() :
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "_" + std::to_string(::getpid());
}
FrameStore::~FrameStore()
{
    // 正常退出必须显式 Seal；异常时只关闭，不谎报成功，也不在析构函数抛异常。
    for (auto &entry : fOpen) if (entry.second.fd >= 0) ::close(entry.second.fd);
}
void FrameStore::Flush(File &file)
{
    if (!file.buffer.empty()) {
        while (file.offset < file.buffer.size()) {
            const ssize_t n = ::write(file.fd, file.buffer.data() + file.offset, file.buffer.size() - file.offset);
            if (n < 0 && errno == EINTR) continue;
            if (n == 0) throw std::runtime_error("Buffered file write returned zero bytes");
            if (n < 0) throw std::runtime_error(std::string("Buffered file write failed: ") + std::strerror(errno));
            file.offset += std::size_t(n);
        }
        file.buffer.clear(); file.offset = 0;
    }
}
void FrameStore::Close(File &file)
{
    if (file.fd < 0) return;
    Flush(file);
    if (fDurable && ::fsync(file.fd) != 0) throw std::runtime_error(std::string("fsync failed: ") + std::strerror(errno));
    const int fd = file.fd; file.fd = -1;
    if (::close(fd) != 0) throw std::runtime_error("File close failed");
}
void FrameStore::CheckFreeSpace(uint64_t incomingBytes) const
{
    if (!fMinimumFreeBytes) return;
    struct statvfs space;
    if (::statvfs(fDirectory.c_str(), &space) != 0)
        throw DiskReserveExceeded(std::string("Cannot verify free disk reserve: ") + std::strerror(errno));
    const uint64_t unit = uint64_t(space.f_frsize ? space.f_frsize : space.f_bsize);
    const uint64_t blocks = uint64_t(space.f_bavail);
    const uint64_t available = unit && blocks > UINT64_MAX / unit ? UINT64_MAX : blocks * unit;
    uint64_t pending = 0;
    for (const auto &entry : fOpen) {
        const File &file = entry.second;
        const std::size_t remaining = file.buffer.size() - file.offset;
        if (uint64_t(remaining) > UINT64_MAX - pending) { pending = UINT64_MAX; break; }
        pending += uint64_t(remaining);
    }
    // Keep an extra MiB for filesystem metadata and allocation granularity. The check
    // runs on the writer thread and accounts for small frames still buffered in memory.
    if (!DiskReserveAllows(available,fMinimumFreeBytes,pending,incomingBytes))
        throw DiskReserveExceeded("Minimum free disk reserve reached; acquisition is incomplete and the output is not sealed");
}
void FrameStore::Write(const uint8_t *data, const FrameInfo &info)
{
    const uint64_t incoming=uint64_t(info.bytes);
    if (fMinimumFreeBytes && DiskReserveCheckNeeded(fBytesSinceSpaceCheck,incoming)) {
        CheckFreeSpace(incoming);
        fBytesSinceSpaceCheck=0;
    }
    const std::string key = info.coboFrame ? "CoBo" + std::to_string(info.cobo) + "_AsAd" + std::to_string(info.asad)
        : info.mutantFrame ? "MuTanT" : "MFM" + std::to_string(info.type) + "_Source" + std::to_string(info.source);
    if (!fOpen.count(key) && fOpen.size() >= 1024) throw std::runtime_error("Too many data sources on one receiver");
    File &file = fOpen[key];
    // 兼容旧 FrameStorage：event <= previousEvent 时开始新文件；不重排事件。
    if (file.fd >= 0 && ((info.hasEvent && file.hasEvent && info.event <= file.event) || file.bytes >= fRotation)) Close(file);
    if (file.fd < 0) {
        uint64_t index = fFiles;
        if (fNaming == "get") {
            if (!fNextIndex.count(key) && fNextIndex.size() >= 1024) throw std::runtime_error("Too many lifetime GET file sources");
            index = fNextIndex[key];
            if (index == UINT64_MAX) throw std::runtime_error("GET source file index exhausted");
        }
        std::ostringstream suffix;
        if (fNaming == "get") suffix << std::setfill('0') << std::setw(4);
        suffix << index;
        const std::string name = fDirectory + "/" + key + "_" + fPrefix + "_" + suffix.str() + ".graw";
        file.fd = OpenRawStorageFile(name);
        if (fNaming == "get") ++fNextIndex[key];
        ++fFiles; file.bytes = 0; file.buffer.reserve(256 * 1024);
    }
    // 小帧批量顺序写；大帧直接写，避免为了凑批次多复制一份大帧。
    const std::size_t batch = 256 * 1024;
    if (file.buffer.size() + info.bytes > batch) Flush(file);
    if (info.bytes >= batch) WriteAll(file.fd, data, info.bytes);
    else file.buffer.insert(file.buffer.end(), data, data + info.bytes);
    file.bytes += info.bytes; fBytes += info.bytes; file.event = info.event; file.hasEvent = info.hasEvent;
    if (fMinimumFreeBytes) {
        fBytesSinceSpaceCheck = incoming > DiskReserveCheckIntervalBytes-fBytesSinceSpaceCheck ?
            DiskReserveCheckIntervalBytes : fBytesSinceSpaceCheck+incoming;
    }
}
void FrameStore::Abandon()
{
    for (auto &entry : fOpen) {
        File &file = entry.second;
        if (file.fd >= 0) { ::close(file.fd); file.fd = -1; }
        file.buffer.clear(); file.offset = 0;
    }
    fOpen.clear();
}
void FrameStore::Seal()
{
    for (auto &entry : fOpen) Close(entry.second);
    if (fDurable) {
        const int fd = ::open(fDirectory.c_str(), O_RDONLY);
        if (fd < 0) throw std::runtime_error("Cannot open directory for sync");
        const int error = ::fsync(fd) == 0 ? 0 : errno; ::close(fd);
        if (error) throw std::runtime_error(std::string("Directory fsync failed: ") + std::strerror(error));
    }
    fOpen.clear();
}
}
