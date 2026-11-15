#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "coldhandoffwindow.h"
#include "failedhandoffwindow.h"
#include "configurationreadbackwindow.h"
#include <QAction>
#include <QDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>
#include <QVBoxLayout>

void MainWindow::InitializeRecoveryAndSafetyMenus(QMenu *configurationMenu,QMenu *runMenu,QMenu *recoveryMenu)
{
    auto *statusManagement=recoveryMenu->addAction(tr("Status, warnings and reset…"));statusManagement->setObjectName("StatusWarningsReset");
    connect(statusManagement,&QAction::triggered,this,&MainWindow::OpenStatusManagement);
    // The legacy ECC lifecycle is global. Native selected-board work is an
    // explicit, Prepared-only maintenance operation instead of a fake state transition.
    auto coldHandoff=recoveryMenu->addAction(tr("Cold data handoff recovery..."));coldHandoff->setObjectName("NativeColdHandoff");
    connect(coldHandoff,&QAction::triggered,this,[this] {
        auto window=new ColdHandoffWindow(this);window->setAttribute(Qt::WA_DeleteOnClose);window->show();
    });
    auto failedHandoff=recoveryMenu->addAction(tr("Failed data-node handoff completion..."));failedHandoff->setObjectName("NativeFailedHandoffCompletion");
    connect(failedHandoff,&QAction::triggered,this,[this] {
        if(!fFailedHandoff){fFailedHandoff=new FailedHandoffWindow(this);fFailedHandoff->setObjectName("NativeFailedHandoffCompletionWindow");
            connect(fFailedHandoff,&QObject::destroyed,this,[this]{fFailedHandoff=nullptr;});}
        fFailedHandoff->show();fFailedHandoff->raise();fFailedHandoff->activateWindow();
    });
    fConfigurationInfo=configurationMenu->addAction(tr("Accepted ECC configuration..."));fConfigurationInfo->setObjectName("NativeConfigurationInfo");
    fConfigurationSummary=new QLabel(tr("ECC configuration: not observed"),this);fConfigurationSummary->setObjectName("NativeConfigurationSummary");
    statusBar()->addPermanentWidget(fConfigurationSummary,1);
    connect(fConfigurationInfo,&QAction::triggered,this,[this] {
        if (!fConfigurationView) {
            auto dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle(tr("Accepted ECC startup configuration"));dialog->resize(1050,600);
            auto layout=new QVBoxLayout(dialog);auto note=new QLabel(tr("This view identifies configuration accepted at ECC startup. Hardware loading remains unverified; this response does not recheck live file health."),dialog);
            note->setWordWrap(true);layout->addWidget(note);fConfigurationView=new QTableWidget(dialog);fConfigurationView->setObjectName("NativeConfigurationTable");
            fConfigurationView->setColumnCount(2);fConfigurationView->setHorizontalHeaderLabels({tr("Field"),tr("Accepted value")});fConfigurationView->setEditTriggers(QAbstractItemView::NoEditTriggers);
            fConfigurationView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);fConfigurationView->horizontalHeader()->setStretchLastSection(true);layout->addWidget(fConfigurationView);
            connect(dialog,&QObject::destroyed,this,[this] {fConfigurationView=nullptr;});dialog->show();
        } else fConfigurationView->window()->show();
        ShowConfigurationSlot(fConfigurationBody,fConfigurationError);emit InspectConfigurationSignal();
    });
    fPause=runMenu->addAction(tr("Pause run")); fResume=runMenu->addAction(tr("Resume run"));
    fFailureUnprepare=recoveryMenu->addAction(tr("Power off original failed preparation..."));fFailureUnprepare->setObjectName("NativeFailureUnprepare");fFailureUnprepare->setEnabled(false);
    connect(fFailureUnprepare,&QAction::triggered,this,[this] {
        if(QMessageBox::question(this,tr("Power off the original failed preparation"),
            tr("After an acknowledged original production stop, power off the declared AsAd boards and close only this preparation's known subscriptions?\n"
               "UNKNOWN stays held. Failed or uncertain stages are not repeated; this does not recover a Run or a restarted service."),
            QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes)emit UnprepareAfterFailureSignal();
    });
    fFailureBreakup=recoveryMenu->addAction(tr("Close original failed acquisition connections..."));
    fReceiverRetirement=recoveryMenu->addAction(tr("Retire original failed data sessions..."));
    fAcquisitionUnprepare=recoveryMenu->addAction(tr("Power off original failed acquisition..."));
    fAcquisitionRecovery=recoveryMenu->addAction(tr("Verify original failed acquisition after cleanup..."));
    fAcquisitionRecovery->setObjectName("NativeAcquisitionRecovery");fAcquisitionRecovery->setEnabled(false);
    connect(fAcquisitionRecovery,&QAction::triggered,this,[this] {
        if(QMessageBox::question(this,tr("Verify original failed acquisition"),
            tr("After original stop, connection cleanup, data retirement and power-off, verify the same hardware and audit evidence read-only?\n"
               "Success returns to Described and requires a fresh Prepare. The old raw and interrupted Run stay intact; no data is marked complete or restarted."),
            QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes)emit ReconcileAcquisitionSignal();
    });
    fAlarmAcquisitionRecovery=recoveryMenu->addAction(tr("Acknowledge original alarm and verify acquisition..."));
    fAlarmAcquisitionRecovery->setObjectName("NativeAlarmAcquisitionRecovery");fAlarmAcquisitionRecovery->setEnabled(false);
    connect(fAlarmAcquisitionRecovery,&QAction::triggered,this,[this] {
        if(QMessageBox::question(this,tr("Acknowledge the original stopped acquisition alarm"),
            tr("Review the original alarm stop ID in the log. After original connection cleanup, data retirement and power-off, acknowledge that alarm and verify the same acquisition read-only?\n"
               "The old Run stays interrupted. Success returns to Described; a fresh Prepare must monitor the hardware again. This does not certify that the physical alarm cause has been repaired."),
            QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes)emit ReconcileAcquisitionWithAlarmSignal();
    });
    fAcquisitionUnprepare->setObjectName("NativeAcquisitionUnprepare");fAcquisitionUnprepare->setEnabled(false);
    connect(fAcquisitionUnprepare,&QAction::triggered,this,[this] {
        if(QMessageBox::question(this,tr("Power off original failed acquisition"),
            tr("After confirmed original data retirement, monitor and power off the same AsAd, then close only owned subscriptions?\n"
               "Old raw and interrupted Run stay intact. This does not clear UNKNOWN or start a new acquisition."),
            QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes)emit UnprepareAcquisitionAfterFailureSignal();
    });
    fReceiverRetirement->setObjectName("NativeReceiverRetirement");fReceiverRetirement->setEnabled(false);
    connect(fReceiverRetirement,&QAction::triggered,this,[this] {
        if(QMessageBox::question(this,tr("Retire original failed data sessions"),
            tr("After confirmed original connection cleanup and receiver Abort, retire the same failed data sessions?\n"
               "Old raw and failed history stay intact. This does not seal data, start receivers or complete hardware/Run recovery."),
            QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes)emit RetireFailedReceiversSignal();
    });
    fFailureBreakup->setObjectName("NativeFailureBreakup");fFailureBreakup->setEnabled(false);
    connect(fFailureBreakup,&QAction::triggered,this,[this] {
        if(QMessageBox::question(this,tr("Close original failed acquisition connections"),
            tr("After confirmed original production stop, stop and close the same known sender connections and withdraw ready configuration?\n"
               "Unknown steps are not repeated. UNKNOWN and incomplete Run stay held; receiver cleanup and data reconciliation remain separate."),
            QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes)emit BreakupAfterFailureSignal();
    });
    fDescriptionRecovery=recoveryMenu->addAction(tr("Verify original failed Describe..."));fDescriptionRecovery->setObjectName("NativeDescriptionRecovery");fDescriptionRecovery->setEnabled(false);
    connect(fDescriptionRecovery,&QAction::triggered,this,[this] {
        const auto answer=QMessageBox::question(this,tr("Verify the original failed description"),
            tr("Verify existing node descriptions and stopped/ready bits, then reconcile only the original failed Describe to Described?\n"
               "No CREATE or hardware writes are sent. Active Runs and cold service takeover are refused. Inspect the recorded outcome before preparing again."),
            QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel);
        if(answer==QMessageBox::Yes)emit ReconcileDescriptionSignal();
    });
    fPreparationRecovery=recoveryMenu->addAction(tr("Verify original failed Prepare after cleanup..."));
    fPreparationRecovery->setObjectName("NativePreparationRecovery");fPreparationRecovery->setEnabled(false);
    connect(fPreparationRecovery,&QAction::triggered,this,[this] {
        if(QMessageBox::question(this,tr("Verify original preparation cleanup"),
            tr("Verify the original failed Prepare, acknowledged production stop, completed power-off and owned subscription closure, then read current descriptions and stopped/power bits?\n"
               "Only complete bound proof can reconcile to Described. No hardware writes, automatic Prepare, Run recovery or cold takeover are performed."),
            QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes)emit ReconcilePreparationSignal();
    });
    fSourceRetirement=recoveryMenu->addAction(tr("Retire original failed source connections (keep UNKNOWN)..."));
    fSourceRetirement->setObjectName("NativeSourceRetirement");fSourceRetirement->setEnabled(false);
    connect(fSourceRetirement,&QAction::triggered,this,[this]{
        if(QMessageBox::question(this,tr("Retire original failed source connections"),
            tr("After the original producer stop is acknowledged, close only the original failed ICE data connections?\n"
               "No drain or End is sent. Original UNKNOWN, raw files and the interrupted Run remain; receiver cleanup and hardware verification are separate."),
            QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes)emit RetireFailedSourcesSignal();
    });
    fSourceRecovery=recoveryMenu->addAction(tr("Verify acquisition after original source retirement..."));
    fSourceRecovery->setObjectName("NativeSourceRecovery");fSourceRecovery->setEnabled(false);
    connect(fSourceRecovery,&QAction::triggered,this,[this]{
        if(QMessageBox::question(this,tr("Verify original retired source acquisition"),
            tr("After original source retirement, connection cleanup, receiver Abort/retirement and power-off, verify the original sources and hardware read-only?\n"
               "Success returns to Described. The old Run stays interrupted and requires a fresh Prepare; physical fault repair is not certified."),
            QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes)emit ReconcileAcquisitionWithSourcesSignal();
    });
    fStoppedAlarmRelease=recoveryMenu->addAction(tr("Acknowledge stopped alarm, close session and power off..."));
    fStoppedAlarmRelease->setObjectName("NativeStoppedAlarmRelease");fStoppedAlarmRelease->setEnabled(false);
    connect(fStoppedAlarmRelease,&QAction::triggered,this,[this] {
        if(QMessageBox::question(this,tr("Release the original known stopped alarm"),
            tr("Review the original alarm in the log. Close the known stopped data connection, wait for receiver drain/seal, power off and verify the same hardware?\n"
               "The old Run stays interrupted even if its received files seal successfully. A fresh Prepare is required; this does not certify the physical alarm cause has been repaired."),
            QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes)emit ReleaseStoppedAlarmSignal();
    });
    fAlarmPreparationRecovery=recoveryMenu->addAction(tr("Acknowledge original alarm and verify Prepare cleanup..."));
    fAlarmPreparationRecovery->setObjectName("NativeAlarmPreparationRecovery");fAlarmPreparationRecovery->setEnabled(false);
    connect(fAlarmPreparationRecovery,&QAction::triggered,this,[this] {
        if(QMessageBox::question(this,tr("Acknowledge the original failed Prepare alarm"),
            tr("Review the original alarm stop ID in the log. After acknowledged production stop and power-off, acknowledge that alarm and verify the original Prepare cleanup read-only?\n"
               "Success returns to Described and requires a fresh Prepare. This does not certify that the physical alarm cause has been repaired."),
            QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes)emit ReconcilePreparationWithAlarmSignal();
    });
    fProductionStop=recoveryMenu->addAction(tr("Native fault: stop production only (keep UNKNOWN)"));
    fPause->setObjectName("NativePause"); fResume->setObjectName("NativeResume"); fProductionStop->setObjectName("NativeProductionStop");
    connect(fPause,&QAction::triggered,this,&MainWindow::PauseSignal); connect(fResume,&QAction::triggered,this,&MainWindow::ResumeSignal);
    connect(fProductionStop,&QAction::triggered,this,&MainWindow::StopProductionOnlySignal);
    fReceiverAbort=recoveryMenu->addAction(tr("Native fault: terminate original failed receivers (keep UNKNOWN)..."));
    fReceiverAbort->setObjectName("NativeReceiverAbort");
    connect(fReceiverAbort,&QAction::triggered,this,[this] {
        if(QMessageBox::question(this,tr("Terminate original failed receiver session"),
            tr("The original ECC must have completed its explicit production stop. Terminate remaining receivers of that same failed session? Raw files are retained; UNKNOWN and the incomplete Run remain."),
            QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes) emit AbortFailedReceiversSignal();
    });
}
