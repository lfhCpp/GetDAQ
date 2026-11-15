#include "scanplanwindow.h"
#include "scanplanoutput.h"
#include "scanplanreview.h"
#include "eccprotocol.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStringList>

#include <stdexcept>

void ScanPlanWindow::ProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    fCreate->setEnabled(true);
    fOpenExisting->setEnabled(true);
    fBindRun->setEnabled(fPlanPointCount > 0);
    fSummarizeRuns->setEnabled(fPlanPointCount > 0);
    UpdatePolicyControls();
    const QByteArray outputBytes = fProcess->readAllStandardOutput();
    const QString output = QString::fromLocal8Bit(outputBytes).trimmed();
    const QString error = QString::fromLocal8Bit(fProcess->readAllStandardError()).trimmed();

    if (!fCatalogOperation.isEmpty()) {
        HandleCatalogProcessFinished(exitCode, status, outputBytes, output, error);
        return;
    }
    if (fPolicyOperation) {
        HandlePolicyProcessFinished(exitCode, status, output, error);
        return;
    }

    if (status != QProcess::NormalExit || exitCode != 0) {
        const QString message = error.isEmpty() ? fProcess->errorString() : error;
        fStatus->setText(tr("Plan was not created. %1").arg(message));
        return;
    }

    const ScanPlanOutputSummary plan = InspectScanPlanOutput(fPlanOutputDirectory);
    if (!plan.isValid()) {
        fStatus->setText(
            tr("The configuration program reported success, but the plan output failed "
               "validation: %1 Inspect the directory before using it: %2")
                .arg(plan.error, fPlanOutputDirectory));
        return;
    }

    fPlanSha256 = plan.planSha256;
    fPlanKind = plan.kind;
    fPlanPointCount = plan.pointCount;
    fRecovery->setEnabled(true);
    fBindRun->setEnabled(true);
    fSummarizeRuns->setEnabled(true);

    QString summary = tr("Plan created: %1\nScan kind: %2\nTarget CoBo: %3\n"
                         "Configuration points: %4\nPoint files present: %5/%4")
                          .arg(fPlanOutputDirectory, plan.kind, plan.targetCoBo)
                          .arg(plan.pointCount)
                          .arg(plan.presentFileCount);
    if (!output.isEmpty()) summary += QLatin1Char('\n') + output;
    summary += tr("\nThe plan contains offline settings only. Review every point and "
                  "run each through the normal full Configure/acquisition workflow.");
    fStatus->setText(summary);
    fReview->setEnabled(true);
    UpdatePolicyControls();
}
