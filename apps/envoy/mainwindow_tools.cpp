#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "waveformwindow.h"
#include "offlinedatatoolswindow.h"
#include "diagnostics.h"
#include <QAction>

bool MainWindow::RequireOfflineToolsClosed() {
    if(!fWaveformViewer && !fDataTools)return true;
    ShowLogEntry(tr("Close the waveform viewer and data tools windows before starting a new Run."),Diagnostics::Warning);
    QWidget *window=fDataTools?static_cast<QWidget *>(fDataTools.data()):fWaveformViewer.data();
    window->show();window->raise();window->activateWindow();return false;
}
void MainWindow::OpenWaveformViewer() {
    if(fActive || fBusy || fRemoteRunning || fAutomaticArmed || fClosing)return;
    if(fWaveformViewer){fWaveformViewer->show();fWaveformViewer->raise();fWaveformViewer->activateWindow();return;}
    auto *window=GetDAQ::CreateWaveformWindow(this);fWaveformViewer=window;emit OfflineToolsStateSignal(true);
    window->setAttribute(Qt::WA_DeleteOnClose);
    connect(window,&QObject::destroyed,this,[this]{
        fWaveformViewer=nullptr;emit OfflineToolsStateSignal(bool(fDataTools));
        if(!fClosing)ShowControlSlot(ui->Phase->text(),fConnected,fBusy,fActive);
    });
    window->show();RefreshOfflineToolsGate();
}
void MainWindow::OpenDataTools() {
    if(fActive || fBusy || fRemoteRunning || fAutomaticArmed || fClosing || fDataToolsClosing || !fDataToolsAvailable)return;
    if(fDataTools){fDataTools->show();fDataTools->raise();fDataTools->activateWindow();return;}
    auto *window=new OfflineDataToolsWindow(this);fDataTools=window;fDataToolsClosing=false;emit OfflineToolsStateSignal(true);
    window->setAttribute(Qt::WA_DeleteOnClose);
    connect(window,&OfflineDataToolsWindow::ClosePendingSignal,this,[this]{
        fDataToolsClosing=true;
        ShowLogEntry(tr("Data file tools are stopping. Run start will be available when the tool process exits."),Diagnostics::Warning);
        RefreshOfflineToolsGate();
    });
    connect(window,&QObject::destroyed,this,[this]{
        fDataTools=nullptr;fDataToolsClosing=false;emit OfflineToolsStateSignal(bool(fWaveformViewer));
        if(!fClosing){ShowControlSlot(ui->Phase->text(),fConnected,fBusy,fActive);RefreshOfflineToolsGate();}
    });
    window->show();RefreshOfflineToolsGate();
}
void MainWindow::RefreshOfflineToolsGate() {
    if(!fWaveformViewerAction)return;
    const bool running=fActive || fRemoteRunning;
    fWaveformViewerAction->setEnabled(!fClosing);
    fDataToolsAction->setEnabled(!fClosing && !fDataToolsClosing && fDataToolsAvailable);
    fDataToolsAction->setToolTip(fDataToolsClosing?
        tr("Stopping the offline tool process; this menu item will be available after it exits."):
        tr("Inspect, merge and convert completed files; optional FITS / ROOT tools require installation."));
    ui->StartRun->setToolTip(tr("Start remains available; warnings require confirmation."));
}
