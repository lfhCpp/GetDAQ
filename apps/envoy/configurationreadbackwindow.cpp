#include "configurationreadbackwindow.h"
#include "ui_configurationreadbackwindow.h"
#include "eccprotocol.h"
#include <QHeaderView>
#include <QTimer>
#include <QDateTime>
#include <stdexcept>
ConfigurationReadbackWindow::ConfigurationReadbackWindow(QWidget *parent) : QDialog(parent),ui(new Ui::ConfigurationReadbackWindow) {
    ui->setupUi(this);fClock.start();setAttribute(Qt::WA_DeleteOnClose);
    ui->ReadbackSummary->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->ReadbackSummary->horizontalHeader()->setStretchLastSection(true);
    ui->ReadbackHardware->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    connect(ui->ReadbackRefresh,&QPushButton::clicked,this,[this]{if(fConnected)emit InspectSignal(fGeneration);});
    auto timer=new QTimer(this);timer->setInterval(2000);
    connect(timer,&QTimer::timeout,this,[this]{Render();if(fConnected && isVisible())emit InspectSignal(fGeneration);});timer->start();
}
ConfigurationReadbackWindow::~ConfigurationReadbackWindow(){delete ui;}
void ConfigurationReadbackWindow::Bind(const QString &snapshot,quint64 generation,bool connected) {
    if(snapshot!=fSnapshot || generation!=fGeneration || !connected){fBody.clear();fObservedAt=-1;fError=connected ? QString() : tr("Native ECC is disconnected");}
    fSnapshot=snapshot;fGeneration=generation;fConnected=connected;ui->ReadbackRefresh->setEnabled(connected);Render();
}
void ConfigurationReadbackWindow::Observe(const QByteArray &body,const QString &error,quint64 generation) {
    if(!fConnected || generation!=fGeneration)return;
    fBody=error.isEmpty() ? body : QByteArray();fError=error;fObservedAt=fBody.isEmpty() ? -1 : fClock.elapsed();Render();
}
void ConfigurationReadbackWindow::Render() {
    QVector<QPair<QString,QString>> rows{{tr("Evidence scope"),tr("Historical Configure RPC/audit checks")},
        {tr("Current complete hardware loading"),tr("Not verified")},{tr("Live hardware checked by this query"),tr("No")}};
    ui->ReadbackHardware->setRowCount(0);QString observation=fError.isEmpty() ? tr("Waiting for a bound ECC response") : fError;
    QString style="color: #a87600";
    try {
        if(!fBody.isEmpty()) {
            const auto record=GetDAQ::DecodeEccControlConfigureReadbacks(fBody.toStdString());
            if(QString::fromStdString(record.snapshotSha256)!=fSnapshot)throw std::runtime_error("Readback evidence differs from the selected ECC snapshot");
            const bool stale=fObservedAt<0 || fClock.elapsed()-fObservedAt>6000;
            if(stale)observation=tr("Observation expired; current evidence availability is unconfirmed");
            else if(record.controller.outcomeUnknown || record.controller.needsReconciliation){observation=tr("Parent ECC requires reconciliation; historical checks do not recover it");style="color: #b00020";}
            else if(record.phase=="captured")observation=tr("Historical Configure checks captured; complete current loading remains unverified");
            else if(record.phase=="pending")observation=tr("Configure is pending; no completed main-plan evidence yet");
            else if(record.phase=="unavailable")observation=tr("Configure did not provide complete evidence; inspect the original operation");
            else observation=tr("No Configure evidence observed in this service instance");
            rows.append({tr("Configure operation"),QString::fromStdString(record.configureOperationId)});
            rows.append({tr("ECC instance"),QString::fromStdString(record.instanceId)});
            rows.append({tr("Parent snapshot"),QString::fromStdString(record.snapshotSha256)});
            rows.append({tr("Parameters / artifact"),QString::fromStdString(record.configurationSha256)+" / "+QString::fromStdString(record.artifactSha256)});
            rows.append({tr("Parent state / operation"),QString::number(record.controller.state)+" / "+QString::fromStdString(record.controller.operationId)});
            rows.append({tr("Received"),tr("%1 seconds ago; historical cache").arg((fClock.elapsed()-fObservedAt)/1000)});
            // Expired observations keep identification, but withdraw displayed per-board confirmations.
            if(!stale) {
                ui->ReadbackHardware->setRowCount(int(record.entries.size()));
                for(int i=0;i<int(record.entries.size());++i) {
                    const auto &entry=record.entries[std::size_t(i)];const auto &e=entry.evidence;
                    const QStringList values{QString::fromStdString(entry.id),QString::fromStdString(entry.stage),QString::number(e.writesAcknowledged),
                        QString::number(e.writesReadbackVerified),QString::number(e.writesAcknowledged-e.writesReadbackVerified),
                        QString::number(e.checksMatched)+" / "+QString::number(e.checksWithWarnings),
                        QString::number(e.readOnlyObservations)+" / "+QString::number(e.maskedUpdatesVerified),
                        QString::number(e.managerCommandsAcknowledged)+" / "+QString::number(e.delaysCompleted),
                        QString::fromStdString(entry.auditFile),QString::fromStdString(e.auditSha256)};
                    const auto detail=tr("Plan started at Unix ms %1\nNode: %2\nTrusted execution chain: %3\nCounts describe steps, not distinct physical registers.")
                        .arg(QString::number(qulonglong(e.startedAtUnixMs)),QString::fromStdString(e.nodeProxy),QString::fromStdString(e.auditChainSha256));
                    for(int column=0;column<values.size();++column){auto item=new QTableWidgetItem(values[column]);item->setToolTip("<pre>"+detail.toHtmlEscaped()+"</pre>");ui->ReadbackHardware->setItem(i,column,item);}
                }
            }
        }
    }catch(const std::exception &e){fBody.clear();fObservedAt=-1;fError=tr("Invalid readback response: %1").arg(QString::fromUtf8(e.what()));observation=fError;}
    ui->ReadbackObservation->setText(observation);ui->ReadbackObservation->setStyleSheet(style);
    ui->ReadbackSummary->setRowCount(rows.size());
    for(int i=0;i<rows.size();++i){ui->ReadbackSummary->setItem(i,0,new QTableWidgetItem(rows[i].first));ui->ReadbackSummary->setItem(i,1,new QTableWidgetItem(rows[i].second));}
}
