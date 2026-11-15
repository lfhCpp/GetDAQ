#include "bytestore.h"
#include "framestore.h"
#include "rawfile.h"
#include "mfmstream.h"
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
const uint64_t MetadataReserve = 1024ull * 1024;
const uint64_t SpaceCheckWindow = 1024ull * 1024;
void WriteAll(int fd, const uint8_t *data, std::size_t bytes) {
    while (bytes) {
        const ssize_t n = ::write(fd, data, bytes);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) throw std::runtime_error(std::string("ByteStorage write failed: ") +
                                             (n == 0 ? "zero bytes" : std::strerror(errno)));
        data += n;
        bytes -= std::size_t(n);
    }
}
std::string Timestamp() {
    const auto usec = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (usec < 0) throw std::runtime_error("Cannot name ByteStorage files before the Unix epoch");
    return FrameStore::FormatGetTimestamp(uint64_t(usec));
}
}
ByteStore::ByteStore(const std::string &directory, uint64_t rotateBytes, bool durable,
                     const std::string &naming, uint64_t minimumFreeBytes)
    : fDirectory(directory), fNaming(naming), fRotate(rotateBytes), fMinimumFree(minimumFreeBytes), fDurable(durable) {
    if (!rotateBytes) throw std::invalid_argument("ByteStorage rotation size must be positive");
    if (naming != "native" && naming != "get") throw std::invalid_argument("File naming must be native or get");
    struct stat info;
    if (::stat(directory.c_str(), &info) != 0 || !S_ISDIR(info.st_mode))
        throw std::runtime_error("Output directory must already exist: " + directory);
    CheckFreeSpace(0);
    if (naming == "get") fPrefix = Timestamp();
    else fPrefix = std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "_" +
                   std::to_string(::getpid());
}
ByteStore::~ByteStore() { if (fFd >= 0) ::close(fFd); }
void ByteStore::CheckFreeSpace(uint64_t incomingBytes) const {
    if (!fMinimumFree) return;
    struct statvfs space;
    if (::statvfs(fDirectory.c_str(), &space) != 0)
        throw DiskReserveExceeded(std::string("Cannot verify free disk reserve: ") + std::strerror(errno));
    const uint64_t unit = uint64_t(space.f_frsize ? space.f_frsize : space.f_bsize);
    const uint64_t blocks = uint64_t(space.f_bavail);
    const uint64_t available = unit && blocks > UINT64_MAX / unit ? UINT64_MAX : blocks * unit;
    if (available < fMinimumFree || available - fMinimumFree < MetadataReserve ||
        available - fMinimumFree - MetadataReserve < incomingBytes)
        throw DiskReserveExceeded("Minimum free disk reserve reached; acquisition is incomplete and the output is not sealed");
}
void ByteStore::OpenNext() {
    std::ostringstream name;
    name << fDirectory << "/ByteStorage_" << fPrefix << '_';
    if (fNaming == "get") name << std::setfill('0') << std::setw(4);
    name << fFiles << ".graw";
    fFd = OpenRawStorageFile(name.str());
    ++fFiles;
    fFileBytes = 0;
}
void ByteStore::Close(bool sync) {
    if (fFd < 0) return;
    const int fd = fFd;
    fFd = -1;
    if (sync && fDurable && ::fsync(fd) != 0) {
        const std::string error = std::strerror(errno);
        ::close(fd);
        throw std::runtime_error("ByteStorage fsync failed: " + error);
    }
    if (::close(fd) != 0) throw std::runtime_error(std::string("ByteStorage close failed: ") + std::strerror(errno));
}
void ByteStore::Write(const uint8_t *data, std::size_t bytes) {
    if (!bytes) return;
    if (!data) throw std::invalid_argument("ByteStorage received a null byte range");
    if (uint64_t(bytes) > UINT64_MAX - fBytes) throw std::runtime_error("ByteStorage byte counter overflow");
    if (fMinimumFree && (fBytesSinceSpaceCheck >= SpaceCheckWindow ||
        uint64_t(bytes) > SpaceCheckWindow - fBytesSinceSpaceCheck)) {
        CheckFreeSpace(uint64_t(bytes));
        fBytesSinceSpaceCheck = 0;
    }
    if (fFd >= 0 && fFileBytes >= fRotate) Close(true);
    if (fFd < 0) OpenNext();
    WriteAll(fFd, data, bytes);
    fFileBytes += uint64_t(bytes);
    fBytes += uint64_t(bytes);
    if (fMinimumFree) fBytesSinceSpaceCheck = uint64_t(bytes) > SpaceCheckWindow - fBytesSinceSpaceCheck
        ? SpaceCheckWindow : fBytesSinceSpaceCheck + uint64_t(bytes);
}
void ByteStore::Abandon() { Close(false); }
void ByteStore::Seal() {
    Close(true);
    if (fDurable) {
        const int fd = ::open(fDirectory.c_str(), O_RDONLY);
        if (fd < 0) throw std::runtime_error("Cannot open ByteStorage directory for sync");
        const int error = ::fsync(fd) == 0 ? 0 : errno;
        ::close(fd);
        if (error) throw std::runtime_error(std::string("ByteStorage directory fsync failed: ") + std::strerror(error));
    }
}
}
