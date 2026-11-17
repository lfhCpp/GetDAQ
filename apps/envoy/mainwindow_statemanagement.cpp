#include "mainwindow.h"
#include "ui_mainwindow.h"
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
    recover->hide();
    auto update=[this,state,reset]{state->setText(tr("ECC phase: %1").arg(ui->Phase->text()));reset->setEnabled(fConnected && !fBusy && !fActive && !fRemoteRunning && fObservedEccState>=2 && fObservedEccState<=4);};
    connect(refresh,&QPushButton::clicked,&dialog,update);
    connect(reset,&QPushButton::clicked,&dialog,[this,&dialog,state,reset]{
        if(fBusy || fActive || fRemoteRunning || !fConnected)return;
        if(QMessageBox::question(&dialog,tr("Return stopped ECC to Idle"),tr("Regress each confirmed stopped ECC to Idle? This sends the original ECC Undo / Breakup commands."),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
        auto *timer=new QTimer(&dialog);timer->setInterval(200);auto clock=std::make_shared<QElapsedTimer>();clock->start();reset->setEnabled(false);
        connect(timer,&QTimer::timeout,&dialog,[this,timer,clock,state]{
            if(!fConnected || fActive || fRemoteRunning || clock->elapsed()>3*fConfig.stepTimeoutMs){timer->stop();state->setText(tr("Reset stopped; inspect the current ECC state."));return;}
            if(fBusy)return;if(fObservedEccState==1){timer->stop();state->setText(tr("ECC is Idle."));return;}
            if(fObservedEccState<2 || fObservedEccState>4){timer->stop();return;}emit RegressSignal(-1);
        });timer->start();
    });
    update();dialog.exec();
}
