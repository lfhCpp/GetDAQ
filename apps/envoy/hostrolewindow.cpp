#include "hostrolewindow.h"
#include "unifiedeccwindow.h"
#include "deploymentcontroller.h"
#include "applicationinstance.h"
#include <QComboBox>
#include <QStackedWidget>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QNetworkInterface>
#include <QFile>
#include <QFileDialog>
#include <QDir>
#include <QCoreApplication>
#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QMessageBox>
#include <QDateTime>
#include <QThread>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
using namespace Deployment;
namespace {
QString RoleKey(){return StartupConfigurationKey()+"/machine-role";}
QString NodeRoot(){return PreferredNodeCacheDirectory(QSettings().value(StartupConfigurationKey()+"/slave-cache-directory",DefaultNodeCacheDirectory()).toString());}
QString DefaultAddress(){const auto hint=qEnvironmentVariable("GETDAQ_CONTROL_IPV4");const auto addresses=QNetworkInterface::allAddresses();if(!hint.isEmpty() && addresses.contains(QHostAddress(hint)))return hint;for(const auto &a:addresses)if(a.protocol()==QAbstractSocket::IPv4Protocol && !a.isLoopback())return a.toString();return "127.0.0.1";}
QJsonObject Settings(const QString &root){if(QFile::exists(root+"/node.json"))return Parse(Read(root+"/node.json"));return {{"bind",DefaultAddress()},{"port",24260},{"data_directory",DefaultWorkspaceDirectory()+"/data"}};}
using Result=QPair<QJsonObject,QString>;
Result Attempt(std::function<QJsonObject()> task){try{return {task(),{}};}catch(const std::exception &e){return {{},QString::fromUtf8(e.what())};}}
}
QString HostRoleWindow::SavedRole(){return QSettings().value(RoleKey(),"master").toString()=="slave"?QString("slave"):QString("master");}
QString HostRoleWindow::ButtonText(){return SavedRole()=="slave"?tr("Slave ▾"):tr("Master ▾");}
void HostRoleWindow::ResumeSlaveAgent(QObject *owner,std::function<void(QString)> report){
    if(SavedRole()!="slave")return;
    const auto root=NodeRoot(),programs=QCoreApplication::applicationDirPath();
    // Resume only an explicitly enabled node; selecting a role alone does not enroll it.
    if(!QFile::exists(root+"/node.json"))return;
    auto *watcher=new QFutureWatcher<Result>(owner);
    QObject::connect(watcher,&QFutureWatcher<Result>::finished,owner,[watcher,report]{const auto result=watcher->result();watcher->deleteLater();report(result.second.isEmpty()?HostRoleWindow::tr("Slave agent ready. The master sends configuration automatically. Acquisition has not started."):HostRoleWindow::tr("Slave agent not ready: %1").arg(result.second));});
    watcher->setFuture(QtConcurrent::run([=]{return Attempt([=]{const auto s=Settings(root);return EnsureLocalNode(root,programs,s["bind"].toString(),s["port"].toInt(),s["data_directory"].toString());});}));
}
HostRoleWindow::HostRoleWindow(const RunConfig &base,QWidget *parent):QDialog(parent),fBase(base),fRoot(NodeRoot()){
    setObjectName("HostRoleWindow");setWindowTitle(tr("Master / Slave"));resize(1120,850);
    auto *layout=new QVBoxLayout(this);auto *row=new QHBoxLayout;row->addWidget(new QLabel(tr("This computer is"),this));fRole=new QComboBox(this);fRole->setObjectName("MachineRoleSelector");fRole->addItem(tr("Master: manage configuration, ECC and data services"),"master");fRole->addItem(tr("Slave: receive configuration and run backend services"),"slave");row->addWidget(fRole,1);layout->addLayout(row);
    fPages=new QStackedWidget(this);layout->addWidget(fPages);fPages->addWidget(new QWidget(this));
    auto *slave=new QWidget(this);auto *slaveLayout=new QVBoxLayout(slave);auto *help=new QLabel(tr("1. Choose local cache and data folders below. 2. Enable this slave. 3. Copy its pairing code to Add slave on the master. Select experiment files on the master only; this slave receives and caches them automatically."),slave);help->setWordWrap(true);slaveLayout->addWidget(help);
    fStatus=new QLabel(tr("Not enabled yet. Click below to start with the default settings."),slave);fStatus->setObjectName("SlaveAgentStatus");fStatus->setWordWrap(true);slaveLayout->addWidget(fStatus);
    auto *actions=new QHBoxLayout;fStart=new QPushButton(tr("Enable / check slave"),slave);fStart->setObjectName("StartSlaveAgent");fCopy=new QPushButton(tr("Copy pairing code"),slave);fCopy->setObjectName("CopySlaveInvitation");fCopy->setEnabled(false);actions->addWidget(fStart);actions->addWidget(fCopy);auto *renew=new QPushButton(tr("Renew / re-pair…"),slave);renew->setObjectName("RenewSlavePairing");actions->addWidget(renew);connect(renew,&QPushButton::clicked,this,&HostRoleWindow::RenewPairing);actions->addStretch();slaveLayout->addLayout(actions);
    fInvitation=new QLineEdit(slave);fInvitation->setObjectName("SlavePairingInvitation");fInvitation->setReadOnly(true);fInvitation->setPlaceholderText(tr("Generated when enabled; no code needed after pairing"));slaveLayout->addWidget(fInvitation);
    auto *paths=new QFormLayout;fCache=new QLineEdit(fRoot,slave);fCache->setObjectName("SlaveCacheDirectory");fData=new QLineEdit(slave);fData->setObjectName("SlaveDataDirectory");
    auto pathRow=[&](QLineEdit *edit,bool cache){auto *row=new QWidget(slave);auto *line=new QHBoxLayout(row);line->setContentsMargins(0,0,0,0);line->addWidget(edit);auto *browse=new QPushButton(tr("Browse…"),row);line->addWidget(browse);connect(browse,&QPushButton::clicked,this,[this,edit,cache]{if(edit->isReadOnly())return;const auto directory=QFileDialog::getExistingDirectory(this,tr("Choose local directory"),edit->text());if(!directory.isEmpty())edit->setText(cache?QDir(directory).filePath("getdaq-node-cache"):directory);});return row;};
    paths->addRow(tr("Configuration cache (on this slave)"),pathRow(fCache,true));paths->addRow(tr("Project data folder (on this slave)"),pathRow(fData,false));slaveLayout->addLayout(paths);
    auto *pathHelp=new QLabel(tr("These are folders on this slave. ECC files are cached in the software cache before project-folder synchronization. ECC, receiver and Sentry start in the selected project folder; received configuration is copied there automatically. Sentry archives them there under <experiment>/run_NNNN after Stop. The GUI Run records directory is separate. Paths are saved when enabled; existing files are never moved by this window."),slave);pathHelp->setWordWrap(true);slaveLayout->addWidget(pathHelp);
    auto *advanced=new QCheckBox(tr("Management network settings (optional)"),slave);slaveLayout->addWidget(advanced);auto *settings=new QWidget(slave);auto *form=new QFormLayout(settings);fAddress=new QLineEdit(slave);fAddress->setObjectName("SlaveManagementAddress");fPort=new QSpinBox(slave);fPort->setObjectName("SlaveManagementPort");fPort->setRange(1,65535);QJsonObject saved;try{saved=Settings(fRoot);}catch(const std::exception &error){saved={{"bind",DefaultAddress()},{"port",24260},{"data_directory",DefaultWorkspaceDirectory()+"/data"}};fStatus->setText(tr("Cannot read saved node settings: %1").arg(QString::fromUtf8(error.what())));}fAddress->setText(saved["bind"].toString());fPort->setValue(saved["port"].toInt());fData->setText(saved["data_directory"].toString());form->addRow(tr("Local management IP"),fAddress);form->addRow(tr("Management port (default: 24260)"),fPort);slaveLayout->addWidget(settings);settings->hide();connect(advanced,&QCheckBox::toggled,settings,&QWidget::setVisible);
    if(QFile::exists(fRoot+"/node.json")){fAddress->setReadOnly(true);fPort->setEnabled(false);fData->setReadOnly(true);fCache->setReadOnly(true);}
    fRemoveEntry=new QPushButton(tr("Remove selected inactive entry"),slave);fRemoveEntry->setObjectName("RemoveSlaveSavedInstance");fRemoveEntry->setToolTip(tr("Remove an inactive item from this slave's list. Running instances, backend audit records, configuration files and raw data are retained."));fRemoveEntry->setEnabled(false);
    fInstances=new QTableWidget(0,6,slave);fInstances->setObjectName("SlaveCachedInstances");fInstances->setHorizontalHeaderLabels({tr("Instance"),tr("Type"),tr("Process"),tr("Local record"),tr("ECC configuration cache"),tr("Raw data / working directory")});fInstances->setEditTriggers(QAbstractItemView::NoEditTriggers);fInstances->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);fInstances->horizontalHeader()->setStretchLastSection(true);slaveLayout->addWidget(fInstances);slaveLayout->addWidget(fRemoveEntry);fInstances->setSelectionBehavior(QAbstractItemView::SelectRows);fInstances->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(fInstances,&QTableWidget::itemSelectionChanged,this,[this]{const auto *item=fInstances->item(fInstances->currentRow(),0);fRemoveEntry->setEnabled(item && item->data(Qt::UserRole).toJsonObject()["actual_pids"].toArray().isEmpty());});
    connect(fRemoveEntry,&QPushButton::clicked,this,[this]{const auto *item=fInstances->item(fInstances->currentRow(),0);if(!item)return;const auto instance=item->data(Qt::UserRole).toJsonObject();if(!instance["actual_pids"].toArray().isEmpty())return;const auto key=StartupConfigurationKey()+"/slave-hidden-instances/"+fRoot;auto hidden=QSettings().value(key).toStringList();const auto identity=instance["instance_id"].toString()+':'+instance["release_id"].toString()+':'+instance["preparation_id"].toString();if(!hidden.contains(identity))hidden.append(identity);QSettings().setValue(key,hidden);ShowNode(fNode);});
    auto *close=new QPushButton(tr("Close (keep services running)"),slave);slaveLayout->addWidget(close);connect(close,&QPushButton::clicked,this,&HostRoleWindow::reject);fPages->addWidget(slave);
    connect(fStart,&QPushButton::clicked,this,&HostRoleWindow::StartSlave);connect(fCopy,&QPushButton::clicked,this,[this]{if(fInvitation->text().isEmpty())return;QApplication::clipboard()->setText(fInvitation->text());fInvitation->setFocus();fInvitation->selectAll();fCopy->setText(tr("Copied"));QTimer::singleShot(1500,this,[this]{fCopy->setText(tr("Copy pairing code"));});});connect(fRole,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&HostRoleWindow::SelectRole);
    const int role=SavedRole()=="slave"?1:0;{const QSignalBlocker blocker(fRole);fRole->setCurrentIndex(role);}SelectRole(role);
    auto *timer=new QTimer(this);timer->setInterval(3000);connect(timer,&QTimer::timeout,this,&HostRoleWindow::ObserveSlave);timer->start();
}
void HostRoleWindow::SelectRole(int index){
    if(fBusy || (fMaster && fMaster->Busy())){const QSignalBlocker blocker(fRole);fRole->setCurrentIndex(fPages->currentIndex());return;}QSettings().setValue(RoleKey(),index==1?"slave":"master");
    if(index==0 && !fMaster){fMaster=new UnifiedEccWindow(fBase,this);fMaster->setWindowFlags(Qt::Widget);auto *old=fPages->widget(0);fPages->removeWidget(old);old->deleteLater();fPages->insertWidget(0,fMaster);connect(fMaster,&QDialog::accepted,this,[this]{fSelected=fMaster->Configuration();accept();});connect(fMaster,&QDialog::rejected,this,&HostRoleWindow::reject);}
    fPages->setCurrentIndex(index);
    if(index==1 && QFile::exists(fRoot+"/node.json") && fNode.isEmpty())QTimer::singleShot(0,this,&HostRoleWindow::StartSlave);
}
void HostRoleWindow::StartSlave(){
    if(fBusy || fRole->currentIndex()!=1){return;}
    fBusy=true;fCloseAfterWait=false;fCanceled=std::make_shared<std::atomic_bool>(false);const auto canceled=fCanceled;for(auto *field:QList<QWidget *>{fCache,fData,fAddress,fPort})field->setEnabled(false);fStart->setEnabled(false);fRole->setEnabled(false);fCopy->setEnabled(false);fStatus->setText(tr("Starting and checking the local slave agent…"));const auto root=fCache->text().trimmed(),programs=QCoreApplication::applicationDirPath(),address=fAddress->text().trimmed(),data=fData->text().trimmed();fRoot=root;const int port=fPort->value();auto *watcher=new QFutureWatcher<Result>(this);connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher]{const auto result=watcher->result();watcher->deleteLater();fBusy=false;fStart->setEnabled(true);fRole->setEnabled(true);for(auto *field:QList<QWidget *>{fCache,fData,fAddress,fPort})field->setEnabled(true);if(!result.second.isEmpty()){fStatus->setText(tr("Could not enable slave: %1").arg(result.second));if(fCloseAfterWait)QDialog::reject();return;}fAddress->setReadOnly(true);fPort->setEnabled(false);fData->setReadOnly(true);fCache->setReadOnly(true);QSettings().setValue(StartupConfigurationKey()+"/slave-cache-directory",fRoot);ShowNode(result.first);if(fCloseAfterWait)QDialog::reject();});watcher->setFuture(QtConcurrent::run([=]{OperationScope scope(canceled,30000);return Attempt([=]{return EnsureLocalNode(root,programs,address,port,data);});}));
}
void HostRoleWindow::ShowNode(const QJsonObject &node){
    fNode=node;const bool paired=QFile::exists(fRoot+"/master.json");if(auto *renew=findChild<QPushButton *>("RenewSlavePairing")){renew->setText(paired?tr("Remove master connection…"):tr("Renew pairing code"));renew->setToolTip(tr("Revoke the saved master connection and generate a fresh pairing code. Accepted work finishes; backend processes and data are retained."));}fStatus->setText(paired?tr("Slave agent online and paired. Configuration is cached automatically. Check backend and acquisition status on the master."):tr("Slave agent online, waiting for pairing. Copy the code to the master once (valid for 15 minutes)."));QString invitation=node["invitation"].toString();bool valid=false;
    if(!paired && QFile::exists(fRoot+"/pairing.json")){const auto saved=Parse(Read(fRoot+"/pairing.json"));valid=saved["expires_ms"].toDouble()>QDateTime::currentMSecsSinceEpoch();if(valid)invitation="getdaq-pair:"+QString::fromLatin1(Json(saved).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals));}
    fInvitation->setText(paired?QString():invitation);fCopy->setEnabled(!paired && valid && !invitation.isEmpty());
    if(paired){const auto master=Parse(Read(fRoot+"/master.json"));fStatus->setText(tr("Paired with %1. The old code was consumed. Use Remove master connection to change master; backends and data are retained.").arg(master["repository_url"].toString()));}
    if(paired && node["inventory"].toObject().contains("master_online") && !node["inventory"].toObject()["master_online"].toBool())fStatus->setText(fStatus->text()+tr(" Master currently unreachable; saved pairing is retained."));
    else if(!valid && !paired)fStatus->setText(tr("No valid pairing code. Click Renew pairing code to generate a fresh code."));
    QJsonArray instances;const auto hidden=QSettings().value(StartupConfigurationKey()+"/slave-hidden-instances/"+fRoot).toStringList();for(const auto &entry:node["inventory"].toObject()["instances"].toArray()){const auto instance=entry.toObject();const auto identity=instance["instance_id"].toString()+':'+instance["release_id"].toString()+':'+instance["preparation_id"].toString();if(instance["actual_pids"].toArray().isEmpty() && hidden.contains(identity))continue;instances.append(instance);}if(fInstances->rowCount()!=instances.size())fInstances->setRowCount(instances.size());for(int row=0;row<instances.size();++row){const auto instance=instances[row].toObject();const bool running=!instance["actual_pids"].toArray().isEmpty();const QStringList values={instance["instance_id"].toString(),instance["role"].toString(),running?tr("Process found (check acquisition on master)"):tr("No matching process"),instance["phase"].toString()+(instance["configuration_pending"].toBool()?tr(" / configuration pending"):QString()),instance["role"]=="ecc"?instance["configuration_repository"].toString():QString(),instance["data_directory"].toString(instance["working_directory"].toString())};for(int col=0;col<values.size();++col){auto *item=fInstances->item(row,col);if(!item){item=new QTableWidgetItem;fInstances->setItem(row,col,item);}if(item->text()!=values[col]){item->setText(values[col]);item->setToolTip(values[col]);}if(col==0)item->setData(Qt::UserRole,instance);}}fRemoveEntry->setEnabled(fInstances->currentRow()>=0 && fInstances->item(fInstances->currentRow(),0) && fInstances->item(fInstances->currentRow(),0)->data(Qt::UserRole).toJsonObject()["actual_pids"].toArray().isEmpty());
}
void HostRoleWindow::ObserveSlave(){
    if(fBusy || fObserving || fRole->currentIndex()!=1 || fNode.isEmpty())return;
    fObserving=true;
    const auto node=fNode;const auto root=fRoot;
    auto *watcher=new QFutureWatcher<Result>(this);
    connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,root]{
        const auto result=watcher->result();watcher->deleteLater();fObserving=false;
        // A periodic read must not disable controls or repaint an unrelated role.
        if(fBusy || fRole->currentIndex()!=1 || root!=fRoot)return;
        if(!result.second.isEmpty()){
            fStatus->setText(tr("Slave agent currently unreachable: %1. Last observed instance paths are retained below.").arg(result.second));
            return;
        }
        auto updated=fNode;updated["inventory"]=result.first;ShowNode(updated);
    });
    watcher->setFuture(QtConcurrent::run([node,root]{OperationScope scope({},10000);return Attempt([node,root]{auto inventory=NodeInventory(node);if(QFile::exists(root+"/master.json")){try{const auto master=Parse(Read(root+"/master.json"));Endpoint endpoint{master["repository_url"].toString(),master["certificate_sha256"].toString(),Secret(master["credential_file"].toString())};inventory["master_online"]=Get(endpoint,"/v1/identity")["repository_id"]==master["repository_id"];}catch(...){inventory["master_online"]=false;}}return inventory;});}));
}
void HostRoleWindow::reject(){if(fBusy){fCloseAfterWait=true;if(fCanceled)fCanceled->store(true);fStatus->setText(tr("Stopping local waiting before closing. Independent services are retained."));return;}if(fMaster && fMaster->Busy()){fMaster->RequestClose();return;}QDialog::reject();}

void HostRoleWindow::RenewPairing(){
 if(fBusy || fRoot.isEmpty())return;const bool paired=QFile::exists(fRoot+"/master.json");
 if(paired && QMessageBox::warning(this,tr("Change master pairing"),tr("Revoke the previous master's management credential and generate a new pairing code? Accepted work finishes first. Backend processes and data are retained."),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
 const auto root=fRoot,program=QCoreApplication::applicationDirPath()+"/getdaq-node-agent";fBusy=true;fStart->setEnabled(false);auto *watcher=new QFutureWatcher<Result>(this);
 connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher]{const auto result=watcher->result();watcher->deleteLater();fBusy=false;fStart->setEnabled(true);if(!result.second.isEmpty()){fStatus->setText(result.second);return;}StartSlave();});
 watcher->setFuture(QtConcurrent::run([=]{return Attempt([=]{Run(program,{"--config",root+"/node.json",paired?"--request-repair":"--renew-pairing"});for(int i=0;i<100;++i){if(QFile::exists(root+"/pairing.json") && !QFile::exists(root+"/re-pair.request"))return QJsonObject();QThread::msleep(100);}throw std::runtime_error("Waiting for accepted work to finish before re-pairing; check this slave again shortly.");return QJsonObject();});}));
}
