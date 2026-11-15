#ifndef GETDAQ_REMOTEHANDOFFEVIDENCE_H
#define GETDAQ_REMOTEHANDOFFEVIDENCE_H
#include "controlprotocol.h"
#include <memory>
#include <string>
#include <vector>
namespace GetDAQ {
struct RemoteHandoffNodeEvidence {
    std::string agentId,instanceId,planSha256;
    DataSupervisorStatus stopped;
    SupervisorControlRequest request;
    // Zero is uncertain delivery. Only 1 (no recorded submission), 409 or 412
    // may authorize a future explicit attempt, after fresh remote checks.
    unsigned int rejection=1;
};
// Read-only, bounded evidence for explicit cold recovery. Reading this object
// does not issue requests, restore control, clear UNKNOWN or change old files.
class RemoteHandoffEvidence {
public:
    RemoteHandoffEvidence(const std::string &directory,const std::string &expectedBinding,
                          const std::string &expectedScope);
    ~RemoteHandoffEvidence();
    void Check() const;
    const std::string &Directory() const;
    const std::string &Epoch() const;
    const std::string &IntentSha256() const;
    const std::vector<RemoteHandoffNodeEvidence> &Nodes() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};

struct RemoteCatalogPublishNodeEvidence {
    std::string agentId,instanceId,planSha256,operationId,requestKey,outcome;
    // The exact validated request is retained for later cold reconstruction;
    // exposing it does not grant permission to submit it again.
    SupervisorControlRequest request;
};
// Read-only parent proof for a persisted cross-node catalog Publish. It
// classifies the original fixed node set; it never restores authorization or
// submits/retries a child operation.
class RemoteCatalogPublishEvidence {
public:
    RemoteCatalogPublishEvidence(const std::string &directory,const std::string &expectedBinding,
                                 const std::string &expectedScope,const std::vector<std::string> &fixedAgentIds);
    ~RemoteCatalogPublishEvidence();
    void Check() const;
    const std::string &Directory() const;
    const std::string &Epoch() const;
    const std::string &IntentSha256() const;
    const std::string &Experiment() const;
    unsigned int RunNumber() const;
    const std::string &ReplacementId() const;
    const std::vector<RemoteCatalogPublishNodeEvidence> &Nodes() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
