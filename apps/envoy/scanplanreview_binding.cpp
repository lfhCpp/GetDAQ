#include "scanplanreview_p.h"

#include "scanoperationrunner.h"
#include "../../common/experiment/scanexecutionstate.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
void Require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
}

using namespace ScanPlanReviewInternal;

ScanPointBindingSession::ScanPointBindingSession(std::unique_ptr<Impl> impl)
    : fImpl(std::move(impl))
{
}

ScanPointBindingSession::~ScanPointBindingSession() = default;

ScanPlanRecoveryStatus ScanPointBindingSession::Current() const
{
    return DescribeJournal(fImpl->journalPath, fImpl->store->Current());
}

ScanPlanRecoveryStatus ScanPointBindingSession::Complete(
    const QByteArray &responseBytes)
{
    const ScanPlanOutputSummary summary = ValidateExpectedPlan(
        fImpl->planDirectory, fImpl->expectedPlanSha256,
        fImpl->expectedPointCount);
    const GetDAQ::ScanExecutionJournal current = fImpl->store->Current();
    Require(current.State() == GetDAQ::ScanPointState::CatalogVerified &&
            current.PendingOperation() == "bind" &&
            current.PendingOperationId() == fImpl->operationId.toStdString() &&
            !current.PendingOperationUnknown(),
            "Scan binding completion does not match its durable intent");

    const QFileInfo bindingInfo(fImpl->bindingPath);
    Require(bindingInfo.isFile() && !bindingInfo.isSymLink() &&
            bindingInfo.size() > 0 && bindingInfo.size() <= 65536 &&
            responseBytes.size() == bindingInfo.size(),
            "Published scan binding is missing, unsafe, or outside its size limit");
    QFile bindingFile(fImpl->bindingPath);
    Require(bindingFile.open(QIODevice::ReadOnly),
            "Cannot read the published scan binding");
    const QByteArray publishedBytes = bindingFile.readAll();
    Require(bindingFile.error() == QFileDevice::NoError &&
            publishedBytes == responseBytes,
            "Published binding differs from the catalog tool response");
    QJsonParseError parseError;
    const QJsonDocument binding = QJsonDocument::fromJson(responseBytes, &parseError);
    Require(parseError.error == QJsonParseError::NoError && binding.isObject(),
            "Catalog tool response is not a valid scan binding object");
    const QJsonObject object = binding.object();
    const auto isDigest = [](const QString &value) {
        if (value.size() != 64) return false;
        for (QChar character : value) {
            if (!((character >= QLatin1Char('0') && character <= QLatin1Char('9')) ||
                  (character >= QLatin1Char('a') && character <= QLatin1Char('f'))))
                return false;
        }
        return true;
    };
    const QJsonValue pointIndexValue = object.value(QStringLiteral("point_index"));
    const double pointIndexNumber = pointIndexValue.toDouble(-1.0);
    const bool pointIndexMatches = pointIndexValue.isDouble() &&
        pointIndexNumber >= 0.0 &&
        pointIndexNumber <= static_cast<double>(std::numeric_limits<unsigned int>::max()) &&
        std::floor(pointIndexNumber) == pointIndexNumber &&
        static_cast<unsigned int>(pointIndexNumber) == current.PointIndex();
    Require(object.value(QStringLiteral("schema")).toInt(-1) == 1 &&
            object.value(QStringLiteral("kind")).toString() ==
                QStringLiteral("getdaq.scan-run-binding") &&
            object.value(QStringLiteral("scan_kind")).toString() == summary.kind &&
            object.value(QStringLiteral("target_cobo")).toString() == summary.targetCoBo &&
            pointIndexMatches &&
            !object.value(QStringLiteral("point_artifact")).toString().isEmpty() &&
            isDigest(object.value(QStringLiteral("configuration_sha256")).toString()) &&
            isDigest(object.value(QStringLiteral("artifact_sha256")).toString()) &&
            object.value(QStringLiteral("scan_manifest_sha256")).toString() ==
                summary.planSha256 &&
            !object.value(QStringLiteral("experiment")).toString().isEmpty() &&
            object.value(QStringLiteral("run_number")).isDouble() &&
            object.value(QStringLiteral("run_number")).toDouble() >= 0 &&
            isDigest(object.value(QStringLiteral("session_id")).toString()) &&
            !object.value(QStringLiteral("catalog_directory")).toString().isEmpty() &&
            isDigest(object.value(QStringLiteral("catalog_request_key")).toString()) &&
            isDigest(object.value(QStringLiteral("catalog_manifest_sha256")).toString()) &&
            isDigest(object.value(QStringLiteral("catalog_receipt_sha256")).toString()) &&
            object.value(QStringLiteral("hardware_loaded_verified")).isBool() &&
            !object.value(QStringLiteral("hardware_loaded_verified")).toBool(),
            "Catalog tool response does not identify this immutable plan point");

    fImpl->runner->Complete(fImpl->operationId.toStdString(),
        std::string(responseBytes.constData(),
                    static_cast<std::size_t>(responseBytes.size())));
    return Current();
}

ScanPlanRecoveryStatus ScanPointBindingSession::MarkUnknown(const QString &reason)
{
    const GetDAQ::ScanExecutionJournal current = fImpl->store->Current();
    Require(current.PendingOperation() == "bind" &&
            current.PendingOperationId() == fImpl->operationId.toStdString(),
            "Unknown scan binding result does not match its durable intent");
    if (!current.PendingOperationUnknown())
        fImpl->runner->MarkUnknown(fImpl->operationId.toStdString(),
                                   reason.toStdString());
    return Current();
}

std::unique_ptr<ScanPointBindingSession> BeginScanPointBinding(
    const QString &planDirectory, const QString &applicationDataDirectory,
    const QString &expectedPlanSha256, int expectedPointCount, int pointIndex,
    const QString &catalogDirectory, const QString &bindingPath,
    const QString &operationId)
{
    const ScanPlanOutputSummary summary = ValidateExpectedPlan(
        planDirectory, expectedPlanSha256, expectedPointCount);
    Require(pointIndex >= 0 && pointIndex < summary.pointCount,
            "Scan binding point index is outside the validated plan");
    Require(QFileInfo(catalogDirectory).isDir() &&
            !QFileInfo(catalogDirectory).isSymLink(),
            "Completed catalog must be an existing non-symlink directory");
    Require(!QFileInfo::exists(bindingPath) && !QFileInfo(bindingPath).isSymLink(),
            "Scan binding output already exists; preserve it");

    const QString journalPath = ReviewJournalPath(applicationDataDirectory,
                                                  summary.planSha256);
    std::unique_ptr<GetDAQ::ScanExecutionJournalStore> store = OpenExpectedJournal(
        planDirectory, applicationDataDirectory, summary, journalPath);
    const GetDAQ::ScanExecutionJournal current = store->Current();
    Require(current.State() == GetDAQ::ScanPointState::CatalogVerified &&
            current.PointIndex() == static_cast<unsigned int>(pointIndex) &&
            !current.HasPendingOperation(),
            "Only the current CatalogVerified scan point can be bound");

    QJsonObject request;
    request.insert(QStringLiteral("operation_id"), operationId);
    request.insert(QStringLiteral("plan_sha256"), summary.planSha256);
    request.insert(QStringLiteral("point_index"), pointIndex);
    request.insert(QStringLiteral("catalog_directory"),
                   QFileInfo(catalogDirectory).absoluteFilePath());
    request.insert(QStringLiteral("binding_path"),
                   QFileInfo(bindingPath).absoluteFilePath());
    const QByteArray requestBytes = QJsonDocument(request).toJson(QJsonDocument::Compact);
    std::unique_ptr<ScanPointBindingSession::Impl> impl(
        new ScanPointBindingSession::Impl);
    impl->planDirectory = planDirectory;
    impl->applicationDataDirectory = applicationDataDirectory;
    impl->expectedPlanSha256 = expectedPlanSha256;
    impl->expectedPointCount = expectedPointCount;
    impl->operationId = operationId;
    impl->bindingPath = QFileInfo(bindingPath).absoluteFilePath();
    impl->journalPath = journalPath;
    impl->summary = summary;
    impl->store = std::move(store);
    impl->runner.reset(new GetDAQ::ScanOperationRunner(*impl->store));
    impl->runner->PersistIntent(operationId.toStdString(), "bind",
                         std::string(requestBytes.constData(),
                                     static_cast<std::size_t>(requestBytes.size())));
    return std::unique_ptr<ScanPointBindingSession>(
        new ScanPointBindingSession(std::move(impl)));
}


