#ifndef GETDAQ_NATIVEECCFILESYSTEMPROOF_P_H
#define GETDAQ_NATIVEECCFILESYSTEMPROOF_P_H

#include <string>
#include <sys/stat.h>

namespace GetDAQ {
namespace NativeEccFilesystemProof {

bool SameIdentity(const struct stat &a,const struct stat &b);

class Directory {
    int fFd=-1;
    struct stat fIdentity{};
    std::string fPath;
public:
    explicit Directory(const std::string &path);
    ~Directory();
    void Check() const;
};

void WriteNewFile(const std::string &path,const std::string &bytes);

class ImmutableFile {
    int fFd=-1;
    struct stat fIdentity{};
    std::string fPath,fExpected;
public:
    ImmutableFile(const std::string &path,const std::string &bytes);
    ~ImmutableFile();
    bool IsPath(const std::string &value) const;
    // Only an authorized append that retains the verified prefix may advance
    // a proof over an audit file written by the current operation.
    void AdvanceVerifiedAppend(const std::string &bytes);
    void Check() const;
};

std::string ReadFailedDescriptionAudit(const std::string &path,
                                       const struct stat &identity,
                                       const std::string &chainSha256);

} // namespace NativeEccFilesystemProof
} // namespace GetDAQ

#endif
