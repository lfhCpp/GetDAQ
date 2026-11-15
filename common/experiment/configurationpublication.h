#ifndef GETDAQ_CONFIGURATIONPUBLICATION_H
#define GETDAQ_CONFIGURATIONPUBLICATION_H
#include "systemexperimentconfiguration.h"
#include <functional>
#include <cstdint>
namespace GetDAQ {
struct PublicationFile { std::string stored,sha256; std::uint64_t bytes=0; };
struct PublicationSource { std::string sourcePath; PublicationFile file; };
struct ConfigurationPublication {
    std::string rootSource,configurationSha256,effectiveSha256,snapshotSha256;
    PublicationFile artifact,profile,effective;
    std::vector<PublicationSource> sources;
};
// 清单和已编译参数/来源字节绑定；不解释板端状态，也不证明硬件实际加载。
std::string EncodeConfigurationPublication(const ConfigurationPublication &publication);
ConfigurationPublication DecodeConfigurationPublication(const std::string &bytes);
SystemExperimentArtifact VerifyConfigurationPublication(const ConfigurationPublication &publication,
    const std::function<std::string(const PublicationFile &)> &read);
std::string PublicationSourceName(std::size_t index);
}
#endif
