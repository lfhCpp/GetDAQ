#include "scanexecutionjournalstore_internal_p.h"

#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

namespace GetDAQ {
namespace ScanExecutionJournalStoreInternal {
namespace {
struct FileDescriptor {
    int value;
    explicit FileDescriptor(int descriptor) : value(descriptor) {}
    ~FileDescriptor() { if (value >= 0) ::close(value); }
    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;
};

void Require(bool okay,const char *message)
{
    if (!okay) throw std::runtime_error(message);
}

bool Same(const struct stat &left,const struct stat &right)
{
    return left.st_dev == right.st_dev && left.st_ino == right.st_ino;
}
} // namespace

std::string ReadCurrent(int directory,const std::string &leaf,
                        struct stat *identity,std::size_t maximumBytes)
{
    const int descriptor=::openat(directory,leaf.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if (descriptor < 0)
        throw std::runtime_error("Cannot open scan journal without following links");
    FileDescriptor file(descriptor);
    struct stat before,after,named;
    Require(::fstat(descriptor,&before)==0 && S_ISREG(before.st_mode) &&
            before.st_uid==::geteuid() && (before.st_mode&0777)==0600 &&
            before.st_nlink==1 && before.st_size>=1 &&
            before.st_size<=static_cast<off_t>(maximumBytes),
            "Scan journal file type, owner, permissions, links, or size are invalid");
    std::string bytes(static_cast<std::size_t>(before.st_size),'\0');
    std::size_t offset=0;
    while(offset<bytes.size()) {
        const ssize_t count=::pread(descriptor,&bytes[offset],bytes.size()-offset,
                                    static_cast<off_t>(offset));
        if(count<0 && errno==EINTR)continue;
        if(count<=0)throw std::runtime_error("Cannot read complete scan execution journal");
        offset+=static_cast<std::size_t>(count);
    }
    Require(::fstat(descriptor,&after)==0 &&
            ::fstatat(directory,leaf.c_str(),&named,AT_SYMLINK_NOFOLLOW)==0 &&
            Same(before,after) && Same(before,named) &&
            before.st_size==after.st_size && after.st_size==named.st_size,
            "Scan journal changed while being read");
    *identity=after;
    return bytes;
}

} // namespace ScanExecutionJournalStoreInternal
} // namespace GetDAQ
