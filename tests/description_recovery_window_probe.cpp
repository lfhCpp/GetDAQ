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
    QApplication app(argc,argv);if(argc!=2)return 2;
    RunConfig config;QString error;
    if(!config.Load(QString::fromLocal8Bit(argv[1]),&error) || !config.nativeEcc || config.nativeClient.address!="127.0.0.1")return 2;
    QFile credential(config.nativeClient.credentialFile);if(!credential.open(QIODevice::ReadOnly))return 2;const auto token=credential.readAll().trimmed();
    const auto folder=QFileInfo(QString::fromLocal8Bit(argv[1])).absolutePath();MainWindow window;if(!window.LoadConfig(QString::fromLocal8Bit(argv[1])))return 2;window.show();
    auto connectButton=window.findChild<QPushButton *>("Connect"),progress=window.findChild<QPushButton *>("Progress");auto recovery=window.findChild<QAction *>("NativeDescriptionRecovery");if(!connectButton || !progress || !recovery)return 2;
    QNetworkAccessManager network;QTimer poll,deadline;deadline.setSingleShot(true);int step=0,confirmedSignals=0;bool querying=false,confirming=false,finished=false;std::string instance,failedId;
    const auto fail=[&](const QString &why){if(!finished){finished=true;poll.stop();deadline.stop();qCritical().noquote()<<"FAIL original description window:"<<why;app.exit(1);}};
    QObject::connect(&window,&MainWindow::ReconcileDescriptionSignal,&app,[&]{++confirmedSignals;});QObject::connect(&deadline,&QTimer::timeout,&app,[&]{fail("deadline step "+QString::number(step));});deadline.start(210000);
    const auto advance=[&](const GetDAQ::EccControlSystemStatus &status){
        if(finished || confirming || status.operationActive)return;
        if(step==0 && status.controller.state==1 && !status.controller.outcomeUnknown && progress->isEnabled()){++step;progress->click();return;}
        if(step==1 && status.controller.outcomeUnknown && recovery->isEnabled()) {
            failedId=status.controller.operationId;if(failedId.empty()){fail("no original operation");return;}confirming=true;
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});recovery->trigger();if(confirmedSignals!=0){fail("cancel sent recovery");return;}
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});recovery->trigger();confirming=false;if(confirmedSignals!=1){fail("confirmation did not submit exactly once");return;}++step;return;
        }
        if(step==2 && status.controller.state==2 && !status.controller.outcomeUnknown && !status.controlNeedsReconciliation && progress->isEnabled()) {
            QFile marker(folder+"/description-reconciled-ready");if(!marker.open(QIODevice::WriteOnly) || marker.write("ready")!=5){fail("handshake marker");return;}marker.close();++step;return;
        }
        if(step==3 && QFileInfo::exists(folder+"/continue-prepare") && progress->isEnabled()){++step;progress->click();return;}
        if(step==4 && status.controller.state==3 && !status.controller.outcomeUnknown && !status.controlNeedsReconciliation && progress->isEnabled()) {
            QJsonObject proof;proof["passed"]=true;proof["actual_main_window_and_runcontrol"]=true;proof["original_failure_recorded_by_gui"]=true;proof["cancel_requests"]=0;proof["confirmed_requests"]=confirmedSignals;proof["prepare_after_recovery"]=true;proof["original_failed_operation_id"]=QString::fromStdString(failedId);proof["raw_transferred_by_gui"]=false;
            QFile file(folder+"/window-description-proof.json");if(!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(proof).toJson())<0){fail("save proof");return;}finished=true;poll.stop();deadline.stop();qInfo()<<"PASS production MainWindow/RunControl original Describe intent, cancel/confirm recovery and subsequent Prepare";app.exit(0);
        }
    };
    QObject::connect(&poll,&QTimer::timeout,&app,[&]{if(querying || finished)return;querying=true;QNetworkRequest request(QUrl("http://127.0.0.1:"+QString::number(config.nativeClient.port)+"/v1/status"));request.setRawHeader("Authorization","Bearer "+token);auto reply=network.get(request);QTimer::singleShot(2000,reply,[reply]{if(!reply->isFinished())reply->abort();});
        QObject::connect(reply,&QNetworkReply::finished,&app,[&,reply]{querying=false;const auto bytes=reply->read(1024*1024+1);const bool okay=reply->error()==QNetworkReply::NoError;reply->deleteLater();if(!okay)return;try{if(bytes.size()>1024*1024){fail("status budget");return;}const auto status=GetDAQ::DecodeEccControlSystemStatus(bytes.toStdString());if(status.snapshotSha256!=config.nativeClient.snapshotSha256.toStdString() || (!instance.empty() && status.instanceId!=instance)){fail("foreign binding");return;}instance=status.instanceId;advance(status);}catch(const std::exception &e){fail(QString::fromUtf8(e.what()));}});
    });poll.start(60);connectButton->click();return app.exec();
}
