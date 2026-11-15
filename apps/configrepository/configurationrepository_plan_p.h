#ifndef GETDAQ_CONFIGURATIONREPOSITORY_PLAN_P_H
#define GETDAQ_CONFIGURATIONREPOSITORY_PLAN_P_H

#include "../../common/experiment/configurationpublication.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace GetDAQ {

struct PreparedConfigurationRepositoryCommit
{
    std::string publicationSha256;
    std::string planSha256;
    std::string manifest;
    ConfigurationPublication publication;
    std::map<std::string, std::string> files;
    SystemExperimentArtifact artifact;
};

PreparedConfigurationRepositoryCommit PrepareConfigurationRepositoryCommit(
    const std::string &planBytes,
    const std::function<std::string(const std::vector<std::string> &, std::size_t, std::size_t)> &assemble);

} // namespace GetDAQ

#endif
