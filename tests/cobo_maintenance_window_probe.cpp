#include "mainwindow.h"
#include "cobomaintenancewindow.h"
#include "runconfig.h"
#include "eccprotocol.h"
#include <QApplication>
#include <QAbstractButton>
#include <QAction>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
namespace {
bool Save(const QString &path,const QByteArray &bytes) {
    QFile file(path);
    if(file.exists()) {
        if(!file.open(QIODevice::ReadOnly))return false;
        const auto existing=file.read(bytes.size()+1);
        return file.atEnd() && existing==bytes;
    }
    if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly) ||
       !file.setPermissions(QFile::ReadOwner|QFile::WriteOwner) ||
       file.write(bytes)!=bytes.size())return false;
    file.close();
    return file.error()==QFileDevice::NoError;
}
QByteArray Read(const QString &path) {
    QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};
    return file.read(1024*1024+1);
}
}
// The fixture owns loopback servers. Only the production window submits control.
int main(int argc,char **argv) {
    QApplication app(argc,argv);if(argc!=3)return 2;
    const QString configFile=QString::fromLocal8Bit(argv[1]),output=QString::fromLocal8Bit(argv[2]);
    RunConfig config;QString error;
    if(!config.Load(configFile,&error) || !config.nativeEcc || config.nativeClient.address!="127.0.0.1")return 2;
    const bool rawGuard=!qgetenv("GETDAQ_RAW_READ_LOG").isEmpty();
    if(rawGuard) {
        const auto sample=QFileInfo(configFile).absolutePath()+"/gui-guard-selfcheck.graw";
        if(!Save(sample,"x"))return 2;
        QFile file(sample);const bool denied=!file.open(QIODevice::ReadOnly);file.close();
        const bool removed=QFile::remove(sample);if(!denied || !removed)return 2;
    }
    MainWindow window;if(!window.LoadConfig(configFile))return 2;window.show();
    auto connectButton=window.findChild<QPushButton *>("Connect");
    auto action=window.findChild<QAction *>("NativeCoBoMaintenance");
    if(!connectButton || !action)return 2;
    CoBoMaintenanceWindow *dialog=nullptr;QTimer poll,deadline;deadline.setSingleShot(true);
    bool finished=false,confirming=false,cancelDefault=false,busyObserved=false,ledBusyObserved=false,pulserBusyObserved=false,blinkBusyObserved=false,flipBusyObserved=false,pulseBusyObserved=false;
    int step=0,submissions=0,ledSubmissions=0,pulserSubmissions=0,blinkSubmissions=0,flipSubmissions=0,pulseSubmissions=0;QString instance,snapshot;
    unsigned int submittedAsad=0,submittedType=0,submittedState=0,submittedFlipId=0,submittedPulseId=0,submittedPulsePeriod=0;
    int submittedVoltage=0;
    const auto fail=[&](const QString &message) {
        if(finished)return;
        finished=true;poll.stop();deadline.stop();
        qCritical().noquote()<<"FAIL CoBo maintenance window:"<<message;app.exit(1);
    };
    QObject::connect(&window,&MainWindow::ReprepareCoBoSignal,&app,
        [&](const QString &target,const QString &id,const QString &digest) {
            ++submissions;instance=id;snapshot=digest;
            if(target!="C0")fail("submitted target differs from selected C0");
        });
    QObject::connect(&window,&MainWindow::SetCoBoLedSignal,&app,
        [&](const QString &target,unsigned int asad,unsigned int type,unsigned int state,const QString &id,const QString &digest) {
            ++ledSubmissions;submittedAsad=asad;submittedType=type;submittedState=state;
            if(target!="C0" || asad!=3 || type!=2 || state!=3 || id!=instance || digest!=snapshot)
                fail("LED submission differs from the reviewed C0 / AsAd 3 / U / Fast selection");
        });
    QObject::connect(&window,&MainWindow::TriggerAsAdPulserSignal,&app,
        [&](const QString &target,int voltage,const QString &id,const QString &digest) {
            ++pulserSubmissions;submittedVoltage=voltage;
            if(target!="C0" || voltage!=321 || id!=instance || digest!=snapshot)
                fail("pulser submission differs from the reviewed C0 / 321 mV offset selection");
        });
    QObject::connect(&window,&MainWindow::BlinkCoBoLedsSignal,&app,
        [&](const QString &target,const QString &id,const QString &digest) {
            ++blinkSubmissions;
            if(target!="C0" || id!=instance || digest!=snapshot)fail("LED blink differs from reviewed C0/instance/snapshot");
        });
    QObject::connect(&window,&MainWindow::FlipCoBoLedSignal,&app,
        [&](const QString &target,unsigned int ledId,const QString &id,const QString &digest) {
            ++flipSubmissions;submittedFlipId=ledId;
            if(target!="C0" || ledId!=10 || id!=instance || digest!=snapshot)fail("LED flip differs from reviewed C0/GPIO bit 10/instance/snapshot");
        });
    QObject::connect(&window,&MainWindow::PulseCoBoLedSignal,&app,
        [&](const QString &target,unsigned int ledId,unsigned int periodMs,const QString &id,const QString &digest) {
            ++pulseSubmissions;submittedPulseId=ledId;submittedPulsePeriod=periodMs;
            if(target!="C0" || ledId!=10 || periodMs!=725 || id!=instance || digest!=snapshot)fail("LED pulse differs from reviewed C0/GPIO bit 10/725 ms/instance/snapshot");
        });
    QObject::connect(&deadline,&QTimer::timeout,&app,[&] {
        const auto label=dialog ? dialog->findChild<QLabel *>("Status") : nullptr;
        fail("deadline step "+QString::number(step)+": "+(label ? label->text() : QString()));
    });deadline.start(210000);
    QObject::connect(&poll,&QTimer::timeout,&app,[&] {
        if(finished || confirming)return;
        if(!dialog) {
            auto progress=window.findChild<QPushButton *>("Progress");
            if(!progress || !progress->isEnabled())return;
            action->trigger();dialog=window.findChild<CoBoMaintenanceWindow *>("NativeCoBoMaintenanceWindow");
            if(!dialog){fail("production maintenance window missing");return;}
        }
        auto submit=dialog->findChild<QPushButton *>("Submit");
        auto target=dialog->findChild<QComboBox *>("Target");
        auto status=dialog->findChild<QLabel *>("Status");
        const auto requests=config.nativeRecordDir+"/requests";
        if(!submit || !target || !status){fail("window controls missing");return;}
        if(step==0) {
            if(!submit->isEnabled())return;
            if(target->count()!=2 || target->findText("C0")<0 || target->findText("C1")<0) {
                fail("fixed active target set differs");return;
            }
            target->setCurrentIndex(target->findText("C0"));confirming=true;
            QTimer::singleShot(0,[&] {
                auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                if(box){cancelDefault=box->defaultButton()==box->button(QMessageBox::Cancel);box->button(QMessageBox::Cancel)->click();}
            });submit->click();
            if(!cancelDefault || submissions || !QDir(config.nativeRecordDir+"/requests").entryList(QDir::Files|QDir::NoDotAndDotDot).isEmpty()) {
                confirming=false;fail("cancel submitted or persisted an intent");return;
            }
            QTimer::singleShot(0,[] {
                auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                if(box)box->button(QMessageBox::Yes)->click();
            });submit->click();confirming=false;
            if(submissions!=1 || submit->isEnabled()){fail("confirmation was not single submission");return;}
            step=1;return;
        }
        bool allDisabled=true;
        for(const auto &name:{"Progress","Regress","StartRun","StopRun"}) {
            auto button=window.findChild<QPushButton *>(name);allDisabled=allDisabled && button && !button->isEnabled();
        }
        if(step==1 && !status->text().startsWith("CoBo C0 maintenance completed")) {
            busyObserved=busyObserved || allDisabled;return;
        }
        if(step==1) {
            if(submissions!=1 || !busyObserved){fail("missing reprepare busy exclusion or duplicate submission");return;}
            const auto names=QDir(config.nativeRecordDir+"/requests").entryList({"*.json"},QDir::Files);
            if(names.size()!=1){fail("expected one durable reprepare intent");return;}
            const auto bytes=Read(config.nativeRecordDir+"/requests/"+names.front());
            try {
                const auto request=GetDAQ::DecodeEccControlRequest(bytes.toStdString());
                if(request.command!="reprepare-cobo" || request.coboId!="C0" ||
                    request.instanceId!=instance.toStdString() || request.snapshotSha256!=snapshot.toStdString()) {
                    fail("reprepare intent differs from original reviewed target/identity");return;
                }
            }catch(const std::exception &e){fail(QString::fromUtf8(e.what()));return;}
            if(!Save(output+"/request.json",bytes)){fail("cannot save actual GUI reprepare intent evidence");return;}
            auto setLed=dialog->findChild<QPushButton *>("SetLed");
            if(!setLed || !setLed->isEnabled())return;
            step=2;return;
        }
        if(step==2) {
            auto setLed=dialog->findChild<QPushButton *>("SetLed");
            auto asad=dialog->findChild<QComboBox *>("LedAsAd");
            auto type=dialog->findChild<QComboBox *>("LedType");
            auto state=dialog->findChild<QComboBox *>("LedState");
            if(!setLed || !asad || !type || !state || !setLed->isEnabled() || asad->findData(3)<0 || type->findData(2)<0 || state->findData(3)<0) {
                fail("LED target/type/state controls are missing");return;
            }
            asad->setCurrentIndex(asad->findData(3));type->setCurrentIndex(type->findData(2));state->setCurrentIndex(state->findData(3));
            confirming=true;
            QTimer::singleShot(0,[&] {
                auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                if(box){cancelDefault=box->defaultButton()==box->button(QMessageBox::Cancel);box->button(QMessageBox::Cancel)->click();}
            });setLed->click();
            if(!cancelDefault || ledSubmissions || QDir(requests).entryList({"*.json"},QDir::Files).size()!=1) {
                confirming=false;fail("cancelled LED confirmation submitted or persisted an intent");return;
            }
            QTimer::singleShot(0,[] {
                auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                if(box)box->button(QMessageBox::Yes)->click();
            });setLed->click();confirming=false;
            bool commandsDisabled=true;
            for(const auto &name:{"Progress","Regress","StartRun","StopRun"}) {
                auto button=window.findChild<QPushButton *>(name);commandsDisabled=commandsDisabled && button && !button->isEnabled();
            }
            QElapsedTimer busyWait;busyWait.start();confirming=true;
            while(!commandsDisabled && busyWait.elapsed()<400) {
                QApplication::processEvents(QEventLoop::AllEvents,20);
                commandsDisabled=true;
                for(const auto &name:{"Progress","Regress","StartRun","StopRun"}) {
                    auto button=window.findChild<QPushButton *>(name);commandsDisabled=commandsDisabled && button && !button->isEnabled();
                }
            }
            confirming=false;ledBusyObserved=commandsDisabled;
            if(ledSubmissions!=1 || setLed->isEnabled() || !commandsDisabled){
                fail(QString("LED busy exclusion: submissions=%1 buttonEnabled=%2 runControlsDisabled=%3")
                    .arg(ledSubmissions).arg(setLed->isEnabled()).arg(commandsDisabled));return;
            }
            step=3;return;
        }
        if(step==3 && !status->text().startsWith("LED command acknowledged")) {
            ledBusyObserved=ledBusyObserved || allDisabled;return;
        }
        if(step==3) {
            auto trigger=dialog->findChild<QPushButton *>("TriggerPulser");
            auto voltage=dialog->findChild<QSpinBox *>("PulserVoltage");
            if(!trigger || !voltage) {fail("single-pulse controls are missing in healthy Prepared");return;}
            QElapsedTimer readyWait;readyWait.start();
            while(!trigger->isEnabled() && readyWait.elapsed()<2000)QApplication::processEvents(QEventLoop::AllEvents,20);
            if(!trigger->isEnabled()) {fail("single-pulse controls did not re-enable after the prior maintenance result and fresh status");return;}
            voltage->setValue(321);confirming=true;
            QTimer::singleShot(0,[&] {
                auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                if(box){cancelDefault=box->defaultButton()==box->button(QMessageBox::Cancel);box->button(QMessageBox::Cancel)->click();}
            });trigger->click();
            if(!cancelDefault || pulserSubmissions || QDir(requests).entryList({"*.json"},QDir::Files).size()!=2) {
                confirming=false;fail("cancelled pulser confirmation submitted or persisted an intent");return;
            }
            QTimer::singleShot(0,[] {
                auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                if(box)box->button(QMessageBox::Yes)->click();
            });trigger->click();confirming=false;
            bool commandsDisabled=true;
            for(const auto &name:{"Progress","Regress","StartRun","StopRun"}) {
                auto button=window.findChild<QPushButton *>(name);commandsDisabled=commandsDisabled && button && !button->isEnabled();
            }
            QElapsedTimer busyWait;busyWait.start();confirming=true;
            while(!commandsDisabled && busyWait.elapsed()<400) {
                QApplication::processEvents(QEventLoop::AllEvents,20);commandsDisabled=true;
                for(const auto &name:{"Progress","Regress","StartRun","StopRun"}) {
                    auto button=window.findChild<QPushButton *>(name);commandsDisabled=commandsDisabled && button && !button->isEnabled();
                }
            }
            confirming=false;pulserBusyObserved=commandsDisabled;
            if(pulserSubmissions!=1 || trigger->isEnabled() || !commandsDisabled) {fail("pulser busy exclusion or single submission failed");return;}
            step=4;return;
        }
        if(step==4 && !status->text().startsWith("One pulser trigger acknowledged")) {
            pulserBusyObserved=pulserBusyObserved || allDisabled;return;
        }
        if(step==4) {
            auto blink=dialog->findChild<QPushButton *>("BlinkLeds");
            if(!blink){fail("one-shot LED blink control is missing in healthy Prepared");return;}
            QElapsedTimer readyWait;readyWait.start();
            while(!blink->isEnabled() && readyWait.elapsed()<2000)QApplication::processEvents(QEventLoop::AllEvents,20);
            if(!blink->isEnabled()){fail("blink control did not re-enable after pulser completion and fresh status");return;}
            confirming=true;
            QTimer::singleShot(0,[&] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box){cancelDefault=box->defaultButton()==box->button(QMessageBox::Cancel);box->button(QMessageBox::Cancel)->click();}});blink->click();
            if(!cancelDefault || blinkSubmissions || QDir(requests).entryList({"*.json"},QDir::Files).size()!=3) {confirming=false;fail("cancelled blink confirmation submitted or persisted an intent");return;}
            QTimer::singleShot(0,[] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});blink->click();confirming=false;
            bool commandsDisabled=true;for(const auto &name:{"Progress","Regress","StartRun","StopRun"}){auto button=window.findChild<QPushButton *>(name);commandsDisabled=commandsDisabled && button && !button->isEnabled();}
            QElapsedTimer busyWait;busyWait.start();confirming=true;
            while(!commandsDisabled && busyWait.elapsed()<400){QApplication::processEvents(QEventLoop::AllEvents,20);commandsDisabled=true;for(const auto &name:{"Progress","Regress","StartRun","StopRun"}){auto button=window.findChild<QPushButton *>(name);commandsDisabled=commandsDisabled && button && !button->isEnabled();}}
            confirming=false;blinkBusyObserved=commandsDisabled;
            if(blinkSubmissions!=1 || blink->isEnabled() || !commandsDisabled){fail("blink busy exclusion or single submission failed");return;}
            step=5;return;
        }
        if(step==5 && !status->text().startsWith("Sequential one-shot LED blink acknowledged")) {blinkBusyObserved=blinkBusyObserved || allDisabled;return;}
        if(step==5) {
            auto flip=dialog->findChild<QPushButton *>("FlipLed");auto bits=dialog->findChild<QComboBox *>("LedFlipTarget");
            if(!flip || !bits || bits->findData(10)<0){fail("single LED flip controls/bit mapping missing");return;}
            QElapsedTimer readyWait;readyWait.start();
            while(!flip->isEnabled() && readyWait.elapsed()<2000)QApplication::processEvents(QEventLoop::AllEvents,20);
            if(!flip->isEnabled()){fail("flip control did not re-enable after blink completion and fresh status");return;}
            bits->setCurrentIndex(bits->findData(10));confirming=true;
            QTimer::singleShot(0,[&] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box){cancelDefault=box->defaultButton()==box->button(QMessageBox::Cancel);box->button(QMessageBox::Cancel)->click();}});flip->click();
            if(!cancelDefault || flipSubmissions || QDir(requests).entryList({"*.json"},QDir::Files).size()!=4){confirming=false;fail("cancelled non-idempotent flip submitted or persisted intent");return;}
            QTimer::singleShot(0,[] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});flip->click();confirming=false;
            bool disabled=true;for(const auto &name:{"Progress","Regress","StartRun","StopRun"}){auto button=window.findChild<QPushButton *>(name);disabled=disabled && button && !button->isEnabled();}
            QElapsedTimer wait;wait.start();confirming=true;
            while(!disabled && wait.elapsed()<400){QApplication::processEvents(QEventLoop::AllEvents,20);disabled=true;for(const auto &name:{"Progress","Regress","StartRun","StopRun"}){auto button=window.findChild<QPushButton *>(name);disabled=disabled && button && !button->isEnabled();}}
            confirming=false;flipBusyObserved=disabled;
            if(flipSubmissions!=1 || flip->isEnabled() || !disabled){fail(QString("flip busy exclusion or single submission failed: submissions=%1 buttonEnabled=%2 runControlsDisabled=%3").arg(flipSubmissions).arg(flip->isEnabled()).arg(disabled));return;}
            step=6;return;
        }
        if(step==6 && !status->text().startsWith("LED state inversion acknowledged")){flipBusyObserved=flipBusyObserved || allDisabled;return;}
        if(step==6) {
            auto pulse=dialog->findChild<QPushButton *>("PulseLed");auto bits=dialog->findChild<QComboBox *>("LedFlipTarget");auto period=dialog->findChild<QSpinBox *>("LedPulsePeriodMs");
            if(!pulse || !bits || !period || bits->findData(10)<0){fail("LED pulse controls missing");return;}
            QElapsedTimer readyWait;readyWait.start();while(!pulse->isEnabled() && readyWait.elapsed()<2000)QApplication::processEvents(QEventLoop::AllEvents,20);
            if(!pulse->isEnabled()){fail("LED pulse did not re-enable after flip completion and fresh status");return;}
            bits->setCurrentIndex(bits->findData(10));period->setValue(725);confirming=true;
            QTimer::singleShot(0,[&]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box){cancelDefault=box->defaultButton()==box->button(QMessageBox::Cancel);box->button(QMessageBox::Cancel)->click();}});pulse->click();
            if(!cancelDefault || pulseSubmissions || QDir(requests).entryList({"*.json"},QDir::Files).size()!=5){confirming=false;fail("cancelled LED pulse submitted or persisted an intent");return;}
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});pulse->click();confirming=false;
            bool disabled=true;for(const auto &name:{"Progress","Regress","StartRun","StopRun"}){auto button=window.findChild<QPushButton *>(name);disabled=disabled && button && !button->isEnabled();}
            QElapsedTimer wait;wait.start();confirming=true;while(!disabled && wait.elapsed()<400){QApplication::processEvents(QEventLoop::AllEvents,20);disabled=true;for(const auto &name:{"Progress","Regress","StartRun","StopRun"}){auto button=window.findChild<QPushButton *>(name);disabled=disabled && button && !button->isEnabled();}}
            confirming=false;pulseBusyObserved=disabled;
            if(pulseSubmissions!=1 || pulse->isEnabled() || !disabled){fail("LED pulse busy exclusion or single submission failed");return;}
            step=7;return;
        }
        if(step==7 && !status->text().startsWith("LED pulse period 725 ms acknowledged")){pulseBusyObserved=pulseBusyObserved || allDisabled;return;}
        if(step!=7 || submissions!=1 || ledSubmissions!=1 || pulserSubmissions!=1 || blinkSubmissions!=1 || flipSubmissions!=1 || pulseSubmissions!=1 || !busyObserved || !ledBusyObserved || !pulserBusyObserved || !blinkBusyObserved || !flipBusyObserved || !pulseBusyObserved ||
           submittedAsad!=3 || submittedType!=2 || submittedState!=3 || submittedFlipId!=10 || submittedPulseId!=10 || submittedPulsePeriod!=725) {
            fail("maintenance actions did not preserve busy exclusion, exact selection and one submission");return;
        }
        const auto names=QDir(config.nativeRecordDir+"/requests").entryList({"*.json"},QDir::Files);
        if(names.size()!=6){fail("expected exactly six durable GUI intents");return;}
        QByteArray ledBytes,pulserBytes,blinkBytes,flipBytes,pulseBytes;bool foundReprepare=false;
        try {
            for(const auto &name:names) {
                const auto bytes=Read(config.nativeRecordDir+"/requests/"+name);
                const auto request=GetDAQ::DecodeEccControlRequest(bytes.toStdString());
                if(request.command=="set-cobo-led") {
                    if(request.coboId!="C0" || request.ledAsadId!=3 || request.ledType!=2 || request.ledState!=3 ||
                       request.instanceId!=instance.toStdString() || request.snapshotSha256!=snapshot.toStdString())
                        throw std::runtime_error("LED intent differs from exact reviewed selection");
                    ledBytes=bytes;
                } else if(request.command=="trigger-asad-pulser") {
                    if(request.coboId!="C0" || request.pulserVoltageMv!=321 || request.instanceId!=instance.toStdString() || request.snapshotSha256!=snapshot.toStdString())
                        throw std::runtime_error("Pulser intent differs from exact reviewed selection");
                    pulserBytes=bytes;
                } else if(request.command=="blink-cobo-leds") {
                    if(request.coboId!="C0" || request.instanceId!=instance.toStdString() || request.snapshotSha256!=snapshot.toStdString())
                        throw std::runtime_error("LED blink intent differs from exact reviewed selection");
                    blinkBytes=bytes;
                } else if(request.command=="flip-cobo-led") {
                    if(request.coboId!="C0" || request.ledFlipId!=10 || request.instanceId!=instance.toStdString() || request.snapshotSha256!=snapshot.toStdString())
                        throw std::runtime_error("LED flip intent differs from exact reviewed GPIO bit");
                    flipBytes=bytes;
                } else if(request.command=="pulse-cobo-led") {
                    if(request.coboId!="C0" || request.ledPulseId!=10 || request.ledPulsePeriodMs!=725 || request.instanceId!=instance.toStdString() || request.snapshotSha256!=snapshot.toStdString())
                        throw std::runtime_error("LED pulse intent differs from exact reviewed selection");
                    pulseBytes=bytes;
                } else if(request.command=="reprepare-cobo" && request.coboId=="C0") foundReprepare=true;
            }
        }catch(const std::exception &e){fail(QString::fromUtf8(e.what()));return;}
        if(ledBytes.isEmpty() || pulserBytes.isEmpty() || blinkBytes.isEmpty() || flipBytes.isEmpty() || pulseBytes.isEmpty() || !foundReprepare || !Save(output+"/led-request.json",ledBytes) || !Save(output+"/pulser-request.json",pulserBytes) || !Save(output+"/blink-request.json",blinkBytes) || !Save(output+"/flip-request.json",flipBytes) || !Save(output+"/pulse-request.json",pulseBytes)) {
            fail("missing durable reprepare/LED/pulser/blink/flip/pulse intent evidence");return;
        }
        if(rawGuard && Read(QString::fromLocal8Bit(qgetenv("GETDAQ_RAW_READ_LOG")))!="raw-read-denied\n") {
            fail("GUI attempted raw reads beyond its explicit guard selfcheck");return;
        }
        const QJsonObject proof{{"passed",true},{"actual_main_window_and_runcontrol",true},
            {"cancel_submissions",0},{"cancel_intents",0},{"cancel_default",cancelDefault},
            {"confirmed_submissions",submissions},{"confirmed_led_submissions",ledSubmissions},{"confirmed_pulser_submissions",pulserSubmissions},{"confirmed_blink_submissions",blinkSubmissions},{"confirmed_flip_submissions",flipSubmissions},{"confirmed_pulse_submissions",pulseSubmissions},
            {"busy_control_buttons_disabled",busyObserved && ledBusyObserved && pulserBusyObserved && blinkBusyObserved && flipBusyObserved && pulseBusyObserved},
            {"led_asad_id",int(submittedAsad)},{"led_type",int(submittedType)},{"led_state",int(submittedState)},
            {"flip_led_id",int(submittedFlipId)},
            {"pulse_led_id",int(submittedPulseId)},{"pulse_period_ms",int(submittedPulsePeriod)},
            {"pulser_voltage_mv",submittedVoltage},
            {"complete_known_prepared",true},{"raw_transferred_by_gui",false},{"raw_read_guard_selfcheck",rawGuard}};
        if(!Save(output+"/window-proof.json",QJsonDocument(proof).toJson())){fail("cannot save window proof");return;}
        finished=true;poll.stop();deadline.stop();qInfo()<<"PASS production selected CoBo window reprepare, LED, pulser, blink, non-idempotent flip and arbitrary LED pulse Cancel/Yes, six intents, busy exclusion and known Prepared";app.exit(0);
    });poll.start(15);connectButton->click();return app.exec();
}
