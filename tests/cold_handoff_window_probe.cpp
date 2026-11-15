#include "coldhandoffwindow.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLabel>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTimer>
#include <QThread>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <functional>
#include <iostream>
#include <stdexcept>
namespace {
void Require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
void Wait(const std::function<bool()> &ready,int ms=30000){QElapsedTimer t;t.start();while(!ready()){Require(t.elapsed()<ms,"Cold window deadline");QApplication::processEvents();QThread::msleep(10);}}
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);app.setApplicationName("getdaq-cold-window-probe");
    try {
        Require(argc==6 || argc==7,"Pass port credential service output submit|inspect [attempt]");
        const bool inspect=std::string(argv[5])=="inspect";Require(!inspect || argc==7,"Inspect requires original attempt");
        ColdHandoffWindow window;window.show();
        const auto edit=[&](const char *name){auto p=window.findChild<QLineEdit *>(name);Require(p,"Missing production cold input");return p;};
        const auto button=[&](const char *name){auto p=window.findChild<QPushButton *>(name);Require(p,"Missing production cold button");return p;};
        window.findChild<QSpinBox *>("port")->setValue(QString::fromLocal8Bit(argv[1]).toInt());edit("credential")->setText(QString::fromLocal8Bit(argv[2]));edit("service")->setText(QString::fromLocal8Bit(argv[3]));
        Require(!button("submit")->isEnabled(),"Unchecked recovery permitted submit");
        button("check")->click();Wait([&]{return button("submit")->isEnabled();});
        if(inspect)edit("attempt")->setText(QString::fromLocal8Bit(argv[6]));
        const auto confirm=[&](QMessageBox::StandardButton answer){bool clicked=false;QTimer timer;timer.setInterval(10);QObject::connect(&timer,&QTimer::timeout,[&]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box){clicked=true;timer.stop();box->button(answer)->click();}});timer.start();button(inspect ? "inspect" : "submit")->click();Require(clicked,"No production cold confirmation");};
        confirm(QMessageBox::Cancel);Require(edit("task")->text().isEmpty(),"Cancel created a recovery task ID");
        confirm(QMessageBox::Yes);Require(!edit("task")->text().isEmpty(),"Confirmed cold intent not retained");
        auto result=window.findChild<QPlainTextEdit *>("result");Require(result,"Missing production cold result");
        Wait([&]{
            const auto message=window.findChild<QLabel *>("status")->text();
            if(message.contains("HTTP 503") || message.contains("HTTP 409"))throw std::runtime_error(message.toStdString());
            return QJsonDocument::fromJson(result->toPlainText().toUtf8()).object().value("phase").toString()=="complete";
        },180000);
        const auto operation=GetDAQ::DecodeColdHandoffOperation(result->toPlainText().toStdString());Require(operation.request.taskId==edit("task")->text().toStdString(),"Window lost original task ID");
        Require(operation.request.action==(inspect ? "inspect" : "submit") && (!inspect || operation.request.attemptId==argv[6]),"Window changed original attempt/action");
        Require(!button("submit")->isEnabled(),"Window enabled implicit second submit");
        const auto receipt=QString::fromLocal8Bit(argv[4])+".receipt.json";QString error;Require(window.SaveReceipt(receipt,&error),"Export checked cold receipt");QFile file(receipt);Require(file.open(QIODevice::ReadOnly) && file.readAll().toStdString()==operation.receipt,"Export changed original proof bytes");
        const auto intent=QString::fromLocal8Bit(argv[4])+".intent.json";QFile saved(intent);Require(saved.open(QIODevice::WriteOnly),"Save probe original intent");saved.write(QByteArray::fromStdString(GetDAQ::EncodeColdHandoffRequest(operation.request)));saved.close();
        Require(window.LoadIntent(intent,&error),"Load same original intent");Require(result->toPlainText().isEmpty(),"Loading intent retained old result");button("observe")->click();Wait([&]{return !result->toPlainText().isEmpty();});
        Require(GetDAQ::DecodeColdHandoffOperation(result->toPlainText().toStdString()).requestKey==operation.requestKey,"Observation changed original intent");
        edit("service")->setText("wrong-recovery");button("check")->click();Wait([&]{return button("check")->isEnabled();});Require(!button("submit")->isEnabled() && !button("exportReceipt")->isEnabled(),"Wrong service retained authority/receipt");
        QJsonObject proof;proof["passed"]=true;proof["real_production_window"]=true;proof["cancel_created_no_id"]=true;proof["loaded_intent_observed_without_post"]=true;proof["wrong_service_refused"]=true;proof["operation"]=QString::fromStdString(GetDAQ::EncodeColdHandoffOperation(operation));
        QFile out(QString::fromLocal8Bit(argv[4]));Require(out.open(QIODevice::WriteOnly),"Save window proof");out.write(QJsonDocument(proof).toJson());
        std::cout<<"PASS production cold window Cancel/Yes, original-ID reload/GET, exact receipt export, wrong-service denial\n";return 0;
    }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
