#include "scanexecutionjournalstore_publish_p.h"

#include <atomic>
#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

namespace GetDAQ {
namespace ScanExecutionJournalStoreInternal {
namespace {

std::atomic<unsigned long> TemporarySequence(0);

struct FileDescriptor {
    int value;
    explicit FileDescriptor(int descriptor = -1) : value(descriptor) {}
    ~FileDescriptor() { if (value >= 0) ::close(value); }
    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;
};

void Require(bool okay, const char *message)
{
    if (!okay) throw std::runtime_error(message);
}

bool Same(const struct stat &left, const struct stat &right)
{
    return left.st_dev == right.st_dev && left.st_ino == right.st_ino;
}

void Sync(int descriptor, const char *message)
{
    int result;
    do { result = ::fsync(descriptor); } while (result != 0 && errno == EINTR);
    if (result != 0) throw std::runtime_error(message);
}

void WriteAll(int descriptor, const std::string &bytes)
{
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const ssize_t count = ::write(descriptor, bytes.data() + offset,
                                      bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) throw std::runtime_error("Cannot write scan execution journal");
        offset += static_cast<std::size_t>(count);
    }
}

std::string TemporaryName()
{
    return ".scan-journal-" + std::to_string(static_cast<unsigned long>(::getpid())) +
           "-" + std::to_string(TemporarySequence.fetch_add(1)) + ".tmp";
}

void WriteTemporary(int directory, const std::string &name,
                    const std::string &bytes)
{
    const int descriptor = ::openat(directory, name.c_str(), O_WRONLY | O_CREAT |
                                    O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (descriptor < 0) throw std::runtime_error("Cannot create private scan journal temporary file");
    FileDescriptor file(descriptor);
    struct stat identity;
    Require(::fstat(descriptor, &identity) == 0 && S_ISREG(identity.st_mode) &&
            identity.st_uid == ::geteuid() && (identity.st_mode & 0777) == 0600 &&
            identity.st_nlink == 1,
            "Scan journal temporary file identity is unsafe");
    WriteAll(descriptor, bytes);
    Sync(descriptor, "Cannot fsync scan journal temporary file");
    struct stat held, named;
    Require(::fstat(descriptor, &held) == 0 &&
            ::fstatat(directory, name.c_str(), &named, AT_SYMLINK_NOFOLLOW) == 0 &&
            Same(identity, held) && Same(identity, named) &&
            held.st_size == static_cast<off_t>(bytes.size()),
            "Scan journal temporary file changed before publication");
}

} // namespace

void PublishNew(int directory, const std::string &leaf, const std::string &bytes)
{
    const std::string temporary = TemporaryName();
    try {
        WriteTemporary(directory, temporary, bytes);
        if (::linkat(directory, temporary.c_str(), directory, leaf.c_str(), 0) != 0)
            throw std::runtime_error("Cannot publish new scan journal without replacing existing data");
        if (::unlinkat(directory, temporary.c_str(), 0) != 0)
            throw std::runtime_error("Cannot remove scan journal publication link");
        Sync(directory, "Cannot sync scan journal directory after creation");
    } catch (...) {
        ::unlinkat(directory, temporary.c_str(), 0);
        throw;
    }
}

void PublishReplace(int directory, const std::string &leaf,
                    const struct stat &expectedIdentity,
                    const std::string &bytes)
{
    const std::string temporary = TemporaryName();
    try {
        WriteTemporary(directory, temporary, bytes);
        struct stat named;
        Require(::fstatat(directory, leaf.c_str(), &named, AT_SYMLINK_NOFOLLOW) == 0 &&
                Same(expectedIdentity, named) && S_ISREG(named.st_mode) &&
                named.st_uid == ::geteuid() && (named.st_mode & 0777) == 0600 &&
                named.st_nlink == 1,
                "Scan journal changed before atomic replacement");
        if (::renameat(directory, temporary.c_str(), directory, leaf.c_str()) != 0)
            throw std::runtime_error("Cannot atomically replace scan journal");
        Sync(directory, "Cannot sync scan journal directory after replacement");
    } catch (...) {
        ::unlinkat(directory, temporary.c_str(), 0);
        throw;
    }
}

} // namespace ScanExecutionJournalStoreInternal
} // namespace GetDAQ
