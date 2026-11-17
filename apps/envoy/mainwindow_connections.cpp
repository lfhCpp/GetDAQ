#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "runcontrol.h"
#include "autorunwidget.h"
#include <QAction>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QThread>
#include <QTimer>

void MainWindow::InitializeRunControl()
{
    fRunControl->moveToThread(&fControlThread);
    connect(&fControlThread, &QThread::finished, fRunControl, &QObject::deleteLater);
    connect(this, &MainWindow::ConnectSignal, fRunControl, &RunControl::ConnectSlot);
    connect(this, &MainWindow::UpdateRunMetadataSignal, fRunControl, &RunControl::UpdateRunMetadataSlot);
    connect(this, &MainWindow::DisconnectSignal, fRunControl, &RunControl::DisconnectSlot);
    connect(this, &MainWindow::ProgressSignal, fRunControl, &RunControl::ProgressSlot);
    connect(this, &MainWindow::RegressSignal, fRunControl, &RunControl::RegressSlot);
    connect(this, &MainWindow::StartSignal, fRunControl, &RunControl::StartRunSlot);
    connect(fRunControl, &RunControl::IncomingFilesConfirmationSignal, this,
            [this](bool replaceCompletedRun, const QString &summary) {
        QMessageBox confirmation(QMessageBox::Warning, tr("Files already in receiver directory"),
            tr("The receiver directory contains unarchived files. This does not mean that the selected Run number already exists.\n\n%1\n\n"
               "Start acquisition while retaining these files? They will remain in place and may be archived together with this Run. "
               "No existing raw files will be deleted.").arg(summary), QMessageBox::Cancel, this);
        confirmation.setObjectName("IncomingFilesConfirmation");
        auto *start = confirmation.addButton(tr("Start and retain files"), QMessageBox::AcceptRole);
        confirmation.setDefaultButton(QMessageBox::Cancel);
        confirmation.exec();
        if (confirmation.clickedButton() == start) emit StartSignal(replaceCompletedRun, true);
    });
    connect(this,&MainWindow::OfflineToolsStateSignal,fRunControl,&RunControl::SetOfflineToolsOpenSlot);
    connect(this,&MainWindow::StopSignal,fRunControl,&RunControl::StopRunSlot);
    connect(this,&MainWindow::FinalizeSignal,fRunControl,&RunControl::FinalizeSlot);
    connect(this,&MainWindow::RecoverSignal,fRunControl,&RunControl::RecoverSlot);
    connect(this,&MainWindow::ShutdownSignal,fRunControl,&RunControl::ShutdownSlot);
    connect(this,&MainWindow::PauseSignal,fRunControl,&RunControl::PauseSlot);
    connect(this,&MainWindow::ResumeSignal,fRunControl,&RunControl::ResumeSlot);
    connect(fRunControl,&RunControl::ConfigurationSnapshotSignal,this,[this](const QString &path){fConfig.configurationSnapshot=path;});
    connect(fRunControl, &RunControl::StatusSignal, this, &MainWindow::ShowStatusSlot);
    connect(fRunControl, &RunControl::ControlSignal, this, &MainWindow::ShowControlSlot);
    connect(fRunControl,&RunControl::LogEntrySignal,this,&MainWindow::ShowLogEntry);
    connect(this,&MainWindow::ConfigureAutoRunSignal,fRunControl,&RunControl::ConfigureAutoRunSlot);
    connect(this,&MainWindow::SetAutoRunEnabledSignal,fRunControl,&RunControl::SetAutoRunEnabledSlot);
    connect(fRunControl,&RunControl::AutoRunEnabledSignal,this,[this](bool enabled){
        fConfig.autoRun.enabled=enabled;
        for(auto *controls:findChildren<AutoRunWidget *>())controls->SetEnabledState(enabled);
    });
    connect(this,&MainWindow::CancelAutomaticRunSignal,fRunControl,&RunControl::CancelAutomaticRunSlot);
    connect(ui->RunNumber,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{emit CancelAutomaticRunSignal(tr("Automatic Run canceled: Run number edited manually."));});
    connect(fRunControl,&RunControl::AutoRunStatusSignal,this,[this](const QString &text,bool armed){
        const bool changed=fAutomaticArmed!=armed;fAutomaticArmed=armed;
        for(auto *controls:findChildren<AutoRunWidget *>()) {controls->SetStatus(text,armed);controls->SetLocked(fActive || armed);}
        ui->AutoRunSettingsButton->setToolTip(text);
        if(changed)ShowControlSlot(ui->Phase->text(),fConnected,fBusy,fActive);
    });
    connect(fRunControl, &RunControl::RunFinishedSignal, this, [this](int next) { fConfig.runNumber = next; const QSignalBlocker blocked(ui->RunNumber);ui->RunNumber->setValue(next); });
    connect(fRunControl, &RunControl::ShutdownFinishedSignal, &fControlThread, &QThread::quit, Qt::DirectConnection);
    connect(fRunControl, &RunControl::ShutdownFinishedSignal, this, [this]() {
        fControlThread.quit(); fControlThread.wait(); fCanClose = true; close();
    });
    fControlThread.start();
}
