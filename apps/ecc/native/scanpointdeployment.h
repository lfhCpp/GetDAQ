#ifndef GETDAQ_SCANPOINTDEPLOYMENT_H
#define GETDAQ_SCANPOINTDEPLOYMENT_H

#include "../../../common/experiment/scanexecutionsnapshot.h"
#include <functional>
#include <string>
#include <vector>

namespace GetDAQ {

struct ScanReceiverPlanTemplate {
    std::string agentId;
    std::string planFile;
    // Path on the data host where the successor plan will be installed.
    std::string targetDirectory;
};

struct ScanPointReceiverDeployment {
    std::string agentId;
    std::string planFile;
    std::string targetPlanFile;
    std::string planSha256;
    std::string auditRoot;
    std::string requiredAuditParentDirectory;
};

struct ScanPointDeployment {
    unsigned int index = 0;
    std::string artifactFile;
    std::string serviceFile;
    std::string controllerAuditRoot;
    std::string requiredControllerAuditParentDirectory;
    std::string configurationSha256;
    std::string artifactSha256;
    std::vector<ScanPointReceiverDeployment> receiverPlans;
};

struct ScanDeploymentBundle {
    std::string manifestFile;
    std::string executionId;
    std::vector<ScanPointDeployment> points;
};

// Build immutable, offline per-point ECC and receiver-plan inputs. This does
// not stage files on remote hosts, retire a running service, or control hardware.
ScanDeploymentBundle CreateScanPointDeployments(
    const ScanExecutionSnapshot &snapshot,
    const std::string &templateServiceFile,
    const std::vector<ScanReceiverPlanTemplate> &receiverTemplates,
    const std::string &newBundleDirectory,
    const std::function<std::string(const std::string &)> &loadArtifact);

// Re-open a sealed bundle and verify every point against the frozen execution
// snapshot. This is metadata-only and never starts a service or installs a plan.
ScanDeploymentBundle VerifyScanPointDeploymentBundle(
    const ScanExecutionSnapshot &snapshot,
    const std::string &bundleDirectory);

}

#endif
