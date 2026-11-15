#ifndef GETDAQ_COBO_LIFECYCLE_JOURNAL_H
#define GETDAQ_COBO_LIFECYCLE_JOURNAL_H

#include "cobolifecycle.h"
#include <cstdint>
#include <string>
#include <vector>

namespace GetDAQ {
// Historical controller evidence only. Reading this never restores a live
// ledger or grants permission to send hardware commands after a restart.
struct CoBoLifecycleHistory {
    std::vector<CoBoLifecycleState> states;
    std::uint64_t transitions = 0;
    std::string chainSha256;
    std::uint64_t bytes = 0;
    bool terminalVerified = false; // Matches the supplied endpoint; caller must verify its provenance.
};

std::string EncodeCoBoLifecycleCheckpoint(const CoBoLifecycleCheckpoint &checkpoint);
CoBoLifecycleCheckpoint DecodeCoBoLifecycleCheckpoint(const std::string &json);

class CoBoLifecycleJournal {
public:
    typedef std::vector<std::pair<std::string,bool>> Topology;
    CoBoLifecycleJournal(const std::string &path,const std::string &scope,const Topology &topology);
    ~CoBoLifecycleJournal();
    CoBoLifecycleJournal(const CoBoLifecycleJournal &) = delete;
    CoBoLifecycleJournal &operator=(const CoBoLifecycleJournal &) = delete;
    void Append(const std::string &id,std::size_t index,CoBoLifecycleState from,CoBoLifecycleState next);
    void RequireHealthy() const;
    CoBoLifecycleCheckpoint Checkpoint() const;
    static CoBoLifecycleHistory ReadHistory(const std::string &path,const std::string &scope,const Topology &topology);
    // Requires the exact terminal endpoint. A stale checkpoint rejects newer
    // history as well; it must come from independent trusted operation evidence.
    static CoBoLifecycleHistory ReadHistory(const std::string &path,const std::string &scope,const Topology &topology,
                                          const CoBoLifecycleCheckpoint &expected);
private:
    int fFd = -1;
    std::string fPath,fBytes,fChain,fScope;
    std::uint64_t fSequence = 0;
    mutable bool fFailed = false;
    void Persist(const std::string &record);
    void VerifyFile() const;
};
}
#endif
