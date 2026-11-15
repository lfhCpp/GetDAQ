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
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QTimer>

// Production MainWindow/worker components; the caller owns all synthetic peers.
int main(int argc,char **argv)
{
    QApplication app(argc,argv);if(argc<2)return 2;
    RunConfig config;QString error;
    if(!config.Load(QString::fromLocal8Bit(argv[1]),&error) || !config.nativeEcc || !config.nativeClient.alarmStatusEnabled)return 2;
    QFile credential(config.nativeClient.credentialFile);if(!credential.open(QIODevice::ReadOnly))return 2;
    const auto token=credential.readAll().trimmed();MainWindow window;
    if(!window.LoadConfig(QString::fromLocal8Bit(argv[1])))return 2;window.show();
    auto connectButton=window.findChild<QPushButton *>("Connect"),progress=window.findChild<QPushButton *>("Progress"),start=window.findChild<QPushButton *>("StartRun");
    auto release=window.findChild<QAction *>("NativeStoppedAlarmRelease");if(!connectButton || !progress || !start || !release)return 2;
    QNetworkAccessManager network;QTimer poll,deadline;deadline.setSingleShot(true);
    int step=0,confirmationSignals=0;bool querying=false,confirming=false,finished=false;
    std::string instance,alarmId;GetDAQ::EccControlSystemStatus original;
    const auto fail=[&](const QString &why){if(!finished){finished=true;poll.stop();deadline.stop();qCritical().noquote()<<"FAIL stopped alarm window:"<<why;app.exit(1);}};
    QObject::connect(&window,&MainWindow::ReleaseStoppedAlarmSignal,&app,[&]{++confirmationSignals;});
    QObject::connect(&deadline,&QTimer::timeout,&app,[&]{fail("deadline step "+QString::number(step));});deadline.start(220000);
    const auto advance=[&](const GetDAQ::EccControlSystemStatus &status){
        if(finished || confirming || status.operationActive)return;
        if(step==0 && status.controller.state==1 && progress->isEnabled()){++step;progress->click();return;}
        if(step>=1 && step<=3 && status.controller.state==unsigned(step+1) && progress->isEnabled()){
            if(step<3){++step;progress->click();}else if(start->isEnabled()){++step;start->click();}return;
        }
        QJsonObject journal;if(step>=4 && !RunRecorder::ReadJournal(config,&journal,&error))return;
        if(step==4 && status.controller.state==5 && journal.value("phase").toString()=="Running"){
            original=status;QFile marker(QDir(config.RunDirectory()).filePath("alarm-probe-ready"));
            if(!marker.open(QIODevice::WriteOnly) || marker.write("Actual GUI Running; ready for synthetic alarm\n")<0){fail("marker");return;}
            ++step;return;
        }
        if(step==5 && status.controller.state==4 && !status.controller.outcomeUnknown && release->isEnabled()){
            const auto bytes=journal.value("native_alarm_cache").toString();if(bytes.isEmpty())return;
            const auto alarm=GetDAQ::DecodeEccControlAlarmStatus(bytes.toStdString());
            if(!alarm.latched || alarm.phase!="complete")return;
            if(alarm.controllerOutcomeUnknown || alarm.operationId!=status.controller.operationId){fail("original known alarm binding");return;}
            alarmId=alarm.operationId;confirming=true;
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});release->trigger();
            if(confirmationSignals){fail("cancel submitted");return;}
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});release->trigger();confirming=false;
            if(confirmationSignals!=1){fail("confirmation count");return;}++step;return;
        }
        if(step==6 && status.controller.state==2 && !status.controlNeedsReconciliation && !status.controller.outcomeUnknown && progress->isEnabled()){
            const auto saved=journal.value("native_alarm_acknowledgement_operation").toString();if(saved.isEmpty())return;
            const auto op=GetDAQ::DecodeEccControlOperation(saved.toStdString());
            if(op.phase!="complete" || op.request.command!="release-stopped-alarm" || op.request.alarmOperationId!=alarmId ||
               op.request.failedOperationId!=alarmId || journal.value("phase").toString()!="Interrupted" ||
               !journal.value("hardware_recovery_complete").toBool() || !journal.value("duration_incomplete").toBool()) {fail("original interrupted Run proof");return;}
            if(status.nodes.size()!=original.nodes.size()){fail("node count");return;}
            for(std::size_t i=0;i<status.nodes.size();++i){const auto &now=status.nodes[i],&before=original.nodes[i];
                if(now.sessionId!=before.sessionId || now.receivers.size()!=1 || now.receivers[0].pid!=before.receivers[0].pid || !now.receivers[0].reaped || !now.receivers[0].sealVerified){fail("original sealed receiver identity");return;}}
            if(QFile::exists(QDir(config.RunDirectory()).filePath("run.csv"))){fail("fabricated Run completion");return;}
            ++step;progress->click();return;
        }
        if(step==7 && status.controller.state==3 && !status.controller.outcomeUnknown && !status.controlNeedsReconciliation && progress->isEnabled()){
            QJsonObject proof{{"passed",true},{"actual_main_window_and_runcontrol",true},{"cancel_signals",0},{"confirmed_signals",confirmationSignals},
                {"original_alarm_id",QString::fromStdString(alarmId)},{"original_run_interrupted",true},{"receivers_normally_sealed",true},{"fresh_prepare_verified",true}};
            QFile file(QDir(config.RunDirectory()).filePath("stopped-alarm-window-proof.json"));
            if(!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(proof).toJson())<0){fail("save proof");return;}
            finished=true;poll.stop();deadline.stop();qInfo()<<"PASS actual stopped alarm release, original interrupted Run, normal receiver seals and manual fresh Prepare";app.exit(0);
        }
    };
    QObject::connect(&poll,&QTimer::timeout,&app,[&]{if(querying || finished)return;querying=true;
        QNetworkRequest request(QUrl("http://127.0.0.1:"+QString::number(config.nativeClient.port)+"/v1/status"));request.setRawHeader("Authorization","Bearer "+token);
        auto reply=network.get(request);QTimer::singleShot(2000,reply,[reply]{if(!reply->isFinished())reply->abort();});
        QObject::connect(reply,&QNetworkReply::finished,&app,[&,reply]{querying=false;const auto bytes=reply->read(1024*1024+1);const bool okay=reply->error()==QNetworkReply::NoError;reply->deleteLater();if(!okay)return;
            try{if(bytes.size()>1024*1024){fail("status budget");return;}const auto status=GetDAQ::DecodeEccControlSystemStatus(bytes.toStdString());
                if(status.snapshotSha256!=config.nativeClient.snapshotSha256.toStdString() || (!instance.empty() && status.instanceId!=instance)){fail("foreign identity");return;}
                instance=status.instanceId;advance(status);
            }catch(const std::exception &e){fail(QString::fromUtf8(e.what()));}});
    });poll.start(60);connectButton->click();return app.exec();
}
