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
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
using namespace Deployment;
namespace {
QString RoleKey(){return StartupConfigurationKey()+"/machine-role";}
QString NodeRoot(){return QSettings().value(StartupConfigurationKey()+"/slave-cache-directory",QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/local-node").toString();}
QString DefaultAddress(){const auto hint=qEnvironmentVariable("GETDAQ_CONTROL_IPV4");const auto addresses=QNetworkInterface::allAddresses();if(!hint.isEmpty() && addresses.contains(QHostAddress(hint)))return hint;for(const auto &a:addresses)if(a.protocol()==QAbstractSocket::IPv4Protocol && !a.isLoopback())return a.toString();return "127.0.0.1";}
QJsonObject Settings(const QString &root){if(QFile::exists(root+"/node.json"))return Parse(Read(root+"/node.json"));return {{"bind",DefaultAddress()},{"port",24260},{"data_directory",QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/data"}};}
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
    auto *actions=new QHBoxLayout;fStart=new QPushButton(tr("Enable / check slave"),slave);fStart->setObjectName("StartSlaveAgent");fCopy=new QPushButton(tr("Copy pairing code"),slave);fCopy->setObjectName("CopySlaveInvitation");fCopy->setEnabled(false);actions->addWidget(fStart);actions->addWidget(fCopy);actions->addStretch();slaveLayout->addLayout(actions);
    fInvitation=new QLineEdit(slave);fInvitation->setObjectName("SlavePairingInvitation");fInvitation->setReadOnly(true);fInvitation->setPlaceholderText(tr("Generated when enabled; no code needed after pairing"));slaveLayout->addWidget(fInvitation);
    auto *paths=new QFormLayout;fCache=new QLineEdit(fRoot,slave);fCache->setObjectName("SlaveCacheDirectory");fData=new QLineEdit(slave);fData->setObjectName("SlaveDataDirectory");
    auto pathRow=[&](QLineEdit *edit,bool cache){auto *row=new QWidget(slave);auto *line=new QHBoxLayout(row);line->setContentsMargins(0,0,0,0);line->addWidget(edit);auto *browse=new QPushButton(tr("Browse…"),row);line->addWidget(browse);connect(browse,&QPushButton::clicked,this,[this,edit,cache]{if(edit->isReadOnly())return;const auto directory=QFileDialog::getExistingDirectory(this,tr("Choose local directory"),edit->text());if(!directory.isEmpty())edit->setText(cache?QDir(directory).filePath("getdaq-node-cache"):directory);});return row;};
    paths->addRow(tr("Configuration cache (on this slave)"),pathRow(fCache,true));paths->addRow(tr("Data output (on this slave)"),pathRow(fData,false));slaveLayout->addLayout(paths);
    auto *pathHelp=new QLabel(tr("Choose local folders, not a configuration file on the master. The master sends configuration automatically. Paths are saved when the slave is enabled."),slave);pathHelp->setWordWrap(true);slaveLayout->addWidget(pathHelp);
    auto *advanced=new QCheckBox(tr("Network and data storage settings (optional)"),slave);slaveLayout->addWidget(advanced);auto *settings=new QWidget(slave);auto *form=new QFormLayout(settings);fAddress=new QLineEdit(slave);fAddress->setObjectName("SlaveManagementAddress");fPort=new QSpinBox(slave);fPort->setObjectName("SlaveManagementPort");fPort->setRange(1,65535);QJsonObject saved;try{saved=Settings(fRoot);}catch(const std::exception &error){saved={{"bind",DefaultAddress()},{"port",24260},{"data_directory",QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/data"}};fStatus->setText(tr("Cannot read saved node settings: %1").arg(QString::fromUtf8(error.what())));}fAddress->setText(saved["bind"].toString());fPort->setValue(saved["port"].toInt());fData->setText(saved["data_directory"].toString());form->addRow(tr("Local management IP"),fAddress);form->addRow(tr("Management port (default: 24260)"),fPort);slaveLayout->addWidget(settings);settings->hide();connect(advanced,&QCheckBox::toggled,settings,&QWidget::setVisible);
    if(QFile::exists(fRoot+"/node.json")){fAddress->setReadOnly(true);fPort->setEnabled(false);fData->setReadOnly(true);fCache->setReadOnly(true);}
    fInstances=new QTableWidget(0,4,slave);fInstances->setObjectName("SlaveCachedInstances");fInstances->setHorizontalHeaderLabels({tr("Instance"),tr("Type"),tr("Process"),tr("Local record")});fInstances->setEditTriggers(QAbstractItemView::NoEditTriggers);fInstances->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);fInstances->horizontalHeader()->setStretchLastSection(true);slaveLayout->addWidget(fInstances);auto *close=new QPushButton(tr("Close (keep services running)"),slave);slaveLayout->addWidget(close);connect(close,&QPushButton::clicked,this,&HostRoleWindow::reject);fPages->addWidget(slave);
    connect(fStart,&QPushButton::clicked,this,&HostRoleWindow::StartSlave);connect(fCopy,&QPushButton::clicked,this,[this]{QApplication::clipboard()->setText(fInvitation->text());});connect(fRole,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&HostRoleWindow::SelectRole);
    const int role=SavedRole()=="slave"?1:0;fRole->setCurrentIndex(role);SelectRole(role);
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
    fNode=node;const bool paired=QFile::exists(fRoot+"/master.json");fStatus->setText(paired?tr("Slave agent online and paired. Configuration is cached automatically. Check backend and acquisition status on the master."):tr("Slave agent online, waiting for pairing. Copy the code to the master once (valid for 15 minutes)."));fInvitation->setText(paired?QString():node["invitation"].toString());fCopy->setEnabled(!paired && !fInvitation->text().isEmpty());
    const auto instances=node["inventory"].toObject()["instances"].toArray();fInstances->setRowCount(instances.size());for(int row=0;row<instances.size();++row){const auto instance=instances[row].toObject();const bool running=!instance["actual_pids"].toArray().isEmpty();const QStringList values={instance["instance_id"].toString(),instance["role"].toString(),running?tr("Process found (check acquisition on master)"):tr("No matching process"),instance["phase"].toString()};for(int col=0;col<values.size();++col)fInstances->setItem(row,col,new QTableWidgetItem(values[col]));}
}
void HostRoleWindow::ObserveSlave(){
    if(fBusy || fRole->currentIndex()!=1 || fNode.isEmpty()){return;}
    fBusy=true;fCloseAfterWait=false;fCanceled=std::make_shared<std::atomic_bool>(false);const auto canceled=fCanceled;fRole->setEnabled(false);fStart->setEnabled(false);const auto node=fNode;auto *watcher=new QFutureWatcher<Result>(this);connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher]{const auto result=watcher->result();watcher->deleteLater();fBusy=false;fRole->setEnabled(true);fStart->setEnabled(true);if(!result.second.isEmpty()){fStatus->setText(tr("Slave agent currently unreachable: %1").arg(result.second));fInstances->setRowCount(0);fCopy->setEnabled(false);if(fCloseAfterWait)QDialog::reject();return;}auto node=fNode;node["inventory"]=result.first;ShowNode(node);if(fCloseAfterWait)QDialog::reject();});watcher->setFuture(QtConcurrent::run([=]{OperationScope scope(canceled,10000);return Attempt([=]{return NodeInventory(node);});}));
}
void HostRoleWindow::reject(){if(fBusy){fCloseAfterWait=true;if(fCanceled)fCanceled->store(true);fStatus->setText(tr("Stopping local waiting before closing. Independent services are retained."));return;}if(fMaster && fMaster->Busy()){fMaster->RequestClose();return;}QDialog::reject();}
