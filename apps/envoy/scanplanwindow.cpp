#include "scanplanwindow.h"
#include "scanplanoutput.h"
#include "scanplanreview.h"
#include "eccprotocol.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSpinBox>
#include <QUuid>

ScanPlanWindow::ScanPlanWindow(QWidget *parent)
    : QDialog(parent), fProcess(new QProcess(this))
{
    BuildUi();
}

ScanPlanWindow::~ScanPlanWindow() = default;

void ScanPlanWindow::BindCompletedRun()
{
    if (fProcess->state() != QProcess::NotRunning) return;
    const ScanPlanOutputSummary plan = InspectScanPlanOutput(fPlanOutputDirectory);
    if (!plan.isValid() || plan.planSha256 != fPlanSha256 || plan.pointCount != fPlanPointCount) {
        fStatus->setText(tr("The scan plan changed or failed validation; no binding was attempted."));
        return;
    }
    ScanPlanRuntimeContext runtime;
    try {
        runtime = LoadScanPlanRuntimeContext(fPlanOutputDirectory,
            fReviewStorageDirectory, fPlanSha256, fPlanPointCount);
    } catch (const std::exception &error) {
        fStatus->setText(tr("Run binding requires an existing scan journal: %1")
                         .arg(QString::fromLocal8Bit(error.what())));
        return;
    }
    if (runtime.state != QStringLiteral("CatalogVerified") ||
        runtime.hasPendingOperation ||
        runtime.pendingUnknown) {
        fStatus->setText(tr("Only the current CatalogVerified point can be bound. "
                            "Current point %1 is %2; no catalog operation was started.")
                         .arg(runtime.pointIndex).arg(runtime.state));
        return;
    }
    const int point = runtime.pointIndex;
    fStatus->setText(tr("Binding the completed Run for current point %1.")
                     .arg(point));
    const QString catalog = QFileDialog::getExistingDirectory(
        this, tr("Choose the completed receiver catalog directory"));
    if (catalog.isEmpty()) return;
    const QString bindingDirectory = QFileDialog::getExistingDirectory(
        this, tr("Choose an existing directory for binding records"));
    if (bindingDirectory.isEmpty()) return;

    const QString executable = ResolveCatalogExecutable();
    if (executable.isEmpty()) {
        fStatus->setText(tr("A runnable getdaq-catalog was not selected; no operation was attempted."));
        return;
    }
    const QString filename = QStringLiteral("point-%1.json").arg(point, 3, 10, QLatin1Char('0'));
    const QString output = QDir(bindingDirectory).filePath(filename);
    const ScanPlanOutputSummary beforeLaunch = InspectScanPlanOutput(fPlanOutputDirectory);
    if (!beforeLaunch.isValid() || beforeLaunch.planSha256 != fPlanSha256 ||
        beforeLaunch.pointCount != fPlanPointCount) {
        fStatus->setText(tr("The scan plan changed while selecting inputs; no binding was attempted."));
        return;
    }
    const QStringList arguments = QStringList() << QStringLiteral("--bind-scan-point")
        << fPlanOutputDirectory << QString::number(point)
        << QFileInfo(catalog).absoluteFilePath() << output;
    const QString operationId = QStringLiteral("gui-%1-scan-bind")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    try {
        fScanBindingSession = BeginScanPointBinding(
            fPlanOutputDirectory, fReviewStorageDirectory, fPlanSha256,
            fPlanPointCount, point, QFileInfo(catalog).absoluteFilePath(),
            QFileInfo(output).absoluteFilePath(), operationId);
        const ScanPlanRecoveryStatus intent = fScanBindingSession->Current();
        if (intent.state != QStringLiteral("CatalogVerified") ||
            intent.operationId != operationId) {
            fStatus->setText(tr("Could not persist the exact scan binding intent; no operation was started."));
            return;
        }
    } catch (const std::exception &error) {
        fStatus->setText(tr("Could not persist the scan binding intent; no operation was started: %1")
                         .arg(QString::fromLocal8Bit(error.what())));
        return;
    }
    fCatalogOperation = tr("Scan point %1 to completed Run binding").arg(point);
    fScanPointBindingOperation = true;
    fPendingScanBindingOperationId = operationId;
    fPendingScanBindingPath = QFileInfo(output).absoluteFilePath();
    fStatus->setText(tr("Binding intent %1 is durable. Checking the catalog and exact configuration digest in a separate process; existing binding files will not be overwritten.")
                     .arg(operationId));
    fCreate->setEnabled(false);
    fOpenExisting->setEnabled(false);
    fBindRun->setEnabled(false);
    fSummarizeRuns->setEnabled(false);
    fProcess->setProgram(executable);
    fProcess->setArguments(arguments);
    fProcess->start();
    fRecovery->setEnabled(false);
    UpdatePolicyControls();
}

void ScanPlanWindow::SummarizeCompletedRuns()
{
    if (fProcess->state() != QProcess::NotRunning) return;
    const ScanPlanOutputSummary plan = InspectScanPlanOutput(fPlanOutputDirectory);
    if (!plan.isValid() || plan.planSha256 != fPlanSha256 || plan.pointCount != fPlanPointCount) {
        fStatus->setText(tr("The scan plan changed or failed validation; no summary was attempted."));
        return;
    }
    const QString bindings = QFileDialog::getExistingDirectory(
        this, tr("Choose the directory containing point-NNN.json bindings"));
    if (bindings.isEmpty()) return;
    const QString output = QFileDialog::getSaveFileName(
        this, tr("Choose a new scan index JSON path"), QString(), tr("JSON files (*.json)"));
    if (output.isEmpty()) return;
    const ScanPlanOutputSummary beforeLaunch = InspectScanPlanOutput(fPlanOutputDirectory);
    if (!beforeLaunch.isValid() || beforeLaunch.planSha256 != fPlanSha256 ||
        beforeLaunch.pointCount != fPlanPointCount) {
        fStatus->setText(tr("The scan plan changed while selecting inputs; no summary was attempted."));
        return;
    }

    const QString executable = ResolveCatalogExecutable();
    if (executable.isEmpty()) {
        fStatus->setText(tr("A runnable getdaq-catalog was not selected; no operation was attempted."));
        return;
    }
    const QStringList arguments = QStringList() << QStringLiteral("--summarize-scan")
        << fPlanOutputDirectory << QFileInfo(bindings).absoluteFilePath()
        << QFileInfo(output).absoluteFilePath();
    fCatalogOperation = tr("Complete scan-to-Run index");
    fStatus->setText(tr("Checking every point binding and archived catalog. The output path must not already exist."));
    fCreate->setEnabled(false);
    fOpenExisting->setEnabled(false);
    fBindRun->setEnabled(false);
    fSummarizeRuns->setEnabled(false);
    fProcess->setProgram(executable);
    fProcess->setArguments(arguments);
    fProcess->start();
}

void ScanPlanWindow::RecordPlanReview()
{
    if (fPlanSha256.size() != 64 || fPlanPointCount <= 0) {
        fStatus->setText(tr("Create and validate a scan plan before recording its review."));
        return;
    }
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Confirm scan plan review"),
        tr("Have you checked the complete point list and every generated configuration?\n\n"
           "This records only the review decision. It does not Configure hardware, start a Run, "
           "or access DAQ data."),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) return;
    try {
        const ScanPlanReviewResult result = RecordScanPlanReview(
            fPlanOutputDirectory, fReviewStorageDirectory,
            fPlanSha256, fPlanPointCount);
        fStatus->setText(result.alreadyReviewed
            ? tr("This exact plan was already marked Reviewed. Journal: %1").arg(result.journalPath)
            : tr("Plan review recorded. No hardware or DAQ action was sent. Journal: %1")
                .arg(result.journalPath));
        UpdatePolicyControls();
    } catch (const std::exception &error) {
        fStatus->setText(tr("Could not record plan review; no hardware action was sent: %1")
                         .arg(QString::fromLocal8Bit(error.what())));
    }
}
