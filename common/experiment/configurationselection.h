#ifndef GETDAQ_CONFIGURATIONSELECTION_H
#define GETDAQ_CONFIGURATIONSELECTION_H
#include "configurationpublication.h"
namespace GetDAQ {
struct JsonValue;
struct ConfigurationRegistration {
    std::string repositoryId,publicationSha256,configurationSha256,artifactSha256;
    std::string effectiveSha256,snapshotSha256,manifestFile;
};
struct ConfigurationSelection {
    ConfigurationRegistration registration;
    std::string registrationSha256;
};
// 仓库收据和明确选择的纯协议；不连接仓库或硬件，不证明板卡实际加载。
ConfigurationRegistration DecodeConfigurationRegistration(const std::string &bytes);
std::string EncodeConfigurationRegistration(const ConfigurationRegistration &registration);
ConfigurationSelection DecodeConfigurationSelection(const std::string &bytes);
ConfigurationSelection DecodeConfigurationSelection(const JsonValue &object);
std::string EncodeConfigurationSelection(const ConfigurationSelection &selection);
ConfigurationSelection SelectConfigurationRegistration(const std::string &registrationBytes);
void VerifyConfigurationSelection(const ConfigurationSelection &selection,const std::string &registrationBytes,
    const std::string &manifestFile,const std::string &manifestBytes,const ConfigurationPublication &publication);
}
#endif
