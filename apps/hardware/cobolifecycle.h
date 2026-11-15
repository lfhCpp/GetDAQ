#ifndef GETDAQ_HARDWARE_COBO_LIFECYCLE_H
#define GETDAQ_HARDWARE_COBO_LIFECYCLE_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace GetDAQ {

enum class CoBoLifecycleState {
    Fresh,
    Preparing,
    Prepared,
    Configuring,
    Configured,
    Ready,
    Starting,
    Running,
    Paused,
    Resuming,
    Stopping,
    Stopped,
    Unknown,
    Disabled
};

const char *CoBoLifecycleStateName(CoBoLifecycleState state);
CoBoLifecycleState InitialCoBoLifecycleState(bool enabled);
bool CanTransitionCoBoLifecycle(CoBoLifecycleState from, CoBoLifecycleState to);

class CoBoLifecycleJournal;
struct CoBoLifecycleCheckpoint {
    std::string scopeSha256,chainSha256;
    std::uint64_t transitions = 0,bytes = 0;
};
// One fixed-topology source of truth shared by preparation and acquisition.
// Production attaches a fresh durable journal before the first hardware step.
// Historical replay deliberately cannot restore a live controller.
class CoBoLifecycleLedger {
public:
    explicit CoBoLifecycleLedger(const std::vector<std::pair<std::string,bool>> &topology);
    ~CoBoLifecycleLedger();
    void BeginPersistence(const std::string &path,const std::string &scopeSha256);
    void RequirePersistenceHealthy() const;
    // A verified current endpoint, for retaining outside the lifecycle file.
    // Throws if no durable journal is attached or its health is uncertain.
    CoBoLifecycleCheckpoint PersistenceCheckpoint() const;
    std::size_t Size() const;
    const std::string &IdAt(std::size_t index) const;
    bool EnabledAt(std::size_t index) const;
    CoBoLifecycleState StateAt(std::size_t index) const;
    CoBoLifecycleState StateFor(const std::string &id) const;
    std::vector<CoBoLifecycleState> Snapshot() const;
    // Non-mutating gate: acquisition may bind only after every active CoBo is
    // confirmed Configured and every disabled CoBo remains Disabled.
    void RequireConfiguredForAcquisition() const;
    CoBoLifecycleState Transition(const std::string &id,std::size_t fixedIndex,CoBoLifecycleState next);
    // UNKNOWN must become visible even when its persistence attempt fails.
    void MarkUnknown(const std::string &id,std::size_t fixedIndex);
    // A broken lifecycle journal must not prevent explicit negative hardware
    // stops. False keeps the target UNKNOWN; it never publishes Stopped.
    bool TransitionForNegativeStop(const std::string &id,std::size_t fixedIndex,CoBoLifecycleState next);
private:
    struct Entry {
        std::string id;
        bool enabled;
        CoBoLifecycleState state;
    };
    std::vector<Entry> fEntries;
    std::map<std::string,std::size_t> fIndexById;
    std::unique_ptr<CoBoLifecycleJournal> fJournal;
    bool fTransitioned = false;
    mutable std::mutex fMutex;
    Entry &ValidateTransition(const std::string &id,std::size_t fixedIndex,CoBoLifecycleState next);
};

} // namespace GetDAQ

#endif
