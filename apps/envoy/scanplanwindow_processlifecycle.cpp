// Process launch failures and close gating for scan-plan tool subprocesses.
#include "scanplanwindow.h"
#include <stdexcept>
#include "scanplanreview.h"

#include <QCloseEvent>
#include <QLabel>
#include <QProcess>
#include <QPushButton>

void ScanPlanWindow::ProcessError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        if (fPolicyOperation) {
            const bool checkerFailed = fPolicyChecking;
            fPolicyOperation = false;
            fPolicyChecking = false;
            fCreate->setEnabled(true);
            fOpenExisting->setEnabled(true);
            UpdatePolicyControls();
            fStatus->setText(checkerFailed
                ? tr("The policy was created, but getdaq-config-check could not start. Preserve it for inspection. %1").arg(fProcess->errorString())
                : tr("Could not start getdaq-config to create the policy: %1").arg(fProcess->errorString()));
            return;
        }
        if (!fCatalogOperation.isEmpty()) {
            const QString catalogFailure = fProcess->errorString();
            if (fScanPointBindingOperation) {
                try {
                    if (!fScanBindingSession)
                        throw std::runtime_error("The journal lock for the pending bind is missing");
                    fScanBindingSession->MarkUnknown(
                        QStringLiteral("getdaq-catalog could not start after the bind intent was persisted: ") + catalogFailure);
                } catch (const std::exception &journalError) {
                    fStatus->setText(tr("Binding outcome is unresolved; inspect the original intent. Journal update also failed: %1")
                        .arg(QString::fromLocal8Bit(journalError.what())));
                    fCatalogOperation.clear();
                    fScanPointBindingOperation = false;
                    fScanBindingSession.reset();
                    fPendingScanBindingOperationId.clear();
                    fPendingScanBindingPath.clear();
                    UpdatePolicyControls();
                    return;
                }
                fCatalogOperation.clear();
                fScanPointBindingOperation = false;
                fScanBindingSession.reset();
                fPendingScanBindingOperationId.clear();
                fPendingScanBindingPath.clear();
                UpdatePolicyControls();
                fStatus->setText(tr("The binding intent is quarantined because the catalog process could not start. Inspect the exact operation before continuing. %1")
                                 .arg(catalogFailure));
                return;
            }
            fCatalogOperation.clear();
            fCreate->setEnabled(true);
            fOpenExisting->setEnabled(true);
            fBindRun->setEnabled(fPlanPointCount > 0);
            fSummarizeRuns->setEnabled(fPlanPointCount > 0);
            fStatus->setText(tr("Could not start getdaq-catalog: %1")
                             .arg(fProcess->errorString()));
            return;
        }
        fCreate->setEnabled(true);
        fOpenExisting->setEnabled(true);
        fStatus->setText(
            tr("Could not start the sibling getdaq-config program: %1")
                .arg(fProcess->errorString()));
    }
}

void ScanPlanWindow::closeEvent(QCloseEvent *event)
{
    if (fProcess->state() != QProcess::NotRunning) {
        fStatus->setText(tr("Wait for the offline metadata operation to finish before closing."));
        event->ignore();
        return;
    }

    QDialog::closeEvent(event);
}
