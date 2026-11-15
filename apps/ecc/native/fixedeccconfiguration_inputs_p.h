#ifndef GETDAQ_FIXEDECCCONFIGURATION_INPUTS_P_H
#define GETDAQ_FIXEDECCCONFIGURATION_INPUTS_P_H

#include <cstddef>
#include <memory>
#include <string>

namespace GetDAQ {
class FixedEccConfigurationInputs {
public:
    struct File {
        std::string path;
        std::string bytes;
        std::string digest;
    };

    FixedEccConfigurationInputs();
    ~FixedEccConfigurationInputs();
    FixedEccConfigurationInputs(const FixedEccConfigurationInputs &) = delete;
    FixedEccConfigurationInputs &operator=(const FixedEccConfigurationInputs &) = delete;

    void SetTotalLimit(std::size_t bytes);
    const File &Add(const std::string &path, std::size_t maximum, const std::string &expectedSha256 = "");
    const File &At(const std::string &path) const;
    void RequireReadOnly(const std::string &path);
    std::string BindingSha256() const;
    void Check() const;

private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}

#endif
