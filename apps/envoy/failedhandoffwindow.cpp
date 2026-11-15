#include "failedhandoffwindow.h"
#include "ui_failedhandoffwindow.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcess>
#include <QSaveFile>
#include <QRegularExpression>
#include <QUuid>
#include <stdexcept>

namespace {
void Require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
QJsonObject Object(const QByteArray &bytes){
    QJsonParseError error;const auto document=QJsonDocument::fromJson(bytes,&error);
    Require(error.error==QJsonParseError::NoError && document.isObject(),"Helper returned invalid JSON");
    return document.object();
}
QString Id(){return QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-');}
}

FailedHandoffWindow::FailedHandoffWindow(QWidget *parent,const QString &queryProgram)
    :QDialog(parent),ui(new Ui::FailedHandoffWindow),fQueryProgram(queryProgram){
    ui->setupUi(this);setWindowTitle(tr("Failed data-node handoff completion"));resize(980,760);
    if(fQueryProgram.isEmpty()){
#ifdef Q_OS_WIN
        fQueryProgram=QCoreApplication::applicationDirPath()+"/getdaq-supervisor-query.exe";
#else
        fQueryProgram=QCoreApplication::applicationDirPath()+"/getdaq-supervisor-query";
#endif
    }
    const QFileInfo helper(fQueryProgram);
    fQueryAvailable=helper.isFile() && !helper.isSymLink() && helper.isExecutable();
    ui->queryAvailability->setVisible(!fQueryAvailable);
    if(!fQueryAvailable)
        ui->queryAvailability->setText(tr("Data-node handoff recovery is unavailable: the native getdaq-supervisor-query tool is missing or not executable beside this application."));
    ui->newId->setText(Id());
    for(auto field:{ui->host,ui->agent,ui->controller,ui->plan,ui->credential,ui->original,ui->failedId,ui->newId,ui->output})
        connect(field,&QLineEdit::textChanged,this,&FailedHandoffWindow::InvalidateRequest);
    connect(ui->port,static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),this,&FailedHandoffWindow::InvalidateRequest);
    connect(ui->generate,&QPushButton::clicked,this,&FailedHandoffWindow::GenerateRequest);
    connect(ui->submit,&QPushButton::clicked,this,&FailedHandoffWindow::SubmitOnce);
    connect(ui->observe,&QPushButton::clicked,this,&FailedHandoffWindow::ObserveSavedOperation);
    connect(ui->load,&QPushButton::clicked,this,&FailedHandoffWindow::LoadSavedRequest);
    connect(ui->browseOriginal,&QPushButton::clicked,this,&FailedHandoffWindow::BrowseOriginalRequest);
    connect(ui->browseCredential,&QPushButton::clicked,this,&FailedHandoffWindow::BrowseCredential);
    connect(ui->browseOutput,&QPushButton::clicked,this,&FailedHandoffWindow::BrowseOutput);
    UpdateControls();
}

FailedHandoffWindow::~FailedHandoffWindow(){if(fProcess && fProcess->state()!=QProcess::NotRunning)fProcess->kill();delete ui;}

QStringList FailedHandoffWindow::CommonArguments() const {
    return {"--host",ui->host->text().trimmed(),"--port",QString::number(ui->port->value()),
        "--credential-file",ui->credential->text().trimmed(),"--expected-agent",ui->agent->text().trimmed(),
        "--controller",ui->controller->text().trimmed(),"--expected-plan",ui->plan->text().trimmed(),"--timeout-ms","15000"};
}

bool FailedHandoffWindow::ValidateInputs(QString *error) const {
    try{
        QHostAddress address;Require(address.setAddress(ui->host->text().trimmed()) && address.protocol()==QAbstractSocket::IPv4Protocol,"Enter the data service IPv4 address");
        Require(!ui->agent->text().trimmed().isEmpty() && !ui->controller->text().trimmed().isEmpty(),"Enter the expected data node and controller IDs");
        Require(QRegularExpression("^[0-9a-fA-F]{64}$").match(ui->plan->text().trimmed()).hasMatch(),"Expected plan must be a 64-digit SHA-256");
        const auto cred=QFileInfo(ui->credential->text().trimmed());
        Require(cred.isAbsolute() && cred.isFile() && !cred.isSymLink(),"Select an absolute regular credential file (not a symbolic link)");
        const auto original=QFileInfo(ui->original->text().trimmed());
        Require(original.isAbsolute() && original.isFile() && !original.isSymLink() && original.size()>0 && original.size()<=8192,"Select the saved original handoff request (regular file, at most 8 KiB)");
        const QRegularExpression id("^[A-Za-z0-9_-]{1,128}$");
        Require(id.match(ui->failedId->text().trimmed()).hasMatch(),"Original operation ID contains unsupported characters");
        Require(id.match(ui->newId->text().trimmed()).hasMatch() && ui->newId->text().trimmed()!=ui->failedId->text().trimmed(),"Completion operation ID must be valid and different from the failed ID");
        const auto output=QFileInfo(ui->output->text().trimmed());
        Require(output.isAbsolute() && !ui->output->text().trimmed().isEmpty(),"Choose an absolute private request output path");
        Require(!output.exists() && !output.isSymLink(),"Completion request output already exists; choose a new file so evidence is never overwritten");
        const QFileInfo helper(fQueryProgram);Require(helper.isFile() && !helper.isSymLink() && helper.isExecutable(),"getdaq-supervisor-query is missing or not executable beside GetDAQ");
        return true;
    }catch(const std::exception &e){if(error)*error=QString::fromUtf8(e.what());return false;}
}

bool FailedHandoffWindow::ValidateCompletion(const QByteArray &bytes,QString *error) const {
    try{
        Require(bytes.size()>0 && bytes.size()<=8192,"Completion request size is invalid");
        const auto request=Object(bytes);
        Require(request.value("schema").toInt()==1 && request.value("command").toString()=="complete-control-handoff","Request is not a native control-handoff completion");
        Require(request.value("operation_id").toString()==ui->newId->text().trimmed(),"Completion operation ID differs from the reviewed input");
        Require(request.value("failed_handoff_operation_id").toString()==ui->failedId->text().trimmed(),"Request does not reference the original failed handoff");
        Require(request.value("plan_sha256").toString().compare(ui->plan->text().trimmed(),Qt::CaseInsensitive)==0,"Completion plan differs from the expected live plan");
        Require(request.value("controller_id").toString()==ui->controller->text().trimmed(),"Completion controller differs from the expected controller");
        Require(QRegularExpression("^[0-9a-f]{64}$").match(request.value("instance_id").toString()).hasMatch(),"Request lacks a valid service instance binding");
        return true;
    }catch(const std::exception &e){if(error)*error=QString::fromUtf8(e.what());return false;}
}

void FailedHandoffWindow::InvalidateRequest(){
    if(fBusy || fSubmitConsumed)return;
    fGenerated=false;fCompletionRequest.clear();ui->requestView->clear();ui->resultView->clear();
    ui->status->setText(tr("Inputs changed. Generate and review a new request; no POST has been sent."));UpdateControls();
}

void FailedHandoffWindow::UpdateControls(){
    const bool valid=!fBusy && !fSubmitConsumed;
    ui->generate->setEnabled(valid && fQueryAvailable);
    ui->load->setEnabled(!fBusy);
    ui->submit->setEnabled(!fBusy && fGenerated && !fSubmitConsumed && fQueryAvailable);
    ui->observe->setEnabled(!fBusy && fGenerated && fQueryAvailable);
    for(auto field:{ui->host,ui->agent,ui->controller,ui->plan,ui->credential,ui->original,ui->failedId,ui->newId,ui->output})field->setEnabled(valid);
    ui->port->setEnabled(valid);ui->browseOriginal->setEnabled(valid);ui->browseCredential->setEnabled(valid);ui->browseOutput->setEnabled(valid);
}

void FailedHandoffWindow::StartQuery(const QString &mode,const QStringList &extra){
    if(fBusy)return;
    fBusy=true;fAction=mode;fProcessOutput.clear();UpdateControls();
    if(!fProcess)fProcess=new QProcess(this);
    fProcess->disconnect(this);
    fProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(fProcess,&QProcess::readyRead,this,[this]{fProcessOutput+=fProcess->readAll();});
    connect(fProcess,static_cast<void(QProcess::*)(int,QProcess::ExitStatus)>(&QProcess::finished),this,&FailedHandoffWindow::ProcessFinished);
    connect(fProcess,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error!=QProcess::FailedToStart)return;
        fBusy=false;
        ui->status->setText(fAction=="submit" ? tr("The submit helper could not start. This attempt is consumed; do not submit again. Use Observe to query the same operation ID.") : tr("The query helper could not start. Check the installed data tools."));
        UpdateControls();
    });
    QStringList args{mode};args+=CommonArguments();args+=extra;
    fProcess->start(fQueryProgram,args,QIODevice::ReadOnly);
    if(mode=="submit")fSubmitConsumed=true;
    UpdateControls();
}

void FailedHandoffWindow::GenerateRequest(){
    QString error;if(!ValidateInputs(&error)){ui->status->setText(error);return;}
    fGenerated=false;fCompletionRequest.clear();ui->resultView->clear();
    StartQuery("build-handoff-completion",{"--request-file",ui->original->text().trimmed(),"--failed-operation-id",ui->failedId->text().trimmed(),"--operation-id",ui->newId->text().trimmed()});
}

void FailedHandoffWindow::SubmitOnce(){
    QString error;if(!fGenerated || fSubmitConsumed || !ValidateCompletion(fCompletionRequest,&error)){ui->status->setText(error.isEmpty()?tr("Generate and review a completion request first."):error);return;}
    if(QMessageBox::question(this,tr("Submit failed handoff completion once"),
        tr("Submit this exact completion request once to the original live data service?\nOriginal failed operation: %1\nNew operation: %2\n\nThe original failure remains recorded. If the reply is lost, this window will only observe the new operation; it will not resubmit.")
            .arg(ui->failedId->text().trimmed(),ui->newId->text().trimmed()),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    StartQuery("submit",{"--request-file",ui->output->text().trimmed()});
}

void FailedHandoffWindow::ObserveSavedOperation(){
    QString error;if(!fGenerated || !ValidateCompletion(fCompletionRequest,&error)){ui->status->setText(error.isEmpty()?tr("Load or generate the saved completion request first."):error);return;}
    StartQuery("operation",{"--operation-id",ui->newId->text().trimmed()});
}

void FailedHandoffWindow::LoadSavedRequest(){
    const auto path=QFileDialog::getOpenFileName(this,tr("Load saved completion request"),{},tr("JSON (*.json)"));if(path.isEmpty())return;
    QString error;if(!LoadRequestFile(path,&error))ui->status->setText(error);
}

bool FailedHandoffWindow::LoadRequestFile(const QString &path,QString *error){
    QFile file(path);QFileInfo info(file);
    if(info.isSymLink() || !info.isFile() || info.size()>8192 || !file.open(QIODevice::ReadOnly)){
        if(error)*error=tr("Cannot read a bounded regular completion request");
        return false;
    }
    const auto bytes=file.readAll();
    try{
        const auto request=Object(bytes);
        ui->plan->setText(request.value("plan_sha256").toString());ui->controller->setText(request.value("controller_id").toString());
        ui->newId->setText(request.value("operation_id").toString());ui->failedId->setText(request.value("failed_handoff_operation_id").toString());
        QString reason;if(!ValidateCompletion(bytes,&reason))throw std::runtime_error(reason.toStdString());
        fCompletionRequest=bytes;fGenerated=true;fSubmitConsumed=true;ui->output->setText(info.absoluteFilePath());ui->requestView->setPlainText(QString::fromUtf8(bytes));
        ui->status->setText(fQueryAvailable
            ? tr("Loaded for read-only observation. A saved request is never resubmitted by this window; inspect the same operation ID.")
            : tr("Loaded for local review only. The native getdaq-supervisor-query tool is unavailable, so this application cannot observe or submit the operation."));
        UpdateControls();return true;
    }catch(const std::exception &e){fGenerated=false;fCompletionRequest.clear();if(error)*error=QString::fromUtf8(e.what());UpdateControls();return false;}
}

void FailedHandoffWindow::BrowseOriginalRequest(){const auto path=QFileDialog::getOpenFileName(this,tr("Select saved original handoff request"),{},tr("JSON (*.json)"));if(!path.isEmpty())ui->original->setText(path);}
void FailedHandoffWindow::BrowseCredential(){const auto path=QFileDialog::getOpenFileName(this,tr("Select private data service credential"));if(!path.isEmpty())ui->credential->setText(path);}
void FailedHandoffWindow::BrowseOutput(){const auto path=QFileDialog::getSaveFileName(this,tr("Save private completion request"),ui->output->text(),tr("JSON (*.json)"));if(!path.isEmpty())ui->output->setText(path);}

void FailedHandoffWindow::ProcessFinished(int exitCode,QProcess::ExitStatus status){
    fBusy=false;fProcessOutput+=fProcess->readAll();
    if(status!=QProcess::NormalExit || exitCode!=0){
        ui->resultView->setPlainText(QString::fromUtf8(fProcessOutput));
        ui->status->setText(fAction=="submit" ? tr("Submit result is not confirmed. Do not submit again; use Observe for the same operation ID. Original and completion evidence remain on the service.") : QString::fromUtf8(fProcessOutput));
        UpdateControls();return;
    }
    try{
        if(fAction=="build-handoff-completion"){
            QString error;Require(ValidateCompletion(fProcessOutput,&error),error.toStdString().c_str());
            const auto output=QFileInfo(ui->output->text().trimmed());Require(output.isAbsolute() && !output.exists() && !output.isSymLink(),"Output exists or is not an absolute new path");
            QSaveFile save(output.absoluteFilePath());Require(save.open(QIODevice::WriteOnly) && save.setPermissions(QFile::ReadOwner|QFile::WriteOwner) && save.write(fProcessOutput)==fProcessOutput.size() && save.commit(),"Cannot atomically save private completion request");
            fCompletionRequest=fProcessOutput;fGenerated=true;ui->requestView->setPlainText(QString::fromUtf8(fCompletionRequest));ui->status->setText(tr("Request validated against the original failure and live service. Review every field above; no POST has been sent."));
        }else{
            const auto result=Object(fProcessOutput);
            if(fAction=="operation"){
                Require(result.value("schema").toInt()==1 && result.value("operation_id").toString()==ui->newId->text().trimmed() &&
                    result.value("command").toString()=="complete-control-handoff","Observed operation differs from the saved completion request");
                const auto phase=result.value("phase").toString();Require(phase=="accepted" || phase=="running" || phase=="complete" || phase=="failed" || phase=="unknown","Observed operation phase is invalid");
                ui->status->setText(tr("Read-only observation returned phase: %1. Original failure and its hold remain recorded.").arg(phase));
            }else ui->status->setText(tr("Completion request accepted. Acceptance is not completion; use Observe for the same operation ID."));
            ui->resultView->setPlainText(QString::fromUtf8(fProcessOutput));
        }
    }catch(const std::exception &e){ui->status->setText(QString::fromUtf8(e.what()));if(!fProcessOutput.isEmpty())ui->resultView->setPlainText(QString::fromUtf8(fProcessOutput));}
    UpdateControls();
}
