#include "scanplanwindow.h"
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

void ScanPlanWindow::BuildRuntimeControls(QVBoxLayout *layout, QWidget *page)
{
    QGroupBox *runtimeGroup = new QGroupBox(tr("Current configured point: ECC checks and Run control"), page);
    runtimeGroup->setObjectName("ScanRuntimeObservationControls");
    QVBoxLayout *runtimeLayout = new QVBoxLayout(runtimeGroup);
    fCurrentPointState = new QLabel(tr("Current point: no plan loaded."), runtimeGroup);
    fCurrentPointState->setObjectName("CurrentScanPointState");
    fCurrentPointState->setWordWrap(true);
    runtimeLayout->addWidget(fCurrentPointState);
    QLabel *runScopeNotice = new QLabel(
        tr("Limited Run control. The selected point must already be loaded and fully Configured in the ECC system. "
           "The identity check matches that live configuration to this point before Run. This starts and stops the "
           "whole ECC system. At the event target it completes Breakup, receiver EOF/seal, catalog and the Run record. "
           "It does not load point settings, bind the archived Run to the plan, or advance to another point."),
        runtimeGroup);
    runScopeNotice->setObjectName("ScanRunScopeNotice");
    runScopeNotice->setWordWrap(true);
    runtimeLayout->addWidget(runScopeNotice);
    QLabel *runtimeExplanation = new QLabel(
        tr("The identity and event-baseline checks are read-only. They bind fresh ECC and receiver metadata to the current scan journal and never access DAQ payloads. The receiver event baseline is available after the point is Configured and its runtime identity is bound."),
        runtimeGroup);
    runtimeExplanation->setWordWrap(true);
    runtimeLayout->addWidget(runtimeExplanation);
    fQueryRuntimeIdentity = new QPushButton(
        tr("Verify / adopt current ECC instance"), runtimeGroup);
    fQueryRuntimeIdentity->setObjectName("QueryScanRuntimeIdentity");
    runtimeLayout->addWidget(fQueryRuntimeIdentity);
    connect(fQueryRuntimeIdentity, &QPushButton::clicked,
            this, &ScanPlanWindow::QueryCurrentRuntimeIdentity);
    fCaptureEventBaseline = new QPushButton(
        tr("Recheck ECC and bind receiver event baseline"), runtimeGroup);
    fCaptureEventBaseline->setObjectName("CaptureScanEventBaseline");
    runtimeLayout->addWidget(fCaptureEventBaseline);
    connect(fCaptureEventBaseline, &QPushButton::clicked,
            this, &ScanPlanWindow::CaptureCurrentEventBaseline);
    fAdvanceScanPoint = new QPushButton(tr("Describe current whole system…"), runtimeGroup);
    fAdvanceScanPoint->setObjectName("AdvanceScanPointLifecycle");
    fAdvanceScanPoint->setToolTip(tr("Submits exactly one whole-system Native ECC lifecycle action. Recheck current ECC identity immediately before each action."));
    runtimeLayout->addWidget(fAdvanceScanPoint);
    connect(fAdvanceScanPoint, &QPushButton::clicked,
            this, &ScanPlanWindow::AdvanceCurrentScanPoint);
    fStartScanPoint = new QPushButton(tr("Run the currently configured point…"), runtimeGroup);
    fStartScanPoint->setObjectName("StartScanPoint");
    fStartScanPoint->setToolTip(tr("Starts one whole-system Native ECC Run for the already loaded point. At the event target it performs the normal Breakup, receiver EOF/seal, configured catalog and Run-record finalization. It does not Configure or load point settings, bind the archive, or advance the plan."));
    runtimeLayout->addWidget(fStartScanPoint);
    connect(fStartScanPoint, &QPushButton::clicked,
            this, &ScanPlanWindow::StartCurrentScanPoint);
    layout->addWidget(runtimeGroup);
    UpdatePolicyControls();
}
