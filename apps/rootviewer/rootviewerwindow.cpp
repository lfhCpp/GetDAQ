#include "rootviewerwindow_p.h"
#include "rootviewerwindow.h"
#include "rootcanvas.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

RootViewerWindow::RootViewerWindow()
    : fCanvas(new EventCanvas), fFrame(new QSpinBox), fOpen(new QPushButton(tr("Open ROOT file…"))),
      fPrevious(new QPushButton(tr("Previous"))), fNext(new QPushButton(tr("Next"))),
      fDisplay(new QComboBox)
{
    setWindowTitle(tr("GetDAQ ROOT Event Viewer"));
    resize(1180, 820);
    fFrame->setRange(0, 0);
    fFrame->setEnabled(false);
    fPrevious->setEnabled(false);
    fNext->setEnabled(false);
    fDisplay->addItem(tr("Channel waveforms"));
    fDisplay->addItem(tr("Bucket × channel heatmap"));
    fDisplay->addItem(tr("3D surface (drag to rotate)"));
    fDisplay->addItem(tr("Legacy: event time"));
    fDisplay->addItem(tr("Legacy: event frequency"));
    fDisplay->addItem(tr("Legacy: event period"));
    fDisplay->addItem(tr("Legacy: bucket distribution"));
    fDisplay->addItem(tr("Legacy: channels per event"));

    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);
    QHBoxLayout *controls = new QHBoxLayout;
    controls->addWidget(fOpen);
    controls->addWidget(fPrevious);
    controls->addWidget(new QLabel(tr("Event entry"), this));
    controls->addWidget(fFrame);
    controls->addWidget(fNext);
    controls->addWidget(fDisplay);
    controls->addStretch(1);
    layout->addLayout(controls);
    layout->addWidget(fCanvas, 1);
    setCentralWidget(central);

    connect(fOpen, &QPushButton::clicked, this, [this]() { OpenFile(); });
    connect(fPrevious, &QPushButton::clicked, this, [this]() { fFrame->setValue(fFrame->value() - 1); });
    connect(fNext, &QPushButton::clicked, this, [this]() { fFrame->setValue(fFrame->value() + 1); });
    connect(fFrame, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, [this]() { ReadEvent(); });
    connect(fDisplay, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this]() { DisplayChanged(); });
    statusBar()->showMessage(tr("Offline ROOT viewer; opens files only and never receives DAQ data"));
}


std::unique_ptr<QMainWindow> CreateRootViewerWindow()
{
    return std::unique_ptr<QMainWindow>(new RootViewerWindow);
}
