#ifndef GETDAQ_SCANEXECUTIONSTATE_H
#define GETDAQ_SCANEXECUTIONSTATE_H

#include <string>

namespace GetDAQ {

class ScanExecutionJournal;

enum class ScanPointState {
    Planned,
    Reviewed,
    EccStarted,
    Described,
    Prepared,
    Configured,
    RunStarted,
    RunStopped,
    ReceiverFinished,
    CatalogVerified,
    Bound,
    Retired,
    NeedsReconciliation
};

// In-memory transition guard for the control plane. It performs no I/O,
// persists no journal, and never submits hardware or receiver commands.
class ScanExecutionState {
public:
    explicit ScanExecutionState(unsigned int pointCount);

    unsigned int PointIndex() const { return fPointIndex; }
    unsigned int PointCount() const { return fPointCount; }
    ScanPointState State() const { return fState; }

    // Callers supply the state they observed to reject stale transitions.
    // This object is not a thread-synchronization primitive.
    bool Transition(ScanPointState expected, ScanPointState next,
                    std::string *error = nullptr);

    // Retired is currently readable only from historical journal replay. New
    // retirement requires a future evidence-validated journal operation.
    bool BeginNextPoint(std::string *error = nullptr);

private:
    friend class ScanExecutionJournal;
    bool ReplayLegacyRetirement(std::string *error = nullptr);
    bool RetireAfterManagedProcessExit(std::string *error = nullptr);
    bool ResolveNeedsReconciliation(ScanPointState resolved,
                                    std::string *error = nullptr);

    unsigned int fPointCount;
    unsigned int fPointIndex;
    ScanPointState fState;
};

const char *ScanPointStateName(ScanPointState state);

}

#endif
