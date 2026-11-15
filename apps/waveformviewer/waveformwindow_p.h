#ifndef GETDAQ_WAVEFORMVIEWER_WAVEFORMWINDOW_P_H
#define GETDAQ_WAVEFORMVIEWER_WAVEFORMWINDOW_P_H

#include <QMainWindow>
#include <QString>
#include <QTimer>
#include <QVector>

#include "grawreader.h"
#include "waveformexport.h"

class QCheckBox;
class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace GetDAQ {
class WaveformCanvas;

namespace detail {
class GrawIndexTask;

class WaveformWindow : public QMainWindow {
public:
    explicit WaveformWindow(QWidget *parent = nullptr);
    ~WaveformWindow() override;
    void LoadFile(const QString &path);

private:
    void UpdateChannelButton();
    void SelectChannels();
    void JumpToEvent();
    void UpdateEventDisplay();
    void CompleteIndex();
    void OpenFile();
    void SetFileAvailable(bool available);
    void StepFrame(bool forward);
    void Refresh();
    void ExportWaveform();
    void ShowMetrics(const QVector<GrawSample> &samples);

    QString fFileName;
    WaveformCanvas *fCanvas;
    QSpinBox *fFrame;
    QSpinBox *fAget;
    QLineEdit *fEvent;
    QPushButton *fFindEvent;
    QComboBox *fEventFrames;
    QPushButton *fChannelsButton;
    QVector<int> fChannels;
    QPushButton *fOpen;
    QPushButton *fPrevious;
    QPushButton *fNext;
    QCheckBox *fAutoScan;
    QSpinBox *fScanPeriod;
    QTimer fPlaybackTimer;
    bool fHasFile;
    QCheckBox *fSurfaceToggle;
    QPushButton *fExport;
    QLabel *fMetrics;
    GrawIndexTask *fIndexTask;
    QString fPendingRawFile;
    QVector<GrawFrameRecord> fRawFrames;
    bool fRawFile;
    WaveformExportMetadata fExportMetadata;
};
}
}

#endif
