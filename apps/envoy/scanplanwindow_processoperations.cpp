// Completion handlers for catalog binding and immutable policy processes.
#include "scanplanwindow.h"
#include "scanplanoutput.h"
#include "scanplanreview.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QStringList>

#include <stdexcept>

void ScanPlanWindow::HandleCatalogProcessFinished(
    int exitCode, QProcess::ExitStatus status, const QByteArray &outputBytes,
    const QString &output, const QString &error)
{
    if (!fCatalogOperation.isEmpty()) {
        const QString operation = fCatalogOperation;
        fCatalogOperation.clear();
        if (status != QProcess::NormalExit || exitCode != 0) {
            const QString message = error.isEmpty() ? fProcess->errorString() : error;
            if (fScanPointBindingOperation) {
                try {
                    if (!fScanBindingSession)
                        throw std::runtime_error("The journal lock for the pending bind is missing");
                    fScanBindingSession->MarkUnknown(
                        QStringLiteral("getdaq-catalog binding did not return a confirmed success: ") + message);
                } catch (const std::exception &journalError) {
                    fStatus->setText(tr("Binding operation outcome is unknown. The original intent must be inspected; journal update also failed: %1")
                        .arg(QString::fromLocal8Bit(journalError.what())));
                    fScanPointBindingOperation = false;
                    fPendingScanBindingOperationId.clear();
                    fPendingScanBindingPath.clear();
                    return;
                }
                fScanPointBindingOperation = false;
                fScanBindingSession.reset();
                fPendingScanBindingOperationId.clear();
                fPendingScanBindingPath.clear();
                UpdatePolicyControls();
                fStatus->setText(tr("Run binding outcome is unresolved and has been quarantined in the scan journal. Inspect the original operation before continuing. %1")
                                 .arg(message));
                return;
            }
            fStatus->setText(tr("Catalog operation failed; inspect existing files before retrying. %1")
                             .arg(message));
            return;
        }
        const ScanPlanOutputSummary currentPlan = InspectScanPlanOutput(fPlanOutputDirectory);
        if (!currentPlan.isValid() || currentPlan.planSha256 != fPlanSha256 ||
            currentPlan.pointCount != fPlanPointCount) {
            if (fScanPointBindingOperation) {
                try { if (fScanBindingSession) fScanBindingSession->MarkUnknown(
                    QStringLiteral("Scan plan changed after the catalog binding process returned")); }
                catch (...) {}
                fScanPointBindingOperation = false;
                fScanBindingSession.reset();
                fPendingScanBindingOperationId.clear();
                fPendingScanBindingPath.clear();
                UpdatePolicyControls();
            }
            fStatus->setText(tr("%1 reported success, but the opened scan plan changed during the operation. Inspect the produced metadata and preserve it; do not treat it as a binding to the previously reviewed plan.")
                             .arg(operation));
            return;
        }
        if (fScanPointBindingOperation) {
            try {
                if (!fScanBindingSession)
                    throw std::runtime_error("The journal lock for the pending bind is missing");
                const ScanPlanRecoveryStatus completed = fScanBindingSession->Complete(outputBytes);
                fScanPointBindingOperation = false;
                fScanBindingSession.reset();
                fPendingScanBindingOperationId.clear();
                fPendingScanBindingPath.clear();
                UpdatePolicyControls();
                fStatus->setText(tr("Run catalog binding verified and journal advanced to %1 at point %2. This records archive/configuration evidence only; it does not prove hardware register readback or start the next point.\n%3")
                    .arg(completed.state).arg(completed.pointIndex).arg(output));
                return;
            } catch (const std::exception &journalError) {
                try { if (fScanBindingSession) fScanBindingSession->MarkUnknown(
                    QStringLiteral("Binding output verification failed: ") +
                    QString::fromLocal8Bit(journalError.what())); }
                catch (...) {}
                fScanPointBindingOperation = false;
                fScanBindingSession.reset();
                fPendingScanBindingOperationId.clear();
                fPendingScanBindingPath.clear();
                UpdatePolicyControls();
                fStatus->setText(tr("Catalog tool returned success, but the binding could not be verified into the journal. Preserve the output and inspect the original operation: %1")
                    .arg(QString::fromLocal8Bit(journalError.what())));
                return;
            }
        }
        fStatus->setText(tr("%1 completed. This was metadata-only: it did not configure hardware, "
                            "control acquisition, or read DAQ payloads.\n%2")
                         .arg(operation, output));
        return;
    }
}

void ScanPlanWindow::HandlePolicyProcessFinished(
    int exitCode, QProcess::ExitStatus status,
    const QString &output, const QString &error)
{
    if (fPolicyOperation) {
        const bool success = status == QProcess::NormalExit && exitCode == 0;
        if (!success) {
            const QString message = error.isEmpty() ? fProcess->errorString() : error;
            const bool checkerFailed = fPolicyChecking;
            fPolicyOperation = false;
            fPolicyChecking = false;
            UpdatePolicyControls();
            fStatus->setText(checkerFailed
                ? tr("The policy file was created but its independent validation failed. Preserve it and inspect it before use. %1\n%2").arg(message, output)
                : tr("The policy was not created. %1").arg(message));
            return;
        }

        const ScanPlanOutputSummary currentPlan = InspectScanPlanOutput(fPlanOutputDirectory);
        if (!currentPlan.isValid() || currentPlan.planSha256 != fPlanSha256 ||
            currentPlan.pointCount != fPlanPointCount || currentPlan.kind != fPlanKind) {
            fPolicyOperation = false;
            fPolicyChecking = false;
            UpdatePolicyControls();
            fStatus->setText(tr("The scan plan changed during policy processing. Preserve the policy file for inspection; it is not approved for the current plan."));
            return;
        }

        if (!fPolicyChecking) {
            QString checker = QDir(QCoreApplication::applicationDirPath())
                .filePath(QStringLiteral("getdaq-config-check"));
#ifdef Q_OS_WIN
            checker += QStringLiteral(".exe");
#endif
            if (!QFileInfo(checker).isExecutable()) {
                fPolicyOperation = false;
                UpdatePolicyControls();
                fStatus->setText(tr("The policy file was created but getdaq-config-check is missing. Preserve the file; independent validation was not completed."));
                return;
            }
            fPolicyChecking = true;
            fCreate->setEnabled(false);
            fOpenExisting->setEnabled(false);
            fCreatePolicy->setEnabled(false);
            fStatus->setText(tr("Policy created. Revalidating its exact plan binding with the independent checker."));
            fProcess->setProgram(checker);
            fProcess->setArguments(QStringList() << QStringLiteral("scan-execution-policy")
                                   << fPlanOutputDirectory << fPolicyOutputPath);
            fProcess->start();
            return;
        }

        if (!output.contains(QStringLiteral("\"valid\":true")) ||
            !output.contains(QStringLiteral("\"hardware_command_sent\":false"))) {
            fPolicyOperation = false;
            fPolicyChecking = false;
            UpdatePolicyControls();
            fStatus->setText(tr("The checker exited successfully but did not return the expected offline validation record. Preserve the policy file and inspect it."));
            return;
        }
        fPolicyOperation = false;
        fPolicyChecking = false;
        UpdatePolicyControls();
        fStatus->setText(tr("Finite stopping policy created and independently validated.\n%1\n"
                            "This is metadata only; no Configure or Run command was sent.\n%2")
                         .arg(fPolicyOutputPath, output));
        return;
    }
}
