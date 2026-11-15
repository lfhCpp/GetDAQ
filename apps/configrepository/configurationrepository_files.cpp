#include "configurationrepository_files_p.h"

#include "../../common/experiment/configurationsha256.h"
#include "../../common/experiment/strictjson.h"

#include <algorithm>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <stdexcept>
#include <sys/file.h>
#include <unistd.h>

namespace GetDAQ {
namespace ConfigurationRepositoryInternal {
bool Same(const struct stat &left, const struct stat &right)
{
    return left.st_dev == right.st_dev && left.st_ino == right.st_ino;
}

Fd::Fd(int descriptor) : value(descriptor) {}
Fd::~Fd() { if (value >= 0) ::close(value); }

void Need(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

void Digest(const std::string &sha256)
{
    if (sha256.size() != 64 || sha256.find_first_not_of("0123456789abcdef") != std::string::npos)
        throw std::invalid_argument("Repository requires lowercase SHA-256");
}

void ValidateRootPath(const std::string &path)
{
    ValidateJsonUtf8(path);
    if (path.size() < 2 || path.size() > 3000 || path[0] != '/' || path.back() == '/' ||
        path.find('\0') != std::string::npos || path.find("//") != std::string::npos ||
        path.find("/./") != std::string::npos || path.find("/../") != std::string::npos ||
        path.substr(path.size() - 2) == "/." ||
        (path.size() >= 3 && path.substr(path.size() - 3) == "/.."))
        throw std::invalid_argument("Repository requires normalized absolute root");
}

Directory::Directory(const std::string &path)
    : fFd(::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)), fPath(path)
{
    Need(fFd.value >= 0 && ::fstat(fFd.value, &fIdentity) == 0, "Cannot open repository directory");
    Check();
}

void Directory::Check() const
{
    struct stat named, held;
    Need(::lstat(fPath.c_str(), &named) == 0 && ::fstat(fFd.value, &held) == 0 &&
         Same(named, fIdentity) && Same(named, held) && S_ISDIR(named.st_mode) &&
         named.st_uid == ::geteuid() && (named.st_mode & 0777) == 0700,
         "Repository directory identity/permissions changed");
}

void Directory::Sync() const
{
    Check();
    Need(::fsync(fFd.value) == 0, "Cannot sync repository directory");
}

void SyncParent(const std::string &path)
{
    const auto parent = path.substr(0, path.find_last_of('/'));
    Fd fd(::open((parent.empty() ? "/" : parent).c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    Need(fd.value >= 0 && ::fsync(fd.value) == 0, "Cannot sync repository parent directory");
}

void MakeDirectory(const std::string &path)
{
    Need(::mkdir(path.c_str(), 0700) == 0, "Cannot exclusively create repository directory");
    SyncParent(path);
}

bool Exists(const std::string &path)
{
    struct stat st;
    if (::lstat(path.c_str(), &st) == 0) return true;
    Need(errno == ENOENT, "Cannot inspect repository path");
    return false;
}

std::string Read(const std::string &path, std::size_t maximum, mode_t mode)
{
    Fd fd(::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC));
    struct stat before, after, named;
    Need(fd.value >= 0 && ::fstat(fd.value, &before) == 0 && S_ISREG(before.st_mode) &&
         before.st_uid == ::geteuid() && (before.st_mode & 0777) == mode && before.st_size >= 0 &&
         std::uint64_t(before.st_size) <= maximum,
         "Invalid repository file identity/permissions/size");
    std::string bytes(std::size_t(before.st_size), '\0');
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto count = ::read(fd.value, &bytes[offset], bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        Need(count > 0, "Incomplete repository read");
        offset += std::size_t(count);
    }
    Need(::fstat(fd.value, &after) == 0 && ::lstat(path.c_str(), &named) == 0 &&
         Same(before, after) && Same(before, named) && after.st_size == before.st_size &&
         named.st_size == before.st_size,
         "Repository file changed during read");
    return bytes;
}

std::string Random()
{
    Fd fd(::open("/dev/urandom", O_RDONLY | O_CLOEXEC));
    char bytes[32];
    std::size_t offset = 0;
    Need(fd.value >= 0, "Cannot open random source");
    while (offset < sizeof(bytes)) {
        const auto count = ::read(fd.value, bytes + offset, sizeof(bytes) - offset);
        if (count < 0 && errno == EINTR) continue;
        Need(count > 0, "Cannot obtain repository random identity");
        offset += std::size_t(count);
    }
    return ConfigurationSha256(std::string(bytes, sizeof(bytes)));
}

void PublishFile(const std::string &path, const std::string &bytes)
{
    const auto parent = path.substr(0, path.find_last_of('/'));
    const auto temporary = parent + "/.pending-" + Random();
    Fd fd(::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
    Need(fd.value >= 0, "Cannot create repository temporary file");
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto count = ::write(fd.value, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        Need(count > 0, "Incomplete repository write");
        offset += std::size_t(count);
    }
    Need(::fchmod(fd.value, 0400) == 0 && ::fsync(fd.value) == 0, "Cannot seal repository file");
    // Existing names are never replaced; only a completely synced file is published.
    Need(::link(temporary.c_str(), path.c_str()) == 0, "Repository file already exists or cannot publish");
    Directory(parent).Sync();
    Need(::unlink(temporary.c_str()) == 0, "Cannot remove own completed repository temporary file");
    Directory(parent).Sync();
}

std::vector<std::string> Names(const std::string &directory)
{
    DIR *raw = ::opendir(directory.c_str());
    Need(raw != nullptr, "Cannot list repository directory");
    std::vector<std::string> names;
    try {
        errno = 0;
        while (auto entry = ::readdir(raw)) {
            const std::string name = entry->d_name;
            if (name != "." && name != ".." && name.compare(0, 9, ".pending-")) names.push_back(name);
            errno = 0;
        }
        Need(errno == 0, "Repository directory listing failed");
    } catch (...) {
        ::closedir(raw);
        throw;
    }
    ::closedir(raw);
    std::sort(names.begin(), names.end());
    return names;
}

} // namespace ConfigurationRepositoryInternal
} // namespace GetDAQ
