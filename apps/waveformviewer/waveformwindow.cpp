#include <QScrollArea>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>
#include <iostream>

#include "waveformcanvas.h"
#include "waveformwindow.h"
#include "waveformwindow_p.h"

namespace GetDAQ {
namespace detail {

WaveformWindow::WaveformWindow(QWidget *parent) : QMainWindow(parent,Qt::Window), fFileName(), fCanvas(new WaveformCanvas), fFrame(new QSpinBox),
        fAget(new QSpinBox), fChannelsButton(new QPushButton), fOpen(new QPushButton(tr("Open waveform..."))),
        fPrevious(new QPushButton(tr("Previous"))), fNext(new QPushButton(tr("Next"))),
        fAutoScan(new QCheckBox(tr("Auto scan"))), fScanPeriod(new QSpinBox), fHasFile(false),
        fSurfaceToggle(new QCheckBox(tr("3D"))),
        fExport(new QPushButton(tr("Export plot/data..."))),
        fMetrics(new QLabel(tr("Pulse metrics: not calculated"))), fIndexTask(0), fRawFile(false)
    {
        setObjectName("WaveformViewerWindow");
        setWindowTitle(tr("GetDAQ Waveform Viewer"));
        resize(1180, 720);
        fCanvas->setObjectName("WaveformCanvas");
        fFrame->setObjectName("WaveformFrame");
        fAget->setObjectName("WaveformAget");
        fChannelsButton->setObjectName("WaveformChannels");
        fOpen->setObjectName("WaveformOpen");
        fFrame->setRange(0, 0);
        fAget->setRange(0, 3);
        for (int channel = 0; channel < 68; ++channel) fChannels.append(channel);
        UpdateChannelButton();
        fScanPeriod->setRange(50, 300000);
        fScanPeriod->setValue(2000);
        fScanPeriod->setSuffix(tr(" ms"));
        fPrevious->setEnabled(false);
        fNext->setEnabled(false);
        fAutoScan->setEnabled(false);
        fSurfaceToggle->setToolTip(tr("Show separate waveforms: time bucket × amplitude × channel. Ctrl+wheel changes channel spacing; double-click resets the view."));
        QWidget *central = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(central);
        QHBoxLayout *controls = new QHBoxLayout;
        controls->addWidget(fOpen);
        controls->addWidget(fPrevious);
        controls->addWidget(fNext);
        controls->addWidget(fAutoScan);
        controls->addWidget(new QLabel(tr("Period")));
        controls->addWidget(fScanPeriod);
        controls->addWidget(fSurfaceToggle);
        auto *align=new QCheckBox(tr("Align baselines"),central);align->setObjectName("WaveformAlignBaselines");align->setChecked(true);align->setEnabled(false);align->setToolTip(tr("3D only: subtract each channel's median baseline for evenly spaced zero references. The amplitude axis becomes delta ADC; 2D and CSV keep raw ADC values. Uncheck for absolute ADC."));controls->addWidget(align);connect(fSurfaceToggle,&QCheckBox::toggled,align,&QWidget::setEnabled);connect(align,&QCheckBox::toggled,fCanvas,[this](bool enabled){fCanvas->SetBaselineAlignment(enabled);});
        fExport->setEnabled(false);
        controls->addWidget(fExport);
        controls->addStretch(1);
        layout->addLayout(controls);
        auto *eventSelection=new QHBoxLayout;
        fEvent=new QLineEdit;fEvent->setObjectName("WaveformEvent");fEvent->setFixedWidth(125);
        fEvent->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9]{1,10}"),fEvent));
        fFindEvent=new QPushButton(tr("Go to event"));fFindEvent->setObjectName("WaveformFindEvent");
        fEventFrames=new QComboBox;fEventFrames->setObjectName("WaveformEventFrames");fEventFrames->setMinimumContentsLength(22);fEventFrames->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        eventSelection->addWidget(new QLabel(tr("Event ID")));eventSelection->addWidget(fEvent);eventSelection->addWidget(fFindEvent);eventSelection->addWidget(fEventFrames,1);
        eventSelection->addSpacing(12);
        eventSelection->addWidget(new QLabel(tr("Frame (0-based)")));eventSelection->addWidget(fFrame);
        eventSelection->addWidget(new QLabel(tr("AGET")));eventSelection->addWidget(fAget);eventSelection->addWidget(fChannelsButton);
        layout->addLayout(eventSelection);
        fEvent->setEnabled(false);fFindEvent->setEnabled(false);fEventFrames->setEnabled(false);
        connect(fFindEvent,&QPushButton::clicked,this,[this]{JumpToEvent();});connect(fEvent,&QLineEdit::returnPressed,this,[this]{JumpToEvent();});
        connect(fEventFrames,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int index){if(index>=0)fFrame->setValue(fEventFrames->itemData(index).toInt());});
        auto *plotScroll=new QScrollArea;plotScroll->setWidgetResizable(true);plotScroll->setFrameShape(QFrame::NoFrame);plotScroll->setWidget(fCanvas);layout->addWidget(plotScroll,1);
        fMetrics->setWordWrap(true);
        layout->addWidget(fMetrics);
        setCentralWidget(central);
        connect(fOpen, &QPushButton::clicked, this, [this]() { OpenFile(); });
        connect(fChannelsButton, &QPushButton::clicked, this, [this]() { SelectChannels(); });
        connect(fSurfaceToggle, &QCheckBox::toggled, fCanvas, [this](bool enabled) { fCanvas->SetSurface(enabled); });
        connect(fExport, &QPushButton::clicked, this, [this]() { ExportWaveform(); });
        connect(fPrevious, &QPushButton::clicked, this, [this]() { StepFrame(false); });
        connect(fNext, &QPushButton::clicked, this, [this]() { StepFrame(true); });
        connect(&fPlaybackTimer, &QTimer::timeout, this, [this]() { StepFrame(true); });
        connect(fScanPeriod, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                this, [this](int period) { fPlaybackTimer.setInterval(period); });
        connect(fAutoScan, &QCheckBox::toggled, this, [this](bool enabled) {
            if (enabled && fHasFile) {
                fPlaybackTimer.start(fScanPeriod->value());
                statusBar()->showMessage(tr("Auto scan every %1 ms; wraps at the last frame").arg(fScanPeriod->value()));
            } else {
                fPlaybackTimer.stop();
                if (enabled) fAutoScan->setChecked(false);
            }
        });
        connect(fFrame, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                this, [this]() { Refresh(); });
        connect(fAget, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                this, [this]() { Refresh(); });
        statusBar()->showMessage(tr("Offline viewer; does not connect to DAQ or receive data"));
    }

}

QMainWindow *CreateWaveformWindow(QWidget *parent)
{
    return new detail::WaveformWindow(parent);
}

int RunWaveformSurfaceSmoke(const QString &screenshot) {

        WaveformCanvas canvas;
        canvas.resize(820, 560);
        QVector<GetDAQ::WaveformPlotTrace> traces;
        for (int channel = 0; channel < (screenshot.isEmpty()?68:8); ++channel) {
            GetDAQ::WaveformPlotTrace trace;
            trace.channel = channel;
            trace.samples.reserve(512);
            for (int bucket = 0; bucket < 512; ++bucket) {
                const double displacement = bucket - (180.0 + channel * 0.6);
                const double pulse = 900.0 * std::exp(-(displacement * displacement) / (48.0 * 48.0));
                trace.samples.append(QPointF(bucket, 200.0 + pulse));
            }
            traces.append(trace);
        }
        canvas.SetTraces(traces, 0);
        canvas.SetSurface(true);
        canvas.show();
        QApplication::processEvents();
        QImage before(canvas.size(), QImage::Format_ARGB32_Premultiplied);
        before.fill(Qt::transparent);
        {
            QPainter painter(&before);
            canvas.render(&painter);
        }
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(400, 250), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &press);
        QMouseEvent move(QEvent::MouseMove, QPointF(470, 285), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &move);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(470, 285), Qt::LeftButton,
                            Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &release);
        QApplication::processEvents();
        QImage after(canvas.size(), QImage::Format_ARGB32_Premultiplied);
        after.fill(Qt::transparent);
        {
            QPainter painter(&after);
            canvas.render(&painter);
        }
        if(!screenshot.isEmpty())before.save(screenshot);
        int changedPixels = 0;
        for (int y = 0; y < before.height(); ++y)
            for (int x = 0; x < before.width(); ++x)
                if (before.pixel(x, y) != after.pixel(x, y)) ++changedPixels;
        if (changedPixels <= 100) {
            std::cerr << "FAIL waveform surface render/rotation; changed_pixels=" << changedPixels << '\n';
            return 1;
        }
        std::cout << "PASS waveform surface render and drag rotation; changed_pixels=" << changedPixels << '\n';
        return 0;
    
}

}
