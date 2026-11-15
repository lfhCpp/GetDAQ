#ifndef GETDAQ_CONFIGURATIONREPOSITORY_FILES_P_H
#define GETDAQ_CONFIGURATIONREPOSITORY_FILES_P_H

#include <string>
#include <sys/stat.h>
#include <vector>

namespace GetDAQ {
namespace ConfigurationRepositoryInternal {

class Fd {
public:
    int value;
    explicit Fd(int descriptor = -1);
    ~Fd();
    Fd(const Fd &) = delete;
    Fd &operator=(const Fd &) = delete;
};

class Directory {
public:
    explicit Directory(const std::string &path);
    void Check() const;
    void Sync() const;
private:
    Fd fFd;
    struct stat fIdentity;
    std::string fPath;
};

void Need(bool condition, const std::string &message);
bool Same(const struct stat &left, const struct stat &right);
void Digest(const std::string &sha256);
void ValidateRootPath(const std::string &path);
void SyncParent(const std::string &path);
void MakeDirectory(const std::string &path);
bool Exists(const std::string &path);
std::string Read(const std::string &path, std::size_t maximum, mode_t mode = 0400);
std::string Random();
void PublishFile(const std::string &path, const std::string &bytes);
std::vector<std::string> Names(const std::string &directory);

} // namespace ConfigurationRepositoryInternal
} // namespace GetDAQ

#endif
