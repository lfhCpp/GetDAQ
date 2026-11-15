#include "transferwindow.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <functional>
#include <iostream>
#include <stdexcept>
namespace {
void Require(bool okay,const char *why){if(!okay)throw std::runtime_error(why);}
void Wait(const std::function<bool()> &ready,int ms=12000){QElapsedTimer timer;timer.start();while(!ready()){Require(timer.elapsed()<ms,"Window observation deadline");QApplication::processEvents();QThread::msleep(10);}}
}
int main(int argc,char **argv){
    QApplication app(argc,argv);app.setApplicationName("getdaq-transfer-probe");
    try {
        Require(argc==5 || argc==6 || argc==7,"Pass port credential agent result file [original task [resume]]");TransferWindow window;window.show();
        auto edit=[&](const char *name){auto field=window.findChild<QLineEdit *>(name);Require(field,"Missing production input");return field;};
        auto button=[&](const char *name){auto field=window.findChild<QPushButton *>(name);Require(field,"Missing production button");return field;};
        window.findChild<QSpinBox *>("port")->setValue(QString::fromLocal8Bit(argv[1]).toInt());edit("credential")->setText(QString::fromLocal8Bit(argv[2]));edit("agent")->setText(QString::fromLocal8Bit(argv[3]));edit("experiment")->setText("Test");
        if(argc>=6) {
            const bool resume=argc==7 && std::string(argv[6])=="resume";
            edit("operation")->setText(QString::fromLocal8Bit(argv[5]));
            const auto confirm=[&](QMessageBox::StandardButton answer) {
                bool clicked=false;QTimer timer;timer.setInterval(10);
                QObject::connect(&timer,&QTimer::timeout,[&]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box){clicked=true;timer.stop();box->button(answer)->click();}});
                timer.start();button(resume ? "resume" : "reconcile")->click();Wait([&]{return clicked;});
            };
            confirm(QMessageBox::Cancel);
            Wait([&]{return window.findChild<QLabel *>("status")->text().contains("cancelled");});
            auto result=window.findChild<QPlainTextEdit *>("result");Require(result,"Missing verification result");
            const auto cancelled=GetDAQ::DecodeTransferOperation(result->toPlainText().toStdString()).phase;
            Require(cancelled=="unknown" || (resume && cancelled=="failed"),"Cancel recovered original operation");
            confirm(QMessageBox::Yes);
            Wait([&]{return QJsonDocument::fromJson(result->toPlainText().toUtf8()).object().value("phase").toString()=="complete";});
            Require(GetDAQ::DecodeTransferOperation(result->toPlainText().toStdString()).request.operationId==argv[5],"Verification changed original ID");
            QJsonObject proof;proof["passed"]=true;proof["cancel_kept_unknown"]=true;proof["confirmed_verification_complete"]=true;proof["resume"]=resume;proof["original_id"]=QString::fromLocal8Bit(argv[5]);
            QFile file(QString::fromLocal8Bit(argv[4]));Require(file.open(QIODevice::WriteOnly),"Save verification dialog proof");file.write(QJsonDocument(proof).toJson());
            std::cout<<"PASS production verification dialog Cancel/Yes, original ID and independent native result\n";return 0;
        }
        button("check")->click();Wait([&]{return button("submit")->isEnabled();});
        bool cancelled=false;QTimer::singleShot(0,[&]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box){cancelled=true;box->button(QMessageBox::Cancel)->click();}});button("submit")->click();
        Require(cancelled && edit("operation")->text().isEmpty(),"Cancel created a submission ID");
        bool confirmed=false;QTimer::singleShot(0,[&]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box){confirmed=true;box->button(QMessageBox::Yes)->click();}});button("submit")->click();
        Require(confirmed && !edit("operation")->text().isEmpty(),"Confirmed submission has no original ID");
        auto result=window.findChild<QPlainTextEdit *>("result");Require(result,"Missing production result");
        Wait([&]{const auto doc=QJsonDocument::fromJson(result->toPlainText().toUtf8());return doc.object().value("phase").toString()=="complete";});
        const auto completed=GetDAQ::DecodeTransferOperation(result->toPlainText().toStdString());Require(completed.request.operationId==edit("operation")->text().toStdString(),"Window changed task ID");
        Require(!button("submit")->isEnabled(),"Completion enabled automatic resubmission");
        edit("agent")->setText("wrong-agent");button("check")->click();Wait([&]{return button("check")->isEnabled();});Require(!button("submit")->isEnabled(),"Wrong identity enabled copy");
        QJsonObject proof;proof["passed"]=true;proof["cancel_created_no_id"]=true;proof["confirmed_id"]=QString::fromStdString(completed.request.operationId);proof["request_key"]=QString::fromStdString(completed.requestKey);proof["receipt"]=QString::fromStdString(completed.receipt);proof["real_production_dialog"]=true;
        QFile file(QString::fromLocal8Bit(argv[4]));Require(file.open(QIODevice::WriteOnly),"Cannot save probe result");const auto bytes=QJsonDocument(proof).toJson();Require(file.write(bytes)==bytes.size(),"Probe result write");
        std::cout<<"PASS actual TransferWindow confirmation/Cancel, HTTP original-ID observation, native completion, wrong-agent denial\n";return 0;
    }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
