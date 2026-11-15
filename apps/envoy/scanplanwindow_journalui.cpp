#include "scanplanwindow.h"
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

void ScanPlanWindow::BuildJournalControls(QVBoxLayout *layout)
{
    fStatus = new QLabel(tr("No plan created."), this);
    fStatus->setObjectName("ScanPlanStatus");
    fStatus->setWordWrap(true);
    layout->addWidget(fStatus);

    fOpenExisting = new QPushButton(tr("Open an existing offline plan for review"), this);
    fOpenExisting->setObjectName("OpenExistingScanPlan");
    layout->addWidget(fOpenExisting);
    connect(fOpenExisting, &QPushButton::clicked,
            this, &ScanPlanWindow::OpenExistingPlan);

    fCreate = new QPushButton(tr("Create new offline plan"), this);
    fCreate->setObjectName("CreateScanPlan");
    layout->addWidget(fCreate);
    connect(fCreate, &QPushButton::clicked,
            this, &ScanPlanWindow::CreatePlan);

    fReview = new QPushButton(tr("Confirm plan reviewed and save journal"), this);
    fReview->setObjectName("RecordScanPlanReview");
    fReview->setEnabled(false);
    layout->addWidget(fReview);
    connect(fReview, &QPushButton::clicked,
            this, &ScanPlanWindow::RecordPlanReview);

    fRecovery = new QPushButton(tr("Inspect or reconcile a pending operation"), this);
    fRecovery->setObjectName("ReconcilePendingScanOperation");
    fRecovery->setEnabled(false);
    layout->addWidget(fRecovery);
    connect(fRecovery, &QPushButton::clicked,
            this, &ScanPlanWindow::ReconcilePendingOperation);
    fBindRun = new QPushButton(tr("Bind completed Run catalog to current point"), this);
    fBindRun->setObjectName("BindCompletedScanRun");
    fBindRun->setToolTip(tr("Only the current journal point can be bound, after receiver sealing and catalog verification."));
    fBindRun->setEnabled(false);
    layout->addWidget(fBindRun);
    connect(fBindRun, &QPushButton::clicked, this, &ScanPlanWindow::BindCompletedRun);
    fSummarizeRuns = new QPushButton(tr("Build the complete scan-to-Run index"), this);
    fSummarizeRuns->setObjectName("SummarizeCompletedScanRuns");
    fSummarizeRuns->setEnabled(false);
    layout->addWidget(fSummarizeRuns);
    connect(fSummarizeRuns, &QPushButton::clicked, this, &ScanPlanWindow::SummarizeCompletedRuns);
    fReviewStorageDirectory = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
}
