#include "scanplanwindow.h"
#include "scanplanoutput.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>

void ScanPlanWindow::ChooseArtifact()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Choose a compiled system experiment artifact"),
        QString(),
        tr("System experiment artifact (*.json)"));

    if (!path.isEmpty()) {
        fArtifact->setText(path);
    }
}

void ScanPlanWindow::ChooseOutputParent()
{
    const QString path = QFileDialog::getExistingDirectory(
        this,
        tr("Choose an existing parent directory for the new plan"),
        fOutputParent->text());

    if (!path.isEmpty()) {
        fOutputParent->setText(path);
    }
}

void ScanPlanWindow::OpenExistingPlan()
{
    QFileDialog chooser(this, tr("Open an existing offline scan plan"));
    chooser.setFileMode(QFileDialog::Directory);
    chooser.setOption(QFileDialog::ShowDirsOnly, true);
    chooser.setOption(QFileDialog::DontUseNativeDialog, true);
    if (chooser.exec() != QDialog::Accepted || chooser.selectedFiles().isEmpty()) return;

    const QString directory = chooser.selectedFiles().first();
    const ScanPlanOutputSummary plan = InspectScanPlanOutput(directory);
    if (!plan.isValid()) {
        fPlanOutputDirectory.clear();
        fPlanSha256.clear();
        fPlanKind.clear();
        fPlanPointCount = 0;
        fReview->setEnabled(false);
        fRecovery->setEnabled(false);
        fBindRun->setEnabled(false);
        fSummarizeRuns->setEnabled(false);
        UpdatePolicyControls();
        fStatus->setText(tr("Existing plan was not opened because validation failed: %1")
                         .arg(plan.error));
        return;
    }

    fPlanOutputDirectory = QFileInfo(directory).absoluteFilePath();
    fPlanSha256 = plan.planSha256;
    fPlanKind = plan.kind;
    fPlanPointCount = plan.pointCount;
    fPolicyEvents->setValue(0);
    fPolicyMaximumDuration->setValue(0);
    fReview->setEnabled(true);
    fRecovery->setEnabled(true);
    fBindRun->setEnabled(true);
    fSummarizeRuns->setEnabled(true);
    UpdatePolicyControls();
    fStatus->setText(tr("Existing plan opened and fully validated: %1\n"
                         "Scan kind: %2\nTarget CoBo: %3\nConfiguration points: %4\n"
                         "Point files present: %5/%4\nSHA-256: %6\n\n"
                         "This is an offline inspection. It does not configure hardware, "
                         "start acquisition, or read DAQ files. The review button can "
                         "record an explicit local review decision.")
                         .arg(fPlanOutputDirectory, plan.kind, plan.targetCoBo)
                         .arg(plan.pointCount).arg(plan.presentFileCount)
                         .arg(plan.planSha256));
}

void ScanPlanWindow::UpdateMode()
{
    const QString mode = fKind->currentData().toString();
    const bool isPulser = mode == QStringLiteral("pulser");
    const bool isThreshold = mode == QStringLiteral("threshold");
    const bool isAutoGain = mode == QStringLiteral("auto-gain");
    fChipControls->setVisible(isThreshold || isAutoGain);
    fChannelControls->setVisible(isAutoGain);
    fRangeLabel->setText(isThreshold ? tr("Threshold code range (0..127)")
                                    : tr("Pulser amplitude range (integer mV)"));
    if (isThreshold) {
        fStart->setRange(0, 127);
        fStop->setRange(0, 127);
        fStep->setRange(1, 127);
        if (fStop->value() < fStart->value()) fStop->setValue(fStart->value());
    } else {
        fStart->setRange(-1000, 1000);
        fStop->setRange(-1000, 1000);
        fStep->setRange(1, 2000);
    }
    fStart->setEnabled(isPulser || isThreshold || isAutoGain);
    fStop->setEnabled(isPulser || isThreshold || isAutoGain);
    fStep->setEnabled(isPulser || isThreshold || isAutoGain);
}

void ScanPlanWindow::CreatePlan()
{
    fReview->setEnabled(false);
    fRecovery->setEnabled(false);
    fBindRun->setEnabled(false);
    fSummarizeRuns->setEnabled(false);
    fPlanSha256.clear();
    fPlanKind.clear();
    fPlanPointCount = 0;
    fPolicyEvents->setValue(0);
    fPolicyMaximumDuration->setValue(0);
    UpdatePolicyControls();
    const QFileInfo artifact(fArtifact->text().trimmed());
    const QFileInfo outputParent(fOutputParent->text().trimmed());
    const QString folder = fFolder->text().trimmed();

    if (!artifact.isFile() || folder.isEmpty() || folder == QStringLiteral(".") ||
        folder == QStringLiteral("..") || folder.contains('/') || folder.contains('\\')) {
        fStatus->setText(tr("Choose an existing system artifact and a simple new folder name."));
        return;
    }

    const QString coboId = fCoBo->text().trimmed();
    if (!outputParent.isDir() || coboId.isEmpty()) {
        fStatus->setText(tr("Choose an existing output parent and enter the target CoBo ID."));
        return;
    }

    const QString executable = ResolveConfigExecutable();
    if (executable.isEmpty()) {
        fStatus->setText(
            tr("The sibling getdaq-config program is missing or not executable."));
        return;
    }

    const QString outputDirectory = QDir(outputParent.absoluteFilePath()).filePath(folder);
    fPlanOutputDirectory = outputDirectory;
    QStringList arguments;
    const QString mode = fKind->currentData().toString();
    if (mode == QStringLiteral("peaking")) {
        arguments << QStringLiteral("plan-peaking-scan")
                  << artifact.absoluteFilePath()
                  << coboId
                  << outputDirectory;
    } else if (mode == QStringLiteral("pulser")) {
        const int start = fStart->value();
        const int stop = fStop->value();
        const int step = fStep->value();
        if (start > stop || step <= 0 || (stop - start) % step != 0) {
            fStatus->setText(
                tr("The ascending mV range must land exactly on the stop value."));
            return;
        }

        arguments << QStringLiteral("plan-pulser-scan")
                  << artifact.absoluteFilePath()
                  << coboId
                  << QString::number(start)
                  << QString::number(stop)
                  << QString::number(step)
                  << outputDirectory;
    } else if (mode == QStringLiteral("auto-gain")) {
        const int start = fStart->value();
        const int stop = fStop->value();
        const int step = fStep->value();
        if (start > stop || step <= 0 || (stop - start) % step != 0 ||
            fFirstChannel->value() > fLastChannel->value()) {
            fStatus->setText(tr("AutoGain channel and ascending mV ranges must be valid and land exactly on the stop value."));
            return;
        }
        arguments << QStringLiteral("plan-autogain-scan")
                  << artifact.absoluteFilePath() << coboId
                  << QString::number(fAsAd->value()) << QString::number(fAget->value())
                  << QString::number(fFirstChannel->value()) << QString::number(fLastChannel->value())
                  << QString::number(start) << QString::number(stop) << QString::number(step)
                  << outputDirectory;
    } else {
        const int start = fStart->value();
        const int stop = fStop->value();
        const int step = fStep->value();
        if (start > stop || step <= 0 || (stop - start) % step != 0) {
            fStatus->setText(tr("The ascending threshold-code range must land exactly on the stop value."));
            return;
        }
        arguments << QStringLiteral("plan-threshold-scan")
                  << artifact.absoluteFilePath()
                  << coboId
                  << QString::number(fAsAd->value())
                  << QString::number(fAget->value())
                  << QString::number(start)
                  << QString::number(stop)
                  << QString::number(step)
                  << outputDirectory;
    }

    fCreate->setEnabled(false);
    fOpenExisting->setEnabled(false);
    fStatus->setText(
        tr("Building plan in a separate configuration process. "
           "No hardware or receiver process is contacted."));
    fProcess->setProgram(executable);
    fProcess->setArguments(arguments);
    fProcess->start();
}

