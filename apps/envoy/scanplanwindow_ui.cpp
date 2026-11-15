#include "scanplanwindow.h"
#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

void ScanPlanWindow::BuildUi()
{
    setObjectName("NativeOfflineScanPlanWindow");
    setWindowTitle(tr("Scan plan and Run control"));
    setMinimumSize(760, 600);
    resize(940, 780);
    setStyleSheet(QStringLiteral(
        "QDialog#NativeOfflineScanPlanWindow { background:#f3f6fa; color:#1d2a3a; }"
        "QLabel#ScanPlanHeading { color:#18324f; font-size:25px; font-weight:700; }"
        "QLabel#ScanPlanIntro { color:#52677e; background:#e8f0fa; border:1px solid #d2e0ef; border-radius:8px; padding:13px; }"
        "QLabel#ScanRunScopeNotice { color:#6d4e16; background:#fff5df; border:1px solid #ead39d; border-left:4px solid #d39a2c; border-radius:6px; padding:10px 12px; font-weight:600; }"
        "QLabel#CurrentScanPointState { color:#254768; background:#e8f0fa; border:1px solid #d2e0ef; border-radius:6px; padding:9px 11px; font-weight:700; }"
        "QLabel#CurrentScanPointState[state='error'] { color:#8b3937; background:#fbe9e7; border-color:#efc3bf; }"
        "QLabel#CurrentScanPointState[state='complete'] { color:#176b50; background:#e0f3eb; border-color:#b6e1cf; }"
        "QLabel#CurrentScanPointState[state='attention'] { color:#755313; background:#fff5df; border-color:#ead39d; }"
        "QGroupBox { background:#ffffff; border:1px solid #d6e0ea; border-radius:8px; margin-top:13px; padding:12px; color:#25415e; font-weight:700; }"
        "QGroupBox::title { subcontrol-origin:margin; left:12px; padding:0 5px; }"
        "QLabel { color:#344b63; }"
        "QLineEdit, QSpinBox, QComboBox { min-height:27px; background:#ffffff; border:1px solid #cbd6e2; border-radius:5px; padding:3px 7px; selection-background-color:#cfe2f7; }"
        "QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border:1px solid #4b83b5; }"
        "QPushButton { min-height:30px; background:#ffffff; border:1px solid #cbd6e2; border-radius:5px; padding:5px 10px; color:#263b51; }"
        "QPushButton:hover { background:#edf4fc; border-color:#8eabc9; }"
        "QPushButton:pressed { background:#dce9f7; }"
        "QPushButton:disabled { background:#eef1f4; color:#9aa6b2; border-color:#dce2e8; }"
        "QPushButton#CreateScanPlan, QPushButton#CreateScanExecutionPolicy { background:#2466a3; color:white; border-color:#2466a3; font-weight:700; }"
        "QPushButton#CreateScanPlan:hover, QPushButton#CreateScanExecutionPolicy:hover { background:#1b568d; }"
        "QPushButton#StartScanPoint { background:#258568; color:white; border-color:#258568; font-weight:700; padding:8px 14px; }"
        "QPushButton#StartScanPoint:hover { background:#1d7057; }"
        "QPushButton#AdvanceScanPointLifecycle { background:#2466a3; color:white; border-color:#2466a3; font-weight:700; padding:8px 14px; }"
        "QPushButton#AdvanceScanPointLifecycle:hover { background:#1b568d; }"
        "QPushButton#AdvanceScanPointLifecycle:disabled { background:#eef1f4; color:#9aa6b2; border-color:#dce2e8; }"
        "QLabel#ScanPlanStatus { background:#ffffff; border:1px solid #d6e0ea; border-left:4px solid #4b83b5; border-radius:5px; padding:9px; color:#344b63; }"
        "QScrollArea { border:0; background:transparent; }"
        "QScrollArea > QWidget > QWidget { background:transparent; }"));

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *page = new QWidget(scroll);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(22, 18, 22, 22);
    layout->setSpacing(12);
    scroll->setWidget(page);
    rootLayout->addWidget(scroll);

    QLabel *heading = new QLabel(tr("Scan plan and Run control"), page);
    heading->setObjectName("ScanPlanHeading");
    layout->addWidget(heading);
    QLabel *explanation = new QLabel(
        tr("Build an immutable scan plan from a reviewed complete-system configuration, then "
           "verify and run one already configured point at a time. Plan creation is offline; "
           "the separate Native Run controls connect to ECC for live identity checks and whole-system "
           "lifecycle actions. DAQ payload transfer remains in the receiver service."),
        page);
    explanation->setObjectName("ScanPlanIntro");
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    BuildPlanForm(layout);
    BuildExecutionPolicyControls(layout);
    BuildRuntimeControls(layout, page);
    BuildJournalControls(layout);

    connect(fProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ScanPlanWindow::ProcessFinished);
    connect(fProcess, &QProcess::errorOccurred,
            this, &ScanPlanWindow::ProcessError);
}

