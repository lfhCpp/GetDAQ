#ifndef GETDAQ_ECCSERVICEPROCESSSTARTEVIDENCE_H
#define GETDAQ_ECCSERVICEPROCESSSTARTEVIDENCE_H

#include <string>

namespace GetDAQ {

class EccServiceProcessSupervisor;
class ScanExecutionJournal;

// The supervisor creates this value only after exec succeeds and the exact
// child is still alive. It binds that observation to one durable scan intent.
class EccServiceProcessStartEvidence {
public:
    int ProcessId() const { return fProcessId; }
    const std::string &OperationId() const { return fOperationId; }
    const std::string &ExecutableSha256() const { return fExecutableSha256; }
    const std::string &ServiceConfigurationSha256() const { return fServiceConfigurationSha256; }
    const std::string &ExecutionId() const { return fExecutionId; }
    const std::string &ExecutionSnapshotSha256() const { return fExecutionSnapshotSha256; }
    unsigned int PointIndex() const { return fPointIndex; }

private:
    friend class EccServiceProcessSupervisor;
    EccServiceProcessStartEvidence(int processId, const std::string &operationId,
        const std::string &executableSha256, const std::string &configurationSha256,
        const std::string &executionId, const std::string &snapshotSha256,
        unsigned int pointIndex)
        : fProcessId(processId), fOperationId(operationId),
          fExecutableSha256(executableSha256),
          fServiceConfigurationSha256(configurationSha256),
          fExecutionId(executionId), fExecutionSnapshotSha256(snapshotSha256),
          fPointIndex(pointIndex) {}

    int fProcessId;
    std::string fOperationId;
    std::string fExecutableSha256;
    std::string fServiceConfigurationSha256;
    std::string fExecutionId;
    std::string fExecutionSnapshotSha256;
    unsigned int fPointIndex;
};

}

#endif
