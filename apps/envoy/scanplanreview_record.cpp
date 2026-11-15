#include "scanplanreview_p.h"

#include <QDir>
#include <QFileInfo>

#include <memory>
#include <stdexcept>
#include <string>

using namespace ScanPlanReviewInternal;

namespace {
void RequireRecord(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
}

ScanPlanReviewResult RecordScanPlanReview(const QString &planDirectory,
                                          const QString &applicationDataDirectory,
                                          const QString &expectedPlanSha256,
                                          int expectedPointCount)
{
    const ScanPlanOutputSummary summary = InspectScanPlanOutput(planDirectory);
    RequireRecord(summary.isValid(), "Cannot record review for an invalid scan plan");
    RequireRecord(summary.planSha256.size() == 64,
            "Validated scan plan does not have a SHA-256 digest");
    RequireRecord(summary.planSha256 == expectedPlanSha256 &&
            summary.pointCount == expectedPointCount,
            "Scan plan changed after the operator opened its review confirmation");

    const QString directory = QDir(applicationDataDirectory).filePath(
        QStringLiteral("scan-journals"));
    RequireRecord(QDir().mkpath(QFileInfo(directory).absolutePath()),
            "Cannot create parent directory for private scan journals");
    EnsurePrivateDirectory(directory);

    const SnapshotIdentity snapshot = LoadOrCreateExecutionSnapshot(
        planDirectory, applicationDataDirectory, summary, true);
    const ScanPlanOutputSummary rechecked = InspectScanPlanOutput(planDirectory);
    RequireRecord(rechecked.isValid() && rechecked.planSha256 == summary.planSha256 &&
            rechecked.pointCount == summary.pointCount && rechecked.kind == summary.kind,
            "Scan plan changed while its execution snapshot was being created");

    const QString path = ReviewJournalPath(applicationDataDirectory,
                                           summary.planSha256);
    const std::string utf8Path = path.toUtf8().toStdString();
    const std::string digest = summary.planSha256.toLatin1().toStdString();
    const std::string executionId =
        std::string("review-") + summary.planSha256.left(56).toStdString();

    std::unique_ptr<GetDAQ::ScanExecutionJournalStore> store;
    if (QFileInfo::exists(path)) {
        store = GetDAQ::ScanExecutionJournalStore::Open(utf8Path);
    } else {
        store = GetDAQ::ScanExecutionJournalStore::Create(utf8Path,
            GetDAQ::ScanExecutionJournal(executionId, digest,
                snapshot.sha256,
                static_cast<unsigned int>(summary.pointCount)));
    }

    GetDAQ::ScanExecutionJournal current = store->Current();
    if (current.SourceSnapshotSha256().empty()) {
        const GetDAQ::ScanExecutionJournal bound =
            current.WithSnapshotBinding(snapshot.sha256);
        store->Persist(bound);
        current = store->Current();
    }
    RequireRecord(current.ExecutionId() == executionId &&
            current.SourcePlanSha256() == digest &&
            current.SourceSnapshotSha256() == snapshot.sha256 &&
            current.PointCount() == static_cast<unsigned int>(summary.pointCount),
            "Existing scan review journal belongs to a different plan or stopping policy");

    ScanPlanReviewResult result;
    result.journalPath = path;
    if (current.State() == GetDAQ::ScanPointState::Reviewed) {
        result.alreadyReviewed = true;
        return result;
    }
    RequireRecord(current.State() == GetDAQ::ScanPointState::Planned &&
            !current.HasPendingOperation(),
            "Scan journal is beyond the review step; preserve it for operator reconciliation");
    store->Persist(current.WithTransition(GetDAQ::ScanPointState::Planned,
                                          GetDAQ::ScanPointState::Reviewed));
    return result;
}

