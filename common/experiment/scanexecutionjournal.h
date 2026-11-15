#ifndef GETDAQ_SCANEXECUTIONJOURNAL_H
#define GETDAQ_SCANEXECUTIONJOURNAL_H

#include "scanexecutionstate.h"
#include "scaneventcountmonitor.h"
#include "../control/eccserviceprocessexitevidence.h"
#include "../control/eccserviceprocessstartevidence.h"

#include <string>
#include <vector>

namespace GetDAQ {

struct JsonValue;

enum class ScanOperationDisposition {
    ConfirmedApplied,
    ConfirmedNotSent
};

// Immutable, bounded journal model. The caller persists Encode() atomically
// before replacing its current value or submitting the represented operation.
class ScanExecutionJournal {
public:
    ScanExecutionJournal(const std::string &executionId,
                         const std::string &sourcePlanSha256,
                         unsigned int pointCount);
    ScanExecutionJournal(const std::string &executionId,
                         const std::string &sourcePlanSha256,
                         const std::string &sourceSnapshotSha256,
                         unsigned int pointCount);

    const std::string &ExecutionId() const { return fExecutionId; }
    const std::string &SourcePlanSha256() const { return fSourcePlanSha256; }
    const std::string &SourceSnapshotSha256() const { return fSourceSnapshotSha256; }
    const std::string &RuntimeIdentitySha256() const { return fRuntimeIdentitySha256; }
    bool HasEventCountBaseline() const { return fHasEventCountBaseline; }
    const ScanEventCountBaseline &EventCountBaseline() const { return fEventCountBaseline; }
    unsigned int PointCount() const { return fState.PointCount(); }
    unsigned int PointIndex() const { return fState.PointIndex(); }
    ScanPointState State() const { return fState.State(); }
    std::size_t EventCount() const { return fEvents.size(); }

    ScanExecutionJournal WithTransition(ScanPointState expected,
                                        ScanPointState next) const;
    ScanExecutionJournal WithNextPoint() const;
    ScanExecutionJournal WithSnapshotBinding(const std::string &snapshotSha256) const;
    ScanExecutionJournal WithRuntimeIdentityBinding(const std::string &identitySha256) const;
    ScanExecutionJournal WithManagedServiceStartIntent(
        const std::string &operationId, const std::string &executableSha256,
        const std::string &serviceConfigurationSha256) const;
    ScanExecutionJournal WithManagedServiceStarted(
        const EccServiceProcessStartEvidence &evidence) const;
    ScanExecutionJournal WithManagedServiceStartUnknown(
        const std::string &reasonSha256) const;
    ScanExecutionJournal WithManagedServiceRetirement(
        const EccServiceProcessExitEvidence &evidence) const;
    ScanExecutionJournal WithManagedServiceShutdownIntent(
        const std::string &retirementOperationId, int processId,
        const std::string &serviceConfigurationSha256,
        const std::string &instanceId, const std::string &serviceSnapshotSha256,
        const std::string &handoffOperationId) const;
    ScanExecutionJournal WithManagedServiceShutdownUnknown(
        const std::string &reasonSha256) const;
    ScanExecutionJournal WithEventCountBaseline(
        const ScanEventCountBaseline &baseline) const;
    ScanExecutionJournal WithOperationIntent(const std::string &operationId,
                                             const std::string &operation,
                                             const std::string &requestSha256) const;
    ScanExecutionJournal WithOperationComplete(const std::string &operationId,
                                               const std::string &responseSha256,
                                               ScanPointState next) const;
    // Resolve a restart-quarantined lifecycle intent only after the caller has
    // validated the exact saved service response and a fresh controller readback.
    ScanExecutionJournal WithObservedOperationComplete(
        const std::string &operationId, const std::string &responseSha256) const;
    ScanExecutionJournal WithOperationUnknown(const std::string &operationId,
                                              const std::string &reasonSha256) const;
    // Explicit human reconciliation only. The evidence digest identifies the
    // observation record; it is not cryptographic proof or hardware readback.
    ScanExecutionJournal WithOperationReconciled(
        const std::string &operationId, const std::string &operatorId,
        ScanOperationDisposition disposition,
        const std::string &evidenceSha256) const;
    const std::string &PendingOperationId() const { return fPendingOperationId; }
    const std::string &PendingOperation() const { return fPendingOperation; }
    const std::string &PendingRequestSha256() const { return fPendingRequestSha256; }
    ScanPointState PendingOperationTarget() const;
    bool HasPendingOperation() const { return !fPendingOperationId.empty(); }
    bool PendingOperationUnknown() const { return fPendingUnknown; }
    bool IsImmediateSuccessorOf(const ScanExecutionJournal &previous) const;
    std::string Encode() const;
    static ScanExecutionJournal Decode(const std::string &bytes);

private:
    struct Event {
        enum Type { Transition, NextPoint, SnapshotBound, RuntimeIdentityBound,
                    ManagedServiceStartIntent, ManagedServiceStarted,
                    ManagedServiceStartUnknown,
                    ManagedServiceShutdownIntent, ManagedServiceShutdownUnknown,
                    ProcessRetirement,
                    EventCountBaselineBound,
                    OperationIntent, OperationComplete,
                    OperationUnknown, OperationReconciled,
                    OperationObservedComplete } type = Transition;
        unsigned int point = 0;
        ScanPointState from = ScanPointState::Planned;
        ScanPointState to = ScanPointState::Planned;
        std::string operationId;
        std::string actorId;
        std::string operation;
        std::string requestSha256;
        std::string responseSha256;
        int processId = 0;
        int exitCode = -1;
        std::string serviceConfigurationSha256;
        std::string instanceId;
        std::string serviceSnapshotSha256;
        std::string handoffOperationId;
        std::string executableSha256;
        std::string runtimeIdentitySha256;
        bool operationApplied = false;
        ScanEventCountBaseline eventCountBaseline;
    };

    struct ReplayContext {
        unsigned int schemaVersion;
        unsigned int pointCount;
        const std::string &snapshotDigest;
        std::vector<Event> &events;
        ScanExecutionState &state;
        std::string &pendingOperationId;
        std::string &pendingOperation;
        std::string &pendingRequestSha256;
        bool &pendingUnknown;
        bool &snapshotBindingSeen;
        std::string &runtimeIdentitySha256;
        ScanEventCountBaseline &eventCountBaseline;
        bool &hasEventCountBaseline;
        bool &legacyRetirementHistory;
        bool &managedServiceStartPending;
        bool &managedServiceShutdownPending;
        Event &managedServiceShutdownIntent;
    };

    static bool ReplayManagedServiceEvent(const std::string &type,
                                          const JsonValue &row,
                                          ReplayContext &context,
                                          Event &event);
    static bool ReplayOperationEvent(const std::string &type,
                                     const JsonValue &row,
                                     ReplayContext &context,
                                     Event &event);
    static bool ReplayBindingEvent(const std::string &type,
                                   const JsonValue &row,
                                   ReplayContext &context,
                                   Event &event);

    ScanExecutionJournal(const std::string &executionId,
                         const std::string &sourcePlanSha256,
                         const std::string &sourceSnapshotSha256,
                         const std::string &runtimeIdentitySha256,
                         const ScanEventCountBaseline &eventCountBaseline,
                         bool hasEventCountBaseline,
                         unsigned int schemaVersion,
                         unsigned int pointCount,
                         const std::vector<Event> &events,
                         const ScanExecutionState &state,
                         const std::string &pendingOperationId,
                         const std::string &pendingOperation,
                         const std::string &pendingRequestSha256,
                         bool pendingUnknown,
                         bool legacyRetirementHistory);

    std::string fExecutionId;
    std::string fSourcePlanSha256;
    std::string fSourceSnapshotSha256;
    std::string fRuntimeIdentitySha256;
    ScanEventCountBaseline fEventCountBaseline;
    bool fHasEventCountBaseline = false;
    unsigned int fSchemaVersion = 1;
    std::vector<Event> fEvents;
    ScanExecutionState fState;
    std::string fPendingOperationId;
    std::string fPendingOperation;
    std::string fPendingRequestSha256;
    bool fPendingUnknown = false;
    bool fLegacyRetirementHistory = false;
};

}

#endif
