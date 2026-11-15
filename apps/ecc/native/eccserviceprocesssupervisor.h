#ifndef GETDAQ_ECCSERVICEPROCESSSUPERVISOR_H
#define GETDAQ_ECCSERVICEPROCESSSUPERVISOR_H

#include "../../../common/control/eccserviceprocessexitevidence.h"
#include "../../../common/control/eccserviceprocessstartevidence.h"

#include <string>

namespace GetDAQ {

// Owns the parent-side wait status for one getdaq-ecc-server child. The caller
// must persist its shutdown intent before calling ShutdownAndWait(). A timeout
// or ambiguous response never kills the service and never returns evidence.
// Destroying this object does not signal a still-running acquisition service.
class EccServiceProcessSupervisor {
public:
    EccServiceProcessSupervisor();
    ~EccServiceProcessSupervisor();
    EccServiceProcessSupervisor(const EccServiceProcessSupervisor &) = delete;
    EccServiceProcessSupervisor &operator=(const EccServiceProcessSupervisor &) = delete;

    void Start(const std::string &executable, const std::string &configurationFile);
    EccServiceProcessStartEvidence StartForScanPoint(
        const std::string &executable, const std::string &configurationFile,
        const std::string &expectedExecutableSha256,
        const std::string &expectedConfigurationSha256,
        const std::string &executionId, const std::string &executionSnapshotSha256,
        unsigned int pointIndex, const std::string &operationId);
    bool Running() const;
    int ProcessId() const;
    const std::string &ServiceConfigurationSha256() const;
    const std::string &ExecutableSha256() const;
    EccServiceProcessExitEvidence ShutdownAndWait(
        const std::string &address, unsigned int port,
        const std::string &credentialFile,
        const std::string &instanceId, const std::string &snapshotSha256,
        const std::string &handoffOperationId, const std::string &retirementOperationId,
        unsigned int timeoutMs,
        const std::string &executionId, const std::string &executionSnapshotSha256,
        unsigned int pointIndex, const std::string &runtimeIdentitySha256);

private:
    class Impl;
    Impl *fImpl;
};

}

#endif
