#include "scanplanwindow.h"
#include "scanplanoutput.h"
#include "scanplanreview.h"
#include "eccprotocol.h"

#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>

#include <exception>
#include <stdexcept>

void ScanPlanWindow::QueryCurrentRuntimeIdentity()
{
    if (fRuntimeQueryPending || fPlanSha256.size() != 64 || fPlanPointCount <= 0)
        return;
    try {
        const ScanPlanRuntimeContext context = LoadScanPlanRuntimeContext(
            fPlanOutputDirectory, fReviewStorageDirectory,
            fPlanSha256, fPlanPointCount);
        if (!context.hasJournal || context.hasPendingOperation || context.pendingUnknown)
            throw std::runtime_error("Resolve the current journal operation before querying ECC identity");
        const bool adopting = context.state == QStringLiteral("Reviewed") &&
                              !context.runtimeIdentityBound;
        if (adopting) {
            const QMessageBox::StandardButton answer = QMessageBox::question(
                this, tr("Adopt the active ECC instance?"),
                tr("The console will not start the ECC service or send a hardware command.\n\n"
                   "Confirm that you already started the intended ECC instance outside this window. "
                   "The application will record an adoption intent, verify its identity and service-state metadata, "
                   "then bind it to the reviewed scan point."),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) return;
        }
        if (!adopting && context.state != QStringLiteral("EccStarted") &&
            context.state != QStringLiteral("ECCStarted") &&
            context.state != QStringLiteral("Described") &&
            context.state != QStringLiteral("Prepared") &&
            context.state != QStringLiteral("Configured"))
            throw std::runtime_error("ECC identity can be checked only after start-ecc and before Run start");
        fAdoptingRuntimeIdentity = adopting;
        fRuntimeIdentityVerified = false;
        fVerifiedRuntimeState.clear();
        fPendingRuntimeState = context.state;
        fRuntimeQueryPending = true;
        fPendingRuntimeJournalPath = context.journalPath;
        UpdatePolicyControls();
        fStatus->setText(adopting
            ? tr("Verifying the externally started ECC instance before adopting it for point %1.").arg(context.pointIndex + 1)
            : tr("Requesting a read-only ECC identity check for point %1. No hardware command or DAQ payload is involved.").arg(context.pointIndex + 1));
        emit QueryScanRuntimeIdentityRequested(context.journalPath,
            QFileInfo(fPlanOutputDirectory).absoluteFilePath(),
            context.snapshotBytes, static_cast<unsigned int>(context.pointIndex));
    } catch (const std::exception &error) {
        fAdoptingRuntimeIdentity = false;
        fStatus->setText(tr("ECC identity check was not started: %1")
                         .arg(QString::fromLocal8Bit(error.what())));
    }
}

void ScanPlanWindow::CaptureCurrentEventBaseline()
{
    if (fRuntimeQueryPending || fPlanSha256.size() != 64 || fPlanPointCount <= 0)
        return;
    try {
        const ScanPlanRuntimeContext context = LoadScanPlanRuntimeContext(
            fPlanOutputDirectory, fReviewStorageDirectory,
            fPlanSha256, fPlanPointCount);
        if (context.state != QStringLiteral("Configured") ||
            !context.runtimeIdentityBound || context.eventCountBaselineBound ||
            context.hasPendingOperation || context.pendingUnknown)
            throw std::runtime_error("Event baseline requires a Configured point with a bound runtime identity, no prior baseline, and no pending operation");
        fPendingRuntimeState = context.state;
        fRuntimeIdentityVerified = false;
        fVerifiedRuntimeState.clear();
        fRuntimeQueryPending = true;
        fAwaitingEventBaseline = true;
        fPendingRuntimeJournalPath = context.journalPath;
        UpdatePolicyControls();
        fStatus->setText(tr("Rechecking the active ECC identity before binding a receiver event baseline for point %1.")
                         .arg(context.pointIndex + 1));
        emit QueryScanRuntimeIdentityRequested(context.journalPath,
            QFileInfo(fPlanOutputDirectory).absoluteFilePath(),
            context.snapshotBytes, static_cast<unsigned int>(context.pointIndex));
    } catch (const std::exception &error) {
        fStatus->setText(tr("Receiver event baseline was not queried: %1")
                         .arg(QString::fromLocal8Bit(error.what())));
    }
}

void ScanPlanWindow::RuntimeIdentityQueryFinished(
    const QByteArray &journalBytes, const QByteArray &responseBytes,
    const QString &error)
{
    Q_UNUSED(journalBytes);
    Q_UNUSED(responseBytes);
    if (!fRuntimeQueryPending) return;
    if (!error.isEmpty()) {
        fRuntimeQueryPending = false;
        fAwaitingEventBaseline = false;
        fAdoptingRuntimeIdentity = false;
        fRuntimeIdentityVerified = false;
        fVerifiedRuntimeState.clear();
        UpdatePolicyControls();
        fStatus->setText(tr("ECC runtime identity was not accepted; any saved operation intent remains for reconciliation: %1")
                         .arg(error));
        return;
    }
    if (fAwaitingEventBaseline) {
        fAwaitingEventBaseline = false;
        fStatus->setText(tr("ECC runtime identity matches the journal. Querying the receiver event counter; this is read-only."));
        emit QueryScanEventCountBaselineRequested(fPendingRuntimeJournalPath);
        return;
    }
    fRuntimeIdentityVerified = true;
    fVerifiedRuntimeState = fPendingRuntimeState;
    fRuntimeQueryPending = false;
    UpdatePolicyControls();
    fStatus->setText(fAdoptingRuntimeIdentity
        ? tr("The matching external ECC instance was recorded and bound to this scan point. No ECC command or DAQ transfer was requested.")
        : tr("ECC runtime identity was freshly verified against the current scan point. No ECC state transition or DAQ transfer was requested."));
    fAdoptingRuntimeIdentity = false;
}

void ScanPlanWindow::EventBaselineQueryFinished(
    const QByteArray &journalBytes, const QByteArray &responseBytes,
    const QString &error)
{
    Q_UNUSED(journalBytes);
    fRuntimeQueryPending = false;
    UpdatePolicyControls();
    if (!error.isEmpty()) {
        fStatus->setText(tr("Receiver event baseline was not bound: %1").arg(error));
        return;
    }
    try {
        const GetDAQ::EccControlReceiverEvents events =
            GetDAQ::DecodeEccControlReceiverEvents(responseBytes.toStdString());
        fStatus->setText(tr("Receiver event baseline bound for %1: %2 cumulative events, PID %3. This records control metadata only; acquisition has not been started or stopped by the scan window.")
            .arg(QString::fromStdString(events.sourceId))
            .arg(QString::fromStdString(std::to_string(events.acquisitionEvents)))
            .arg(events.pid));
    } catch (const std::exception &decodeError) {
        fStatus->setText(tr("Event baseline was stored, but its display response could not be decoded: %1")
                         .arg(QString::fromLocal8Bit(decodeError.what())));
    }
}
