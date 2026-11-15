#include "../apps/envoy/failedhandoffwindow.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QThread>
#include <QFile>
#include <QFileInfo>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
void Wait(const std::function<bool()> &ready,int timeout=30000){QElapsedTimer timer;timer.start();while(!ready()){Require(timer.elapsed()<timeout,"Failed handoff window deadline");QApplication::processEvents();QThread::msleep(10);}}
}

int main(int argc,char **argv){
    QApplication app(argc,argv);
    try{
        Require(argc==12,"Pass query host port credential agent controller plan original failed-id new-id output");
        const QString query=QString::fromLocal8Bit(argv[1]);
        const auto fill=[&](FailedHandoffWindow &window){
            window.findChild<QLineEdit *>("host")->setText(QString::fromLocal8Bit(argv[2]));
            window.findChild<QSpinBox *>("port")->setValue(QString::fromLocal8Bit(argv[3]).toInt());
            window.findChild<QLineEdit *>("credential")->setText(QString::fromLocal8Bit(argv[4]));
            window.findChild<QLineEdit *>("agent")->setText(QString::fromLocal8Bit(argv[5]));
            window.findChild<QLineEdit *>("controller")->setText(QString::fromLocal8Bit(argv[6]));
            window.findChild<QLineEdit *>("plan")->setText(QString::fromLocal8Bit(argv[7]));
            window.findChild<QLineEdit *>("original")->setText(QString::fromLocal8Bit(argv[8]));
            window.findChild<QLineEdit *>("failedId")->setText(QString::fromLocal8Bit(argv[9]));
            window.findChild<QLineEdit *>("newId")->setText(QString::fromLocal8Bit(argv[10]));
            window.findChild<QLineEdit *>("output")->setText(QString::fromLocal8Bit(argv[11]));
        };
        FailedHandoffWindow window(nullptr,query);fill(window);window.show();
        auto button=[&](FailedHandoffWindow &w,const char *name){auto *p=w.findChild<QPushButton *>(name);Require(p,"Missing handoff UI button");return p;};
        Require(!button(window,"submit")->isEnabled() && !button(window,"observe")->isEnabled(),"Unchecked completion was actionable");
        button(window,"generate")->click();Wait([&]{return QFileInfo(QString::fromLocal8Bit(argv[11])).exists() || !window.findChild<QLabel *>("status")->text().contains("Inputs changed");});
        Require(button(window,"submit")->isEnabled(),"Live CLI did not produce a reviewable completion request");
        QFile saved(argv[11]);Require(saved.open(QIODevice::ReadOnly),"Window did not save the completion request");const auto request=saved.readAll();
#ifdef Q_OS_UNIX
        Require((QFileInfo(saved).permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ExeGroup|QFile::ReadOther|QFile::WriteOther|QFile::ExeOther))==0,"Completion request is not private");
#endif
        QJsonParseError parse;const auto reviewed=QJsonDocument::fromJson(request,&parse).object();Require(parse.error==QJsonParseError::NoError && reviewed.value("command").toString()=="complete-control-handoff" && reviewed.value("operation_id").toString()==argv[10],"Reviewed request differs from the requested operation");
        const auto confirm=[&](QMessageBox::StandardButton answer){bool handled=false;QTimer timer;timer.setInterval(5);QObject::connect(&timer,&QTimer::timeout,[&]{auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box){handled=true;timer.stop();box->button(answer)->click();}});timer.start();button(window,"submit")->click();Wait([&]{return handled;});};
        confirm(QMessageBox::Cancel);Require(button(window,"submit")->isEnabled(),"Cancel consumed the single submit before POST");
        confirm(QMessageBox::Yes);Wait([&]{return window.findChild<QLabel *>("status")->text().contains("accepted") || window.findChild<QLabel *>("status")->text().contains("not confirmed");});
        Require(!button(window,"submit")->isEnabled() && button(window,"observe")->isEnabled(),"Submit was not consumed or observe is unavailable");
        button(window,"observe")->click();Wait([&]{return QJsonDocument::fromJson(window.findChild<QPlainTextEdit *>("resultView")->toPlainText().toUtf8()).object().value("phase").toString()=="complete";},60000);
        window.close();
        FailedHandoffWindow reopened(nullptr,query);fill(reopened);QString error;Require(reopened.LoadRequestFile(QString::fromLocal8Bit(argv[11]),&error),"Cannot reload saved completion for observation");
        Require(!button(reopened,"submit")->isEnabled() && button(reopened,"observe")->isEnabled(),"Reloaded completion request allowed a second POST");
        button(reopened,"observe")->click();Wait([&]{return QJsonDocument::fromJson(reopened.findChild<QPlainTextEdit *>("resultView")->toPlainText().toUtf8()).object().value("phase").toString()=="complete";},60000);
        QJsonObject proof;proof["passed"]=true;proof["live_cli_request_generated"]=true;proof["cancel_sent_no_post"]=true;proof["submit_attempt_consumed_once"]=true;proof["operation_observed_complete"]=true;proof["reloaded_request_observation_only"]=true;
        QFile output(QString::fromLocal8Bit(argv[11])+".window-result.json");Require(output.open(QIODevice::WriteOnly),"Cannot write window probe result");output.write(QJsonDocument(proof).toJson());
        std::cout<<"PASS failed handoff completion UI: validate, review, cancel, single POST, observe, reload read-only\n";return 0;
    }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
