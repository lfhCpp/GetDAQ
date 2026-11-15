#include "scanplanreview_p.h"

#include "scanexecutionjournalstore.h"
#include "scanoperationrunner.h"
#include "../../common/experiment/scanexecutionstate.h"
#include "scanplanoutput.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>

#include <stdexcept>
#include <string>

namespace ScanPlanReviewInternal {

void Require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

QString ReviewJournalPath(const QString &applicationDataDirectory,
                          const QString &planSha256)
{
    return QDir(applicationDataDirectory).filePath(
        QStringLiteral("scan-journals/review-%1.json").arg(planSha256.left(56)));
}

ScanPlanRecoveryStatus DescribeJournal(
    const QString &path, const GetDAQ::ScanExecutionJournal &journal)
{
    ScanPlanRecoveryStatus result;
    result.journalPath = path;
    result.hasJournal = true;
    result.state = QString::fromLatin1(GetDAQ::ScanPointStateName(journal.State()));
    result.pointIndex = static_cast<int>(journal.PointIndex());
    result.pendingUnknown = journal.PendingOperationUnknown();
    if (journal.HasPendingOperation()) {
        result.operationId = QString::fromStdString(journal.PendingOperationId());
        result.operation = QString::fromStdString(journal.PendingOperation());
    }
    return result;
}

ScanPlanOutputSummary ValidateExpectedPlan(const QString &planDirectory,
                                           const QString &expectedPlanSha256,
                                           int expectedPointCount)
{
    const ScanPlanOutputSummary summary = InspectScanPlanOutput(planDirectory);
    Require(summary.isValid(), "Cannot access recovery journal for an invalid scan plan");
    Require(summary.planSha256 == expectedPlanSha256 &&
            summary.pointCount == expectedPointCount,
            "Scan plan changed after the operator opened it");
    return summary;
}

std::unique_ptr<GetDAQ::ScanExecutionJournalStore> OpenExpectedJournal(
    const QString &planDirectory,
    const QString &applicationDataDirectory,
    const ScanPlanOutputSummary &summary,
    const QString &path)
{
    const QString directory = QDir(applicationDataDirectory).filePath(
        QStringLiteral("scan-journals"));
    Require(QFileInfo(directory).isDir(), "No private scan journal exists for this plan");
    EnsurePrivateDirectory(directory);
    Require(QFileInfo::exists(path), "No scan journal exists for this plan");
    std::unique_ptr<GetDAQ::ScanExecutionJournalStore> store =
        GetDAQ::ScanExecutionJournalStore::Open(path.toUtf8().toStdString());
    const GetDAQ::ScanExecutionJournal current = store->Current();
    const std::string digest = summary.planSha256.toLatin1().toStdString();
    const std::string executionId = std::string("review-") +
        summary.planSha256.left(56).toStdString();
    Require(current.ExecutionId() == executionId &&
            current.SourcePlanSha256() == digest &&
            current.PointCount() == static_cast<unsigned int>(summary.pointCount),
            "Scan journal belongs to a different plan identity");
    if (!current.SourceSnapshotSha256().empty()) {
        const SnapshotIdentity snapshot = LoadOrCreateExecutionSnapshot(
            planDirectory, applicationDataDirectory, summary, false);
        Require(current.SourceSnapshotSha256() == snapshot.sha256,
                "Scan journal is bound to a different execution snapshot");
    }
    return store;
}

}

using namespace ScanPlanReviewInternal;

ScanPlanRecoveryStatus InspectScanPlanRecovery(
    const QString &planDirectory, const QString &applicationDataDirectory,
    const QString &expectedPlanSha256, int expectedPointCount)
{
    const ScanPlanOutputSummary summary = ValidateExpectedPlan(
        planDirectory, expectedPlanSha256, expectedPointCount);
    const QString path = ReviewJournalPath(applicationDataDirectory,
                                           summary.planSha256);
    if (!QFileInfo::exists(path)) {
        ScanPlanRecoveryStatus result;
        result.journalPath = path;
        return result;
    }
    std::unique_ptr<GetDAQ::ScanExecutionJournalStore> store =
        OpenExpectedJournal(planDirectory, applicationDataDirectory, summary, path);
    return DescribeJournal(path, store->Current());
}

ScanPlanRuntimeContext LoadScanPlanRuntimeContext(
    const QString &planDirectory, const QString &applicationDataDirectory,
    const QString &expectedPlanSha256, int expectedPointCount)
{
    const ScanPlanOutputSummary summary = ValidateExpectedPlan(
        planDirectory, expectedPlanSha256, expectedPointCount);
    const QString journalPath = ReviewJournalPath(applicationDataDirectory,
                                                  summary.planSha256);
    Require(QFileInfo::exists(journalPath),
            "No reviewed scan execution journal exists for this plan");
    std::unique_ptr<GetDAQ::ScanExecutionJournalStore> store =
        OpenExpectedJournal(planDirectory, applicationDataDirectory, summary, journalPath);
    const SnapshotIdentity snapshot = LoadOrCreateExecutionSnapshot(
        planDirectory, applicationDataDirectory, summary, false);
    const GetDAQ::ScanExecutionJournal &journal = store->Current();

    ScanPlanRuntimeContext result;
    result.journalPath = journalPath;
    result.state = QString::fromLatin1(GetDAQ::ScanPointStateName(journal.State()));
    result.pointIndex = static_cast<int>(journal.PointIndex());
    result.hasJournal = true;
    result.hasPendingOperation = journal.HasPendingOperation();
    result.pendingUnknown = journal.PendingOperationUnknown();
    if (journal.HasPendingOperation()) {
        result.pendingOperationId = QString::fromStdString(journal.PendingOperationId());
        result.pendingOperation = QString::fromStdString(journal.PendingOperation());
        result.pendingRequestSha256 = QString::fromStdString(journal.PendingRequestSha256());
    }
    result.runtimeIdentityBound = !journal.RuntimeIdentitySha256().empty();
    result.eventCountBaselineBound = journal.HasEventCountBaseline();
    result.snapshotBytes = QByteArray(snapshot.bytes.data(),
                                      static_cast<int>(snapshot.bytes.size()));
    return result;
}

ScanPlanRecoveryStatus ReconcileScanPlanOperation(
    const QString &planDirectory, const QString &applicationDataDirectory,
    const QString &expectedPlanSha256, int expectedPointCount,
    const QString &expectedOperationId, const QString &operatorId,
    bool confirmedApplied, const QString &observationText)
{
    const ScanPlanOutputSummary summary = ValidateExpectedPlan(
        planDirectory, expectedPlanSha256, expectedPointCount);
    const QString path = ReviewJournalPath(applicationDataDirectory,
                                           summary.planSha256);
    std::unique_ptr<GetDAQ::ScanExecutionJournalStore> store =
        OpenExpectedJournal(planDirectory, applicationDataDirectory, summary, path);
    const GetDAQ::ScanExecutionJournal current = store->Current();
    Require(current.PendingOperationUnknown() && current.HasPendingOperation(),
            "There is no unknown operation requiring reconciliation");
    Require(current.PendingOperationId() == expectedOperationId.toStdString(),
            "Journal advanced or changed; reopen recovery status before retrying");
    const QByteArray observation = observationText.toUtf8();
    Require(!observation.trimmed().isEmpty() && observation.size() <= 16384,
            "Enter an observation note of 1 to 16384 UTF-8 bytes");
    const QString digest = QString::fromLatin1(
        QCryptographicHash::hash(observation, QCryptographicHash::Sha256).toHex());
    const GetDAQ::ScanExecutionJournal next = current.WithOperationReconciled(
        current.PendingOperationId(), operatorId.toStdString(),
        confirmedApplied ? GetDAQ::ScanOperationDisposition::ConfirmedApplied
                         : GetDAQ::ScanOperationDisposition::ConfirmedNotSent,
        digest.toLatin1().toStdString());
    store->Persist(next);
    return DescribeJournal(path, store->Current());
}
