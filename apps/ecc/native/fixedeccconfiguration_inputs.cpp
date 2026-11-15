#include "fixedeccconfiguration_inputs_p.h"

#include "configurationsha256.h"
#include "strictjson.h"
#include "systemexperimentconfiguration.h"
#include "supervisorio.h"

#include <fcntl.h>
#include <map>
#include <stdexcept>
#include <unistd.h>

namespace GetDAQ {
namespace {
using namespace SupervisorIO;

class BoundInputFile {
    Fd descriptor;
    struct stat identity;
    bool readOnly = false;

public:
    FixedEccConfigurationInputs::File file;
    std::size_t maximum;

    BoundInputFile(const std::string &path, std::size_t limit, const std::string &expectedSha256)
        : descriptor(-1), maximum(limit)
    {
        Path(path);
        file.path = path;
        descriptor.value = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
        if (descriptor.value < 0 || ::fstat(descriptor.value, &identity))
            throw std::runtime_error("Cannot bind fixed ECC input file");
        file.bytes = ReadFile(path, maximum);
        file.digest = ConfigurationSha256(file.bytes);
        if (!expectedSha256.empty()) {
            Digest(expectedSha256);
            if (expectedSha256 != file.digest)
                throw std::invalid_argument("Fixed ECC input file SHA differs from configuration");
        }
        Check();
    }

    void Check() const
    {
        struct stat current, held;
        if (::lstat(file.path.c_str(), &current) || ::fstat(descriptor.value, &held) ||
            !Same(current, identity) || !Same(current, held) || !S_ISREG(current.st_mode) ||
            current.st_uid != ::geteuid() || (current.st_mode & 0022) ||
            (readOnly && (current.st_mode & 0222)) || ReadFile(file.path, maximum) != file.bytes)
            throw std::runtime_error("Fixed ECC input identity/permissions/content changed");
    }

    void RequireReadOnly()
    {
        readOnly = true;
        Check();
    }
};
}

class FixedEccConfigurationInputs::Impl {
public:
    std::map<std::string, std::unique_ptr<BoundInputFile>> files;
    std::size_t totalBytes = 0;
    std::size_t totalLimit = 128 * 1024 * 1024;
};

FixedEccConfigurationInputs::FixedEccConfigurationInputs() : fImpl(new Impl) {}
FixedEccConfigurationInputs::~FixedEccConfigurationInputs() {}

void FixedEccConfigurationInputs::SetTotalLimit(std::size_t bytes)
{
    if (bytes < fImpl->totalBytes)
        throw std::invalid_argument("Fixed ECC input byte budget is below bytes already loaded");
    fImpl->totalLimit = bytes;
}

const FixedEccConfigurationInputs::File &FixedEccConfigurationInputs::Add(
    const std::string &path, std::size_t maximum, const std::string &expectedSha256)
{
    const auto found = fImpl->files.find(path);
    if (found != fImpl->files.end()) {
        if (found->second->file.bytes.size() > maximum)
            throw std::invalid_argument("Fixed ECC input exceeds referenced role size limit");
        if (!expectedSha256.empty() && expectedSha256 != found->second->file.digest)
            throw std::invalid_argument("Conflicting fixed ECC file reference SHA");
        return found->second->file;
    }
    std::unique_ptr<BoundInputFile> input(new BoundInputFile(path, maximum, expectedSha256));
    fImpl->totalBytes += input->file.bytes.size();
    if (fImpl->totalBytes > fImpl->totalLimit)
        throw std::invalid_argument("Fixed ECC input byte budget exceeded");
    const auto &file = input->file;
    fImpl->files.emplace(path, std::move(input));
    return file;
}

const FixedEccConfigurationInputs::File &FixedEccConfigurationInputs::At(const std::string &path) const
{
    const auto found = fImpl->files.find(path);
    if (found == fImpl->files.end()) throw std::invalid_argument("Fixed ECC input file was not registered");
    return found->second->file;
}

void FixedEccConfigurationInputs::RequireReadOnly(const std::string &path)
{
    const auto found = fImpl->files.find(path);
    if (found == fImpl->files.end()) throw std::invalid_argument("Fixed ECC input file was not registered");
    found->second->RequireReadOnly();
}

void FixedEccConfigurationInputs::Check() const
{
    for (const auto &entry : fImpl->files) entry.second->Check();
}

std::string FixedEccConfigurationInputs::BindingSha256() const
{
    std::string manifest = "getdaq.fixed-ecc-inputs:1\n";
    for (const auto &entry : fImpl->files)
        manifest += EncodeJsonString(entry.first) + ' ' + entry.second->file.digest + '\n';
    return ConfigurationSha256(manifest);
}
}
