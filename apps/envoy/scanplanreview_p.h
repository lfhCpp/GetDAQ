#ifndef GETDAQ_SCANPLANREVIEW_P_H
#define GETDAQ_SCANPLANREVIEW_P_H

#include "scanplanreview.h"
#include "scanplanoutput.h"
#include "../ecc/scanexecutionjournalstore.h"
#include "../ecc/scanoperationrunner.h"

#include <memory>
#include <string>

namespace ScanPlanReviewInternal {

struct SnapshotIdentity
{
    QString path;
    std::string bytes;
    std::string sha256;
};

QString ReviewJournalPath(const QString &applicationDataDirectory,
                          const QString &planSha256);
void EnsurePrivateDirectory(const QString &path);
SnapshotIdentity LoadOrCreateExecutionSnapshot(
    const QString &planDirectory, const QString &applicationDataDirectory,
    const ScanPlanOutputSummary &summary, bool createIfMissing);
ScanPlanRecoveryStatus DescribeJournal(
    const QString &path, const GetDAQ::ScanExecutionJournal &journal);
ScanPlanOutputSummary ValidateExpectedPlan(const QString &planDirectory,
                                           const QString &expectedPlanSha256,
                                           int expectedPointCount);
std::unique_ptr<GetDAQ::ScanExecutionJournalStore> OpenExpectedJournal(
    const QString &planDirectory, const QString &applicationDataDirectory,
    const ScanPlanOutputSummary &summary, const QString &path);

}

class ScanPointBindingSession::Impl
{
public:
    QString planDirectory;
    QString applicationDataDirectory;
    QString expectedPlanSha256;
    int expectedPointCount = 0;
    QString operationId;
    QString bindingPath;
    QString journalPath;
    ScanPlanOutputSummary summary;
    std::unique_ptr<GetDAQ::ScanExecutionJournalStore> store;
    std::unique_ptr<GetDAQ::ScanOperationRunner> runner;
};

#endif
