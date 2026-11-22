#include "mainwindow.h"
#include <QMenu>
#include <QAction>
void MainWindow::InitializeRecoveryAndSafetyMenus(QMenu *,QMenu *runMenu,QMenu *recoveryMenu){
 auto *status=recoveryMenu->addAction(tr("Status and warnings…"));status->setObjectName("StatusWarningsReset");connect(status,&QAction::triggered,this,&MainWindow::OpenStatusManagement);
 fPause=runMenu->addAction(tr("Pause run"));fPause->setObjectName("PauseRun");fResume=runMenu->addAction(tr("Resume run"));fResume->setObjectName("ResumeRun");
 connect(fPause,&QAction::triggered,this,&MainWindow::PauseSignal);connect(fResume,&QAction::triggered,this,&MainWindow::ResumeSignal);
}
