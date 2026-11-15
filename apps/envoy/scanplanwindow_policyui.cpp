#include "scanplanwindow.h"
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

void ScanPlanWindow::BuildExecutionPolicyControls(QVBoxLayout *layout)
{
    QGroupBox *policyGroup = new QGroupBox(tr("Finite stopping policy for a validated plan"), this);
    policyGroup->setObjectName("ScanExecutionPolicyControls");
    QVBoxLayout *policyLayout = new QVBoxLayout(policyGroup);
    QLabel *policyExplanation = new QLabel(
        tr("This records offline stopping limits only. It does not configure or start a Run. "
           "Choose the values approved for this experiment; no acquisition defaults are supplied."),
        policyGroup);
    policyExplanation->setWordWrap(true);
    policyLayout->addWidget(policyExplanation);

    fEventPolicyControls = new QWidget(policyGroup);
    fEventPolicyControls->setObjectName("EventCountPolicyControls");
    QFormLayout *eventPolicyForm = new QFormLayout(fEventPolicyControls);
    fPolicyEvents = new QSpinBox(fEventPolicyControls);
    fPolicyEvents->setObjectName("PolicyEventsPerPoint");
    fPolicyEvents->setRange(0, 1000000000);
    fPolicyEvents->setSpecialValueText(tr("Choose an event count"));
    fPolicyMaximumDuration = new QSpinBox(fEventPolicyControls);
    fPolicyMaximumDuration->setObjectName("PolicyMaximumDurationMs");
    fPolicyMaximumDuration->setRange(0, 86400000);
    fPolicyMaximumDuration->setSpecialValueText(tr("Choose a timeout"));
    fPolicyMaximumDuration->setSuffix(tr(" ms"));
    eventPolicyForm->addRow(tr("Events per point"), fPolicyEvents);
    eventPolicyForm->addRow(tr("Maximum run time per point (timeout aborts)"), fPolicyMaximumDuration);
    policyLayout->addWidget(fEventPolicyControls);

    fThresholdPolicyNotice = new QLabel(
        tr("Threshold plans are manual-only for now. Automatic Run and timed-window policies are disabled; collect each point only through the separately reviewed manual procedure."),
        policyGroup);
    fThresholdPolicyNotice->setObjectName("ThresholdManualOnlyNotice");
    fThresholdPolicyNotice->setWordWrap(true);
    policyLayout->addWidget(fThresholdPolicyNotice);
    fCreatePolicy = new QPushButton(tr("Create finite stopping policy"), policyGroup);
    fCreatePolicy->setObjectName("CreateScanExecutionPolicy");
    fCreatePolicy->setEnabled(false);
    policyLayout->addWidget(fCreatePolicy);
    layout->addWidget(policyGroup);
    connect(fCreatePolicy, &QPushButton::clicked,
            this, &ScanPlanWindow::CreateExecutionPolicy);
    UpdatePolicyControls();
}
