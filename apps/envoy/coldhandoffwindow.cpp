#include "coldhandoffwindow.h"
#include "ui_coldhandoffwindow.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <stdexcept>
namespace {
void Require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
void Save(const QString &path,const QByteArray &bytes) {
    QSaveFile file(path);Require(file.open(QIODevice::WriteOnly) && file.setPermissions(QFile::ReadOwner|QFile::WriteOwner) && file.write(bytes)==bytes.size() && file.commit(),"Cannot save complete private metadata");
}
}
ColdHandoffWindow::ColdHandoffWindow(QWidget *parent) : QDialog(parent),ui(new Ui::ColdHandoffWindow),fHttp(new HttpClient(this)) {
    ui->setupUi(this);
    for(auto field:{ui->address,ui->credential,ui->service})connect(field,&QLineEdit::textChanged,this,&ColdHandoffWindow::Invalidate);
    connect(ui->port,static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),this,&ColdHandoffWindow::Invalidate);
    connect(ui->check,&QPushButton::clicked,this,&ColdHandoffWindow::Check);
    connect(ui->submit,&QPushButton::clicked,this,[this]{Submit(false);});connect(ui->inspect,&QPushButton::clicked,this,[this]{Submit(true);});
    connect(ui->observe,&QPushButton::clicked,this,&ColdHandoffWindow::Observe);
    connect(ui->exportReceipt,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getSaveFileName(this,tr("Export original cold handoff receipt"),"handoff-receipt.json",tr("JSON (*.json)"));if(path.isEmpty())return;QString error;if(!SaveReceipt(path,&error))ui->status->setText(error);});
    connect(ui->loadIntent,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getOpenFileName(this,tr("Load saved original recovery intent"),{},tr("JSON (*.json)"));if(path.isEmpty())return;QString error;if(!LoadIntent(path,&error))ui->status->setText(error);});
    connect(ui->newAttempt,&QPushButton::clicked,this,[this]{if(fBusy)return;fPoll.stop();fHaveRequest=false;fRequest=GetDAQ::ColdHandoffRequest();fReceipt.clear();fReady=false;ui->task->clear();ui->result->clear();ui->status->setText(tr("Check the same fixed recovery policy before confirming another attempt. Previous service records remain."));Controls();});
    fPoll.setInterval(2000);connect(&fPoll,&QTimer::timeout,this,&ColdHandoffWindow::Observe);Controls();
}
ColdHandoffWindow::~ColdHandoffWindow(){fPoll.stop();fHttp->Cancel();delete ui;}
QUrl ColdHandoffWindow::Url(const QString &path) const {QUrl url;url.setScheme("http");url.setHost(ui->address->text().trimmed());url.setPort(ui->port->value());url.setPath(path);return url;}
QMap<QByteArray,QByteArray> ColdHandoffWindow::Headers() {
    Require(!fCredentialFault,"Recovery credential fault; reselect endpoint before continuing");
    QFile file(ui->credential->text());const QFileInfo info(file);
    if(!info.isFile() || info.isSymLink() || !file.open(QIODevice::ReadOnly) || file.size()>66){fCredentialFault=true;throw std::runtime_error("Cannot read bounded recovery credential file");}
    auto token=file.readAll();if(token.endsWith('\n')){token.chop(1);if(token.endsWith('\r'))token.chop(1);}
    if(token.size()!=64 || QString::fromLatin1(token).contains(QRegularExpression("[^0-9a-f]")) || (!fCredential.isEmpty() && fCredential!=token)){fCredentialFault=true;throw std::runtime_error("Recovery credential invalid or changed");}
    fCredential=token;return {{"Authorization","Bearer "+token}};
}
void ColdHandoffWindow::Controls() {
    const bool fresh=fReady && fFresh.isValid() && fFresh.elapsed()<30000;
    ui->check->setEnabled(!fBusy);ui->submit->setEnabled(!fBusy && fresh && !fHaveRequest);ui->inspect->setEnabled(!fBusy && fresh);
    ui->observe->setEnabled(!fBusy);ui->loadIntent->setEnabled(!fBusy);ui->newAttempt->setEnabled(!fBusy);ui->exportReceipt->setEnabled(!fReceipt.isEmpty());
    ui->task->setReadOnly(fHaveRequest);ui->attempt->setReadOnly(fBusy);
}
void ColdHandoffWindow::Invalidate() {
    ++fGeneration;fHttp->Cancel();fPoll.stop();fBusy=fReady=false;fCredential.clear();fCredentialFault=false;fReceipt.clear();fIdentity=GetDAQ::ColdHandoffIdentity();
    ui->identity->clear();ui->result->clear();ui->status->setText(tr("Endpoint changed. Saved intent is retained; check identity before observing. No retry is sent."));Controls();
}
void ColdHandoffWindow::Check() {
    if(fBusy)return;
    fBusy=true;fReady=false;Controls();const auto generation=fGeneration;
    try {
        const auto headers=Headers();fHttp->Request(Url("/v1/cold-handoff/identity"),{},"",6000,[this,generation](const QByteArray &body,const QString &error){
            if(generation!=fGeneration)return;
            fBusy=false;
            try {
                Require(error.isEmpty(),error.toStdString().c_str());const auto identity=GetDAQ::DecodeColdHandoffIdentity(body.toStdString());
                Require(identity.serviceId==ui->service->text().trimmed().toStdString(),"Recovery service ID differs");
                if(fHaveRequest)Require(fRequest.policySha256==identity.policySha256,"Saved intent belongs to another fixed policy");
                fIdentity=identity;fFresh.restart();fReady=true;ui->identity->setPlainText(QString::fromUtf8(body));ui->status->setText(tr("Review the original operation, request SHA and successor plan SHA above. This service does not restore old hardware authority."));
            }catch(const std::exception &e){fReady=false;ui->status->setText(QString::fromUtf8(e.what()));}Controls();
        },headers,8*1024*1024);
    }catch(const std::exception &e){fBusy=false;ui->status->setText(QString::fromUtf8(e.what()));Controls();}
}
void ColdHandoffWindow::Submit(bool inspect) {
    Controls();if(fBusy || !fReady || fFresh.elapsed()>=30000 || (!inspect && fHaveRequest))return;
    const auto generation=fGeneration;const auto identity=GetDAQ::EncodeColdHandoffIdentity(fIdentity);const auto attempt=ui->attempt->text().trimmed();
    if(QMessageBox::question(this,inspect ? tr("Inspect original cold attempt") : tr("Complete original data handoff"),
        inspect ? tr("Inspect the saved completion of attempt %1? This creates a separate read-only task, sends no node POST and may re-export the same receipt. It cannot complete unfinished work.").arg(attempt) :
        tr("Submit one explicit cold DATA completion for original operation %1?\nOriginal request SHA: %2\nSuccessor plan SHA: %3\nOnly original child requests may be completed. Original failures and the old hardware fence remain. New services and the next ECC still require explicit startup.")
            .arg(QString::fromStdString(fIdentity.originalOperationId),QString::fromStdString(fIdentity.originalRequestSha256),QString::fromStdString(fIdentity.successorPlanSha256)),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    if(fBusy || !fReady || (!inspect && fHaveRequest) || generation!=fGeneration || identity!=GetDAQ::EncodeColdHandoffIdentity(fIdentity) || attempt!=ui->attempt->text().trimmed() || fFresh.elapsed()>=30000){ui->status->setText(tr("Review changed during confirmation; check identity again."));return;}
    bool persisted=false;
    try {
        GetDAQ::ColdHandoffRequest request;request.taskId="cold-"+QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();request.instanceId=fIdentity.instanceId;request.policySha256=fIdentity.policySha256;request.action=inspect ? "inspect" : "submit";request.attemptId=inspect ? attempt.toStdString() : request.taskId;
        const auto bytes=QByteArray::fromStdString(GetDAQ::EncodeColdHandoffRequest(request));const auto headers=Headers();
        const auto directory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/cold-handoff-intents";
        Require(QDir().mkpath(directory),"Cannot create local recovery intent directory");const auto path=directory+'/'+QString::fromStdString(request.taskId)+".json";
        Require(!QFileInfo::exists(path),"Recovery intent already exists");Save(path,bytes);persisted=true;
        fRequest=request;fHaveRequest=true;fBusy=true;fReceipt.clear();ui->task->setText(QString::fromStdString(request.taskId));ui->attempt->setText(QString::fromStdString(request.attemptId));Controls();
        ui->status->setText(tr("Original intent saved: %1. Submit once; observe this task ID if the reply is lost.").arg(path));
        fHttp->Request(Url("/v1/cold-handoff/tasks"),bytes,"application/json",6000,[this,generation,request](const QByteArray &body,const QString &error){if(generation!=fGeneration)return;
            fBusy=false;Show(body,error,QString::fromStdString(request.taskId));},headers,8*1024*1024);
    }catch(const std::exception &e){fBusy=false;if(persisted)fHaveRequest=true;ui->status->setText(QString::fromUtf8(e.what()));Controls();}
}
void ColdHandoffWindow::Show(const QByteArray &body,const QString &error,const QString &taskId) {
    if(!error.isEmpty()){fReceipt.clear();fPoll.stop();ui->status->setText(error+tr(" Preserve the saved task ID; observation never resubmits. A 404 is not permission to retry."));Controls();return;}
    try {
        const auto operation=GetDAQ::DecodeColdHandoffOperation(body.toStdString());
        Require(operation.request.taskId==taskId.toStdString() && operation.request.policySha256==fIdentity.policySha256,"Recovery response task/policy differs");
        if(fHaveRequest)Require(operation.requestKey==GetDAQ::ColdHandoffRequestKey(fRequest),"Recovery response differs from saved original intent");
        fRequest=operation.request;fHaveRequest=true;ui->task->setText(taskId);ui->attempt->setText(QString::fromStdString(operation.request.attemptId));ui->result->setPlainText(QString::fromUtf8(body));
        fReceipt=QByteArray::fromStdString(operation.receipt);ui->status->setText(QString::fromStdString(operation.phase+": "+operation.message));
        if(operation.phase=="accepted" || operation.phase=="running")fPoll.start();else fPoll.stop();
    }catch(const std::exception &e){fReceipt.clear();fPoll.stop();ui->status->setText(QString::fromUtf8(e.what()));}Controls();
}
void ColdHandoffWindow::Observe() {
    if(fBusy || !fReady)return;
    const auto task=ui->task->text().trimmed();if(task.isEmpty())return;
    try {
        const auto headers=Headers();const auto generation=fGeneration;fBusy=true;Controls();
        fHttp->Request(Url("/v1/cold-handoff/tasks/"+task),{},"",6000,[this,generation,task](const QByteArray &body,const QString &error){if(generation!=fGeneration)return;
            fBusy=false;Show(body,error,task);},headers,8*1024*1024);
    }catch(const std::exception &e){fBusy=false;ui->status->setText(QString::fromUtf8(e.what()));Controls();}
}
bool ColdHandoffWindow::SaveReceipt(const QString &path,QString *error) const {
    try {Require(!fReceipt.isEmpty(),"No checked complete receipt available");Save(path,fReceipt);return true;}catch(const std::exception &e){if(error)*error=QString::fromUtf8(e.what());return false;}
}
bool ColdHandoffWindow::LoadIntent(const QString &path,QString *error) {
    try {
        Require(!fBusy,"Wait for current observation");QFile file(path);Require(!QFileInfo(file).isSymLink() && file.open(QIODevice::ReadOnly) && file.size()<=8192,"Cannot read bounded saved intent");
        const auto request=GetDAQ::DecodeColdHandoffRequest(file.readAll().toStdString());
        if(fReady)Require(request.policySha256==fIdentity.policySha256,"Saved intent belongs to another policy");
        fPoll.stop();fRequest=request;fHaveRequest=true;fReceipt.clear();ui->task->setText(QString::fromStdString(request.taskId));ui->attempt->setText(QString::fromStdString(request.attemptId));ui->result->clear();ui->status->setText(tr("Original intent loaded. Check the fixed service and observe; loading never POSTs."));Controls();return true;
    }catch(const std::exception &e){if(error)*error=QString::fromUtf8(e.what());return false;}
}
