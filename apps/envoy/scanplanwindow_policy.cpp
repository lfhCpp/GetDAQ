#include "scanplanwindow.h"
#include "scanplanoutput.h"
#include "scanplanreview.h"
#include "eccprotocol.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>

void ScanPlanWindow::SetReviewStorageDirectory(const QString &directory)
{
    fReviewStorageDirectory = directory;
}

void ScanPlanWindow::SetNativeCatalogEnabled(bool enabled)
{
    fNativeCatalogEnabled=enabled;
    if(fStartScanPoint) {
        fStartScanPoint->setToolTip(enabled
            ? tr("Starts one whole-system Native ECC Run for the already loaded point. At the event target it performs Breakup, receiver EOF/seal, catalog and Run-record finalization. It does not Configure or load point settings, bind the archive, or advance the plan.")
            : tr("Enable native_catalog in the fixed Native configuration before running a scan point; each point needs its own completed archive."));
    }
    UpdatePolicyControls();
}

QString ScanPlanWindow::ResolveConfigExecutable() const
{
    QString executable = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("getdaq-config"));
#ifdef Q_OS_WIN
    executable += QStringLiteral(".exe");
#endif
    return QFileInfo(executable).isExecutable() ? executable : QString();
}

QString ScanPlanWindow::ResolveCatalogExecutable()
{
    if (!fCatalogExecutable.isEmpty() && QFileInfo(fCatalogExecutable).isExecutable())
        return fCatalogExecutable;

    QString executable = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("getdaq-catalog"));
#ifdef Q_OS_WIN
    executable += QStringLiteral(".exe");
#endif
    if (!QFileInfo(executable).isExecutable()) {
        QString name = QStringLiteral("getdaq-catalog");
#ifdef Q_OS_WIN
        name += QStringLiteral(".exe");
#endif
        executable = QStandardPaths::findExecutable(name);
    }
    if (executable.isEmpty() || !QFileInfo(executable).isExecutable()) {
#ifdef Q_OS_WIN
        const QString filter = tr("getdaq-catalog executable (getdaq-catalog.exe)");
#else
        const QString filter = tr("getdaq-catalog executable (getdaq-catalog)");
#endif
        executable = QFileDialog::getOpenFileName(
            this, tr("Locate the separately installed catalog metadata tool"),
            QCoreApplication::applicationDirPath(), filter);
    }
    if (executable.isEmpty() || !QFileInfo(executable).isExecutable())
        return QString();

    fCatalogExecutable = QFileInfo(executable).absoluteFilePath();
    return fCatalogExecutable;
}

void ScanPlanWindow::CreateExecutionPolicy()
{
    if (fProcess->state() != QProcess::NotRunning || fPolicyOperation) return;
    const ScanPlanOutputSummary plan = InspectScanPlanOutput(fPlanOutputDirectory);
    if (!plan.isValid() || plan.planSha256 != fPlanSha256 ||
        plan.pointCount != fPlanPointCount || plan.kind != fPlanKind) {
        UpdatePolicyControls();
        fStatus->setText(tr("The scan plan changed or failed validation; no stopping policy was created."));
        return;
    }

    const bool threshold = plan.kind == QStringLiteral("getdaq.aget-threshold-scan");
    if (threshold) {
        fStatus->setText(tr("Threshold plans are manual-only; automatic Run and timed-window policies are disabled."));
        UpdatePolicyControls();
        return;
    }
    const QString strategy = QStringLiteral("event-count");
    const int first = fPolicyEvents->value();
    const int second = fPolicyMaximumDuration->value();
    if (first <= 0 || second <= 0 ||
        static_cast<qint64>(first) > 1000000000LL || second > 86400000) {
        fStatus->setText(tr("Choose an event count and a maximum run time; timeout is an abort, not success."));
        return;
    }

    const QString executable = ResolveConfigExecutable();
    if (executable.isEmpty()) {
        fStatus->setText(tr("The sibling getdaq-config program is missing or not executable."));
        return;
    }
    const QString policyPath = QDir(fPlanOutputDirectory)
        .filePath(QStringLiteral("scan-execution-policy.json"));
    if (QFileInfo::exists(policyPath) || QFileInfo(policyPath).isSymLink()) {
        UpdatePolicyControls();
        fStatus->setText(tr("A policy file already exists. It is immutable; inspect it or create a new plan before choosing different values."));
        return;
    }

    const QString stoppingSummary =
        tr("%1 events per point, with a %2 ms maximum runtime (timeout aborts the point)")
            .arg(first).arg(second);
    if (QMessageBox::question(this, tr("Confirm offline stopping policy"),
            tr("This policy will be bound to the currently validated plan:\n%1\n\n"
               "Scan type: %2\nCoBo: %3\nPoints: %4\nStopping condition: %5\n\n"
               "This only writes control metadata. It will not configure hardware or start acquisition.")
                .arg(fPlanOutputDirectory, plan.kind, plan.targetCoBo)
                .arg(plan.pointCount).arg(stoppingSummary),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;

    const ScanPlanOutputSummary beforeLaunch = InspectScanPlanOutput(fPlanOutputDirectory);
    if (!beforeLaunch.isValid() || beforeLaunch.planSha256 != fPlanSha256 ||
        beforeLaunch.pointCount != fPlanPointCount || beforeLaunch.kind != fPlanKind) {
        fStatus->setText(tr("The plan changed while confirming values; no stopping policy was created."));
        return;
    }

    QStringList arguments;
    arguments << QStringLiteral("create-scan-policy") << fPlanOutputDirectory << strategy
              << QString::number(first) << QString::number(second) << policyPath;
    fPolicyOperation = true;
    fPolicyChecking = false;
    fPolicyOutputPath = policyPath;
    fCreate->setEnabled(false);
    fOpenExisting->setEnabled(false);
    fCreatePolicy->setEnabled(false);
    fStatus->setText(tr("Creating the finite stopping policy in a separate offline process. No hardware command will be sent."));
    fProcess->setProgram(executable);
    fProcess->setArguments(arguments);
    fProcess->start();
}
