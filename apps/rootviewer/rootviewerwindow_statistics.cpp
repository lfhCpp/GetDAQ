#include "rootviewerwindow_p.h"
#include "rootstatistics.h"
#include "rootcanvas.h"

#include <QComboBox>
#include <QEventLoop>
#include <QProgressDialog>
#include <QStatusBar>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <string>

namespace {
class StatisticsWorker : public QThread {
public:
    StatisticsWorker(const std::string &path, GetDAQ::RootStatistics::Measure measure)
        : fPath(path), fMeasure(measure), fCancelled(false), fProgressPermille(0), fSucceeded(false)
    {
    }

    void Cancel() { fCancelled.store(true); }
    int ProgressPermille() const { return fProgressPermille.load(); }
    bool WasCancelled() const { return fCancelled.load(); }
    bool Succeeded() const { return fSucceeded; }
    const GetDAQ::RootStatistics::Histogram &Result() const { return fResult; }
    const std::string &Error() const { return fError; }

protected:
    void run() override
    {
        fSucceeded = GetDAQ::RootStatistics::ScanFile(fPath, fMeasure, &fCancelled,
                                                       &fProgressPermille, &fResult, &fError);
    }

private:
    std::string fPath;
    GetDAQ::RootStatistics::Measure fMeasure;
    std::atomic<bool> fCancelled;
    std::atomic<int> fProgressPermille;
    bool fSucceeded;
    GetDAQ::RootStatistics::Histogram fResult;
    std::string fError;
};

} // namespace

void RootViewerWindow::BuildStatistics(int mode)
{
    if (!fReader) return;
    const GetDAQ::RootStatistics::Measure measure = mode == 3 ? GetDAQ::RootStatistics::Measure::EventTime :
        mode == 4 ? GetDAQ::RootStatistics::Measure::EventFrequency :
        mode == 5 ? GetDAQ::RootStatistics::Measure::EventPeriod :
        mode == 6 ? GetDAQ::RootStatistics::Measure::Bucket :
                    GetDAQ::RootStatistics::Measure::ChannelCount;
    StatisticsWorker worker(fReader->Path(), measure);
    QProgressDialog progress(tr("Scanning ROOT events…"), tr("Cancel"), 0, 1000, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(300);
    QEventLoop eventLoop;
    QTimer progressTimer;
    connect(&worker, &QThread::finished, &eventLoop, &QEventLoop::quit);
    connect(&progress, &QProgressDialog::canceled, &worker, [&worker]() { worker.Cancel(); });
    connect(&progressTimer, &QTimer::timeout, &progress,
            [&worker, &progress]() { progress.setValue(worker.ProgressPermille()); });
    worker.start();
    progressTimer.start(50);
    eventLoop.exec();
    progressTimer.stop();
    worker.wait();
    progress.setValue(worker.ProgressPermille());

    if (!worker.Succeeded()) {
        if (worker.WasCancelled()) statusBar()->showMessage(tr("Statistics scan cancelled"));
        else statusBar()->showMessage(tr("Cannot scan ROOT events: %1")
                                      .arg(QString::fromLocal8Bit(worker.Error().c_str())));
        return;
    }
    const GetDAQ::RootStatistics::Histogram &result = worker.Result();
    RootHistogram histogram;
    histogram.bins = result.bins;
    histogram.minimum = result.minimum;
    histogram.maximum = result.maximum;
    histogram.title = fDisplay->itemText(mode);
    histogram.xTitle = mode == 3 ? tr("Event time [s]") :
                       mode == 4 ? tr("Event frequency [Hz]") :
                       mode == 5 ? tr("Delay between events [ms]") :
                       mode == 6 ? tr("Bucket index") : tr("Channels per event");
    fCanvas->SetHistogram(histogram);
    statusBar()->showMessage(tr("Scanned %1 entries; counted %2 values")
                             .arg(fReader->Entries()).arg(result.values));
}
