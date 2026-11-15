#ifndef GETDAQ_SCANPLANREVIEW_H
#define GETDAQ_SCANPLANREVIEW_H

#include <QByteArray>
#include <QString>

#include <memory>

namespace GetDAQ { class ScanExecutionJournal; }

struct ScanPlanReviewResult
{
    QString journalPath;
    bool alreadyReviewed = false;
};

struct ScanPlanRecoveryStatus
{
    QString journalPath;
    QString state;
    QString operationId;
    QString operation;
    int pointIndex = -1;
    bool hasJournal = false;
    bool pendingUnknown = false;
};

struct ScanPlanRuntimeContext
{
    QString journalPath;
    QString state;
    int pointIndex = -1;
    bool hasJournal = false;
    bool hasPendingOperation = false;
    bool pendingUnknown = false;
    QString pendingOperationId;
    QString pendingOperation;
    QString pendingRequestSha256;
    bool runtimeIdentityBound = false;
    bool eventCountBaselineBound = false;
    QByteArray snapshotBytes;
};

// Records only the operator's plan-review decision. It never contacts ECC,
// receiver processes, hardware, or the raw-data path.
ScanPlanReviewResult RecordScanPlanReview(const QString &planDirectory,
                                          const QString &applicationDataDirectory,
                                          const QString &expectedPlanSha256,
                                          int expectedPointCount);

// Read-only inspection; it never creates a journal or contacts hardware.
ScanPlanRecoveryStatus InspectScanPlanRecovery(const QString &planDirectory,
                                               const QString &applicationDataDirectory,
                                               const QString &expectedPlanSha256,
                                               int expectedPointCount);

// Reads the reviewed journal and its frozen snapshot for an online read-only
// ECC identity/event-baseline query. It never creates or changes either file.
ScanPlanRuntimeContext LoadScanPlanRuntimeContext(
    const QString &planDirectory, const QString &applicationDataDirectory,
    const QString &expectedPlanSha256, int expectedPointCount);

// Owns the single-writer journal lock while the metadata-only catalog helper
// runs. Destroying an incomplete session leaves a durable intent that will be
// quarantined on reopen, rather than permitting an automatic replay.
class ScanPointBindingSession
{
public:
    ~ScanPointBindingSession();
    ScanPointBindingSession(const ScanPointBindingSession &) = delete;
    ScanPointBindingSession &operator=(const ScanPointBindingSession &) = delete;
    ScanPlanRecoveryStatus Current() const;
    ScanPlanRecoveryStatus Complete(const QByteArray &responseBytes);
    ScanPlanRecoveryStatus MarkUnknown(const QString &reason);

private:
    class Impl;
    explicit ScanPointBindingSession(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> fImpl;
    friend std::unique_ptr<ScanPointBindingSession> BeginScanPointBinding(
        const QString &, const QString &, const QString &, int, int,
        const QString &, const QString &, const QString &);
};

// Journal a single archive binding before launching getdaq-catalog. The
// returned session holds the journal's exclusive lock until Complete/Unknown.
std::unique_ptr<ScanPointBindingSession> BeginScanPointBinding(
    const QString &planDirectory, const QString &applicationDataDirectory,
    const QString &expectedPlanSha256, int expectedPointCount, int pointIndex,
    const QString &catalogDirectory, const QString &bindingPath,
    const QString &operationId);

// Records an operator's manual reconciliation of one exact unknown operation.
// The observation text is hashed and must be retained by the operator; no
// control command is sent and no observation text is persisted by this API.
ScanPlanRecoveryStatus ReconcileScanPlanOperation(
    const QString &planDirectory, const QString &applicationDataDirectory,
    const QString &expectedPlanSha256, int expectedPointCount,
    const QString &expectedOperationId, const QString &operatorId,
    bool confirmedApplied, const QString &observationText);

#endif
