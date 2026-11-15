#ifndef GETDAQ_WAVEFORM_WINDOW_H
#define GETDAQ_WAVEFORM_WINDOW_H
class QWidget;
class QMainWindow;
namespace GetDAQ {
// Shared offline window for the console and independent executable.
// The caller owns the returned window; no DAQ/control connection is created.
QMainWindow *CreateWaveformWindow(QWidget *parent=nullptr);
int RunWaveformSurfaceSmoke();
}
#endif
