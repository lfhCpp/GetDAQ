#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "localdeploymentservices.h"
#include "applicationinstance.h"
#include <QSettings>
#include <QCoreApplication>
#include <QMessageBox>
#include <QTimer>
void MainWindow::CreateLocalServiceController() {
    if(fLocalServices)fLocalServices->deleteLater();
    fLocalServices=new LocalDeploymentServices(fConfig,QCoreApplication::applicationDirPath(),this);
    connect(fLocalServices,&LocalDeploymentServices::Log,this,&MainWindow::ShowLogSlot);
    connect(fLocalServices,&LocalDeploymentServices::Ready,this,[this]{
        fServiceStarting=false;ShowLogSlot(tr("Background services ready; connecting ECC."));emit ConnectSignal(fConfig);
    });
    connect(fLocalServices,&LocalDeploymentServices::Failed,this,[this](const QString &error){
        const bool starting=fServiceStarting;fServiceStarting=false;
        if(fExitManaged)CancelManagedExit(error);else ShowLogSlot(error);
        if(starting)ShowControlSlot(tr("Local service check failed"),false,false,false);
    });
    connect(fLocalServices,&LocalDeploymentServices::Stopped,this,[this]{
        ShowLogSlot(tr("Local backend services exited."));
        if(fExitManaged){fExitManaged=false;fServiceExitTimer->stop();fClosing=true;setEnabled(false);emit ShutdownSignal();}
    });
    connect(fLocalServices,&LocalDeploymentServices::ForceRequired,this,[this](const QString &processes){
        if(QMessageBox::warning(this,tr("Services still running"),tr("These processes did not exit:\n%1\n\nForce terminate them? Unfinished state and audit records will be retained.").arg(processes),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes)
            fLocalServices->ForceRemaining();else fLocalServices->CancelForce();
    });
}
void MainWindow::CancelManagedExit(const QString &reason) {
    fExitManaged=fExitAfterRun=fExitBreakup=false;
    if(fServiceExitTimer)fServiceExitTimer->stop();
    ShowLogSlot(tr("Backend shutdown stopped: %1").arg(reason));
    QMessageBox::warning(this,tr("Backend retained"),reason);
}
void MainWindow::ContinueManagedExit() {
    if(!fExitManaged)return;
    if(fExitClock.elapsed()>qMax(120000,fConfig.stepTimeoutMs*3)){CancelManagedExit(tr("Shutdown timed out. Background services and records were retained; inspect the current operation."));return;}
    const auto phase=ui->Phase->text();
    if(!fBusy && (fExitAfterRun || fExitBreakup) && (phase.contains("failed",Qt::CaseInsensitive) || phase.contains("error",Qt::CaseInsensitive) || phase.contains("inspection",Qt::CaseInsensitive))) {
        CancelManagedExit(tr("Acquisition could not be finalized: %1").arg(phase));return;
    }
    if(fExitAfterRun || fBusy)return;
    if(fConnected && fObservedEccState==4) {
        if(!fExitBreakup){fExitBreakup=true;ShowLogSlot(tr("Releasing configured sender/receiver session before service shutdown."));emit RegressSignal(-1);}
        return;
    }
    if(fExitBreakup && (fObservedEccState<1 || fObservedEccState>3))return;
    fExitBreakup=false;fServiceExitTimer->stop();
    CreateLocalServiceController();fLocalServices->StopSafely();
}

void MainWindow::RememberStartupConfiguration() {
    if(!fConfig.filePath.isEmpty())QSettings().setValue(StartupConfigurationKey(),fConfig.filePath);
}
