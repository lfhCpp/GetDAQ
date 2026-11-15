#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "eccsystemclient.h"
#include "eccprotocol.h"
#include "earlywarningrecovery.h"
#include "hardwaresetupwindow.h"
#include "localdeploymentservices.h"
#include "applicationinstance.h"
#include "diagnostics.h"
#include <QDialog>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QTimer>
#include <QCoreApplication>
#include <QProcess>
#include <QSettings>
#include <QCryptographicHash>
#include <QColor>
#include <QFileInfo>
#include <memory>
void MainWindow::OpenStatusManagement() {
    QDialog dialog(this);dialog.setWindowTitle(tr("Status / warnings / Idle reset"));dialog.resize(1050,620);
    auto *layout=new QVBoxLayout(&dialog);auto *state=new QLabel(tr("Checking the current ECC state…"),&dialog);state->setWordWrap(true);layout->addWidget(state);
    auto *explanation=new QLabel(tr("Idle is the initial state of a fresh, stopped control session. Closing a window leaves the backend and hardware state intact. Accepting a notification does not prove a hardware action completed."),&dialog);explanation->setWordWrap(true);layout->addWidget(explanation);
    auto *table=new QTableWidget(0,3,&dialog);table->setHorizontalHeaderLabels({tr("Type"),tr("Notification"),tr("Disposition")});table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);table->setColumnWidth(0,170);table->setColumnWidth(2,170);table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setEditTriggers(QAbstractItemView::NoEditTriggers);layout->addWidget(table,1);
    auto *notifications=new QHBoxLayout;auto *ack=new QPushButton(tr("Acknowledge selected")),*acceptWarning=new QPushButton(tr("Accept selected warning")),*clear=new QPushButton(tr("Clear acknowledged notifications")),*clearView=new QPushButton(tr("Clear displayed log"));
    for(auto *b:{ack,acceptWarning,clear,clearView})notifications->addWidget(b);
    layout->addLayout(notifications);
    auto *actions=new QHBoxLayout;auto *refresh=new QPushButton(tr("Recheck state")),*reset=new QPushButton(tr("Reset to Idle")),*recover=new QPushButton(tr("Recover Idle / accept compatibility warning…")),*close=new QPushButton(tr("Close"));
    recover->setEnabled(false);reset->setEnabled(false);for(auto *b:{refresh,reset,recover,close})actions->addWidget(b);layout->addLayout(actions);
    const QString key="accepted-notifications/"+QString::fromLatin1(QCryptographicHash::hash(fConfig.filePath.toUtf8(),QCryptographicHash::Sha256).toHex());
    auto render=[this,table,key]{
        const auto accepted=QSettings().value(key).toStringList();table->setRowCount(fNotifications.size());
        for(int row=0;row<fNotifications.size();++row) {
            auto &n=fNotifications[row];const auto fingerprint=QString::fromLatin1(QCryptographicHash::hash(n.message.toUtf8(),QCryptographicHash::Sha256).toHex());
            table->setItem(row,0,new QTableWidgetItem(n.severity==Diagnostics::Warning?tr("Warning"):tr("Needs verification")));
            table->item(row,0)->setForeground(n.severity==Diagnostics::Warning?QColor("#b86b00"):QColor("#c74349"));
            table->setItem(row,1,new QTableWidgetItem(n.message));table->item(row,1)->setToolTip(n.message);
            table->setItem(row,2,new QTableWidgetItem(accepted.contains(fingerprint)?tr("Warning accepted"):n.acknowledged?tr("Acknowledged"):tr("Unacknowledged")));
        }
    };render();
    connect(ack,&QPushButton::clicked,&dialog,[this,table,render]{const int row=table->currentRow();if(row<0 || row>=fNotifications.size())return;fNotifications[row].acknowledged=true;ShowLogEntry(tr("Operator acknowledged notification: %1").arg(fNotifications[row].message),Diagnostics::Information);render();});
    connect(acceptWarning,&QPushButton::clicked,&dialog,[this,table,render,key,&dialog]{
        const int row=table->currentRow();if(row<0 || row>=fNotifications.size())return;auto &n=fNotifications[row];
        if(n.severity!=Diagnostics::Warning){QMessageBox::information(&dialog,tr("Result needs verification"),tr("This is a failed or uncertain action. Use Recheck / the relevant recovery action. Acknowledging the message alone cannot determine whether the action executed."));return;}
        n.acknowledged=true;auto accepted=QSettings().value(key).toStringList();const auto fingerprint=QString::fromLatin1(QCryptographicHash::hash(n.message.toUtf8(),QCryptographicHash::Sha256).toHex());if(!accepted.contains(fingerprint))accepted.append(fingerprint);while(accepted.size()>200)accepted.removeFirst();QSettings().setValue(key,accepted);ShowLogEntry(tr("Operator accepted warning: %1").arg(n.message),Diagnostics::Information);render();
    });
    connect(clear,&QPushButton::clicked,&dialog,[this,render]{for(int i=fNotifications.size()-1;i>=0;--i)if(fNotifications[i].acknowledged)fNotifications.remove(i);render();});
    connect(clearView,&QPushButton::clicked,&dialog,[this]{ui->Log->clear();ShowLogEntry(tr("Displayed log cleared by operator; persistent logs and controller state retained."),Diagnostics::Information);});
    connect(close,&QPushButton::clicked,&dialog,&QDialog::reject);
    EccSystemClient *client=nullptr;
    if(fConfig.nativeEcc)try {client=new EccSystemClient(fConfig.nativeClient,{},&dialog);}catch(const std::exception &e){state->setText(QString::fromUtf8(e.what()));}
    auto current=std::make_shared<GetDAQ::EccControlSystemStatus>();auto haveStatus=std::make_shared<bool>(false);
    if(client) {
    connect(client,&EccSystemClient::IdentitySignal,&dialog,[client,state](bool matched,QByteArray,QString error){if(matched)client->QueryStatus();else state->setText(tr("ECC connection/identity unavailable: %1").arg(error));});
    connect(client,&EccSystemClient::StatusSignal,&dialog,[this,state,reset,recover,current,haveStatus,render](QByteArray body,QString error){
        *haveStatus=false;reset->setEnabled(false);recover->setEnabled(false);
        if(!error.isEmpty()){state->setText(error);return;}
        try {
            *current=GetDAQ::DecodeEccControlSystemStatus(body.toStdString());*haveStatus=true;const auto &s=*current;
            const bool held=s.controlNeedsReconciliation || s.controller.outcomeUnknown || s.controller.needsReconciliation || s.dataNeedsReconciliation;
            state->setText(tr("ECC: %1%2\nLast operation: %3\n%4").arg(StateName(s.controller.state),held?tr(" — result pending verification (UNKNOWN)"):QString(),QString::fromStdString(s.controller.operationId),QString::fromStdString(s.controller.message)));
            reset->setEnabled(!held && !s.operationActive && s.controller.state>=1 && s.controller.state<=4 && fConnected && !fBusy && !fActive && !fRemoteRunning);
            if(LocalDeploymentServices::IsLocalDeployment(fConfig) && !s.operationActive && !fBusy && !fActive && !fRemoteRunning)try {
                ValidateEarlyDelayWarningEvidence(QFileInfo(fConfig.filePath).absolutePath(),body);recover->setEnabled(true);recover->setToolTip(tr("An empty Idle baseline or a specific early legacy readback warning was identified. Fresh stopped/power-off and data checks are required before restoring an Idle session."));
            }catch(const std::exception &e){recover->setToolTip(QString::fromUtf8(e.what()));}
            render();
        }catch(const std::exception &e){state->setText(QString::fromUtf8(e.what()));}
    });
    }
    connect(refresh,&QPushButton::clicked,&dialog,[this,client,state,haveStatus]{*haveStatus=false;if(fConfig.nativeEcc && client)client->QueryIdentity();else state->setText(tr("Legacy / simulation phase: %1").arg(ui->Phase->text()));});
    connect(reset,&QPushButton::clicked,&dialog,[this,&dialog,current,haveStatus,state,reset]{
        if(!*haveStatus || fBusy || fActive || !fConnected)return;
        const auto &s=*current;if(s.operationActive || s.controlNeedsReconciliation || s.controller.outcomeUnknown || s.controller.needsReconciliation || s.dataNeedsReconciliation)return;
        if(QMessageBox::question(&dialog,tr("Reset known stopped state to Idle"),tr("Release the configured connection if present, unprepare/power off as required, then return the known stopped ECC to Idle? This is an explicit reset sequence, not a log clear."),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
        auto *timer=new QTimer(&dialog);timer->setInterval(200);auto expected=std::make_shared<int>(0);auto clock=std::make_shared<QElapsedTimer>();clock->start();reset->setEnabled(false);
        connect(timer,&QTimer::timeout,&dialog,[this,timer,expected,clock,state]{
            if(!fConnected || fActive || clock->elapsed()>qMax(120000,3*fConfig.stepTimeoutMs)){timer->stop();state->setText(tr("Idle reset stopped or timed out; inspect the current operation."));return;}
            if(fBusy)return;
            if(*expected && fObservedEccState!=*expected){if(fObservedEccState<1){timer->stop();state->setText(tr("Idle reset stopped: fresh known state is unavailable."));}return;}
            if(fObservedEccState==1){timer->stop();state->setText(tr("ECC is Idle. No automatic Prepare or acquisition was started."));ShowLogEntry(tr("Operator reset completed: ECC Idle"),Diagnostics::Information);return;}
            if(fObservedEccState<2 || fObservedEccState>4){timer->stop();state->setText(tr("Idle reset requires a known stopped state. Recheck or recover first."));return;}
            *expected=fObservedEccState-1;emit RegressSignal(-1);
        });timer->start();
    });
    connect(recover,&QPushButton::clicked,&dialog,[this,&dialog]{
        if(fBusy || fActive || fRemoteRunning)return;
        HardwareSetupWindow workflow(fConfig,&dialog);QTimer::singleShot(0,&workflow,&HardwareSetupWindow::StartAcceptedWarningRecovery);
        if(workflow.exec()!=QDialog::Accepted)return;
        const auto next=workflow.Configuration();
        QSettings().setValue(StartupConfigurationKey(),next.filePath);
        if(!QProcess::startDetached(QCoreApplication::applicationFilePath(),{"--config",next.filePath})) {QMessageBox::warning(&dialog,tr("Idle session saved"),tr("New stopped session created at %1. Open its envoy.yml to continue.").arg(next.filePath));return;}
        ShowLogEntry(tr("Accepted warning recovery created new Idle session: %1; original failure audits retained.").arg(next.filePath),Diagnostics::Information);
        QCoreApplication::quit();
    });
    QTimer::singleShot(0,refresh,&QPushButton::click);dialog.exec();
}
