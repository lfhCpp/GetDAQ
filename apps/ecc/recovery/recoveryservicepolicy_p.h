#ifndef GETDAQ_COLD_RECOVERY_SERVICE_POLICY_P_H
#define GETDAQ_COLD_RECOVERY_SERVICE_POLICY_P_H

#include "coldhandoffprotocol.h"
#include "credential.h"
#include "eccprotocol.h"
#include "recoveryfiles_p.h"
#include <memory>
#include <string>
#include <vector>
#include <sys/stat.h>

namespace GetDAQ { namespace RecoveryPrivate {

class RecoveryServicePolicy {
    std::unique_ptr<PinnedFile> config;
    std::vector<std::unique_ptr<PinnedFile>> inputs;
    std::unique_ptr<SupervisorCredential> credential;
    std::unique_ptr<SupervisorIO::Directory> root;
    SupervisorIO::Fd lease;
    struct stat leaseIdentity;
    std::unique_ptr<PinnedFile> policyFile;
    ColdHandoffIdentity identity;
    EccControlRequest original;
    EccControlSystemStatus before;
    std::string bind,oldConfig,newConfig,plan,originalEvidence,executable;
    unsigned int port;

public:
    explicit RecoveryServicePolicy(const std::string &file);
    void Check() const;
    bool Authorize(const std::string &header) const;
    SupervisorIO::Directory &Root() const;
    const ColdHandoffIdentity &Identity() const;
    const EccControlRequest &OriginalRequest() const;
    const EccControlSystemStatus &OriginalStatus() const;
    const std::string &BindAddress() const;
    const std::string &OldConfig() const;
    const std::string &NewConfig() const;
    const std::string &SuccessorPlan() const;
    const std::string &OriginalEvidence() const;
    const std::string &Executable() const;
    unsigned int Port() const;
};

} }

#endif
