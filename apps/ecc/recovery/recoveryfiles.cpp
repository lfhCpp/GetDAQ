#include "recoveryfiles_p.h"
#include <stdexcept>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace GetDAQ { namespace RecoveryPrivate {
using namespace SupervisorIO;

void RequireRecovery(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}

bool RecoveryPathExists(const std::string &path) {
    struct stat info;
    if (!::lstat(path.c_str(),&info)) return true;
    RequireRecovery(errno==ENOENT,"Cannot inspect recovery metadata");
    return false;
}

PinnedFile::PinnedFile(const std::string &p,std::size_t maximum,bool exec,bool sealed)
    : path(p),fd(::open(p.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC)),executable(exec),
      permissions(sealed ? 0400 : 0600),bytes(ReadFile(p,maximum)) {
    Path(path);
    RequireRecovery(fd.value>=0 && !::fstat(fd.value,&identity),"Cannot bind recovery metadata");
    Check();
}

void PinnedFile::Check() const {
    struct stat held,named;
    RequireRecovery(!::fstat(fd.value,&held) && !::lstat(path.c_str(),&named) && Same(identity,held) && Same(identity,named) &&
        S_ISREG(named.st_mode) && named.st_uid==::geteuid() && named.st_nlink==1 &&
        (executable ? (!(named.st_mode&0022) && (named.st_mode&0111)) : ((named.st_mode&0777)==permissions)) &&
        ReadFile(path,bytes.size())==bytes,"Recovery fixed metadata identity/content/permissions changed");
}

} }
