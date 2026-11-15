#include "hardwaresetupwindow.h"
#include "localserviceswindow.h"
#include "localserviceprocesses.h"
#include "localdeploymentservices.h"
#include "earlywarningrecovery.h"
#include "backendpresets.h"
#include "localbackendregistry.h"
#include <QCheckBox>
#include <QComboBox>
#include <QUrl>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QProcessEnvironment>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTcpServer>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>
#include <stdexcept>

namespace {
QString NewDirectory() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(
        QStringLiteral("deployments/site-%1-%2").arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss"),QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)));
}
QString Cell(QTableWidget *table,int row,int column) {return table->item(row,column)->text().trimmed();}
int Number(QTableWidget *table,int row,int column) {
    bool ok=false;int n=Cell(table,row,column).toInt(&ok);
    if(!ok)throw std::runtime_error("Board/data/status ports must be integers");
    return n;
}
}
HardwareSetupWindow::HardwareSetupWindow(const RunConfig &base,QWidget *parent):QDialog(parent),fBase(base) {
    setObjectName("HardwareSetupWindow");setWindowTitle(tr("Nodes / Paths — hardware and services"));resize(1180,800);
    auto *layout=new QVBoxLayout(this);
    auto *note=new QLabel(tr("Enter board IP, receiver IP, ports and data directory. Service files and credentials are generated automatically. "
        "Starting services does not configure the board or start acquisition."),this);
    note->setWordWrap(true);layout->addWidget(note);
    auto *presetSummary=new QLabel(tr("Electronics: current saved configuration"),this);presetSummary->setObjectName("SetupPresetSummary");presetSummary->setWordWrap(true);layout->addWidget(presetSummary);
    auto *services=new QPushButton(tr("Manage current ECC / data services…"),this);layout->addWidget(services);
    connect(services,&QPushButton::clicked,this,[this]{const auto current=fLaunched?fFiles.console:fBase;LocalServicesWindow window(current,this);if(window.exec()==QDialog::Accepted && window.Configuration().filePath!=current.filePath){fFiles.console=window.Configuration();accept();}});
    auto *scroll=new QScrollArea(this);scroll->setWidgetResizable(true);fInputs=new QWidget(scroll);auto *inputs=new QVBoxLayout(fInputs);auto *form=new QFormLayout;
    auto path=[this,form](const QString &label,const QString &name,bool directory) {
        auto *row=new QWidget(fInputs);auto *line=new QHBoxLayout(row);line->setContentsMargins(0,0,0,0);
        auto *edit=new QLineEdit(row);edit->setObjectName(name);auto *browse=new QPushButton(tr("Browse…"),row);line->addWidget(edit);line->addWidget(browse);form->addRow(label,row);
        connect(browse,&QPushButton::clicked,this,[this,edit,directory] {
            const auto value=directory?QFileDialog::getExistingDirectory(this,tr("Choose directory"),edit->text()):QFileDialog::getOpenFileName(this,tr("Choose reviewed input"),edit->text());
            if(!value.isEmpty())edit->setText(value);
        });return edit;
    };
    fExperiment=path(tr("Electronics template (.xcfg or saved experiment.json)"),"SetupExperiment",false);
    fProfile=path(tr("Hardware profile (only for .xcfg import)"),"SetupProfile",false);
    fPrograms=path(tr("Installed program directory (runtime-linux/bin)"),"SetupPrograms",true);
    fPrograms->setText(QCoreApplication::applicationDirPath());
    fRoot=path(tr("Save configuration set to (new directory)"),"SetupRoot",true);fRoot->setText(NewDirectory());
    fData=path(tr("Data output directory"),"SetupData",true);fData->setText(QDir(fRoot->text()).filePath("data"));
    fCallback=new QLineEdit(fInputs);fCallback->setObjectName("SetupCallback");fCallback->setText(qEnvironmentVariable("GETDAQ_CONTROL_IPV4"));fCallback->setPlaceholderText(tr("This Linux computer's IPv4 reachable from the boards"));form->addRow(tr("This computer: control-network IPv4"),fCallback);
    auto port=[this,form](const QString &label,const QString &name,int value) {auto *p=new QSpinBox(fInputs);p->setObjectName(name);p->setRange(1,65535);p->setValue(value);form->addRow(label,p);return p;};
    fEccPort=port(tr("ECC control port (local)"),"SetupEccPort",24000);
    fNodePort=port(tr("Receiver supervisor port (local)"),"SetupNodePort",23100);
    fCallbackPort=port(tr("First CoBo callback port (one per CoBo)"),"SetupCallbackPort",24100);
    fDelayWarning=new QCheckBox(tr("Accept legacy readDataDelay=255 readback as a warning (AGET0–3 only)"),fInputs);
    fDelayWarning->setObjectName("SetupDelayReadbackWarning");form->addRow(fDelayWarning);
    connect(fDelayWarning,&QCheckBox::toggled,this,[this](bool checked){
        if(checked && QMessageBox::question(this,tr("Accept readback compatibility warning"),tr("Accept only ctrl.readDataDelay.AGET0–3 returning 255 as unverified readback? Writes still execute; timing/data quality remains unverified. This does not waive transport failures, production-stop checks, power/clock protection or other register mismatches."),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes){const QSignalBlocker blocker(fDelayWarning);fDelayWarning->setChecked(false);}
    });
    auto *advancedInputs=new QCheckBox(tr("Advanced: electronics template and service internals"),fInputs);advancedInputs->setObjectName("SetupAdvanced");
    advancedInputs->setChecked(true);inputs->addWidget(advancedInputs);
    inputs->addLayout(form);
    auto *parallel=new QPushButton(tr("New ECC + data service instance on this computer"),fInputs);parallel->setObjectName("SetupNewParallelInstance");inputs->addWidget(parallel);
    connect(parallel,&QPushButton::clicked,this,&HardwareSetupWindow::NewParallelInstance);
    auto *buttons=new QHBoxLayout;
    fLoad=new QPushButton(tr("Read electronics template"),fInputs);fLoad->setObjectName("SetupLoadBoards");
    auto *description=new QPushButton(tr("Choose selected board description…"),fInputs);
    auto *advanced=new QPushButton(tr("Selected board advanced settings…"),fInputs);advanced->setObjectName("SetupBoardServices");
    buttons->addWidget(fLoad);buttons->addWidget(description);buttons->addWidget(advanced);inputs->addLayout(buttons);
    auto *editButtons=new QHBoxLayout;auto *add=new QPushButton(tr("Add CoBo (copy selected settings)"),fInputs);
    auto *remove=new QPushButton(tr("Remove selected board"),fInputs);auto *details=new QCheckBox(tr("Show advanced board fields"),fInputs);
    editButtons->addWidget(add);editButtons->addWidget(remove);editButtons->addWidget(details);editButtons->addStretch();inputs->addLayout(editButtons);
    connect(add,&QPushButton::clicked,this,&HardwareSetupWindow::AddCoBo);
    connect(remove,&QPushButton::clicked,this,&HardwareSetupWindow::RemoveBoard);
    fBoards=new QTableWidget(0,10,fInputs);fBoards->setObjectName("HardwareBoardTable");
    fBoards->setHorizontalHeaderLabels({tr("Board ID"),tr("Role / active"),tr("Board IPv4"),tr("Board port"),tr("Hardware description"),tr("Receiver IPv4"),tr("Data port"),tr("Status port"),tr("Node object"),tr("Electronics template board")});
    fBoards->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    const QList<int> widths={100,140,165,105,230,180,105,85};
    for(int i=0;i<widths.size();++i)fBoards->setColumnWidth(i,widths[i]);
    fBoards->setColumnHidden(4,true);fBoards->setColumnHidden(7,true);fBoards->setColumnHidden(8,true);
    fBoards->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(details,&QCheckBox::toggled,this,[this](bool show){for(int c:{4,7,8})fBoards->setColumnHidden(c,!show);});
    fBoards->setColumnWidth(9,220);fBoards->setColumnHidden(9,true);fBoards->horizontalHeader()->setStretchLastSection(false);fBoards->horizontalHeader()->setSectionResizeMode(5,QHeaderView::Stretch);
    connect(advancedInputs,&QCheckBox::toggled,this,[this,form,description,advanced,parallel](bool show) {
        for(auto *field:QList<QWidget *>{fExperiment->parentWidget(),fProfile->parentWidget(),fPrograms->parentWidget(),fRoot->parentWidget(),fCallbackPort,fNodePort,fDelayWarning}) {
            field->setVisible(show);if(auto *label=form->labelForField(field))label->setVisible(show);
        }
        fLoad->setVisible(show);description->setVisible(show);advanced->setVisible(show);parallel->setVisible(show);
        fBoards->setColumnHidden(9,!show);fGenerate->setVisible(show);
    });
    fBoards->setMinimumHeight(200);inputs->addWidget(fBoards);
    fStopped=new QCheckBox(tr("I verified exclusive control of this instance's boards and that their acquisition / old data senders are stopped."),this);fStopped->setObjectName("SetupStopped");
    scroll->setWidget(fInputs);layout->addWidget(scroll,3);layout->addWidget(fStopped);
    fLog=new QPlainTextEdit(this);fLog->setObjectName("HardwareSetupLog");fLog->setReadOnly(true);fLog->setMaximumBlockCount(500);layout->addWidget(fLog,1);
    auto *actions=new QHBoxLayout;
    fGenerate=new QPushButton(tr("Save configuration files and check"),this);fGenerate->setObjectName("SetupGenerate");
    fLaunch=new QPushButton(tr("Start ECC + data service"),this);fLaunch->setObjectName("SetupLaunch");
    fLaunch->setToolTip(tr("Start or check saved services. For a new or edited setup, this saves and checks the configuration first and reports any missing settings."));
    fUse=new QPushButton(tr("Use in Run Setup"),this);fUse->setObjectName("SetupUse");fUse->setEnabled(false);
    auto *close=new QPushButton(tr("Close"),this);actions->addWidget(fGenerate);actions->addWidget(fLaunch);actions->addStretch();actions->addWidget(fUse);actions->addWidget(close);layout->addLayout(actions);
    fProcess=new QProcess(this);
    connect(fProcess,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,&HardwareSetupWindow::Finished);
    connect(fProcess,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){if(e==QProcess::FailedToStart)Fail(tr("Cannot start configuration checker: %1").arg(fProcess->errorString()));});
    connect(fLoad,&QPushButton::clicked,this,&HardwareSetupWindow::LoadBoards);
    connect(advanced,&QPushButton::clicked,this,&HardwareSetupWindow::EditAdvanced);
    connect(description,&QPushButton::clicked,this,[this] {int row=fBoards->currentRow();if(row<0)return;auto path=QFileDialog::getOpenFileName(this,tr("Hardware description"),QString(),tr("Descriptions (*.xcfg *.xml *.json)"));if(!path.isEmpty())fBoards->item(row,4)->setText(path);});
    connect(fGenerate,&QPushButton::clicked,this,&HardwareSetupWindow::Generate);
    connect(fLaunch,&QPushButton::clicked,this,&HardwareSetupWindow::StartOrCheckServices);
    connect(fUse,&QPushButton::clicked,this,[this]{if(fBusy)return;if(fProjectDraft){try{DraftSetup();if(!fStopped->isChecked()){Log(tr("Confirm stopped acquisition and exclusive control before adding this group."));return;}accept();}catch(const std::exception &error){Log(QString::fromUtf8(error.what()));}return;}if(fCheckedDigest!=InputsDigest()){Fail(tr("Fields changed after checking; generate a new deployment."));return;}accept();});
    connect(close,&QPushButton::clicked,this,&HardwareSetupWindow::reject);
    const auto deployment=QFileInfo(base.filePath).absolutePath();
    if(base.nativeEcc && QFileInfo::exists(QDir(deployment).filePath("ecc.json"))) {
        try {RestoreSetup(deployment);advancedInputs->setChecked(false);}catch(const std::exception &e){Log(tr("Cannot import current deployment: %1").arg(QString::fromUtf8(e.what())));}
    }
    else try {
        const auto preset=BuiltinPedestalsBackendSetup();fExperiment->setText(preset.experimentFile);fProfile->setText(preset.profileFile);fCallback->setText(preset.callbackAddress);fLoaded=preset.boards;PopulateBoards();
        presetSummary->setText(tr("Preset: pedestals · CoBo hardware v5 · AGET 2.2 · AsAd slot 0. Adjust electronics under Advanced when the actual wiring differs."));
        Log(tr("Built-in preset: single CoBo pedestals, hardware v5, AGET 2.2, AsAd slot 0. Electronics settings can be changed under Advanced. No hardware commands have been sent."));
    }catch(const std::exception &e){Log(tr("Built-in preset unavailable: %1").arg(QString::fromUtf8(e.what())));}
    advancedInputs->setChecked(false);
    if(fSavedDirectory.isEmpty())Log(tr("No saved local backend is loaded. Start ECC + data service will check the required settings and save/check a new configuration before starting services."));
#ifndef Q_OS_LINUX
    fGenerate->setEnabled(false);fLoad->setEnabled(false);fLaunch->setEnabled(false);
    Log(tr("Start this setup window on the Linux service computer. Remote Windows control continues to use the existing native ECC connection settings."));
#endif
}
void HardwareSetupWindow::Log(const QString &message) {fLog->appendPlainText(message);}
void HardwareSetupWindow::EnableProjectDraftMode(){fProjectDraft=true;fLaunch->hide();fGenerate->hide();fUse->setText(tr("Add to distributed project"));fUse->setEnabled(true);fData->setReadOnly(true);for(auto *browse:fData->parentWidget()->findChildren<QPushButton *>())browse->setEnabled(false);fData->setToolTip(tr("Actual data parent is configured on the selected receiving host; the node agent assigns this group's private subdirectory."));}
void HardwareSetupWindow::SetReceivingHosts(const QJsonArray &nodes,const QString &defaultNode){fReceivingHosts=nodes;fDefaultReceivingHost=defaultNode;fBoards->setColumnCount(11);fBoards->setHorizontalHeaderItem(10,new QTableWidgetItem(tr("Receiving host")));fBoards->setColumnWidth(10,170);PopulateBoards();}
void HardwareSetupWindow::SetProjectDraft(const HardwareDeploymentSetup &setup){fExperiment->setText(setup.experimentFile);fProfile->setText(setup.profileFile);fCallback->setText(setup.callbackAddress);fData->setText(setup.dataDirectory);fEccPort->setValue(setup.eccPort);fNodePort->setValue(setup.supervisorPort);fCallbackPort->setValue(setup.callbackPort);fLoaded=setup.boards;{const QSignalBlocker blocker(fDelayWarning);fDelayWarning->setChecked(setup.acceptLegacyDelayReadback);}PopulateBoards();fSavedDirectory.clear();fSavedInputsDigest.clear();fFiles=HardwareDeploymentFiles();fCheckedDigest.clear();fStopped->setChecked(false);fUse->setEnabled(fProjectDraft);if(auto *label=findChild<QLabel *>("SetupPresetSummary"))label->setText(tr("Electronics: saved project template; inspect or change under Advanced."));}
HardwareDeploymentSetup HardwareSetupWindow::DraftSetup(){ReadRows();HardwareDeploymentSetup setup;setup.experimentFile=fExperiment->text().trimmed();setup.profileFile=fProfile->text().trimmed();setup.programDirectory=fPrograms->text().trimmed();setup.deploymentDirectory=fRoot->text().trimmed();setup.dataDirectory=fData->text().trimmed();setup.callbackAddress=fCallback->text().trimmed();setup.eccPort=fEccPort->value();setup.supervisorPort=fNodePort->value();setup.callbackPort=fCallbackPort->value();setup.boards=fLoaded;setup.acceptLegacyDelayReadback=fDelayWarning->isChecked();return setup;}
void HardwareSetupWindow::Busy(bool busy) {
    fBusy=busy;fInputs->setEnabled(!busy && !fLaunched);fStopped->setEnabled(!busy && !fLaunched);fGenerate->setEnabled(!busy && !fLaunched);
    if(busy){fLaunch->setEnabled(false);fUse->setEnabled(false);}
}
void HardwareSetupWindow::Fail(const QString &message) {
    if(fRecoveryMode){fRecoveryMode=false;fStopped->setChecked(false);}
    fStep.clear();fLaunchAfterCheck=false;Busy(false);fLaunch->setEnabled(true);fUse->setEnabled(fProjectDraft);Log(tr("Setup incomplete: %1").arg(message));
    if(fSupervisorPid || fEccPid)Log(tr("Started services are retained; closing this window does not stop them. PIDs supervisor=%1 ECC=%2. Inspect logs before any retry.").arg(fSupervisorPid).arg(fEccPid));
}
void HardwareSetupWindow::reject() {if(fBusy){Log(tr("Wait for the current check to finish before closing."));return;}QDialog::reject();}
void HardwareSetupWindow::LoadBoards() {
    try {
        fLoaded=HardwareSetupBoards(CompileHardwareSetupExperiment(fExperiment->text(),fProfile->text()));
        for(auto &board:fLoaded)if(board.receivesData)board.receiverAddress=qEnvironmentVariable("GETDAQ_RECEIVER_IPV4");
        PopulateBoards();
        Log(tr("Loaded %1 boards. Added CoBos copy the selected board's electronics settings; review hardware compatibility before saving.").arg(fLoaded.size()));
        fLaunch->setEnabled(true);fUse->setEnabled(fProjectDraft);
    }catch(const std::exception &e){Fail(QString::fromUtf8(e.what()));}
}
void HardwareSetupWindow::EditAdvanced() {
    const int row=fBoards->currentRow();if(row<0)return;auto &b=fLoaded[row];
    QDialog dialog(this);dialog.setWindowTitle(tr("Board services — %1").arg(b.id));auto *layout=new QVBoxLayout(&dialog);auto *form=new QFormLayout;
    auto edit=[&](const QString &label,const QString &value){auto *field=new QLineEdit(value,&dialog);form->addRow(label,field);return field;};
    auto *nodeName=edit(tr("Expected hardware node name"),b.nodeName);
    auto *nodeObject=edit(tr("Node object identity"),Cell(fBoards,row,8));
    auto *link=edit(b.mutant?tr("Alignment object identity"):tr("Link manager object identity"),b.mutant?b.alignmentObject:b.linkObject);
    auto *sender=edit(tr("DAQ control object identity (data source)"),b.senderObject);sender->setEnabled(b.receivesData);
    auto *port=new QSpinBox(&dialog);port->setRange(1,65535);port->setValue(b.senderPort);port->setEnabled(b.receivesData);form->addRow(tr("DAQ control port"),port);
    layout->addLayout(form);auto *box=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);layout->addWidget(box);
    connect(box,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(box,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted) {b.nodeName=nodeName->text().trimmed();b.nodeObject=nodeObject->text().trimmed();fBoards->item(row,8)->setText(b.nodeObject);if(b.mutant)b.alignmentObject=link->text().trimmed();else b.linkObject=link->text().trimmed();b.senderObject=sender->text().trimmed();b.senderPort=port->value();fLaunch->setEnabled(true);fUse->setEnabled(fProjectDraft);}
}
QString HardwareSetupWindow::InputsDigest(bool includeConfirmation) const {
    QByteArray bytes;
    for(const auto *field:fInputs->findChildren<QLineEdit *>())bytes+=field->text().toUtf8()+'\0';
    for(const auto *field:fInputs->findChildren<QSpinBox *>())bytes+=QByteArray::number(field->value())+'\0';
    if(includeConfirmation)bytes+=fStopped->isChecked()?'1':'0';
    bytes+=fDelayWarning->isChecked()?'1':'0';
    for(int r=0;r<fBoards->rowCount();++r)for(int c=0;c<fBoards->columnCount();++c)bytes+=Cell(fBoards,r,c).toUtf8()+'\0'+QByteArray::number(fBoards->item(r,c)->checkState())+'\0';
    for(const auto &b:fLoaded)bytes+=b.nodeName.toUtf8()+'\0'+b.senderObject.toUtf8()+'\0'+QByteArray::number(b.senderPort)+'\0'+b.linkObject.toUtf8()+'\0'+b.alignmentObject.toUtf8()+'\0';
    return HardwareSetupSha256(bytes);
}
void HardwareSetupWindow::Generate() {
    if(!fStopped->isChecked()){Log(tr("Verify stopped acquisition and exclusive control first."));return;}
    try {
        ReadRows();
        HardwareDeploymentSetup setup;setup.experimentFile=fExperiment->text().trimmed();setup.profileFile=fProfile->text().trimmed();setup.programDirectory=fPrograms->text().trimmed();setup.deploymentDirectory=fRoot->text().trimmed();setup.dataDirectory=fData->text().trimmed();setup.callbackAddress=fCallback->text().trimmed();setup.eccPort=fEccPort->value();setup.supervisorPort=fNodePort->value();setup.callbackPort=fCallbackPort->value();setup.boards=fLoaded;setup.acceptLegacyDelayReadback=fDelayWarning->isChecked();
        for(int i=0;i<setup.boards.size();++i) {auto &b=setup.boards[i];b.address=Cell(fBoards,i,2);b.controlPort=Number(fBoards,i,3);b.descriptionFile=Cell(fBoards,i,4);b.receiverAddress=Cell(fBoards,i,5);b.dataPort=Number(fBoards,i,6);b.statusPort=Number(fBoards,i,7);b.nodeObject=Cell(fBoards,i,8);}
        fCheckedDigest=InputsDigest();
        Busy(true);
        Log(tr("Compiling experiment and hardware descriptions…"));
        typedef QPair<HardwareDeploymentFiles,QString> Prepared;
        auto *watcher=new QFutureWatcher<Prepared>(this);
        connect(watcher,&QFutureWatcher<Prepared>::finished,this,[this,watcher] {
            const auto result=watcher->result();watcher->deleteLater();
            if(!result.second.isEmpty()){Fail(result.second);return;}
            fFiles=result.first;
            if(fRecoveryMode)try {WriteHardwareSetupFile(QDir(fFiles.directory).filePath("accepted-warning-reset-proof.json"),QJsonDocument(fRecoveryProof).toJson());}
                catch(const std::exception &e){Fail(QString::fromUtf8(e.what()));return;}
            Log(tr("Saved configuration directory: %1").arg(fFiles.directory));
            for(const auto &name:{"setup-inputs.json","topology.json","experiment.json","supervisor.json","ecc.json","envoy.yml"})
                Log(QDir(fFiles.directory).filePath(name));
            Execute("getdaq-supervisor",{"--config",fFiles.supervisorFile,"--plan-config"},"node-plan");
        });
        const RunConfig base=fBase;
        watcher->setFuture(QtConcurrent::run([setup,base] {
            try {return qMakePair(WriteHardwareDeployment(setup,base),QString());}
            catch(const std::exception &e){return qMakePair(HardwareDeploymentFiles(),QString::fromUtf8(e.what()));}
        }));
    }catch(const std::exception &e){Fail(QString::fromUtf8(e.what()));}
}
void HardwareSetupWindow::Execute(const QString &program,const QStringList &arguments,const QString &step) {
    fStep=step;fProcess->setProgram(QDir(fPrograms->text()).filePath(program));fProcess->setArguments(arguments);fProcess->start();
    QTimer::singleShot(30000,this,[this,step]{if(fBusy && fStep==step && fProcess->state()!=QProcess::NotRunning){fProcess->kill();Fail(tr("Offline checker timed out; generated files retained."));}});
}
void HardwareSetupWindow::Finished(int code,QProcess::ExitStatus status) {
    if(fStep.isEmpty())return;
    const auto step=fStep;const auto bytes=fProcess->readAllStandardOutput();const auto error=fProcess->readAllStandardError();
    if(code || status!=QProcess::NormalExit){
        if(step=="node-identity" && fIdentityAttempts<10){QTimer::singleShot(500,this,&HardwareSetupWindow::VerifyServices);return;}
        Fail(QString::fromLocal8Bit(error).left(8192));return;
    }
    try {
        if(step=="node-plan") {
            auto remote=fFiles.ecc["remote_receivers"].toObject();auto endpoints=remote["endpoints"].toArray();auto endpoint=endpoints[0].toObject();endpoint["plan_sha256"]=HardwareSetupSha256(bytes);endpoints[0]=endpoint;remote["endpoints"]=endpoints;fFiles.ecc["remote_receivers"]=remote;
            WriteHardwareSetupFile(fFiles.eccFile,QJsonDocument(fFiles.ecc).toJson());
            Execute("getdaq-ecc-server",{"--config",fFiles.eccFile,"--plan-config"},"ecc-plan");return;
        }
        if(step=="ecc-plan") {
            const auto object=QJsonDocument::fromJson(bytes).object();const QRegularExpression digest("^[0-9a-f]{64}$");
            auto &c=fFiles.console;c.nativeClient.snapshotSha256=object["snapshot_sha256"].toString();c.nativeClient.inputBindingSha256=object["input_binding_sha256"].toString();
            if(!digest.match(c.nativeClient.snapshotSha256).hasMatch() || !digest.match(c.nativeClient.inputBindingSha256).hasMatch())throw std::runtime_error("ECC checker did not return valid setup bindings");
            QString problem;if(!c.Validate(&problem))throw std::runtime_error(problem.toStdString());
            WriteHardwareSetupFile(QDir(fFiles.directory).filePath("ecc-plan.json"),bytes);
            if(!c.Save(c.filePath,&problem))throw std::runtime_error(problem.toStdString());
            RememberLocalBackend(c.filePath);
            // Retain immutable byte hashes; launch refuses all post-check changes.
            QJsonObject hashes;for(const auto &name:QDir(fFiles.directory).entryList(QDir::Files))if(name!="BUILD_INCOMPLETE")hashes[name]=HardwareSetupSha256(ReadHardwareSetupFile(QDir(fFiles.directory).filePath(name)));
            for(const auto &name:QDir(QDir(fFiles.directory).filePath("credentials")).entryList(QDir::Files))hashes["credentials/"+name]=HardwareSetupSha256(ReadHardwareSetupFile(QDir(fFiles.directory).filePath("credentials/"+name)));
            for(const auto &name:{QStringLiteral("getdaq-ecc-server"),QStringLiteral("getdaq-supervisor"),QStringLiteral("getdaq-receiver"),QStringLiteral("getdaq-supervisor-query")})hashes[QDir(fPrograms->text()).absoluteFilePath(name)]=HardwareSetupSha256(ReadHardwareSetupFile(QDir(fPrograms->text()).filePath(name),256*1024*1024));
            WriteHardwareSetupFile(QDir(fFiles.directory).filePath("checked-files.json"),QJsonDocument(hashes).toJson());
            QFile::remove(QDir(fFiles.directory).filePath("BUILD_INCOMPLETE"));Busy(false);fLaunch->setEnabled(true);fUse->setEnabled(true);
            Log(tr("Offline checks passed. Addresses and configuration bindings are filled automatically. Next: start services, use this setup, then Connect → Describe → Prepare → Configure in the main window. No hardware commands have been sent."));
            if(fRecoveryMode)StopOriginalForWarningRecovery();
            else if(fLaunchAfterCheck){fLaunchAfterCheck=false;Launch();}
            return;
        }
        if(step=="launch-check-node") {Execute("getdaq-ecc-server",{"--config",fFiles.eccFile,"--plan-config"},"launch-check-ecc");return;}
        if(step=="launch-check-ecc") {
            const auto plan=QJsonDocument::fromJson(bytes).object();
            if(plan["snapshot_sha256"].toString()!=fFiles.console.nativeClient.snapshotSha256 || plan["input_binding_sha256"].toString()!=fFiles.console.nativeClient.inputBindingSha256)throw std::runtime_error("ECC binding changed since review");
            for(int port:{fEccPort->value(),fNodePort->value()}){QTcpServer probe;if(!probe.listen(QHostAddress::LocalHost,quint16(port)))throw std::runtime_error("Control port is already in use; existing services will not be replaced");}
            auto start=[this](const QString &program,const QString &config,qint64 *pid) {
                QProcess process;
                auto environment=QProcessEnvironment::systemEnvironment();environment.remove("GETDAQ_READBACK_WARNING_POLICY_FILE");
                if(program=="getdaq-ecc-server" && QFileInfo::exists(QDir(fFiles.directory).filePath("readback-warning-policy.json")))environment.insert("GETDAQ_READBACK_WARNING_POLICY_FILE",QDir(fFiles.directory).filePath("readback-warning-policy.json"));
                process.setProcessEnvironment(environment);process.setProgram(QDir(fPrograms->text()).filePath(program));process.setArguments({"--config",config});process.setWorkingDirectory(fFiles.directory);
                const QString log=QDir(fFiles.directory).filePath("logs/"+program+".log");WriteHardwareSetupFile(log,QByteArray());process.setStandardOutputFile(log,QIODevice::Append);process.setStandardErrorFile(log,QIODevice::Append);
                if(!process.startDetached(pid))throw std::runtime_error(("Cannot launch "+program).toStdString());
                WriteHardwareSetupFile(QDir(fFiles.directory).filePath(program+".pid"),QByteArray::number(*pid)+'\n');
                Log(tr("Started %1, PID %2; log %3").arg(program).arg(*pid).arg(log));
            };
            fLaunched=true;start("getdaq-supervisor",fFiles.supervisorFile,&fSupervisorPid);start("getdaq-ecc-server",fFiles.eccFile,&fEccPid);
            WriteHardwareSetupFile(QDir(fFiles.directory).filePath("service-processes.json"),QJsonDocument(QJsonObject{{"supervisor_pid",double(fSupervisorPid)},{"ecc_pid",double(fEccPid)}}).toJson());
            fIdentityAttempts=0;QTimer::singleShot(500,this,&HardwareSetupWindow::VerifyServices);return;
        }
        if(step=="node-identity") {
            // Only identity GETs are made here; never Describe/Prepare/Start.
            fStep="identity";auto *client=new EccSystemClient(fFiles.console.nativeClient,{},this);
            connect(client,&EccSystemClient::IdentitySignal,this,[this,client](bool matched,QByteArray,QString error){
                client->deleteLater();
                if(!matched){if(fIdentityAttempts<10){QTimer::singleShot(500,this,&HardwareSetupWindow::VerifyServices);return;}Fail(tr("Service identity not ready or mismatched: %1. Services/logs retained; use saved envoy.yml after checking startup.").arg(error));return;}
                fStep.clear();Busy(false);fLaunch->setEnabled(true);fUse->setEnabled(true);Log(tr("ECC and supervisor identities verified. Services stay running when this window closes. Use in Run Setup, then Connect."));if(fRecoveryMode){Log(tr("New Idle baseline created. Original audits are preserved; no Describe/Prepare/Start was sent."));accept();}
            });
            client->QueryIdentity();return;
        }
    }catch(const std::exception &e){Fail(QString::fromUtf8(e.what()));}
}
void HardwareSetupWindow::StartOrCheckServices() {
    if(fBusy)return;
    if((!fSavedDirectory.isEmpty() && fFiles.directory.isEmpty() && fSavedInputsDigest==InputsDigest(false)) ||
       (!fFiles.directory.isEmpty() && fCheckedDigest==InputsDigest())){Launch();return;}
    QStringList missing;
    if(fExperiment->text().trimmed().isEmpty())missing<<tr("electronics template");
    if(fCallback->text().trimmed().isEmpty())missing<<tr("this computer's control IPv4");
    if(!missing.isEmpty()){Log(tr("Cannot start ECC yet. Fill in: %1. No services or hardware commands were started.").arg(missing.join(", ")));return;}
    if(!fStopped->isChecked()){Log(tr("For a new or edited setup, confirm exclusive control and stopped acquisition below the board table, then click Start ECC + data service again. Services can start without a connected board."));return;}
    if(fLoaded.isEmpty()){LoadBoards();if(fLoaded.isEmpty())return;}
    Log(tr("Saving and checking the new/edited configuration before starting ECC and data services…"));
    fLaunchAfterCheck=true;Generate();if(!fBusy)fLaunchAfterCheck=false;
}
void HardwareSetupWindow::Launch() {
    if(fLaunched || (!fSavedDirectory.isEmpty() && fFiles.directory.isEmpty())) {
        if(!fLaunched && fSavedInputsDigest!=InputsDigest(false)){Fail(tr("Settings changed. Save configuration files and check before starting the edited setup."));return;}
        Busy(true);
        const auto config=fLaunched?fFiles.console:fBase;
        auto *services=new LocalDeploymentServices(config,fPrograms->text().trimmed(),this);
        connect(services,&LocalDeploymentServices::Log,this,&HardwareSetupWindow::Log);
        connect(services,&LocalDeploymentServices::Failed,this,[this,services](const QString &error){services->deleteLater();Fail(error);fLaunch->setEnabled(true);});
        connect(services,&LocalDeploymentServices::Ready,this,[this,services,config]{
            services->deleteLater();fFiles.console=config;fCheckedDigest=InputsDigest();Busy(false);fLaunch->setEnabled(true);fUse->setEnabled(true);
            Log(tr("Saved setup: ECC and data service identities verified. Use in Run Setup, then Connect. No Describe, Prepare or Start was sent."));
        });
        services->EnsureStarted();return;
    }
    if(fCheckedDigest!=InputsDigest()){Fail(tr("Setup fields changed after validation; generate a fresh deployment before launch."));return;}
    try {
        const auto hashes=QJsonDocument::fromJson(ReadHardwareSetupFile(QDir(fFiles.directory).filePath("checked-files.json"))).object();
        if(hashes.isEmpty())throw std::runtime_error("Missing checked deployment manifest");
        for(auto it=hashes.begin();it!=hashes.end();++it)if(HardwareSetupSha256(ReadHardwareSetupFile(QDir(fFiles.directory).filePath(it.key()),256*1024*1024))!=it.value().toString())throw std::runtime_error("A checked setup file or program changed; launch refused");
        LocalDeploymentServices preflight(fFiles.console,fPrograms->text().trimmed());preflight.ValidateStartupConfiguration();
        Busy(true);Execute("getdaq-supervisor",{"--config",fFiles.supervisorFile,"--check-config"},"launch-check-node");
    }catch(const std::exception &e){Fail(QString::fromUtf8(e.what()));}
}

void HardwareSetupWindow::VerifyServices() {
    ++fIdentityAttempts;
    const auto remote=fFiles.ecc["remote_receivers"].toObject();
    const auto endpoint=remote["endpoints"].toArray()[0].toObject();
    Execute("getdaq-supervisor-query",{"identity","--host","127.0.0.1","--port",QString::number(fNodePort->value()),
        "--credential-file",endpoint["credential_file"].toString(),"--expected-agent","data-node-local",
        "--controller","ecc-local","--expected-plan",endpoint["plan_sha256"].toString()},"node-identity");
}

void HardwareSetupWindow::PopulateBoards() {
    fBoards->setRowCount(fLoaded.size());
    for(int i=0;i<fLoaded.size();++i) {
        const auto &b=fLoaded[i];
        const QStringList values={b.id,b.mutant?QStringLiteral("mutant"):(b.enabled?QStringLiteral("cobo"):QStringLiteral("cobo (disabled)")),b.address,QString::number(b.controlPort),
            b.descriptionFile,b.receiverAddress,QString::number(b.dataPort),QString::number(b.statusPort),b.nodeObject,b.templateId};
        for(int j=0;j<values.size();++j) {
            auto *item=new QTableWidgetItem(values[j]);
            if(j==1 || j==9 || (b.mutant && !b.receivesData && j>=5 && j<=7))item->setFlags(item->flags()&~Qt::ItemIsEditable);
            if(j==1 && !b.mutant){item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(b.enabled?Qt::Checked:Qt::Unchecked);item->setToolTip(tr("Checked: active CoBo. Unchecked: retained in topology, no acquisition."));}
            fBoards->setItem(i,j,item);
        }
        if(fProjectDraft && !fReceivingHosts.isEmpty()) {
            const auto selected=b.receiverNodeId.isEmpty()?fDefaultReceivingHost:b.receiverNodeId;
            auto *item=new QTableWidgetItem(selected);fBoards->setItem(i,10,item);auto *hosts=new QComboBox(fBoards);
            for(const auto &value:fReceivingHosts){const auto node=value.toObject();hosts->addItem(node["name"].toString(),node["id"].toString());}
            hosts->setCurrentIndex(hosts->findData(selected));hosts->setEnabled(b.receivesData);fBoards->setCellWidget(i,10,hosts);
            connect(hosts,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this,hosts,i]{if(fBoards->cellWidget(i,10)!=hosts || !fBoards->item(i,10))return;fBoards->item(i,10)->setText(hosts->currentData().toString());for(const auto &value:fReceivingHosts){const auto node=value.toObject();if(node["id"].toString()!=hosts->currentData().toString())continue;const auto addresses=node["inventory"].toObject()["addresses"].toArray();const auto preferred=qEnvironmentVariable("GETDAQ_RECEIVER_IPV4");const auto address=addresses.contains(preferred)?preferred:QUrl(node["url"].toString()).host();if(fBoards->item(i,5))fBoards->item(i,5)->setText(address);}});
        }
    }
}
void HardwareSetupWindow::ReadRows() {
    if(fBoards->rowCount()!=fLoaded.size())throw std::runtime_error("Board table and electronics templates differ; reload setup");
    for(int i=0;i<fLoaded.size();++i) {
        auto &b=fLoaded[i];b.id=Cell(fBoards,i,0);
        b.address=Cell(fBoards,i,2);b.controlPort=Number(fBoards,i,3);b.descriptionFile=Cell(fBoards,i,4);
        b.receiverAddress=Cell(fBoards,i,5);b.dataPort=Number(fBoards,i,6);b.statusPort=Number(fBoards,i,7);b.nodeObject=Cell(fBoards,i,8);
        if(fProjectDraft && fBoards->columnCount()>10)b.receiverNodeId=Cell(fBoards,i,10);
        if(!b.mutant){b.enabled=fBoards->item(i,1)->checkState()==Qt::Checked;b.receivesData=b.enabled;}
    }
}
void HardwareSetupWindow::AddCoBo() {
    try {
        ReadRows();const int selected=fBoards->currentRow();
        if(selected<0 || fLoaded[selected].mutant){Log(tr("Select a CoBo to copy its reviewed electronics and hardware settings first."));return;}
        bool ok=false;const QString id=QInputDialog::getText(this,tr("Add CoBo"),tr("New board ID (electronics settings copied from the selected CoBo)"),QLineEdit::Normal,QString("CoBo_%1").arg(fLoaded.size()),&ok).trimmed();
        if(!ok || id.isEmpty())return;
        for(const auto &b:fLoaded)if(b.id==id)throw std::runtime_error("Board ID already exists");
        auto b=fLoaded[selected];b.id=id;b.address.clear();
        QSet<int> ports{fEccPort->value(),fNodePort->value()};
        for(int i=0;i<=fLoaded.size();++i)ports.insert(fCallbackPort->value()+i);
        for(const auto &old:fLoaded){ports.insert(old.dataPort);ports.insert(old.statusPort);}
        int port=23001;while(port<65535 && (ports.contains(port)||ports.contains(port+1)))port+=2;
        if(port>=65535)throw std::runtime_error("No free receiver port pair");
        b.dataPort=port;b.statusPort=port+1;fLoaded.append(b);PopulateBoards();fBoards->selectRow(fLoaded.size()-1);
        fLaunch->setEnabled(true);fUse->setEnabled(fProjectDraft);
        Log(tr("Added %1 from electronics template %2. Enter its actual board IPv4; data/status ports %3/%4.").arg(id,b.templateId).arg(b.dataPort).arg(b.statusPort));
    }catch(const std::exception &e){Log(QString::fromUtf8(e.what()));}
}
void HardwareSetupWindow::RemoveBoard() {
    try {
        const int row=fBoards->currentRow();if(row<0)return;ReadRows();
        fLoaded.removeAt(row);PopulateBoards();fLaunch->setEnabled(true);fUse->setEnabled(fProjectDraft);
        Log(tr("Board removed from this draft. Existing service configuration is unchanged until a new configuration set is saved and selected."));
    }catch(const std::exception &e){Log(QString::fromUtf8(e.what()));}
}
void HardwareSetupWindow::RestoreSetup(const QString &directory) {
    const auto setup=ReadHardwareDeploymentSetup(directory);
    fExperiment->setText(setup.experimentFile);fProfile->setText(setup.profileFile);
    // Programs always come from the currently running installation.
    fData->setText(setup.dataDirectory);fCallback->setText(setup.callbackAddress);
    fEccPort->setValue(setup.eccPort);fNodePort->setValue(setup.supervisorPort);fCallbackPort->setValue(setup.callbackPort);
    {const QSignalBlocker blocker(fDelayWarning);fDelayWarning->setChecked(setup.acceptLegacyDelayReadback);}
    fLoaded=setup.boards;PopulateBoards();
    Log(tr("Loaded actual saved configuration from %1. Board IPs and receiver addresses below come from these files.").arg(directory));
    Log(tr("Save creates a new configuration set at %1; the existing set and its operation records are preserved.").arg(fRoot->text()));
    if(LocalDeploymentServices::IsLocalDeployment(fBase)) {
        fSavedDirectory=directory;fSavedInputsDigest=InputsDigest(false);
        fLaunch->setText(tr("Start / check ECC + data service"));fLaunch->setEnabled(true);
        Log(tr("Existing saved setup is ready to start/check services directly. Saving a new configuration is only needed after editing settings."));
    }
}

void HardwareSetupWindow::NewParallelInstance() {
    try {
        ReadRows();if(fLoaded.isEmpty())throw std::runtime_error("Load an electronics template first");
        QSet<int> reserved;QStringList files;
        for(const auto &name:{QString("getdaq-ecc-server"),QString("getdaq-supervisor")})files+=LocalServiceConfigurations(QDir(fPrograms->text()).filePath(name));
        const auto original=QFileInfo(fBase.filePath).absolutePath();
        for(const auto &name:{"ecc.json","supervisor.json"})if(QFileInfo::exists(original+'/'+name))files.append(original+'/'+name);
        files.removeDuplicates();
        for(const auto &file:files) {
            const auto object=QJsonDocument::fromJson(ReadHardwareSetupFile(file)).object();reserved.insert(object["port"].toInt());
            for(const auto &value:object["receiver_plan"].toObject()["sources"].toArray()) {
                const auto source=value.toObject();for(const auto &key:{"data_port","status_port","export_port"})reserved.insert(source[key].toInt());
            }
            for(const auto &value:object["cobo_policies"].toArray()) {
                const auto match=QRegularExpression("(?:^| )-p ([0-9]+)(?: |$)").match(value.toObject()["callback_endpoints"].toString());
                if(match.hasMatch())reserved.insert(match.captured(1).toInt());
            }
        }
        int selected=0;
        for(int base=24000;base<=64000;base+=1000) {
            QSet<int> ports{base,base+1};
            for(int i=0;i<fLoaded.size();++i) {
                if(!fLoaded[i].mutant)ports.insert(base+10+i);
                if(fLoaded[i].receivesData){ports.insert(base+300+2*i);ports.insert(base+301+2*i);}
            }
            bool available=true;
            for(int port:ports) {
                if(reserved.contains(port)){available=false;break;}
                QTcpServer probe;if(!probe.listen(QHostAddress::AnyIPv4,quint16(port))){available=false;break;}
            }
            if(available){selected=base;break;}
        }
        if(!selected)throw std::runtime_error("No suggested port range is available; choose ports manually");
        fRoot->setText(NewDirectory());fData->setText(QDir(fRoot->text()).filePath("data"));
        fEccPort->setValue(selected);fNodePort->setValue(selected+1);fCallbackPort->setValue(selected+10);
        for(int i=0;i<fLoaded.size();++i){fLoaded[i].address.clear();fLoaded[i].dataPort=selected+300+2*i;fLoaded[i].statusPort=selected+301+2*i;}
        PopulateBoards();fFiles=HardwareDeploymentFiles();fCheckedDigest.clear();fSavedDirectory.clear();fSavedInputsDigest.clear();fLaunch->setEnabled(true);fUse->setEnabled(fProjectDraft);fStopped->setChecked(false);
        {const QSignalBlocker blocker(fDelayWarning);fDelayWarning->setChecked(false);}
        Log(tr("New independent instance: ECC %1, supervisor %2. Enter the distinct boards' IPs. Each instance keeps separate configuration, state, control records and data directories. Suggested ports are checked again at service startup.").arg(selected).arg(selected+1));
    }catch(const std::exception &e){Log(QString::fromUtf8(e.what()));}
}

void HardwareSetupWindow::StartAcceptedWarningRecovery() {
    if(fBusy)return;
    if(QMessageBox::question(this,tr("Recover an empty Idle session / accept compatibility warning"),
        tr("Restore a fresh Idle control session? For the specific early delay-readback failure, accept ctrl.readDataDelay.AGET0–3 returning 255 as unverified compatibility warnings; for a known Idle restart, retain the existing policy.\n\nThe software will verify this exact early Prepare failure, stopped production, AsAd power off and no receiver session. If verified, it will retain the old audit, stop this instance's backend and create a fresh Idle control session with the accepted warning policy. It will not replay Prepare or start acquisition.\n\nConfirm exclusive control of this board and continue?"),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    fRecoveryMode=true;Busy(true);
    auto *probe=new EarlyWarningRecovery(fBase,fPrograms->text(),this);
    connect(probe,&EarlyWarningRecovery::Log,this,&HardwareSetupWindow::Log);
    connect(probe,&EarlyWarningRecovery::Failed,this,&HardwareSetupWindow::Fail);
    connect(probe,&EarlyWarningRecovery::Verified,this,[this](const QJsonObject &proof){
        fRecoveryProof=proof;fRecoveryClock.start();
        if(proof["requires_compatibility_warning"].toBool()){const QSignalBlocker blocker(fDelayWarning);fDelayWarning->setChecked(true);}
        fData->setText(QDir(fRoot->text()).filePath("data"));fStopped->setChecked(true);Busy(false);Generate();
    });probe->Start();
}
void HardwareSetupWindow::StopOriginalForWarningRecovery() {
    try {
        if(!fRecoveryClock.isValid() || fRecoveryClock.elapsed()>10000)throw std::runtime_error("Recovery checks expired before backend shutdown; original backend retained");
        const auto files=fRecoveryProof["original_files"].toObject();
        for(auto i=files.begin();i!=files.end();++i)if(HardwareSetupSha256(ReadHardwareSetupFile(i.key()))!=i.value().toString())throw std::runtime_error("Original failure evidence changed; backend retained");
        Busy(true);auto *services=new LocalDeploymentServices(fBase,fPrograms->text(),this);
        connect(services,&LocalDeploymentServices::Log,this,&HardwareSetupWindow::Log);
        connect(services,&LocalDeploymentServices::Failed,this,&HardwareSetupWindow::Fail);
        connect(services,&LocalDeploymentServices::ForceRequired,this,[this,services](const QString &names){
            if(QMessageBox::warning(this,tr("Old backend did not exit"),tr("Force terminate only these verified stopped-instance processes?\n%1").arg(names),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes)services->ForceRemaining();else services->CancelForce();
        });
        connect(services,&LocalDeploymentServices::Stopped,this,[this]{Busy(false);fLaunch->setEnabled(true);Launch();});
        services->StopExplicitly();
    }catch(const std::exception &e){Fail(QString::fromUtf8(e.what()));}
}
