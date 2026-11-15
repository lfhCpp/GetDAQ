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
    auto cleanup=window.findChild<QAction *>("NativeReceiverAbort");
    if(!connectButton || !progress || !start || !cleanup) return 2;
    QNetworkAccessManager network;QTimer poll,deadline;deadline.setSingleShot(true);
    int step=0,cleanupSignals=0;bool querying=false,confirming=false,finished=false;
    std::string instance;GetDAQ::EccControlSystemStatus original;
    const auto fail=[&](const QString &message) {if(!finished){finished=true;poll.stop();deadline.stop();qCritical().noquote()<<"FAIL actual window cleanup:"<<message;app.exit(1);}};
    QObject::connect(&window,&MainWindow::AbortFailedReceiversSignal,&app,[&] {++cleanupSignals;});
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
            confirming=true;
            QTimer::singleShot(0,[] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});cleanup->trigger();
            if(cleanupSignals!=0){fail("cancel emitted cleanup");return;}
            QTimer::singleShot(0,[] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});cleanup->trigger();
            confirming=false;if(cleanupSignals!=1){fail("confirmation did not emit exactly once");return;}++step;return;
        }
        if(step==6) {
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
            QJsonObject proof;proof["passed"]=true;proof["actual_main_window"]=true;proof["cancel_requests"]=0;proof["confirmed_requests"]=cleanupSignals;
            proof["original_run_retained"]=true;proof["hardware_unknown_preserved"]=true;proof["duration_incomplete"]=true;proof["raw_transferred_by_gui"]=false;
            QFile file(QDir(config.RunDirectory()).filePath("window-cleanup-proof.json"));if(!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(proof).toJson())<0){fail("window proof");return;}
            finished=true;poll.stop();deadline.stop();qInfo()<<"PASS real MainWindow confirm/cancel, ECC original-session cleanup, saved incomplete Run; synthetic localhost only";app.exit(0);
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
