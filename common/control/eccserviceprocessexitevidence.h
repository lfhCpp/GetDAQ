#ifndef GETDAQ_ECCSERVICEPROCESSEXITEVIDENCE_H
#define GETDAQ_ECCSERVICEPROCESSEXITEVIDENCE_H

#include <string>

namespace GetDAQ {

class EccServiceProcessSupervisor;
class ScanExecutionJournal;

// Unforgeable through the public C++ API: only the process supervisor can
// construct this value after an accepted shutdown response and waitpid(0).
// The journal checks its scan and point bindings before allowing retirement.
class EccServiceProcessExitEvidence {
public:
    int ProcessId() const { return fProcessId; }
    int ExitCode() const { return fExitCode; }
    const std::string &ServiceConfigurationSha256() const { return fServiceConfigurationSha256; }
    const std::string &InstanceId() const { return fInstanceId; }
    const std::string &ServiceSnapshotSha256() const { return fServiceSnapshotSha256; }
    const std::string &HandoffOperationId() const { return fHandoffOperationId; }
    const std::string &RetirementOperationId() const { return fRetirementOperationId; }
    const std::string &ShutdownRequestSha256() const { return fShutdownRequestSha256; }
    const std::string &ShutdownResponseSha256() const { return fShutdownResponseSha256; }
    const std::string &ExecutionId() const { return fExecutionId; }
    const std::string &ExecutionSnapshotSha256() const { return fExecutionSnapshotSha256; }
    unsigned int PointIndex() const { return fPointIndex; }
    const std::string &RuntimeIdentitySha256() const { return fRuntimeIdentitySha256; }

private:
    friend class EccServiceProcessSupervisor;

    EccServiceProcessExitEvidence(int processId, int exitCode,
                                  const std::string &serviceConfigurationSha256,
                                  const std::string &instanceId,
                                  const std::string &serviceSnapshotSha256,
                                  const std::string &handoffOperationId,
                                  const std::string &retirementOperationId,
                                  const std::string &shutdownRequestSha256,
                                  const std::string &shutdownResponseSha256,
                                  const std::string &executionId,
                                  const std::string &executionSnapshotSha256,
                                  unsigned int pointIndex,
                                  const std::string &runtimeIdentitySha256)
        : fProcessId(processId), fExitCode(exitCode),
          fServiceConfigurationSha256(serviceConfigurationSha256),
          fInstanceId(instanceId), fServiceSnapshotSha256(serviceSnapshotSha256),
          fHandoffOperationId(handoffOperationId), fRetirementOperationId(retirementOperationId),
          fShutdownRequestSha256(shutdownRequestSha256),
          fShutdownResponseSha256(shutdownResponseSha256), fExecutionId(executionId),
          fExecutionSnapshotSha256(executionSnapshotSha256), fPointIndex(pointIndex),
          fRuntimeIdentitySha256(runtimeIdentitySha256) {}

    int fProcessId;
    int fExitCode;
    std::string fServiceConfigurationSha256;
    std::string fInstanceId;
    std::string fServiceSnapshotSha256;
    std::string fHandoffOperationId;
    std::string fRetirementOperationId;
    std::string fShutdownRequestSha256;
    std::string fShutdownResponseSha256;
    std::string fExecutionId;
    std::string fExecutionSnapshotSha256;
    unsigned int fPointIndex;
    std::string fRuntimeIdentitySha256;
};

}

#endif
