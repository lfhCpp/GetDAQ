// Scan-plan execution policy and runtime-state button gates.
#include "scanplanwindow.h"
#include "scanplanreview.h"

#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QStyle>
#include <QVariant>

void ScanPlanWindow::UpdatePolicyControls()
{
    const bool hasPlan = !fPlanOutputDirectory.isEmpty() && !fPlanSha256.isEmpty() &&
                         fPlanPointCount > 0;
    if (fCurrentPointState) {
        QString text = tr("Current point: no plan loaded.");
        QString visualState = QStringLiteral("normal");
        if (hasPlan) {
            try {
                const ScanPlanRuntimeContext context = LoadScanPlanRuntimeContext(
                    fPlanOutputDirectory, fReviewStorageDirectory,
                    fPlanSha256, fPlanPointCount);
                if (!context.hasJournal) {
                    text = tr("Plan loaded — no execution journal has been reviewed yet.");
                } else if (context.hasPendingOperation || context.pendingUnknown) {
                    visualState = QStringLiteral("error");
                    text = tr("Point %1 of %2 — %3; operation %4 needs reconciliation before continuing.")
                        .arg(context.pointIndex + 1).arg(fPlanPointCount)
                        .arg(context.state, context.pendingOperationId);
                } else if (context.state == QStringLiteral("Bound")) {
                    text = tr("Point %1 of %2 — Run catalog bound. Hardware retirement and next-point configuration are not automatic.")
                        .arg(context.pointIndex + 1).arg(fPlanPointCount);
                } else if (context.state == QStringLiteral("RunStarted")) {
                    visualState = QStringLiteral("attention");
                    text = tr("Point %1 of %2 — RunStarted is recorded. Check the live ECC state and inspect or reconcile the scan journal before issuing another lifecycle command.")
                        .arg(context.pointIndex + 1).arg(fPlanPointCount);
                } else if (context.state == QStringLiteral("RunStopped")) {
                    visualState = QStringLiteral("attention");
                    text = tr("Point %1 of %2 — acquisition Stop is recorded; Breakup, receiver sealing, catalog verification and Run binding may still be outstanding.")
                        .arg(context.pointIndex + 1).arg(fPlanPointCount);
                } else if (context.state == QStringLiteral("Retired")) {
                    visualState = QStringLiteral("complete");
                    text = context.pointIndex + 1 < fPlanPointCount
                        ? tr("Point %1 of %2 — retired. The next point still requires its own configuration and ECC identity check.")
                            .arg(context.pointIndex + 1).arg(fPlanPointCount)
                        : tr("Point %1 of %2 — retired; the scan plan is complete.")
                            .arg(context.pointIndex + 1).arg(fPlanPointCount);
                } else {
                    text = tr("Current point %1 of %2 — %3.")
                        .arg(context.pointIndex + 1).arg(fPlanPointCount)
                        .arg(context.state);
                }
            } catch (const std::exception &error) {
                const QString reason = QString::fromLocal8Bit(error.what());
                if (reason.contains(QStringLiteral("No reviewed scan execution journal exists"))) {
                    text = tr("Plan validated — confirm review to create its execution journal.");
                } else {
                    visualState = QStringLiteral("error");
                    text = tr("Current scan journal cannot be verified: %1").arg(reason);
                }
            }
        }
        fCurrentPointState->setText(text);
        fCurrentPointState->setProperty("state", visualState);
        fCurrentPointState->style()->unpolish(fCurrentPointState);
        fCurrentPointState->style()->polish(fCurrentPointState);
    }
    const bool threshold = fPlanKind == QStringLiteral("getdaq.aget-threshold-scan");
    fEventPolicyControls->setVisible(hasPlan && !threshold);
    fThresholdPolicyNotice->setVisible(hasPlan && threshold);
    const QString policyPath = QDir(fPlanOutputDirectory)
        .filePath(QStringLiteral("scan-execution-policy.json"));
    const QFileInfo policyInfo(policyPath);
    if (fReview)
        fReview->setEnabled(hasPlan && policyInfo.isFile() && !policyInfo.isSymLink() &&
                            fProcess->state() == QProcess::NotRunning &&
                            !fScanBindingSession);
    if (fQueryRuntimeIdentity)
        fQueryRuntimeIdentity->setEnabled(hasPlan && !fRuntimeQueryPending &&
                                          fProcess->state() == QProcess::NotRunning &&
                                          !fScanBindingSession);
    if (fCaptureEventBaseline)
        fCaptureEventBaseline->setEnabled(hasPlan && !fRuntimeQueryPending &&
                                          fProcess->state() == QProcess::NotRunning &&
                                          !fScanBindingSession);
    if (fRecovery)
        fRecovery->setEnabled(hasPlan && fProcess->state() == QProcess::NotRunning &&
                              !fScanBindingSession);
    QString lifecycleCommand;
    QString lifecycleLabel;
    if (hasPlan && fProcess->state() == QProcess::NotRunning &&
        !fScanBindingSession) {
        try {
            const ScanPlanRuntimeContext context = LoadScanPlanRuntimeContext(
                fPlanOutputDirectory, fReviewStorageDirectory, fPlanSha256, fPlanPointCount);
            if (!context.hasPendingOperation && !context.pendingUnknown) {
                if (context.state == QStringLiteral("ECCStarted")) {
                    lifecycleCommand = QStringLiteral("describe");
                    lifecycleLabel = tr("Describe current whole system…");
                } else if (context.state == QStringLiteral("Described")) {
                    lifecycleCommand = QStringLiteral("prepare");
                    lifecycleLabel = tr("Prepare current whole system…");
                } else if (context.state == QStringLiteral("Prepared")) {
                    lifecycleCommand = QStringLiteral("configure");
                    lifecycleLabel = tr("Configure current whole system…");
                }
            }
        } catch (...) {}
    }
    if (fAdvanceScanPoint) {
        fAdvanceScanPoint->setText(lifecycleLabel.isEmpty()
            ? tr("No lifecycle action available") : lifecycleLabel);
        fAdvanceScanPoint->setEnabled(!lifecycleCommand.isEmpty() &&
            fRuntimeIdentityVerified && fVerifiedRuntimeState==
                (lifecycleCommand==QStringLiteral("describe") ? QStringLiteral("ECCStarted") :
                 lifecycleCommand==QStringLiteral("prepare") ? QStringLiteral("Described") : QStringLiteral("Prepared")) &&
            !fRuntimeQueryPending && fProcess->state()==QProcess::NotRunning);
    }
    bool canStartPoint = false;
    if (hasPlan && !threshold && fNativeCatalogEnabled && fProcess->state() == QProcess::NotRunning &&
        !fScanBindingSession &&
        !fRuntimeQueryPending && QFileInfo(policyPath).isFile() && !policyInfo.isSymLink()) {
        try {
            const ScanPlanRuntimeContext context = LoadScanPlanRuntimeContext(
                fPlanOutputDirectory, fReviewStorageDirectory, fPlanSha256, fPlanPointCount);
            canStartPoint = context.state == QStringLiteral("Configured") &&
                context.runtimeIdentityBound && context.eventCountBaselineBound &&
                !context.hasPendingOperation && !context.pendingUnknown;
        } catch (...) {}
    }
    if (fStartScanPoint) fStartScanPoint->setEnabled(canStartPoint);
    bool canBindPoint = false;
    if (hasPlan && fProcess->state() == QProcess::NotRunning &&
        !fScanBindingSession &&
        !fRuntimeQueryPending) {
        try {
            const ScanPlanRuntimeContext context = LoadScanPlanRuntimeContext(
                fPlanOutputDirectory, fReviewStorageDirectory, fPlanSha256,
                fPlanPointCount);
            canBindPoint = context.state == QStringLiteral("CatalogVerified") &&
                !context.hasPendingOperation && !context.pendingUnknown;
        } catch (...) {}
    }
    if (fBindRun) fBindRun->setEnabled(canBindPoint);
    const bool outputAvailable = hasPlan && !QFileInfo::exists(policyPath) &&
                                 !QFileInfo(policyPath).isSymLink();
    fCreatePolicy->setEnabled(outputAvailable && !threshold &&
                              fProcess->state() == QProcess::NotRunning &&
                              !fScanBindingSession);
}

