#ifndef GETDAQ_RAWFILE_H
#define GETDAQ_RAWFILE_H
#include <string>

namespace GetDAQ {
// Open a raw-storage path for a new file or truncate an exact filename
// collision. Existing files must be regular, owned by this user, unlinked
// elsewhere, and exclusively lockable; FIFOs are rejected without blocking,
// and symlinks/hardlinked archives are never truncated. The returned
// descriptor stays flock-locked until closed.
int OpenRawStorageFile(const std::string &path);
}
#endif
