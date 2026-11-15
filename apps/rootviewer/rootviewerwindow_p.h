#ifndef GETDAQ_ROOTVIEWERWINDOW_P_H
#define GETDAQ_ROOTVIEWERWINDOW_P_H

#include "roottreereader.h"
#include <QMainWindow>
#include <memory>

class EventCanvas;
class QComboBox;
class QPushButton;
class QSpinBox;

class RootViewerWindow : public QMainWindow {
public:
    RootViewerWindow();

private:
    void OpenFile();
    void ReadEvent();
    void DisplayChanged();
    void SetLegacyStatisticsEnabled(bool enabled);
    void DisplayCurrentEvent();
    void BuildStatistics(int mode);

    EventCanvas *fCanvas;
    QSpinBox *fFrame;
    QPushButton *fOpen;
    QPushButton *fPrevious;
    QPushButton *fNext;
    QComboBox *fDisplay;
    std::unique_ptr<GetDAQ::RootTreeReader> fReader;
    GetDAQ::RootEvent fEvent;
    bool fHasEvent = false;
};

#endif
