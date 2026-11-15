#ifndef GETDAQ_SCANEXECUTIONSNAPSHOT_H
#define GETDAQ_SCANEXECUTIONSNAPSHOT_H

#include "scanexecutionpolicy.h"

#include <functional>
#include <string>
#include <vector>

namespace GetDAQ {

struct ScanExecutionSnapshotPoint {
    unsigned int index = 0;
    std::string artifact;
    std::string configurationSha256;
    std::string artifactSha256;
};

// Frozen, offline identity of a validated scan plan plus its stopping policy.
// It does not contain runtime ECC/receiver identities and cannot execute DAQ.
class ScanExecutionSnapshot {
public:
    static ScanExecutionSnapshot Create(
        const std::string &executionId,
        const std::string &planBytes,
        const std::string &policyBytes,
        const std::function<std::string(const std::string &)> &loadArtifact);

    static ScanExecutionSnapshot Decode(
        const std::string &snapshotBytes,
        const std::string &planBytes,
        const std::string &policyBytes,
        const std::function<std::string(const std::string &)> &loadArtifact);

    std::string Encode() const;
    const std::string &ExecutionId() const { return fExecutionId; }
    const std::string &SourcePlanSha256() const { return fSourcePlanSha256; }
    const std::string &SourcePolicySha256() const { return fSourcePolicySha256; }
    const std::string &ScanKind() const { return fScanKind; }
    const std::string &TargetCoBo() const { return fTargetCoBo; }
    unsigned int PointCount() const { return static_cast<unsigned int>(fPoints.size()); }
    const std::vector<ScanExecutionSnapshotPoint> &Points() const { return fPoints; }

private:
    ScanExecutionSnapshot() = default;

    std::string fExecutionId;
    std::string fSourcePlanSha256;
    std::string fSourcePolicySha256;
    std::string fScanKind;
    std::string fTargetCoBo;
    std::vector<ScanExecutionSnapshotPoint> fPoints;
};

}

#endif
