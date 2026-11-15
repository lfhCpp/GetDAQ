#include "localserviceswindow.h"
#include "localserviceprocesses.h"
#include "localdeploymentservices.h"
#include "hardwaresetupwindow.h"
#include "localbackendregistry.h"
#ifdef Q_OS_LINUX
#include "hostrolewindow.h"
#endif
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QProcessEnvironment>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QMessageBox>
#include <QFormLayout>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QLabel>
#include <QJsonArray>
#include <QNetworkInterface>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTimer>
#include <QUuid>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <stdexcept>

namespace {
void Require(bool ok,const QString &message){if(!ok)throw std::runtime_error(message.toStdString());}
void PortAvailable(const QString &address,int port) {
    QTcpServer probe;Require(probe.listen(QHostAddress(address),quint16(port)),QObject::tr("Port %1:%2 is unavailable: %3").arg(address).arg(port).arg(probe.errorString()));
}
}
LocalServicesWindow::LocalServicesWindow(const RunConfig &config,QWidget *parent):QDialog(parent),fRunConfig(config) {
    setObjectName("LocalServicesWindow");setWindowTitle(tr("Local backend services"));resize(850,600);
    auto *layout=new QVBoxLayout(this);
    auto *instances=new QHBoxLayout;instances->addWidget(new QLabel(tr("ECC / data service instance"),this));
    fInstances=new QComboBox(this);fInstances->setObjectName("BackendInstances");instances->addWidget(fInstances,1);
    auto *create=new QPushButton(tr("Add service instance…"),this);create->setObjectName("AddBackendInstance");instances->addWidget(create);
    auto *open=new QPushButton(tr("Import existing…"),this);instances->addWidget(open);layout->addLayout(instances);
#ifdef Q_OS_LINUX
    auto *distributed=new QPushButton(tr("Master / Slave…"),this);distributed->setObjectName("OpenDistributedDeployment");layout->addWidget(distributed);
    connect(distributed,&QPushButton::clicked,this,[this]{if(fBusy)return;HostRoleWindow window(fRunConfig,this);if(window.exec()==QDialog::Accepted){fRunConfig=window.Configuration();accept();}});
#endif
    connect(create,&QPushButton::clicked,this,&LocalServicesWindow::CreateBackendInstance);
    connect(open,&QPushButton::clicked,this,[this]{if(fBusy)return;const auto path=QFileDialog::getOpenFileName(this,tr("Select saved service instance"),QString(),tr("GetDAQ configuration (*.yml *.yaml)"));if(path.isEmpty())return;RunConfig config;QString error;if(!config.Load(path,&error) || !LocalDeploymentServices::IsLocalDeployment(config)){fLog->appendPlainText(error.isEmpty()?tr("Select a local ECC/data service configuration."):error);return;}SelectBackend(config);});
    connect(fInstances,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int index){if(fBusy || index<0)return;const auto file=fInstances->itemData(index).toString();if(file.isEmpty())return;RunConfig config;QString error;if(!config.Load(file,&error)){fLog->appendPlainText(error);return;}SelectBackend(config);});
    fSettings=new QWidget(this);auto *form=new QFormLayout(fSettings);
    fMode=new QComboBox;fMode->setObjectName("ServiceMode");fMode->addItems({tr("ECC control service"),tr("Managed data service (supervisor)"),tr("Standalone data receiver (manual source control)")});form->addRow(tr("Program to start"),fMode);
    auto path=[this](QFormLayout *target,const QString &label,const QString &name,bool directory,QWidget **container=nullptr) {
        auto *row=new QWidget(fSettings);auto *line=new QHBoxLayout(row);line->setContentsMargins(0,0,0,0);auto *edit=new QLineEdit;edit->setObjectName(name);auto *browse=new QPushButton(tr("Browse…"));line->addWidget(edit);line->addWidget(browse);target->addRow(label,row);
        connect(browse,&QPushButton::clicked,this,[this,edit,directory]{const auto file=directory?QFileDialog::getExistingDirectory(this,tr("Directory"),edit->text()):QFileDialog::getOpenFileName(this,tr("Service configuration"),edit->text(),tr("JSON (*.json)"));if(!file.isEmpty())edit->setText(file);});
        if(container)*container=row;
        return edit;
    };
    fPrograms=path(form,tr("Installed bin directory"),"ServicePrograms",true);fPrograms->setText(QCoreApplication::applicationDirPath());
    fConfig=path(form,tr("Fixed service configuration"),"ServiceConfig",false,&fConfigRow);
    const QString deployment=config.filePath.isEmpty()?QString():QFileInfo(config.filePath).absolutePath();
    if(!config.filePath.isEmpty() && QFileInfo::exists(deployment+"/ecc.json"))fConfig->setText(deployment+"/ecc.json");
    fLogs=path(form,tr("Launch log parent directory"),"ServiceLogs",true);fLogs->setText(QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("service-launches"));
    fReceiverFields=new QWidget(fSettings);auto *receiver=new QFormLayout(fReceiverFields);receiver->setContentsMargins(0,0,0,0);
    fOutput=path(receiver,tr("Existing empty raw output directory"),"ServiceOutput",true);
    fBind=new QLineEdit(qEnvironmentVariableIsEmpty("GETDAQ_RECEIVER_IPV4")?QStringLiteral("127.0.0.1"):qEnvironmentVariable("GETDAQ_RECEIVER_IPV4"));fBind->setObjectName("ServiceBind");receiver->addRow(tr("Receiver IPv4 (this computer)"),fBind);
    fDataPort=new QSpinBox;fDataPort->setObjectName("ServiceDataPort");fDataPort->setRange(1,65535);fDataPort->setValue(23001);receiver->addRow(tr("Data port"),fDataPort);
    fStatusPort=new QSpinBox;fStatusPort->setObjectName("ServiceStatusPort");fStatusPort->setRange(1,65535);fStatusPort->setValue(23002);receiver->addRow(tr("Status port (127.0.0.1)"),fStatusPort);
    fStandalone=new QCheckBox(tr("This is a standalone source; ECC/supervisor does not manage this receiver or its ports."));fStandalone->setObjectName("ServiceStandalone");receiver->addRow(fStandalone);form->addRow(fReceiverFields);
    fSummary=new QLabel(this);fSummary->setObjectName("BackendSummary");fSummary->setWordWrap(true);layout->addWidget(fSummary);
    fBackendActions=new QWidget(this);auto *backendButtons=new QHBoxLayout(fBackendActions);backendButtons->setContentsMargins(0,0,0,0);
    for(int action=0;action<4;++action) {
        const QStringList labels={tr("Start / check backend"),tr("Stop backend safely"),tr("Refresh process status"),tr("Terminate backend processes…")};
        auto *button=new QPushButton(labels[action],fBackendActions);backendButtons->addWidget(button);
        button->setObjectName(QStringLiteral("BackendAction%1").arg(action));
        button->setEnabled(action==0 || LocalDeploymentServices::IsLocalDeployment(config));
        if(!LocalDeploymentServices::IsLocalDeployment(config))button->setToolTip(action==0?tr("Set up a local ECC and data service configuration first."):tr("Select a saved local backend configuration first; there is no current backend to manage."));
        connect(button,&QPushButton::clicked,this,[this,action]{BackendAction(action);});
    }
    layout->addWidget(fBackendActions);
    auto *advanced=new QCheckBox(tr("Advanced: individual services / standalone receiver"),this);layout->addWidget(advanced);
    layout->addWidget(fSettings);fSettings->hide();connect(advanced,&QCheckBox::toggled,fSettings,&QWidget::setVisible);
    auto *note=new QLabel(tr("Programs start as independent processes. Closing GetDAQ or this window leaves them running. "
        "Managed mode starts supervisor; ECC starts its receivers during the normal preparation/configuration workflow. "
        "Standalone receiver starts listening immediately and needs separate sender control."),this);note->setWordWrap(true);layout->addWidget(note);
    fLog=new QPlainTextEdit;fLog->setObjectName("ServiceLaunchLog");fLog->setReadOnly(true);fLog->setMaximumBlockCount(300);layout->addWidget(fLog,1);
    if(!LocalDeploymentServices::IsLocalDeployment(config))fLog->appendPlainText(tr("No saved local backend configuration is selected. Click Start / check backend to open the board/IP/port/path setup. Save/check and start the services there, then choose Use in Run Setup. No connected board is required to start the services; hardware acquisition is a separate action."));
    auto *buttons=new QHBoxLayout;fStart=new QPushButton(tr("Start independent process"));fStart->setObjectName("StartIndependentService");auto *close=new QPushButton(tr("Close"));auto *use=new QPushButton(tr("Use selected instance in Run Setup"),this);use->setObjectName("UseBackendInstance");connect(use,&QPushButton::clicked,this,[this]{if(!fBusy && LocalDeploymentServices::IsLocalDeployment(fRunConfig))accept();});auto *stop=new QPushButton(tr("Stop selected service"),this);stop->setObjectName("StopIndependentService");buttons->addWidget(fStart);buttons->addWidget(stop);fStart->hide();stop->hide();connect(advanced,&QCheckBox::toggled,fStart,&QWidget::setVisible);connect(advanced,&QCheckBox::toggled,stop,&QWidget::setVisible);buttons->addStretch();buttons->addWidget(use);buttons->addWidget(close);layout->addLayout(buttons);
    fCheck=new QProcess(this);
    connect(fMode,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int index){if(index<2 && !fRunConfig.filePath.isEmpty()) {const auto file=QFileInfo(fRunConfig.filePath).absolutePath()+(index==0?"/ecc.json":"/supervisor.json");if(QFileInfo::exists(file))fConfig->setText(file);}UpdateMode();});
    connect(stop,&QPushButton::clicked,this,[this]{
        if(fBusy)return;
        const int index=fMode->currentIndex();if(index>1){fLog->appendPlainText(tr("Standalone receiver stop requires its recorded PID; select ECC or supervisor."));return;}
        const QString program=QDir(fPrograms->text()).absoluteFilePath(index==0?"getdaq-ecc-server":"getdaq-supervisor");
        const auto processes=FindLocalServiceProcesses(program,fConfig->text());
        if(processes.isEmpty()){fLog->appendPlainText(tr("No running service matches this configuration. PID files are not required."));return;}
        QStringList names;for(const auto &p:processes)names<<tr("PID %1 — %2").arg(p.pid).arg(p.executable);
        if(QMessageBox::question(this,tr("Stop selected service"),tr("Stop these matching processes?\n%1\n\nFinish the acquisition with Run stop first. Stopping a service does not stop the board or complete the Run.").arg(names.join('\n')))!=QMessageBox::Yes)return;
        for(const auto &p:processes)if(SameLocalServiceProcess(p)) {
            const int code=QProcess::execute("/bin/kill",{"-TERM",QString::number(p.pid)});
            fLog->appendPlainText(code==0?tr("Stop requested for PID %1.").arg(p.pid):tr("Failed to signal PID %1.").arg(p.pid));
        }
        QTimer::singleShot(5000,this,[this,processes] {
            QVector<LocalServiceProcess> remaining;QStringList pids;
            for(const auto &p:processes)if(SameLocalServiceProcess(p)){remaining.append(p);pids<<QString::number(p.pid);}
            if(remaining.isEmpty()){fLog->appendPlainText(tr("Selected service has exited."));return;}
            if(QMessageBox::question(this,tr("Service still running"),tr("PID %1 did not exit. Force terminate these processes? Unfinished acquisition records are retained and may require inspection.").arg(pids.join(", ")),
                QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
            for(const auto &p:remaining)if(SameLocalServiceProcess(p)) {
                if(QProcess::execute("/bin/kill",{"-KILL",QString::number(p.pid)})==0)fLog->appendPlainText(tr("Force termination requested for PID %1.").arg(p.pid));
            }
        });
    });
    connect(fStart,&QPushButton::clicked,this,&LocalServicesWindow::Start);connect(close,&QPushButton::clicked,this,&LocalServicesWindow::reject);
    connect(fCheck,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){if(e==QProcess::FailedToStart)Failure(fCheck->errorString());});
    connect(fCheck,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){if(!fBusy)return;if(code || status!=QProcess::NormalExit){Failure(QString::fromLocal8Bit(fCheck->readAllStandardError()).left(8192));return;}Detached();});
    UpdateMode();
    SelectBackend(config);
#ifndef Q_OS_LINUX
    fStart->setEnabled(false);fLog->appendPlainText(tr("Launch these POSIX services on their Linux computer."));
#endif
}
void LocalServicesWindow::SelectBackend(const RunConfig &config) {
    fRunConfig=config;const bool local=LocalDeploymentServices::IsLocalDeployment(config);
    if(local)RememberLocalBackend(config.filePath);
    QStringList files=KnownLocalBackends();if(local && !files.contains(config.filePath))files.prepend(config.filePath);
    for(const auto &file:LocalServiceConfigurations(QDir(fPrograms->text()).filePath("getdaq-ecc-server"))) {const auto yaml=QFileInfo(file).absolutePath()+"/envoy.yml";if(QFileInfo::exists(yaml) && !files.contains(yaml))files.append(yaml);}
    const QSignalBlocker blocker(fInstances);fInstances->clear();if(!local)fInstances->addItem(tr("No service instance selected"),QString());
    for(const auto &file:files){RunConfig saved;QString error;if(saved.Load(file,&error) && LocalDeploymentServices::IsLocalDeployment(saved)){QStringList boards;for(const auto &node:saved.nodes)boards<<node.host;fInstances->addItem(tr("ECC %1 · %2").arg(saved.nativeClient.port).arg(boards.join(", ")),file);}}
    if(local)fInstances->setCurrentIndex(fInstances->findData(config.filePath));
    QStringList details;details<<tr("Selected service instance: %1").arg(local?QFileInfo(config.filePath).absoluteDir().dirName():tr("Not configured"));
    if(local){details<<tr("ECC: %1:%2").arg(config.nativeClient.address).arg(config.nativeClient.port);for(const auto &node:config.nodes)details<<tr("%1: board %2:%3 → receiver %4:%5").arg(node.name,node.host).arg(node.eccPort).arg(node.routerHost).arg(node.routerPort);fConfig->setText(QFileInfo(config.filePath).absolutePath()+(fMode->currentIndex()==1?"/supervisor.json":"/ecc.json"));}
    fSummary->setText(details.join('\n'));
    for(int action=0;action<4;++action)if(auto *button=findChild<QPushButton *>(QStringLiteral("BackendAction%1").arg(action)))button->setEnabled(action==0 || local);
    findChild<QPushButton *>("UseBackendInstance")->setEnabled(local);
}
void LocalServicesWindow::CreateBackendInstance() {
    if(fBusy)return;
    HardwareSetupWindow setup(fRunConfig,this);if(LocalDeploymentServices::IsLocalDeployment(fRunConfig))setup.PrepareParallelDraft();
    if(setup.exec()==QDialog::Accepted){SelectBackend(setup.Configuration());fLog->appendPlainText(tr("Service instance added in this window. Other instances and the main window remain open. Select Use selected instance in Run Setup to control acquisition with it."));}
}
void LocalServicesWindow::UpdateMode(){const bool standalone=fMode->currentIndex()==2;fConfigRow->setEnabled(!standalone);fReceiverFields->setVisible(standalone);}
void LocalServicesWindow::Failure(const QString &message){fBusy=false;fSettings->setEnabled(true);fStart->setEnabled(true);fLog->appendPlainText(tr("Not started: %1").arg(message));}
void LocalServicesWindow::reject(){if(fBusy){fLog->appendPlainText(tr("Wait for the configuration check to finish."));return;}QDialog::reject();}
void LocalServicesWindow::Start() {
    if(fBusy)return;
    const quint64 generation=++fGeneration;
    try {
        const int mode=fMode->currentIndex();const QString name=mode==0?"getdaq-ecc-server":mode==1?"getdaq-supervisor":"getdaq-receiver";
        fProgram=QDir(fPrograms->text().trimmed()).absoluteFilePath(name);
        const QFileInfo executable(fProgram);Require(executable.isFile() && !executable.isSymLink() && executable.isExecutable() && !(executable.permissions()&(QFile::WriteGroup|QFile::WriteOther)),tr("Choose an installed, non-writable executable: %1").arg(fProgram));
        fArguments.clear();
        if(mode<2) {
            QFile config(fConfig->text().trimmed());Require(config.open(QIODevice::ReadOnly) && config.size()<=1024*1024,tr("Choose a readable fixed service JSON"));const auto object=QJsonDocument::fromJson(config.readAll()).object();
            const QString kind=mode==0?"getdaq.ecc-service":"getdaq.supervisor-service";Require(object["kind"].toString()==kind,tr("The configuration belongs to a different service type"));
            const QHostAddress bind(object["bind"].toString());const int port=object["port"].toInt();Require(bind.protocol()==QAbstractSocket::IPv4Protocol && port>0 && port<=65535,tr("Invalid service control address/port"));PortAvailable(bind.toString(),port);
            fArguments=QStringList{"--config",QFileInfo(config).absoluteFilePath()};
            fBusy=true;fSettings->setEnabled(false);fStart->setEnabled(false);fCheck->start(fProgram,fArguments+QStringList{"--check-config"});
            QTimer::singleShot(30000,this,[this,generation]{if(fGeneration==generation && fBusy && fCheck->state()!=QProcess::NotRunning){fCheck->kill();Failure(tr("Configuration check timed out"));}});return;
        }
        Require(fStandalone->isChecked(),tr("Confirm that this receiver is not managed by ECC/supervisor"));
        const QHostAddress bind(fBind->text().trimmed());Require(bind.protocol()==QAbstractSocket::IPv4Protocol && bind!=QHostAddress::AnyIPv4 && QNetworkInterface::allAddresses().contains(bind),tr("Receiver IP must be an actual IPv4 on this computer"));
        const QString output=QFileInfo(fOutput->text().trimmed()).absoluteFilePath();const QFileInfo directory(output);
        Require(!fOutput->text().trimmed().isEmpty() && directory.isDir() && !directory.isSymLink() && directory.isWritable() && QDir(output).entryList(QDir::AllEntries|QDir::Hidden|QDir::NoDotAndDotDot).isEmpty(),tr("Select an existing empty raw output directory for this standalone receiver"));
        Require(fDataPort->value()!=fStatusPort->value(),tr("Data and status ports must differ"));PortAvailable(bind.toString(),fDataPort->value());PortAvailable("127.0.0.1",fStatusPort->value());
        fArguments=QStringList{"--output",output,"--bind",bind.toString(),"--port",QString::number(fDataPort->value()),"--status-bind","127.0.0.1","--status-port",QString::number(fStatusPort->value()),"--export-port","0","--input-protocol","TCP","--queue-mib","64","--min-free-mib","2048"};
        Detached();
    }catch(const std::exception &e){Failure(QString::fromUtf8(e.what()));}
}
void LocalServicesWindow::Detached() {
    try {
        const QString root=QDir(fLogs->text().trimmed()).absoluteFilePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
        Require(!fLogs->text().trimmed().isEmpty() && QDir().mkpath(root) && QFile::setPermissions(root,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner),tr("Cannot create private launch log directory"));
        const QString log=root+"/service.log";QFile file(log);Require(file.open(QIODevice::WriteOnly|QIODevice::NewOnly) && file.setPermissions(QFile::ReadOwner|QFile::WriteOwner),tr("Cannot create launch log"));file.close();
        // Save the exact command before requesting a detached launch. No shell is used.
        QJsonArray arguments;for(const auto &argument:fArguments)arguments.append(argument);
        QFile command(root+"/command.json");Require(command.open(QIODevice::WriteOnly|QIODevice::NewOnly),tr("Cannot record launch command"));command.setPermissions(QFile::ReadOwner|QFile::WriteOwner);const auto bytes=QJsonDocument(QJsonObject{{"program",fProgram},{"arguments",arguments}}).toJson();Require(command.write(bytes)==bytes.size() && command.flush(),tr("Cannot save launch command"));command.close();
        QProcess service;auto environment=QProcessEnvironment::systemEnvironment();environment.remove("GETDAQ_READBACK_WARNING_POLICY_FILE");
        if(QFileInfo(fProgram).fileName()=="getdaq-ecc-server") {const auto policy=QFileInfo(fConfig->text()).absolutePath()+"/readback-warning-policy.json";if(QFileInfo::exists(policy))environment.insert("GETDAQ_READBACK_WARNING_POLICY_FILE",policy);}
        service.setProcessEnvironment(environment);service.setProgram(fProgram);service.setArguments(fArguments);service.setWorkingDirectory(root);service.setStandardOutputFile(log,QIODevice::Append);service.setStandardErrorFile(log,QIODevice::Append);qint64 pid=0;
        Require(service.startDetached(&pid),tr("Independent launch failed; see %1").arg(log));
        fLog->appendPlainText(tr("Independent process launched: PID %1\nProgram: %2\nLog: %3\nLaunch acceptance is not readiness; check the log and Connect/status. Closing this window leaves the process running.").arg(pid).arg(fProgram,log));
        QFile receipt(root+"/process.json");if(receipt.open(QIODevice::WriteOnly|QIODevice::NewOnly)){receipt.setPermissions(QFile::ReadOwner|QFile::WriteOwner);receipt.write(QJsonDocument(QJsonObject{{"pid",double(pid)},{"program",fProgram}}).toJson());}
        fBusy=false;fSettings->setEnabled(true);fStart->setEnabled(true);
    }catch(const std::exception &e){Failure(QString::fromUtf8(e.what()));}
}

void LocalServicesWindow::BackendAction(int action) {
    if(fBusy)return;
    if(!LocalDeploymentServices::IsLocalDeployment(fRunConfig)) {
        if(action==0) {
            HardwareSetupWindow setup(fRunConfig,this);
            if(setup.exec()==QDialog::Accepted)SelectBackend(setup.Configuration());
        } else fLog->appendPlainText(tr("Select a saved local backend configuration before managing its processes."));
        return;
    }
    const auto directory=QFileInfo(fRunConfig.filePath).absolutePath();
    if(action==2) {
        for(const auto &name:{QString("getdaq-ecc-server"),QString("getdaq-supervisor")}) {
            const auto processes=FindLocalServiceProcesses(QDir(fPrograms->text()).filePath(name),directory+(name=="getdaq-ecc-server"?"/ecc.json":"/supervisor.json"));
            if(processes.isEmpty())fLog->appendPlainText(tr("%1: not running").arg(name));
            for(const auto &p:processes)fLog->appendPlainText(tr("%1: PID %2 (%3)").arg(name).arg(p.pid).arg(p.executable));
        }
        const auto receivers=FindLocalReceiverProcesses(QDir(fPrograms->text()).filePath("getdaq-receiver"),directory+"/supervisor.json");
        fLog->appendPlainText(tr("Matching receiver processes: %1. Process presence alone does not establish readiness.").arg(receivers.size()));return;
    }
    if(action==3 && QMessageBox::warning(this,tr("Terminate backend processes"),tr("Terminate the ECC, supervisor and receivers matching this configuration? This does not stop the board or complete a Run. Use this only after stopping acquisition or when repairing a failed session. Records are retained."),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    if(fBackend)fBackend->deleteLater();
    fBackend=new LocalDeploymentServices(fRunConfig,fPrograms->text(),this);
    connect(fBackend,&LocalDeploymentServices::Log,fLog,&QPlainTextEdit::appendPlainText);
    auto done=[this]{fBusy=false;fInstances->setEnabled(true);fSettings->setEnabled(true);fBackendActions->setEnabled(true);fStart->setEnabled(true);};
    connect(fBackend,&LocalDeploymentServices::Ready,this,[this,done]{done();fLog->appendPlainText(tr("ECC and data service identities verified. Use Connect in the main window."));});
    connect(fBackend,&LocalDeploymentServices::Stopped,this,[this,done]{done();fLog->appendPlainText(tr("Matching backend processes exited."));});
    connect(fBackend,&LocalDeploymentServices::Failed,this,[this,done](const QString &error){done();fLog->appendPlainText(error);});
    connect(fBackend,&LocalDeploymentServices::ForceRequired,this,[this](const QString &names){
        if(QMessageBox::warning(this,tr("Processes still running"),tr("Force terminate these remaining processes?\n%1\nUnfinished data/state may require inspection.").arg(names),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes)fBackend->ForceRemaining();else fBackend->CancelForce();
    });
    fBusy=true;fInstances->setEnabled(false);fSettings->setEnabled(false);fBackendActions->setEnabled(false);fStart->setEnabled(false);
    if(action==0)fBackend->EnsureStarted();else if(action==1)fBackend->StopSafely();else fBackend->StopExplicitly();
}
