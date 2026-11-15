#include "scanplanwindow.h"
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

void ScanPlanWindow::BuildPlanForm(QVBoxLayout *layout)
{
    QFormLayout *form = new QFormLayout;
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(9);
    layout->addLayout(form);

    QHBoxLayout *artifactRow = new QHBoxLayout;
    fArtifact = new QLineEdit(this);
    fArtifact->setObjectName("ScanArtifactPath");
    QPushButton *artifactBrowse = new QPushButton(tr("Browse..."), this);
    artifactBrowse->setObjectName("ScanArtifactBrowse");
    artifactRow->addWidget(fArtifact, 1);
    artifactRow->addWidget(artifactBrowse);

    QWidget *artifactWidget = new QWidget(this);
    artifactWidget->setLayout(artifactRow);
    form->addRow(tr("Complete system artifact"), artifactWidget);
    connect(artifactBrowse, &QPushButton::clicked,
            this, &ScanPlanWindow::ChooseArtifact);

    fCoBo = new QLineEdit(this);
    fCoBo->setObjectName("ScanTargetCoBo");
    fCoBo->setPlaceholderText(tr("For example C0"));
    form->addRow(tr("Target enabled CoBo ID"), fCoBo);

    fKind = new QComboBox(this);
    fKind->setObjectName("ScanKind");
    fKind->addItem(tr("Peaking time (16 hardware values)"),
                   QStringLiteral("peaking"));
    fKind->addItem(tr("Pulser amplitude (integer mV range)"),
                   QStringLiteral("pulser"));
    fKind->addItem(tr("AGET threshold code (0..127)"),
                   QStringLiteral("threshold"));
    fKind->addItem(tr("AutoGain (active channel × Pulser amplitude)"),
                   QStringLiteral("auto-gain"));
    form->addRow(tr("Scan type"), fKind);

    fStart = new QSpinBox(this);
    fStart->setObjectName("ScanStartMv");
    fStart->setRange(-1000, 1000);
    fStart->setValue(-1000);

    fStop = new QSpinBox(this);
    fStop->setObjectName("ScanStopMv");
    fStop->setRange(-1000, 1000);
    fStop->setValue(-900);

    fStep = new QSpinBox(this);
    fStep->setObjectName("ScanStepMv");
    fStep->setRange(1, 2000);
    fStep->setValue(100);

    fAsAd = new QSpinBox(this);
    fAsAd->setObjectName("ScanTargetAsAd");
    fAsAd->setRange(0, 3);
    fAget = new QSpinBox(this);
    fAget->setObjectName("ScanTargetAget");
    fAget->setRange(0, 3);
    QHBoxLayout *chipRow = new QHBoxLayout;
    chipRow->addWidget(new QLabel(tr("AsAd"), this));
    chipRow->addWidget(fAsAd);
    chipRow->addWidget(new QLabel(tr("AGET"), this));
    chipRow->addWidget(fAget);
    fChipControls = new QWidget(this);
    fChipControls->setObjectName("ScanTargetAgetControls");
    fChipControls->setLayout(chipRow);
    form->addRow(tr("Target active chip"), fChipControls);

    fFirstChannel = new QSpinBox(this);
    fFirstChannel->setObjectName("AutoGainFirstChannel");
    fFirstChannel->setRange(0, 67);
    fLastChannel = new QSpinBox(this);
    fLastChannel->setObjectName("AutoGainLastChannel");
    fLastChannel->setRange(0, 67);
    fLastChannel->setValue(67);
    QHBoxLayout *channelRow = new QHBoxLayout;
    channelRow->addWidget(new QLabel(tr("First channel"), this));
    channelRow->addWidget(fFirstChannel);
    channelRow->addWidget(new QLabel(tr("Last channel"), this));
    channelRow->addWidget(fLastChannel);
    fChannelControls = new QWidget(this);
    fChannelControls->setObjectName("AutoGainChannelControls");
    fChannelControls->setLayout(channelRow);
    form->addRow(tr("AutoGain channel range (0..67)"), fChannelControls);

    QHBoxLayout *rangeRow = new QHBoxLayout;
    rangeRow->addWidget(new QLabel(tr("Start"), this));
    rangeRow->addWidget(fStart);
    rangeRow->addWidget(new QLabel(tr("Stop"), this));
    rangeRow->addWidget(fStop);
    rangeRow->addWidget(new QLabel(tr("Step"), this));
    rangeRow->addWidget(fStep);
    rangeRow->addWidget(new QLabel(tr("mV"), this));

    QWidget *rangeWidget = new QWidget(this);
    rangeWidget->setLayout(rangeRow);
    fRangeLabel = new QLabel(this);
    fRangeLabel->setObjectName("ScanRangeDescription");
    form->addRow(fRangeLabel, rangeWidget);

    connect(fKind, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScanPlanWindow::UpdateMode);
    UpdateMode();

    QHBoxLayout *parentRow = new QHBoxLayout;
    fOutputParent = new QLineEdit(this);
    fOutputParent->setObjectName("ScanOutputParent");
    QPushButton *parentBrowse = new QPushButton(tr("Browse..."), this);
    parentBrowse->setObjectName("ScanOutputBrowse");
    parentRow->addWidget(fOutputParent, 1);
    parentRow->addWidget(parentBrowse);

    QWidget *parentWidget = new QWidget(this);
    parentWidget->setLayout(parentRow);
    form->addRow(tr("Existing output parent"), parentWidget);
    connect(parentBrowse, &QPushButton::clicked,
            this, &ScanPlanWindow::ChooseOutputParent);

    fFolder = new QLineEdit(QStringLiteral("scan-plan"), this);
    fFolder->setObjectName("ScanOutputFolder");
    form->addRow(tr("New plan folder name"), fFolder);
}
