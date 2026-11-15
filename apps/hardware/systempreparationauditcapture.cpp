#include "systempreparationauditcapture_p.h"

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdexcept>

namespace GetDAQ {

std::string ReadSystemPreparationConfigureAudit(const std::string &path) {
    const int fd=::open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if (fd<0) throw std::runtime_error("Cannot read completed Configure audit");
    struct Close {int fd;~Close(){::close(fd);}} close{fd};
    struct stat before,after,current;
    if (::fstat(fd,&before) || !S_ISREG(before.st_mode) || before.st_uid!=::geteuid() || (before.st_mode&0777)!=0600 ||
        before.st_nlink!=1 || before.st_size<1 || before.st_size>8*1024*1024)
        throw std::runtime_error("Configure audit identity/permissions/size invalid");
    std::string bytes(static_cast<std::size_t>(before.st_size),'\0');std::size_t offset=0;
    while (offset<bytes.size()) {
        const auto n=::read(fd,&bytes[offset],bytes.size()-offset);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) throw std::runtime_error("Configure audit could not be read completely");
        offset+=std::size_t(n);
    }
    if (::fstat(fd,&after) || ::lstat(path.c_str(),&current) || current.st_dev!=before.st_dev || current.st_ino!=before.st_ino ||
        !S_ISREG(current.st_mode) || current.st_size!=before.st_size || current.st_uid!=before.st_uid || current.st_mode!=before.st_mode ||
        after.st_size!=before.st_size || after.st_mtime!=before.st_mtime || after.st_ctime!=before.st_ctime)
        throw std::runtime_error("Configure audit changed during capture");
    return bytes;
}

}
