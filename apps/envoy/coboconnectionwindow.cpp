#include "coboconnectionwindow.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
namespace {
const char *const names[]={"hardwareVersion","firmwareRelease","firmwareDate","firmwareEnable","intEnable"};
}
CoBoConnectionWindow::CoBoConnectionWindow(QWidget *parent,bool connectNow):QDialog(parent) {
    setObjectName("CoBoConnectionWindow");setWindowTitle(tr("Connect CoBo directly (read-only)"));resize(800,560);
    auto *layout=new QVBoxLayout(this);auto *note=new QLabel(tr("Connects to the board's Ice HwNode directly. No AsAd or ECC acquisition profile is required. Reads only device names, version ROM and fixed control bits; no power, calibration, Configure or Start commands."),this);note->setWordWrap(true);layout->addWidget(note);
    auto *form=new QFormLayout;
    fAddress=new QLineEdit(qEnvironmentVariable("GETDAQ_COBO_IPV4"));fAddress->setObjectName("CoBoAddress");fAddress->setPlaceholderText("192.168.40.64");form->addRow(tr("CoBo IPv4 (board)"),fAddress);
    fPort=new QSpinBox;fPort->setObjectName("CoBoPort");fPort->setRange(1,65535);fPort->setValue(46001);form->addRow(tr("Board control port"),fPort);
    fObject=new QLineEdit("HwNode");fObject->setObjectName("CoBoObject");form->addRow(tr("Ice object identity"),fObject);
    fPrograms=new QLineEdit(QCoreApplication::applicationDirPath());fPrograms->setObjectName("CoBoPrograms");form->addRow(tr("Installed bin directory"),fPrograms);layout->addLayout(form);
    auto *actions=new QHBoxLayout;fConnect=new QPushButton(tr("Connect / refresh CoBo"));fConnect->setObjectName("ConnectDirectCoBo");fDisconnect=new QPushButton(tr("Disconnect observation"));fDisconnect->setObjectName("DisconnectDirectCoBo");auto *close=new QPushButton(tr("Close"));actions->addWidget(fConnect);actions->addWidget(fDisconnect);actions->addStretch();actions->addWidget(close);layout->addLayout(actions);
    fStatus=new QLabel(tr("Not connected to CoBo. This is separate from the main ECC connection."));fStatus->setObjectName("CoBoConnectionStatus");fStatus->setWordWrap(true);layout->addWidget(fStatus);
    fValues=new QTreeWidget;fValues->setObjectName("CoBoObservationValues");fValues->setHeaderLabels({tr("Item"),tr("Observed value")});fValues->header()->setSectionResizeMode(QHeaderView::ResizeToContents);layout->addWidget(fValues,1);
    fProcess=new QProcess(this);
    connect(fConnect,&QPushButton::clicked,this,&CoBoConnectionWindow::Inspect);connect(fDisconnect,&QPushButton::clicked,this,&CoBoConnectionWindow::Invalidate);connect(close,&QPushButton::clicked,this,&CoBoConnectionWindow::reject);
    for(auto *edit:{fAddress,fObject,fPrograms})connect(edit,&QLineEdit::textChanged,this,[this]{Invalidate();});
    connect(fPort,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{Invalidate();});
    connect(fProcess,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,&CoBoConnectionWindow::Finished);
    connect(fProcess,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(fBusy && error==QProcess::FailedToStart)Fail(tr("Cannot start getdaq-hardware: %1").arg(fProcess->errorString()));});
    auto *expiry=new QTimer(this);expiry->setInterval(1000);connect(expiry,&QTimer::timeout,this,[this]{if(fFresh.isValid() && fFresh.elapsed()>30000){fFresh.invalidate();fValues->clear();fStatus->setText(tr("CoBo observation expired. Click Connect / refresh to query the board again."));}});expiry->start();
    if(connectNow)QTimer::singleShot(0,this,&CoBoConnectionWindow::Inspect);
}
void CoBoConnectionWindow::Invalidate(){++fGeneration;fBusy=false;fFresh.invalidate();if(fProcess->state()!=QProcess::NotRunning)fProcess->kill();fValues->clear();fConnect->setEnabled(true);fStatus->setStyleSheet(QString());fStatus->setText(tr("No current CoBo observation. No hardware control action was sent."));}
void CoBoConnectionWindow::reject(){Invalidate();QDialog::reject();}
void CoBoConnectionWindow::Fail(const QString &message){fBusy=false;fFresh.invalidate();fValues->clear();fConnect->setEnabled(true);fStatus->setStyleSheet("color:#b53840;");fStatus->setText(tr("CoBo connection not verified: %1").arg(message));}
void CoBoConnectionWindow::Inspect(){
    if(fBusy || fProcess->state()!=QProcess::NotRunning)return;
    Invalidate();const QHostAddress address(fAddress->text().trimmed());
    if(address.protocol()!=QAbstractSocket::IPv4Protocol || address.toString()!=fAddress->text().trimmed() || address==QHostAddress::AnyIPv4 || address.isMulticast() || address==QHostAddress::Broadcast){Fail(tr("Enter the CoBo board's IPv4 address, not an ECC URL or receiver address."));return;}
    const auto object=fObject->text().trimmed();if(!QRegularExpression("^[A-Za-z0-9_./-]{1,128}$").match(object).hasMatch()){Fail(tr("Invalid Ice object identity"));return;}
    fProgram=QDir(fPrograms->text().trimmed()).absoluteFilePath("getdaq-hardware");const QFileInfo executable(fProgram);
    if(!executable.isFile() || !executable.isExecutable() || executable.isSymLink() || (executable.permissions()&(QFile::WriteGroup|QFile::WriteOther))){Fail(tr("Select the full installed bin directory containing getdaq-hardware."));return;}
    fProxy=QStringLiteral("%1:tcp -h %2 -p %3").arg(object,address.toString()).arg(fPort->value());fStep=0;fBusy=true;fConnect->setEnabled(false);fStatus->setText(tr("Querying the real CoBo…"));Next();
}
void CoBoConnectionWindow::Next(){
    QStringList arguments;
    if(fStep==0)arguments=QStringList{QStringLiteral("devices"),fProxy};
    else if(fStep<=3)arguments=QStringList{"read-register",fProxy,"ctrl",QString::fromLatin1(names[fStep-1])};
    else arguments=QStringList{"read-field",fProxy,"ctrl","pipeCtrl",QString::fromLatin1(names[fStep-1])};
    fProcess->start(fProgram,arguments);const auto generation=fGeneration;const int step=fStep;
    QTimer::singleShot(12000,this,[this,generation,step]{if(fBusy && generation==fGeneration && step==fStep && fProcess->state()!=QProcess::NotRunning){fBusy=false;fProcess->kill();Fail(tr("CoBo RPC timed out; no control command was sent."));}});
}
void CoBoConnectionWindow::Finished(int code,QProcess::ExitStatus status){
    if(!fBusy)return;
    const auto output=fProcess->readAllStandardOutput();const auto error=fProcess->readAllStandardError();
    if(code || status!=QProcess::NormalExit){Fail(QString::fromLocal8Bit(error).left(4096));return;}
    if(output.size()>1024*1024){Fail(tr("Board reply exceeds metadata limit"));return;}
    QJsonParseError parse;const auto object=QJsonDocument::fromJson(output,&parse).object();
    if(parse.error!=QJsonParseError::NoError){Fail(tr("Invalid board metadata response"));return;}
    if(fStep==0){
        if(!object["node"].isString() || !object["devices"].isArray()){Fail(tr("Invalid board identity/device response"));return;}
        new QTreeWidgetItem(fValues,{tr("Board endpoint"),fProxy});new QTreeWidgetItem(fValues,{tr("Actual node name"),object["node"].toString()});auto *devices=new QTreeWidgetItem(fValues,{tr("Devices"),QString::number(object["devices"].toArray().size())});
        for(const auto &device:object["devices"].toArray())new QTreeWidgetItem(devices,{device.toString(),tr("Metadata observed")});
    }else {
        if(!object["value"].isString()){Fail(tr("Invalid fixed register response"));return;}
        new QTreeWidgetItem(fValues,{QString::fromLatin1(names[fStep-1]),object["value"].toString()});
    }
    ++fStep;if(fStep<=5){Next();return;}
    fBusy=false;fFresh.start();fConnect->setEnabled(true);fStatus->setStyleSheet("color:#087f5b;");fStatus->setText(tr("Real CoBo connection verified (read-only): %1. Values are a snapshot; refresh within 30 seconds. This does not mark ECC Ready or permit acquisition.").arg(fAddress->text().trimmed()));fValues->expandAll();
}
