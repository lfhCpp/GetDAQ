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

void ScanPlanWindow::AdvanceCurrentScanPoint()
{
    if (fRuntimeQueryPending || fPlanSha256.size()!=64 || fPlanPointCount<=0) return;
    try {
        const ScanPlanRuntimeContext context=LoadScanPlanRuntimeContext(
            fPlanOutputDirectory,fReviewStorageDirectory,fPlanSha256,fPlanPointCount);
        const QString command=context.state==QStringLiteral("ECCStarted") ? QStringLiteral("describe") :
            context.state==QStringLiteral("Described") ? QStringLiteral("prepare") :
            context.state==QStringLiteral("Prepared") ? QStringLiteral("configure") : QString();
        if (command.isEmpty() || context.hasPendingOperation || context.pendingUnknown ||
            !context.runtimeIdentityBound || !fRuntimeIdentityVerified ||
            fVerifiedRuntimeState!=context.state)
            throw std::runtime_error("A fresh identity check for the current journal state is required, with no unresolved operation");
        const QString action=command==QStringLiteral("describe") ? tr("Describe") :
            command==QStringLiteral("prepare") ? tr("Prepare") : tr("Configure");
        if (QMessageBox::question(this,tr("%1 the current scan point?").arg(action),
                tr("This action changes the whole Native ECC system, not only the selected CoBo. It submits exactly one %1 operation for the currently selected point; it does not load the point artifact, start Run, or advance to another point.\n\nPlan: %2\nPoint: %3 of %4\nTarget CoBo: %5\n\nRecheck ECC identity after this action before continuing.")
                    .arg(command).arg(fPlanOutputDirectory).arg(context.pointIndex+1)
                    .arg(fPlanPointCount).arg(fCoBo->text()),
                QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)
            return;
        fRuntimeIdentityVerified=false; fVerifiedRuntimeState.clear();
        fRuntimeQueryPending=true; UpdatePolicyControls();
        fStatus->setText(tr("Persisting one journaled whole-system %1 operation.").arg(command));
        emit ExecuteScanPointLifecycleOperationRequested(context.journalPath,command);
    } catch (const std::exception &error) {
        fStatus->setText(tr("Scan point action was not submitted: %1")
                         .arg(QString::fromLocal8Bit(error.what())));
    }
}

void ScanPlanWindow::StartCurrentScanPoint()
{
    if (fRuntimeQueryPending || fPlanSha256.size() != 64 || fPlanPointCount <= 0 ||
        fPlanKind == QStringLiteral("getdaq.aget-threshold-scan")) return;
    try {
        const ScanPlanRuntimeContext context = LoadScanPlanRuntimeContext(
            fPlanOutputDirectory, fReviewStorageDirectory,
            fPlanSha256, fPlanPointCount);
        if (context.state != QStringLiteral("Configured") ||
            !context.runtimeIdentityBound || !context.eventCountBaselineBound ||
            context.hasPendingOperation || context.pendingUnknown)
            throw std::runtime_error("Run start requires a Configured point, bound runtime identity and event baseline, with no pending operation");
        if (QMessageBox::question(this, tr("Start scan point?"),
                tr("This starts the complete Native ECC system, not only the selected CoBo. The selected point must already be loaded and fully Configured; this action does not Configure or load its artifact. "
                   "The bound receiver event count will stop the system Run at the configured threshold. "
                   "The maximum duration is an abort limit; a timeout will not mark this point successful. On success, the receiver will be finished/sealed and cataloged as a separate Run; this does not bind the archive or advance the plan.\n\n"
                   "Plan: %1\nPoint: %2 of %3\nCoBo: %4\nThe immutable execution policy in the scan journal supplies the required event count and timeout.")
                    .arg(fPlanOutputDirectory).arg(context.pointIndex + 1)
                    .arg(fPlanPointCount).arg(fCoBo->text()),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
            return;
        fRuntimeQueryPending = true;
        UpdatePolicyControls();
        fStatus->setText(tr("Submitting one journaled Native system Run start for this scan point."));
        emit StartScanPointRequested(context.journalPath);
    } catch (const std::exception &error) {
        fStatus->setText(tr("Scan point was not started: %1")
                         .arg(QString::fromLocal8Bit(error.what())));
    }
}

void ScanPlanWindow::ScanPointRunStatus(QString message,bool active)
{
    fRuntimeQueryPending = false;
    fRuntimeIdentityVerified = false;
    fVerifiedRuntimeState.clear();
    UpdatePolicyControls();
    fStatus->setText(message);
    if (active) fStartScanPoint->setEnabled(false);
}

void ScanPlanWindow::ReconcilePendingOperation()
{
    if (fPlanSha256.size() != 64 || fPlanPointCount <= 0) return;
    try {
        const ScanPlanRecoveryStatus current = InspectScanPlanRecovery(
            fPlanOutputDirectory, fReviewStorageDirectory,
            fPlanSha256, fPlanPointCount);
        if (!current.hasJournal) {
            fStatus->setText(tr("No local scan execution journal exists for this plan. "
                                "No hardware or DAQ action was sent."));
            return;
        }
        if (!current.pendingUnknown) {
            fStatus->setText(tr("Journal state: %1; point index: %2; no unknown "
                                "operation needs reconciliation. Journal: %3")
                             .arg(current.state).arg(current.pointIndex)
                             .arg(current.journalPath));
            return;
        }

        if (current.operation == QStringLiteral("describe") ||
            current.operation == QStringLiteral("prepare") ||
            current.operation == QStringLiteral("configure")) {
            const auto query = QMessageBox::question(
                this, tr("Observe the saved Native operation?"),
                tr("The scan journal quarantined this operation after restart. You can query the Native ECC service using the original operation ID. This sends only a read-only GET; it will not repeat the hardware command. A successful exact response plus a fresh controller-state readback can resolve the journal.\n\nOperation: %1\nID: %2\n\nIf the service has no result or reports failure, the journal remains unresolved and you can still record independent evidence manually.")
                    .arg(current.operation, current.operationId),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (query == QMessageBox::Yes) {
                const ScanPlanRuntimeContext context = LoadScanPlanRuntimeContext(
                    fPlanOutputDirectory, fReviewStorageDirectory,
                    fPlanSha256, fPlanPointCount);
                if (!context.hasPendingOperation || !context.pendingUnknown ||
                    context.pendingOperationId != current.operationId ||
                    context.pendingOperation != current.operation)
                    throw std::runtime_error("The reviewed plan/journal changed before recovery could start");
                fRuntimeQueryPending = true;
                UpdatePolicyControls();
                fStatus->setText(tr("Querying the saved operation ID with a read-only Native ECC request. No command will be resubmitted."));
                emit ObservePendingScanLifecycleOperationRequested(
                    current.journalPath, current.operationId, fPlanSha256,
                    context.snapshotBytes);
                return;
            }
        }

        const QString detail = tr(
            "The journal contains an operation whose result could not be determined.\n\n"
            "Point index: %1\nOperation: %2\nOperation ID: %3\n\n"
            "Inspect independent system logs/readbacks before choosing. The journal's "
            "evidence digest records your note but does not prove what the hardware did. "
            "Retain the note and source evidence in your experiment records.")
            .arg(current.pointIndex).arg(current.operation, current.operationId);
        QMessageBox decision(this);
        decision.setWindowTitle(tr("Manual scan operation reconciliation"));
        decision.setIcon(QMessageBox::Warning);
        decision.setText(detail);
        QPushButton *applied = decision.addButton(
            tr("Confirmed completed"), QMessageBox::AcceptRole);
        QPushButton *notSent = decision.addButton(
            tr("Confirmed not sent"), QMessageBox::DestructiveRole);
        decision.addButton(QMessageBox::Cancel);
        decision.exec();
        if (decision.clickedButton() != applied && decision.clickedButton() != notSent)
            return;

        bool accepted = false;
        const QString operatorId = QInputDialog::getText(
            this, tr("Operator identification"),
            tr("Operator ID (ASCII letters, digits, dot, underscore, or hyphen):"),
            QLineEdit::Normal, QString(), &accepted).trimmed();
        if (!accepted) return;
        const QString observation = QInputDialog::getMultiLineText(
            this, tr("Evidence observation"),
            tr("Describe which independent evidence you inspected and why it supports "
               "the selected disposition. This text is hashed, not stored in the journal; "
               "retain it with your experiment records:"), QString(), &accepted);
        if (!accepted) return;
        const ScanPlanRecoveryStatus updated = ReconcileScanPlanOperation(
            fPlanOutputDirectory, fReviewStorageDirectory,
            fPlanSha256, fPlanPointCount, current.operationId, operatorId,
            decision.clickedButton() == applied, observation);
        fStatus->setText(tr("Manual reconciliation recorded. Journal state is now %1 "
                            "at point index %2. No hardware or DAQ action was sent. "
                            "Journal: %3")
                         .arg(updated.state).arg(updated.pointIndex)
                         .arg(updated.journalPath));
    } catch (const std::exception &error) {
        fStatus->setText(tr("Could not inspect or reconcile the scan journal: %1")
                         .arg(QString::fromLocal8Bit(error.what())));
    }
}
