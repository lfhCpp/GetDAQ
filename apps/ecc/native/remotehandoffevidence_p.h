#ifndef GETDAQ_REMOTEHANDOFFEVIDENCE_P_H
#define GETDAQ_REMOTEHANDOFFEVIDENCE_P_H

#include "remotehandoffevidence.h"
#include "supervisorio.h"
#include "strictjson.h"
#include <algorithm>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace GetDAQ {
namespace RemoteEvidenceInternal {
using namespace SupervisorIO;
void Require(bool value,const char *message);
std::string Parent(const std::string &path);
std::vector<std::string> Names(const SupervisorIO::Directory &directory);
std::vector<JsonValue> Journal(const std::string &bytes);
class EvidenceFile {
    std::string path;Fd fd;struct stat identity;
public:
    const std::string bytes;
    explicit EvidenceFile(const std::string &file);
    void Check() const;
};
}
}

#endif
