#include <QCheckBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSignalBlocker>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <utility>

#include "waveformcanvas.h"
#include "waveformwindow_p.h"

namespace GetDAQ {
namespace detail {
namespace {
const qint64 kSamplesPerMatrix = 4LL * 68LL * 512LL;
const qint64 kBytesPerMatrix = kSamplesPerMatrix * 8LL;
}

class GrawIndexTask : public QThread {
public:
    explicit GrawIndexTask(const QString &path) : fPath(path), fCancelled(false) {}
    GrawIndexResult TakeResult() { return std::move(fResult); }
    void Cancel() { fCancelled.store(true); }

protected:
    void run() override { fResult = GrawFrameReader::BuildIndex(fPath, &fCancelled); }

private:
    QString fPath;
    GrawIndexResult fResult;
    std::atomic<bool> fCancelled;
};

WaveformWindow::~WaveformWindow()
    {
        if (fIndexTask) {
            fIndexTask->Cancel();
            fIndexTask->wait();
            delete fIndexTask;
        }
    }

void WaveformWindow::CompleteIndex()
    {
        if (!fIndexTask) return;
        const GetDAQ::GrawIndexResult result = fIndexTask->TakeResult();
        fIndexTask->deleteLater();
        fIndexTask = 0;
        fOpen->setEnabled(true);
        if (!result.error.isEmpty()) {
            statusBar()->showMessage(tr("Cannot index .graw file: %1").arg(result.error));
            return;
        }
        if (result.frames.isEmpty()) {
            statusBar()->showMessage(tr("No CoBo frames were found in the selected .graw file"));
            return;
        }
        fFileName = fPendingRawFile;
        fRawFile = true;
        fRawFrames = result.frames;
        fEventFrames->clear();
        SetFileAvailable(true);
        fFrame->setRange(0, int(fRawFrames.size() - 1));
        fFrame->setValue(0);
        statusBar()->showMessage(tr("Indexed %1 CoBo frames in %2 MiB; reads one frame when selected")
                                 .arg(fRawFrames.size()).arg(result.inputBytes / (1024 * 1024)));
        Refresh();
    }

void WaveformWindow::OpenFile()
    {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open waveform file"), QString(),
                                                           tr("CoBo data (*.graw *.gmat);;Raw data (*.graw);;Waveform matrix (*.gmat);;All files (*)"));
        LoadFile(path);
    }

void WaveformWindow::LoadFile(const QString &path)
    {
        if (path.isEmpty() || fIndexTask) return;
        fPlaybackTimer.stop();
        fAutoScan->setChecked(false);
        fFileName.clear();
        SetFileAvailable(false);
        fCanvas->Clear();
        fExport->setEnabled(false);
        fMetrics->setText(tr("Pulse metrics: not calculated"));
        if (path.endsWith(QStringLiteral(".graw"), Qt::CaseInsensitive)) {
            fPendingRawFile = path;
            fOpen->setEnabled(false);
            statusBar()->showMessage(tr("Indexing .graw frames in a background worker; the input file is read-only"));
            fIndexTask = new GrawIndexTask(path);
            fIndexTask->setParent(this);
            connect(fIndexTask, &QThread::finished, this, [this]() { CompleteIndex(); });
            fIndexTask->start();
            return;
        }
        QFile input(path);
        if (!input.open(QIODevice::ReadOnly)) {
            statusBar()->showMessage(tr("Cannot open file: %1").arg(input.errorString()));
            return;
        }
        const qint64 bytes = input.size();
        if (bytes <= 0 || bytes % kBytesPerMatrix != 0) {
            statusBar()->showMessage(tr("Invalid .gmat size: expected complete 4 x 68 x 512 binary64 frames"));
            return;
        }
        fFileName = path;
        fRawFile = false;
        fRawFrames.clear();
        fEventFrames->clear();
        SetFileAvailable(true);
        fFrame->setRange(0, int(qMin<qint64>(bytes / kBytesPerMatrix - 1, 2147483647LL)));
        fFrame->setValue(0);
        statusBar()->showMessage(tr("%1 frames; reads only the selected channel samples").arg(bytes / kBytesPerMatrix));
        Refresh();
    }

void WaveformWindow::SetFileAvailable(bool available)
    {
        fHasFile = available;
        fEvent->setEnabled(available && fRawFile);fFindEvent->setEnabled(available && fRawFile);fEventFrames->setEnabled(available && fRawFile);
        if(!available || !fRawFile){fEvent->clear();fEvent->setPlaceholderText(fRawFile ? tr("Event ID") : tr("No event IDs in .gmat"));}
        fFrame->setEnabled(available);
        fPrevious->setEnabled(available);
        fNext->setEnabled(available);
        fAutoScan->setEnabled(available);
    }

}
}
