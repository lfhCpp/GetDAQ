#include "transferwindow.h"
#include "ui_transferwindow.h"
#include "eccprotocol.h"
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <stdexcept>
TransferWindow::TransferWindow(QWidget *parent) : QDialog(parent),ui(new Ui::TransferWindow),fHttp(new HttpClient(this)) {
    ui->setupUi(this);Ready(false);
    connect(ui->check,&QPushButton::clicked,this,&TransferWindow::CheckSource);connect(ui->submit,&QPushButton::clicked,this,&TransferWindow::Submit);connect(ui->observe,&QPushButton::clicked,this,&TransferWindow::Observe);
    connect(ui->reconcile,&QPushButton::clicked,this,&TransferWindow::Reconcile);
    connect(ui->resume,&QPushButton::clicked,this,&TransferWindow::Resume);
    for(auto field:{ui->address,ui->credential,ui->agent,ui->experiment})connect(field,&QLineEdit::textChanged,this,&TransferWindow::Invalidate);
    connect(ui->port,static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),this,&TransferWindow::Invalidate);connect(ui->run,static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),this,&TransferWindow::Invalidate);
    fPoll.setInterval(2000);connect(&fPoll,&QTimer::timeout,this,&TransferWindow::Observe);
}
TransferWindow::~TransferWindow(){fPoll.stop();fHttp->Cancel();delete ui;}
QUrl TransferWindow::Url(const QString &path) const {QUrl url;url.setScheme("http");url.setHost(ui->address->text().trimmed());url.setPort(ui->port->value());url.setPath(path);return url;}
void TransferWindow::Ready(bool ready) {fReady=ready;ui->submit->setEnabled(ready && !fBusy && !fHaveRequest);ui->check->setEnabled(!fBusy);ui->reconcile->setEnabled(!fBusy);ui->resume->setEnabled(!fBusy);}
void TransferWindow::Invalidate(){++fGeneration;fHttp->Cancel();fPoll.stop();fBusy=false;fCredential.clear();fCredentialFault=false;fHaveRequest=false;fIdentity=GetDAQ::TransferIdentity();fSourceKey.clear();Ready(false);ui->status->setText(tr("Endpoint or source changed. Check identity and source again; no copy is started."));}
QMap<QByteArray,QByteArray> TransferWindow::Headers() {
    if(fCredentialFault)throw std::runtime_error("Transfer credential fault; reselect endpoint before submitting");
    QFile file(ui->credential->text());if(!file.open(QIODevice::ReadOnly) || file.size()>4096) {fCredentialFault=true;throw std::runtime_error("Cannot read bounded transfer credential file");}
    const auto token=file.readAll().trimmed();if(token.size()<32 || token.contains('\n') || token.contains('\r') || (!fCredential.isEmpty() && fCredential!=token)){fCredentialFault=true;throw std::runtime_error("Transfer credential empty/invalid/changed");}
    fCredential=token;return {{"Authorization","Bearer "+token}};
}
void TransferWindow::CheckSource() {
    if(fBusy)return;
    Ready(false);fBusy=true;ui->check->setEnabled(false);const auto generation=fGeneration;
    try {
        const auto headers=Headers();fHttp->Request(Url("/v1/transfers/identity"),{},"",6000,[this,generation](const QByteArray &body,const QString &error) {
            if(generation!=fGeneration)return;
            try {
                if(!error.isEmpty())throw std::runtime_error(error.toStdString());
                const auto identity=GetDAQ::DecodeTransferIdentity(body.toStdString());
                if(identity.agentId!=ui->agent->text().toStdString())throw std::runtime_error("Wrong data agent identity");
                fIdentity=identity;ui->identity->setPlainText(QString::fromUtf8(body));
                const auto path="/v1/transfers/source/"+ui->experiment->text()+"/"+QString::number(ui->run->value());
                fHttp->Request(Url(path),{},"",30000,[this,generation](const QByteArray &source,const QString &failure) {
                    if(generation!=fGeneration)return;
                    fBusy=false;
                    try {
                        if(!failure.isEmpty())throw std::runtime_error(failure.toStdString());
                        const auto receipt=GetDAQ::DecodeEccControlCatalogReceipt(source.toStdString());
                        const auto prefix=QString::fromStdString(fIdentity.sourceRoot)+"/"+ui->experiment->text()+"/run_"+QString("%1").arg(ui->run->value(),4,10,QChar('0'))+"/";
                        if(!QString::fromStdString(receipt.directory).startsWith(prefix))throw std::runtime_error("Source catalog outside selected fixed root/Run");
                        fSourceKey=QString::fromStdString(receipt.requestKey);ui->result->setPlainText(QString::fromUtf8(source));fFresh.start();Ready(true);ui->status->setText(tr("Source metadata checked. Confirm copy to the fixed data-host backup root; raw remains in the native child."));
                    }catch(const std::exception &e){Ready(false);ui->status->setText(QString::fromUtf8(e.what()));}
                },Headers());
            }catch(const std::exception &e){fBusy=false;Ready(false);ui->status->setText(QString::fromUtf8(e.what()));}
        },headers);
    }catch(const std::exception &e){fBusy=false;Ready(false);ui->status->setText(QString::fromUtf8(e.what()));}
}
void TransferWindow::Submit() {
    if(fBusy || !fReady || fHaveRequest || !fFresh.isValid() || fFresh.elapsed()>10000)return;
    const auto answer=QMessageBox::question(this,tr("Start native data-host copy"),tr("Copy the selected completed catalog on agent %1 to its fixed backup root %2?\nThe source is retained. This does not complete or recover an ECC Run.").arg(ui->agent->text(),QString::fromStdString(fIdentity.backupRoot)),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel);
    if(answer!=QMessageBox::Yes || !fReady || fFresh.elapsed()>10000)return;
    try {
        fRequest.operationId=("copy-"+QUuid::createUuid().toString(QUuid::WithoutBraces)).toStdString();fRequest.instanceId=fIdentity.instanceId;fRequest.policySha256=fIdentity.policySha256;fRequest.experiment=ui->experiment->text().toStdString();fRequest.runNumber=unsigned(ui->run->value());fRequest.sourceRequestKey=fSourceKey.toStdString();
        const auto bytes=QByteArray::fromStdString(GetDAQ::EncodeTransferRequest(fRequest));const auto directory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/transfer-intents";
        if(!QDir().mkpath(directory))throw std::runtime_error("Cannot create local transfer intent directory");
        QSaveFile file(directory+"/"+QString::fromStdString(fRequest.operationId)+".json");file.setDirectWriteFallback(false);
        QJsonObject intent;intent["address"]=ui->address->text();intent["port"]=ui->port->value();intent["agent"]=ui->agent->text();intent["request"]=QString::fromUtf8(bytes);const auto journal=QJsonDocument(intent).toJson();
        if(!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner) || file.write(journal)!=journal.size() || !file.commit())throw std::runtime_error("Cannot save local transfer intent; no submission sent");
        const auto headers=Headers();fHaveRequest=true;ui->operation->setText(QString::fromStdString(fRequest.operationId));fBusy=true;Ready(false);const auto generation=fGeneration;
        fHttp->Request(Url("/v1/transfers/operations"),bytes,"application/json",6000,[this,generation](const QByteArray &body,const QString &error){if(generation==fGeneration)Result(body,error,true);},headers);
    }catch(const std::exception &e){fBusy=false;Ready(false);ui->status->setText(QString::fromUtf8(e.what()));}
}
void TransferWindow::Observe() {
    if(fBusy || ui->operation->text().isEmpty())return;
    fBusy=true;
    if(fIdentity.instanceId.empty()){fBusy=false;ui->status->setText(tr("Check the expected agent identity before observing an original task."));return;}
    try {const auto generation=fGeneration;const auto path="/v1/transfers/operations/"+ui->operation->text();fHttp->Request(Url(path),{},"",6000,[this,generation](const QByteArray &body,const QString &error){if(generation==fGeneration)Result(body,error,false);},Headers());}
    catch(const std::exception &e){fBusy=false;ui->status->setText(QString::fromUtf8(e.what()));}
}
void TransferWindow::Reconcile(){Recover(false);}
void TransferWindow::Resume(){Recover(true);}
void TransferWindow::Recover(bool resume) {
    if(fBusy || ui->operation->text().isEmpty())return;
    fPoll.stop();fBusy=true;Ready(false);const auto generation=fGeneration;
    try {
        fHttp->Request(Url("/v1/transfers/identity"),{},"",6000,[this,generation,resume](const QByteArray &body,const QString &error){
            if(generation!=fGeneration)return;
            try {
                if(!error.isEmpty())throw std::runtime_error(error.toStdString());
                const auto identity=GetDAQ::DecodeTransferIdentity(body.toStdString());
                if(identity.agentId!=ui->agent->text().toStdString())throw std::runtime_error("Verification agent identity differs");
                fIdentity=identity;
                fHttp->Request(Url("/v1/transfers/operations/"+ui->operation->text()),{},"",6000,[this,generation,identity,resume](const QByteArray &bytes,const QString &failure){
                    if(generation!=fGeneration)return;
                    try {
                        if(!failure.isEmpty())throw std::runtime_error(failure.toStdString());
                        const auto operation=GetDAQ::DecodeTransferOperation(bytes.toStdString());
                        if(operation.request.operationId!=ui->operation->text().toStdString() || operation.request.policySha256!=identity.policySha256 || (operation.phase!="unknown" && !(resume && operation.phase=="failed")) || operation.source.compare(0,identity.sourceRoot.size()+1,identity.sourceRoot+'/')!=0 || operation.destination.compare(0,identity.backupRoot.size()+1,identity.backupRoot+'/')!=0)throw std::runtime_error("Original transfer is not an unresolved task in this policy");
                        ui->result->setPlainText(QString::fromUtf8(bytes));
                        const auto prompt=resume ? tr("Check all existing prefixes and append missing bytes to this original destination on agent %1?\n%2\n\nTask: %3\nOriginal request SHA256: %4\nThe original source must be online and unchanged. Existing files will not be truncated. This may take a long time and does not change the ECC Run.") : tr("Read and hash every file of this original destination on agent %1?\n%2\n\nTask: %3\nOriginal request SHA256: %4\nThe native verifier may take a long time. It will not copy again, overwrite files, stop an old process or change the ECC Run.");
                        const auto answer=QMessageBox::question(this,resume ? tr("Resume original partial copy") : tr("Verify original copy"),prompt.arg(ui->agent->text(),QString::fromStdString(operation.destination),QString::fromStdString(operation.request.operationId),QString::fromStdString(operation.requestKey)),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel);
                        if(generation!=fGeneration)return;
                        if(operation.request.operationId!=ui->operation->text().toStdString())throw std::runtime_error("Original task selection changed during confirmation");
                        if(answer!=QMessageBox::Yes){fBusy=false;Ready(false);ui->status->setText(tr("Verification cancelled; original task and data are unchanged."));return;}
                        GetDAQ::TransferReconciliationRequest request;request.verificationId=((resume ? "resume-" : "verify-")+QUuid::createUuid().toString(QUuid::WithoutBraces)).toStdString();request.operationId=operation.request.operationId;request.instanceId=identity.instanceId;request.policySha256=identity.policySha256;request.originalRequestKey=operation.requestKey;
                        const auto payload=QByteArray::fromStdString(resume ? GetDAQ::EncodeTransferResumeRequest(request) : GetDAQ::EncodeTransferReconciliationRequest(request));
                        const auto directory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/transfer-intents";
                        if(!QDir().mkpath(directory))throw std::runtime_error("Cannot save local verification intent");
                        QSaveFile file(directory+"/"+QString::fromStdString(request.verificationId)+".json");file.setDirectWriteFallback(false);
                        QJsonObject intent;intent["address"]=ui->address->text();intent["port"]=ui->port->value();intent["agent"]=ui->agent->text();intent["request"]=QString::fromUtf8(payload);const auto saved=QJsonDocument(intent).toJson();
                        if(!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner) || file.write(saved)!=saved.size() || !file.commit())throw std::runtime_error("Cannot persist verification intent; no POST sent");
                        fRequest=operation.request;fHaveRequest=true;
                        fHttp->Request(Url(resume ? "/v1/transfers/resumptions" : "/v1/transfers/reconciliations"),payload,"application/json",6000,[this,generation](const QByteArray &result,const QString &problem){if(generation==fGeneration)Result(result,problem,true);},Headers());
                    }catch(const std::exception &e){fBusy=false;Ready(false);ui->status->setText(QString::fromUtf8(e.what()));}
                },Headers());
            }catch(const std::exception &e){fBusy=false;Ready(false);ui->status->setText(QString::fromUtf8(e.what()));}
        },Headers());
    }catch(const std::exception &e){fBusy=false;Ready(false);ui->status->setText(QString::fromUtf8(e.what()));}
}
void TransferWindow::Result(const QByteArray &body,QString error,bool submission) {
    fBusy=false;Ready(false);
    try {
        if(!error.isEmpty())throw std::runtime_error(error.toStdString());
        const auto operation=GetDAQ::DecodeTransferOperation(body.toStdString());
        if(operation.request.operationId!=ui->operation->text().toStdString())throw std::runtime_error("Transfer operation ID differs");
        if(operation.request.policySha256!=fIdentity.policySha256 || operation.source.compare(0,fIdentity.sourceRoot.size()+1,fIdentity.sourceRoot+'/')!=0 || operation.destination.compare(0,fIdentity.backupRoot.size()+1,fIdentity.backupRoot+'/')!=0)throw std::runtime_error("Transfer observation belongs to another policy/root");
        if(fHaveRequest && (operation.requestKey!=GetDAQ::TransferRequestKey(fRequest) || operation.request.policySha256!=fIdentity.policySha256))throw std::runtime_error("Transfer response binding changed");
        ui->result->setPlainText(QString::fromUtf8(body));ui->status->setText(tr("Task %1; confirmed native-child copied bytes %2. Original Run/ECC status is unchanged.").arg(QString::fromStdString(operation.phase),QString::number(operation.copiedBytes)));
        if(operation.request.instanceId!=fIdentity.instanceId)ui->status->setText(ui->status->text()+tr("\nHistorical task from a previous service instance; no replay or process adoption."));
        if(operation.phase=="accepted" || operation.phase=="running" || operation.phase=="verifying")fPoll.start();else fPoll.stop();
    }catch(const std::exception &e){fPoll.stop();ui->status->setText(QString::fromUtf8(e.what())+(submission ? tr("\nSubmission outcome unknown. Observe this original ID; do not submit another copy automatically.") : tr("\nCurrent task observation unavailable; no restart or completion inferred.")));}
}
