#ifndef GETDAQ_COLD_RECOVERY_FILES_P_H
#define GETDAQ_COLD_RECOVERY_FILES_P_H

#include "supervisorio.h"
#include <cstddef>
#include <string>
#include <sys/stat.h>

namespace GetDAQ { namespace RecoveryPrivate {

void RequireRecovery(bool value, const char *message);
bool RecoveryPathExists(const std::string &path);

class PinnedFile {
    std::string path;
    SupervisorIO::Fd fd;
    struct stat identity;
    bool executable;
    mode_t permissions;
public:
    const std::string bytes;
    PinnedFile(const std::string &path, std::size_t maximum, bool executable=false, bool sealed=false);
    void Check() const;
};

} }

#endif
