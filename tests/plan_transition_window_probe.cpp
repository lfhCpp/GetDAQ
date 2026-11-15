#include "mainwindow.h"
#include "plantransitionwindow.h"
#include "runconfig.h"
#include "eccprotocol.h"
#include <QApplication>
#include <QAbstractButton>
#include <QAction>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
namespace {
bool Save(const QString &path,const QByteArray &bytes) {
    QFile file(path);return file.open(QIODevice::WriteOnly|QIODevice::NewOnly) && file.setPermissions(QFile::ReadOwner|QFile::WriteOwner) && file.write(bytes)==bytes.size() && file.flush();
}
QByteArray Read(const QString &path) {QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};return file.read(1024*1024+1);}
}
// Runs only under the loopback acquisition fixture. The GUI never opens raw.
int main(int argc,char **argv) {
    QApplication app(argc,argv);const bool nodeRepair=argc==6 && QByteArray(argv[5])=="--failed-node-completion";
    const bool partial=(argc==6 && QByteArray(argv[5])=="--partial-completion") || nodeRepair;if(argc!=5 && !partial)return 2;
    const QString configFile=QString::fromLocal8Bit(argv[1]),successor=QString::fromLocal8Bit(argv[2]),targets=QString::fromLocal8Bit(argv[3]),output=QString::fromLocal8Bit(argv[4]);
    const auto root=QFileInfo(configFile).absolutePath(),held=root+"/gui-retirement-held",release=root+"/gui-retirement-release";
    RunConfig config;QString error;if(!config.Load(configFile,&error) || !config.nativeEcc || config.nativeClient.address!="127.0.0.1")return 2;
    const bool rawGuard=!qgetenv("GETDAQ_RAW_READ_LOG").isEmpty();
    if(rawGuard) {
        const auto sample=root+"/gui-guard-selfcheck.graw";if(!Save(sample,"x"))return 2;
        QFile read(sample);const bool denied=!read.open(QIODevice::ReadOnly);read.close();const bool removed=QFile::remove(sample);
        if(!denied || !removed){qCritical()<<"GUI raw read guard did not deny its owned one-byte selfcheck";return 2;}
    }
    MainWindow window;if(!window.LoadConfig(configFile))return 2;window.show();
    auto connectButton=window.findChild<QPushButton *>("Connect");auto action=window.findChild<QAction *>("NativePlanTransition");if(!connectButton || !action)return 2;
    PlanTransitionWindow *dialog=nullptr;QTimer poll,deadline;deadline.setSingleShot(true);bool finished=false,confirming=false,leaseChecked=false;int step=0,submissions=0,completionSubmissions=0,completionObservations=0;bool cancelDefault=false,completionCancelDefault=false;
    const auto fail=[&](const QString &message) {
        if(finished)return;finished=true;poll.stop();deadline.stop();if(QFileInfo::exists(held) && !QFileInfo::exists(release))Save(release,"release after probe failure\n");
        qCritical().noquote()<<"FAIL plan transition window:"<<message;app.exit(1);
    };
    QObject::connect(&window,&MainWindow::RetireDataPlansSignal,&app,[&](const QByteArray &,const QByteArray &){
        ++submissions;
        if(nodeRepair && !Save(root+"/retirement-fault-armed","Qt submitted and durably saved the parent intent\n"))fail("cannot arm the accepted-child failure fixture");
    });
    QObject::connect(&window,&MainWindow::CompletePlanRetirementSignal,&app,[&](const QString &){++completionSubmissions;});
    QObject::connect(&window,&MainWindow::InspectPlanRetirementCompletionSignal,&app,[&]{++completionObservations;});
    const auto requests=[&] {return QDir(config.nativeRecordDir+"/requests").entryList({"*.json"},QDir::Files);};
    const auto savedRequest=[&](const std::string &command) {
        QByteArray found;
        for(const auto &name:requests()) {
            const auto bytes=Read(config.nativeRecordDir+"/requests/"+name);
            if(GetDAQ::DecodeEccControlRequest(bytes.toStdString()).command==command)found=bytes;
        }
        return found;
    };
    const auto terminalOperation=[&](const QByteArray &requestBytes,const QString &phase) {
        const auto id=QString::fromStdString(GetDAQ::DecodeEccControlRequest(requestBytes.toStdString()).operationId);
        QByteArray found;
        for(const auto &line:Read(config.nativeRecordDir+"/observations.jsonl").split('\n')) {
            if(line.isEmpty())continue;const auto record=QJsonDocument::fromJson(line).object();if(record.value("kind").toString()!="observation")continue;
            const auto payload=record.value("payload").toString().toUtf8();const auto operation=GetDAQ::DecodeEccControlOperation(payload.toStdString());
            if(QString::fromStdString(operation.request.operationId)==id && QString::fromStdString(operation.phase)==phase)found=payload;
        }
        return found;
    };
    QObject::connect(&deadline,&QTimer::timeout,&app,[&]{const auto label=dialog ? dialog->findChild<QLabel *>("Status") : nullptr;fail("deadline step "+QString::number(step)+": "+(label ? label->text() : QString()));});deadline.start(210000);
    QObject::connect(&poll,&QTimer::timeout,&app,[&] {
        if(finished || confirming)return;
        if(!dialog) {action->trigger();dialog=window.findChild<PlanTransitionWindow *>("NativePlanTransitionWindow");if(!dialog){fail("missing production review window");return;}}
        if(step==0) {
            QString reviewError;if(!dialog->LoadReview(successor,targets,&reviewError))return;
            auto submit=dialog->findChild<QPushButton *>("Submit");if(!submit || !submit->isEnabled())return;
            confirming=true;
            QTimer::singleShot(0,[&] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box){cancelDefault=box->defaultButton()==box->button(QMessageBox::Cancel);box->button(QMessageBox::Cancel)->click();}});
            submit->click();
            if(!cancelDefault || submissions || !QDir(config.nativeRecordDir+"/requests").entryList(QDir::Files|QDir::NoDotAndDotDot).isEmpty()){confirming=false;fail("cancel persisted or submitted a control intent");return;}
            QTimer::singleShot(0,[] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});
            submit->click();confirming=false;
            if(submissions!=1 || submit->isEnabled()){fail("confirmation did not emit exactly once");return;}step=1;return;
        }
        if(step==1 && QFileInfo::exists(held)) {
            const int fd=::open(successor.toUtf8().constData(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0){fail("cannot inspect successor test lock");return;}
            errno=0;const int locked=::flock(fd,LOCK_EX|LOCK_NB);const auto reason=errno;::close(fd);
            if(locked==0 || (reason!=EWOULDBLOCK && reason!=EAGAIN)){fail("original ECC did not hold the reviewed identity during retirement");return;}
            leaseChecked=true;if(!Save(release,"release original node reply\n")){fail("cannot release fixture barrier");return;}step=2;return;
        }
        if(partial && step==2) {
            auto complete=dialog->findChild<QPushButton *>("CompletePartial");if(!complete || !complete->isEnabled())return;
            const auto original=savedRequest("retire-data-plans"),failed=terminalOperation(original,"failed");
            if(failed.isEmpty() || requests().size()!=1){fail("partial parent was not durably observed failed");return;}
            if((!QFileInfo::exists(output+"/original-operation.json") && !Save(output+"/original-operation.json",failed)) ||
                (!QFileInfo::exists(output+"/original-status.json") && !Save(output+"/original-status.json",Read(config.nativeRecordDir+"/plan-retirement-context.json")))){fail("cannot save original partial evidence");return;}
            if(nodeRepair) {
                if(!QFileInfo::exists(root+"/gui-node-repair-request") && !Save(root+"/gui-node-repair-request",original)){fail("cannot request the separate operator node repair");return;}
                if(QFileInfo::exists(root+"/gui-node-repair-failed")){fail("explicit operator node repair failed");return;}
                if(!QFileInfo::exists(root+"/gui-node-repair-complete"))return;
                const auto repair=QJsonDocument::fromJson(Read(root+"/gui-node-repair-proof.json")).object();
                if(!repair.value("passed").toBool() || !repair.value("original_child_failure_retained").toBool() || !repair.value("parent_was_not_repaired").toBool()) {fail("operator node repair proof differs");return;}
            }
            confirming=true;QTimer::singleShot(0,[&] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box){completionCancelDefault=box->defaultButton()==box->button(QMessageBox::Cancel);box->button(QMessageBox::Cancel)->click();}});
            complete->click();
            if(!completionCancelDefault || completionSubmissions || requests().size()!=1){confirming=false;fail("completion cancel saved/submitted an intent");return;}
            QTimer::singleShot(0,[] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});complete->click();confirming=false;
            if(completionSubmissions!=1 || complete->isEnabled()){fail("first completion was not submitted once");return;}step=3;return;
        }
            if(partial && !nodeRepair && step==3) {
            auto complete=dialog->findChild<QPushButton *>("CompletePartial");if(!complete || !complete->isEnabled() || requests().size()!=2)return;
            const auto completion=savedRequest("complete-data-handoff"),failed=terminalOperation(completion,"failed");if(failed.isEmpty())return;
            if(!Save(output+"/first-completion-request.json",completion) || !Save(output+"/first-completion-operation.json",failed) || !Save(root+"/gui-completion-restore","Explicit failed completion observed; restore exact reviewed plan\n")){fail("cannot preserve first failed completion");return;}
            step=4;return;
        }
        if(partial && !nodeRepair && step==4) {
            auto complete=dialog->findChild<QPushButton *>("CompletePartial");if(!QFileInfo::exists(root+"/gui-completion-restored") || !complete->isEnabled())return;
            confirming=true;QTimer::singleShot(0,[] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});complete->click();confirming=false;
            if(completionSubmissions!=2 || complete->isEnabled()){fail("second explicit completion was not submitted once");return;}step=5;return;
        }
        if((!partial && step==2) || (partial && nodeRepair && step==3) || (partial && !nodeRepair && step==5)) {
            auto exportButton=dialog->findChild<QPushButton *>("ExportReceipt");if(!exportButton || !exportButton->isEnabled())return;
            for(const auto &name:{"Progress","Regress","StartRun","StopRun","Finalize","Recover"}) {
                auto button=window.findChild<QPushButton *>(name);if(!button || button->isEnabled())return;
            }
            QString failure;if(!dialog->SaveReceipt(output+"/receipt.json",&failure)){fail(failure);return;}
            const auto names=QDir(config.nativeRecordDir+"/requests").entryList({"*.json"},QDir::Files);
            if(names.size()!=(nodeRepair ? 2 : partial ? 3 : 1)){fail("GUI control intent count differs");return;}
            const auto bytes=savedRequest("retire-data-plans");
            try {
                const auto request=GetDAQ::DecodeEccControlRequest(bytes.toStdString());const auto identity=GetDAQ::DecodeEccSuccessorIdentity(Read(successor).toStdString());
                if(request.command!="retire-data-plans" || request.successorPlanFile!=identity.planFile || !leaseChecked){fail("saved GUI retirement identity differs");return;}
            }catch(const std::exception &e){fail(QString::fromUtf8(e.what()));return;}
            if(!Save(output+"/request.json",bytes)){fail("cannot save actual GUI request evidence");return;}
            if(partial) {
                QByteArray last;
                for(const auto &name:names) {const auto candidate=Read(config.nativeRecordDir+"/requests/"+name);if(GetDAQ::DecodeEccControlRequest(candidate.toStdString()).command=="complete-data-handoff" && !terminalOperation(candidate,"complete").isEmpty())last=candidate;}
                if(last.isEmpty() || !Save(output+"/completion-request.json",last) || !Save(output+"/completion-operation.json",terminalOperation(last,"complete"))){fail("missing completed explicit GUI operation");return;}
                if(terminalOperation(bytes,"failed").isEmpty() || !terminalOperation(bytes,"complete").isEmpty()){fail("completion rewrote original failed parent");return;}
                if(dialog->findChild<QPushButton *>("CompletePartial")->isEnabled()){fail("successful completion permits another completion");return;}
                auto inspect=dialog->findChild<QPushButton *>("InspectCompletion");if(!inspect->isEnabled())return;inspect->click();
                if(completionObservations!=1 || requests().size()!=(nodeRepair ? 2 : 3)){fail("observation created a new intent");return;}
            }
            if(rawGuard && Read(QString::fromLocal8Bit(qgetenv("GETDAQ_RAW_READ_LOG")))!="raw-read-denied\n"){fail("GUI attempted to read raw beyond the explicit guard selfcheck");return;}
            QJsonObject proof{{"passed",true},{"actual_main_window_and_runcontrol",true},{"cancel_submissions",0},{"cancel_intents",0},{"confirmed_submissions",submissions},
                {"server_review_lease_blocks_startup",leaseChecked},{"old_control_buttons_disabled",true},{"receipt_exported",true},{"raw_transferred_by_gui",false},{"raw_read_guard_selfcheck",rawGuard}};
            proof["partial_completion"]=partial;proof["completion_cancel_default"]=completionCancelDefault;proof["completion_confirmed_submissions"]=completionSubmissions;
            proof["operator_node_repair_confirmed"]=nodeRepair;
            proof["completion_observation_signals"]=completionObservations;proof["saved_original_pre_retirement_context"]=partial;proof["original_failure_retained"]=partial;
            if(!Save(output+"/window-proof.json",QJsonDocument(proof).toJson())){fail("cannot save GUI evidence");return;}
            finished=true;poll.stop();deadline.stop();qInfo()<<"PASS production plan review window, Cancel/Yes, original server lease, retirement receipt and old-control hold";app.exit(0);
        }
    });poll.start(15);connectButton->click();return app.exec();
}
