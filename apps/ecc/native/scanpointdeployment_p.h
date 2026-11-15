#ifndef GETDAQ_SCANPOINTDEPLOYMENT_P_H
#define GETDAQ_SCANPOINTDEPLOYMENT_P_H

#include "scanpointdeployment.h"
#include "../datasupervisor/planio.h"
#include <string>
#include <vector>

namespace GetDAQ {
class FixedEccConfiguration;
namespace ScanPointDeploymentInternal {
struct ReceiverTemplate {
    std::string agentId;
    std::string planFile;
    std::string bytes;
    std::string targetDirectory;
    std::string executableSha256;
    DataSupervisorOptions options;
};

struct PreparedPoint {
    const ScanExecutionSnapshotPoint *snapshotPoint;
    std::string artifactBytes;
    std::string artifactName;
    std::string serviceBytes;
    std::vector<std::string> receiverBytes;
};

void Require(bool condition, const char *message);
std::string Suffix(unsigned int value);
std::string DeploymentId(const std::string &executionId, unsigned int point);
std::string Parent(const std::string &path);
std::string ChildRoot(const std::string &root, const std::string &executionId,
                      unsigned int point);
ScanDeploymentBundle PublishScanPointDeploymentBundle(
    const ScanExecutionSnapshot &snapshot,
    const FixedEccConfiguration &original,
    const std::vector<ScanReceiverPlanTemplate> &receiverTemplates,
    const std::vector<ReceiverTemplate> &receivers,
    const std::string &newBundleDirectory,
    const std::vector<PreparedPoint> &prepared);
}
}

#endif
