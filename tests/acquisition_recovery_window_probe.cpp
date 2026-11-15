#include "mainwindow.h"
#include "runconfig.h"
#include "runrecorder.h"
#include "eccprotocol.h"
#include <QApplication>
#include <QAbstractButton>
#include <QAction>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <functional>

// Real MainWindow -> worker RunControl -> ECC -> two data services.
// The surrounding fixture supplies loopback endpoints and synthetic board data.
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    if(argc!=3 || QString::fromLocal8Bit(argv[2])!="--expect-receiver-cleanup") return 2;
    RunConfig config;QString error;
    if(!config.Load(QString::fromLocal8Bit(argv[1]),&error) || !config.nativeEcc || config.nativeClient.address!="127.0.0.1") return 2;
    QFile credential(config.nativeClient.credentialFile);
    if(!credential.open(QIODevice::ReadOnly)) return 2;
    const auto token=credential.readAll().trimmed();
    MainWindow window;if(!window.LoadConfig(QString::fromLocal8Bit(argv[1]))) return 2;window.show();
    auto connectButton=window.findChild<QPushButton *>("Connect"),progress=window.findChild<QPushButton *>("Progress"),start=window.findChild<QPushButton *>("StartRun");
    auto cleanup=window.findChild<QAction *>("NativeReceiverAbort"),connections=window.findChild<QAction *>("NativeFailureBreakup"),retirement=window.findChild<QAction *>("NativeReceiverRetirement"),powerOff=window.findChild<QAction *>("NativeAcquisitionUnprepare"),recovery=window.findChild<QAction *>("NativeAcquisitionRecovery");
    if(!connectButton || !progress || !start || !cleanup || !connections || !retirement || !powerOff || !recovery) return 2;
    QNetworkAccessManager network;QTimer poll,deadline;deadline.setSingleShot(true);
    int step=0,cleanupSignals=0,connectionSignals=0,retirementSignals=0,powerOffSignals=0,recoverySignals=0;bool querying=false,confirming=false,finished=false;
    std::string instance;GetDAQ::EccControlSystemStatus original;
    const auto fail=[&](const QString &message) {if(!finished){finished=true;poll.stop();deadline.stop();qCritical().noquote()<<"FAIL actual window cleanup:"<<message;app.exit(1);}};
    QObject::connect(&window,&MainWindow::AbortFailedReceiversSignal,&app,[&] {++cleanupSignals;});
    QObject::connect(&window,&MainWindow::BreakupAfterFailureSignal,&app,[&]{++connectionSignals;});
    QObject::connect(&window,&MainWindow::RetireFailedReceiversSignal,&app,[&]{++retirementSignals;});
    QObject::connect(&window,&MainWindow::UnprepareAcquisitionAfterFailureSignal,&app,[&]{++powerOffSignals;});
    QObject::connect(&window,&MainWindow::ReconcileAcquisitionSignal,&app,[&]{++recoverySignals;});
    QObject::connect(&deadline,&QTimer::timeout,&app,[&] {fail("deadline at step "+QString::number(step));});deadline.start(300000);
    const auto advance=[&](const GetDAQ::EccControlSystemStatus &status) {
        if(finished || confirming || status.operationActive) return;
        if(step==0 && status.controller.state==1 && progress->isEnabled()) {if(cleanup->isEnabled()){fail("healthy cleanup enabled");return;}++step;progress->click();return;}
        if(step>=1 && step<=3 && status.controller.state==unsigned(step+1) && progress->isEnabled()) {
            if(step<3){++step;progress->click();}else if(start->isEnabled()){++step;start->click();}return;
        }
        QJsonObject journal;
        if(step>=4 && !RunRecorder::ReadJournal(config,&journal,&error)) return;
        if(step==4 && status.controller.state==5 && journal.value("phase").toString()=="Running") {
            original=status;QFile marker(QDir(config.RunDirectory()).filePath("receiver-failure-probe-ready"));
            if(!marker.open(QIODevice::WriteOnly) || marker.write("Real MainWindow Running; ready for owned fixture failure\n")<0){fail("running marker");return;}
            ++step;return;
        }
        if(step==5 && status.controller.outcomeUnknown && cleanup->isEnabled()) {
            const auto cached=journal.value("native_receiver_failure_cache").toString();if(cached.isEmpty()) return;
            const auto policy=GetDAQ::DecodeEccControlReceiverFailureStatus(cached.toStdString());if(policy.phase!="complete") return;
            if(!connections->isEnabled())return;
            confirming=true;
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});connections->trigger();
            if(connectionSignals!=0){fail("cancel emitted connection cleanup");return;}
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});connections->trigger();confirming=false;
            if(connectionSignals!=1){fail("connection cleanup confirmation count");return;}++step;return;
        }
        if(step==6 && cleanup->isEnabled()) {
            const auto saved=journal.value("native_failure_breakup_operation").toString();if(saved.isEmpty())return;
            const auto closed=GetDAQ::DecodeEccControlOperation(saved.toStdString());
            if(closed.phase!="complete" || closed.request.command!="breakup-after-failure" || !status.controller.outcomeUnknown ||
               status.controller.operationId!=original.controller.operationId || !journal.value("duration_incomplete").toBool()) {fail("connection cleanup did not preserve original interrupted Run");return;}
            confirming=true;
            QTimer::singleShot(0,[] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});cleanup->trigger();
            if(cleanupSignals!=0){fail("cancel emitted cleanup");return;}
            QTimer::singleShot(0,[] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});cleanup->trigger();
            confirming=false;if(cleanupSignals!=1){fail("confirmation did not emit exactly once");return;}++step;return;
        }
        if(step==7) {
            const auto bytes=journal.value("native_receiver_cleanup_operation").toString();if(bytes.isEmpty()) return;
            const auto operation=GetDAQ::DecodeEccControlOperation(bytes.toStdString());
            if(operation.phase!="complete" || operation.request.command!="abort-failed-receivers"){fail("saved cleanup outcome");return;}
            if(!status.controller.outcomeUnknown || !status.controlNeedsReconciliation || !status.dataNeedsReconciliation ||
               status.controller.operationId!=original.controller.operationId || !journal.value("duration_incomplete").toBool() ||
               journal.value("phase").toString()=="Complete" || QFileInfo::exists(QDir(config.RunDirectory()).filePath("run.csv")) ||
               window.findChild<QSpinBox *>("RunNumber")->value()!=config.runNumber){fail("cleanup cleared UNKNOWN/completed or advanced Run");return;}
            if(status.nodes.size()!=original.nodes.size()){fail("data host coverage");return;}
            for(std::size_t i=0;i<status.nodes.size();++i) {
                const auto &now=status.nodes[i],&before=original.nodes[i];
                if(now.instanceId!=before.instanceId || now.sessionId!=before.sessionId || now.receivers.size()!=before.receivers.size()){fail("node instance/session changed");return;}
                for(std::size_t j=0;j<now.receivers.size();++j) if(now.receivers[j].pid!=before.receivers[j].pid || now.receivers[j].outputDirectory!=before.receivers[j].outputDirectory || !now.receivers[j].reaped || now.receivers[j].sealVerified){fail("original receiver reap/directory/seal");return;}
            }
            if(!retirement->isEnabled())return;
            confirming=true;
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});retirement->trigger();
            if(retirementSignals!=0){fail("cancel emitted data retirement");return;}
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});retirement->trigger();
            confirming=false;if(retirementSignals!=1){fail("retirement confirmation count");return;}++step;return;
        }
        if(step==8) {
            const auto saved=journal.value("native_receiver_retirement_operation").toString();if(saved.isEmpty())return;
            const auto retired=GetDAQ::DecodeEccControlOperation(saved.toStdString());
            if(retired.phase!="complete" || retired.request.command!="retire-failed-receivers" || !status.controller.outcomeUnknown ||
               !status.controlNeedsReconciliation || status.dataNeedsReconciliation || status.controller.operationId!=original.controller.operationId ||
               !journal.value("duration_incomplete").toBool() || journal.value("phase").toString()=="Complete" ||
               QFileInfo::exists(QDir(config.RunDirectory()).filePath("run.csv")) || window.findChild<QSpinBox *>("RunNumber")->value()!=config.runNumber){fail("retirement completed/advanced Run or cleared parent");return;}
            if(status.nodes.size()!=original.nodes.size()){fail("retired data host coverage");return;}
            for(std::size_t i=0;i<status.nodes.size();++i) {
                const auto &now=status.nodes[i],&before=original.nodes[i];
                if(now.state!="retired" || now.instanceId!=before.instanceId || now.sessionId!=before.sessionId || now.receivers.size()!=before.receivers.size()){fail("retired node identity/state");return;}
                for(std::size_t j=0;j<now.receivers.size();++j)if(now.receivers[j].pid!=before.receivers[j].pid || now.receivers[j].outputDirectory!=before.receivers[j].outputDirectory || !now.receivers[j].reaped || now.receivers[j].sealVerified){fail("retirement adopted/sealed receiver");return;}
            }
            if(!powerOff->isEnabled())return;confirming=true;
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});powerOff->trigger();
            if(powerOffSignals!=0){fail("cancel emitted power-off");return;}
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});powerOff->trigger();
            confirming=false;if(powerOffSignals!=1){fail("power-off confirmation count");return;}++step;return;
        }
        if(step==9) {
            const auto saved=journal.value("native_acquisition_unprepare_operation").toString();if(saved.isEmpty())return;
            const auto unprepared=GetDAQ::DecodeEccControlOperation(saved.toStdString());
            if(unprepared.phase!="complete" || unprepared.request.command!="unprepare-acquisition-after-failure" || !status.controller.outcomeUnknown ||
               !status.controlNeedsReconciliation || status.dataNeedsReconciliation || status.controller.operationId!=original.controller.operationId ||
               !journal.value("duration_incomplete").toBool() || journal.value("phase").toString()=="Complete" ||
               QFileInfo::exists(QDir(config.RunDirectory()).filePath("run.csv")) || window.findChild<QSpinBox *>("RunNumber")->value()!=config.runNumber){fail("power-off completed/advanced Run or cleared parent");return;}
            for(const auto &node:status.nodes)if(node.state!="retired"){fail("power-off changed data lifecycle");return;}
            if(!recovery->isEnabled())return;
            confirming=true;
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});recovery->trigger();
            if(recoverySignals!=0){fail("cancel emitted acquisition recovery");return;}
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});recovery->trigger();
            confirming=false;if(recoverySignals!=1){fail("acquisition recovery confirmation count");return;}++step;return;
        }
        if(step==10) {
            const auto saved=journal.value("native_acquisition_recovery_operation").toString();if(saved.isEmpty())return;
            const auto recovered=GetDAQ::DecodeEccControlOperation(saved.toStdString());
            if(recovered.phase!="complete" || recovered.request.command!="reconcile-acquisition" ||
               recovered.request.failedOperationId!=original.controller.operationId || !journal.value("hardware_recovery_complete").toBool() ||
               journal.value("phase").toString()!="Interrupted" || !journal.value("duration_incomplete").toBool() ||
               status.controller.state!=2 || status.controller.outcomeUnknown || status.controlNeedsReconciliation || status.dataNeedsReconciliation ||
               status.controller.operationId!=recovered.request.operationId || QFileInfo::exists(QDir(config.RunDirectory()).filePath("run.csv")) ||
               window.findChild<QSpinBox *>("RunNumber")->value()!=config.runNumber){fail("recovery did not retain interrupted Run or bound Described");return;}
            if(!progress->isEnabled())return;
            ++step;progress->click();return;
        }
        if(step==11 && status.controller.state==3 && progress->isEnabled()) {
            if(status.controller.outcomeUnknown || status.controlNeedsReconciliation || status.dataNeedsReconciliation ||
               journal.value("phase").toString()!="Interrupted" || !journal.value("hardware_recovery_complete").toBool() ||
               !journal.value("duration_incomplete").toBool() || QFileInfo::exists(QDir(config.RunDirectory()).filePath("run.csv")) ||
               window.findChild<QSpinBox *>("RunNumber")->value()!=config.runNumber){fail("fresh Prepare completed or advanced original Run");return;}
            if(status.nodes.size()!=original.nodes.size()){fail("recovered node coverage");return;}
            for(std::size_t i=0;i<status.nodes.size();++i){const auto &now=status.nodes[i],&before=original.nodes[i];
                if(now.state!="retired" || now.sessionId!=before.sessionId || now.receivers[0].pid!=before.receivers[0].pid ||
                   !now.receivers[0].reaped || now.receivers[0].sealVerified){fail("recovery or Prepare restarted/sealed original receiver");return;}}
            QJsonObject proof;proof["passed"]=true;proof["actual_main_window"]=true;proof["cancel_requests"]=0;
            proof["confirmed_requests"]=cleanupSignals;proof["retirement_confirmed_requests"]=retirementSignals;
            proof["power_off_confirmed_requests"]=powerOffSignals;proof["recovery_confirmed_requests"]=recoverySignals;
            proof["original_run_retained"]=true;proof["hardware_recovery_complete"]=true;proof["fresh_prepare_complete"]=true;
            proof["duration_incomplete"]=true;proof["raw_transferred_by_gui"]=false;
            QFile file(QDir(config.RunDirectory()).filePath("window-cleanup-proof.json"));if(!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(proof).toJson())<0){fail("window proof");return;}
            finished=true;poll.stop();deadline.stop();qInfo()<<"PASS real MainWindow recovery confirmation, retained interrupted Run and manual fresh Prepare; synthetic localhost only";app.exit(0);
        }
    };
    QObject::connect(&poll,&QTimer::timeout,&app,[&] {
        if(querying || finished) return;querying=true;
        QNetworkRequest request(QUrl("http://127.0.0.1:"+QString::number(config.nativeClient.port)+"/v1/status"));request.setRawHeader("Authorization","Bearer "+token);
        auto reply=network.get(request);QTimer::singleShot(2000,reply,[reply] {if(!reply->isFinished())reply->abort();});
        QObject::connect(reply,&QNetworkReply::finished,&app,[&,reply] {
            querying=false;const auto body=reply->read(1024*1024+1);const bool okay=reply->error()==QNetworkReply::NoError;reply->deleteLater();if(!okay)return;
            try {
                if(body.size()>1024*1024){fail("status budget");return;}
                const auto status=GetDAQ::DecodeEccControlSystemStatus(body.toStdString());
                if(status.snapshotSha256!=config.nativeClient.snapshotSha256.toStdString() || (!instance.empty() && status.instanceId!=instance)){fail("foreign ECC binding");return;}
                instance=status.instanceId;advance(status);
            }catch(const std::exception &e){fail(QString::fromUtf8(e.what()));}
        });
    });poll.start(60);connectButton->click();return app.exec();
}
