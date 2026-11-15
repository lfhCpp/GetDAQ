#include "rawfile.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
namespace {
bool SameFile(const struct stat &left, const struct stat &right) {
    return left.st_dev == right.st_dev && left.st_ino == right.st_ino;
}
void CheckOwnedRegularFile(int descriptor, const std::string &path, struct stat *identity) {
    struct stat opened, named;
    if (::fstat(descriptor, &opened) || !S_ISREG(opened.st_mode) || opened.st_uid != ::geteuid() ||
        opened.st_nlink != 1 || (opened.st_mode & 0022) ||
        ::lstat(path.c_str(), &named) || !S_ISREG(named.st_mode) || !SameFile(opened, named))
        throw std::runtime_error("Raw storage collision is not a private, single-link regular file: " + path);
    if (identity) *identity = opened;
}
}

int OpenRawStorageFile(const std::string &path) {
    for (unsigned int attempt = 0; attempt < 2; ++attempt) {
        // A colliding path may be a FIFO. Open it nonblocking so validating
        // the file type cannot hang the receiver before fstat/lstat rejects it.
        int descriptor = ::open(path.c_str(), O_WRONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
        if (descriptor >= 0) {
            try {
                struct stat identity;
                CheckOwnedRegularFile(descriptor, path, &identity);
                if (::flock(descriptor, LOCK_EX | LOCK_NB))
                    throw std::runtime_error("Raw storage filename is already in use: " + path);
                struct stat current;
                CheckOwnedRegularFile(descriptor, path, &current);
                if (!SameFile(identity, current) || ::ftruncate(descriptor, 0) || ::lseek(descriptor, 0, SEEK_SET) < 0)
                    throw std::runtime_error("Cannot safely truncate same-name raw storage file: " + path);
                return descriptor;
            } catch (...) {
                ::close(descriptor);
                throw;
            }
        }
        if (errno != ENOENT) throw std::runtime_error("Cannot open raw storage file " + path + ": " + std::strerror(errno));
        descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0640);
        if (descriptor >= 0) {
            try {
                CheckOwnedRegularFile(descriptor, path, nullptr);
                if (::flock(descriptor, LOCK_EX | LOCK_NB))
                    throw std::runtime_error("New raw storage file could not be exclusively locked: " + path);
                return descriptor;
            } catch (...) {
                ::close(descriptor);
                throw;
            }
        }
        if (errno != EEXIST || attempt != 0)
            throw std::runtime_error("Cannot create raw storage file " + path + ": " + std::strerror(errno));
    }
    throw std::runtime_error("Raw storage filename changed repeatedly: " + path);
}
}
