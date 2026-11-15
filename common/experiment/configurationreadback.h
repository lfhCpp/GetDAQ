#ifndef GETDAQ_CONFIGURATIONREADBACK_H
#define GETDAQ_CONFIGURATIONREADBACK_H
#include <cstdint>
#include <string>
namespace GetDAQ {
// Historical evidence from one completed Configure hardware audit, never a live attestation.
struct ConfigurationReadbackEvidence {
    std::string auditSha256,auditChainSha256,nodeProxy;
    std::uint64_t startedAtUnixMs=0;
    unsigned int steps=0,writesAcknowledged=0,writesReadbackVerified=0;
    unsigned int checksMatched=0,checksWithWarnings=0,managerCommandsAcknowledged=0,delaysCompleted=0;
    unsigned int readOnlyObservations=0,maskedUpdatesVerified=0;
};
ConfigurationReadbackEvidence DecodeConfigurationReadbackAudit(const std::string &bytes,const std::string &expectedAuditChainSha256);
std::string EncodeConfigurationReadbackEvidence(const ConfigurationReadbackEvidence &evidence);
}
#endif
