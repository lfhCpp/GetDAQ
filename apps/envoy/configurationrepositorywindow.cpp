#include "configurationrepositorywindow.h"
#include <QComboBox>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFile>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
ConfigurationRepositoryWindow::ConfigurationRepositoryWindow(QWidget *parent):QDialog(parent) {
    setWindowTitle(tr("Configuration repository"));resize(950,600);auto layout=new QVBoxLayout(this);
    auto description=new QLabel(tr("Upload and inspect configuration publications. Raw DAQ and hardware configuration are separate.\n"
        "Query the server identity, then retain the reviewed repository ID. Registered versions still require a full check before use."),this);description->setWordWrap(true);layout->addWidget(description);
    auto form=new QFormLayout;fAddress=new QLineEdit("127.0.0.1",this);fAddress->setObjectName("RepositoryAddress");fPort=new QSpinBox(this);fPort->setRange(1,65535);fPort->setValue(24002);
    fCredential=new QLineEdit(this);fCredential->setObjectName("RepositoryCredential");auto credentialRow=new QHBoxLayout;fBrowse=new QPushButton(tr("Credential file..."),this);credentialRow->addWidget(fCredential);credentialRow->addWidget(fBrowse);
    fId=new QLineEdit(this);fId->setObjectName("RepositoryId");form->addRow(tr("Server IPv4"),fAddress);form->addRow(tr("Port"),fPort);form->addRow(tr("Private credential path"),credentialRow);form->addRow(tr("Reviewed repository ID"),fId);layout->addLayout(form);
    auto actions=new QHBoxLayout;fIdentity=new QPushButton(tr("Query identity"),this);fList=new QPushButton(tr("List registered versions"),this);fUpload=new QPushButton(tr("Upload publication..."),this);fResume=new QPushButton(tr("Observe and resume"),this);fResume->setEnabled(false);
    fIdentity->setObjectName("RepositoryQueryIdentity");fList->setObjectName("RepositoryList");fUpload->setObjectName("RepositoryUpload");fResume->setObjectName("RepositoryResume");
    for (auto button:{fIdentity,fList,fUpload,fResume}) actions->addWidget(button);
    layout->addLayout(actions);
    auto versions=new QHBoxLayout;fVersions=new QComboBox(this);fVersions->setObjectName("RepositoryVersions");fVerify=new QPushButton(tr("Verify selected version"),this);versions->addWidget(fVersions,1);versions->addWidget(fVerify);layout->addLayout(versions);
    fResult=new QPlainTextEdit(this);fResult->setObjectName("RepositoryResult");fResult->setReadOnly(true);layout->addWidget(fResult,1);
    fExport=new QPushButton(tr("Export reviewed ECC selection..."),this);fExport->setObjectName("RepositoryExportSelection");fExport->setEnabled(false);layout->addWidget(fExport);
    connect(fExport,&QPushButton::clicked,this,[this] {
        if (!fClient || fClient->SelectionBytes().isEmpty()) return;
        const auto path=QFileDialog::getSaveFileName(this,tr("Save a new ECC selection record"),QString(),tr("JSON (*.json)"));if (path.isEmpty()) return;
        QFile file(path);const auto bytes=fClient->SelectionBytes();
        if (!file.open(QIODevice::WriteOnly|QIODevice::NewOnly) || file.write(bytes)!=bytes.size() || !file.flush() || !file.setPermissions(QFile::ReadOwner)) {
            fResult->setPlainText(tr("Cannot save a new read-only selection. Existing files are never replaced; inspect any partial file."));return;
        }
        file.close();fResult->setPlainText(tr("Saved selection: %1. Use getdaq-ecc-select on the ECC host to validate and create a new startup configuration. Hardware is unchanged.").arg(path));
    });
    connect(fBrowse,&QPushButton::clicked,this,[this] {const auto file=QFileDialog::getOpenFileName(this,tr("Private repository credential"));if (!file.isEmpty()) fCredential->setText(file);});
    connect(fAddress,&QLineEdit::textChanged,this,[this] {ClearClient();});connect(fCredential,&QLineEdit::textChanged,this,[this] {ClearClient();});connect(fId,&QLineEdit::textChanged,this,[this] {ClearClient();});connect(fPort,QOverload<int>::of(&QSpinBox::valueChanged),this,[this] {ClearClient();});
    connect(fIdentity,&QPushButton::clicked,this,[this] {try {MakeClient();Busy(true);fClient->QueryIdentity();}catch (const std::exception &e) {Busy(false);fResult->setPlainText(QString::fromUtf8(e.what()));}});
    connect(fList,&QPushButton::clicked,this,[this] {try {MakeClient();Busy(true);fClient->QueryVersions();}catch (const std::exception &e) {Busy(false);fResult->setPlainText(QString::fromUtf8(e.what()));}});
    connect(fUpload,&QPushButton::clicked,this,[this] {
        const auto file=QFileDialog::getOpenFileName(this,tr("Local publication manifest"),QString(),tr("Publication manifest (publication.json)"));if (file.isEmpty()) return;
        try {MakeClient();Busy(true);fClient->Upload(file);}catch (const std::exception &e) {Busy(false);fResult->setPlainText(QString::fromUtf8(e.what()));}
    });
    connect(fResume,&QPushButton::clicked,this,[this] {if (fClient) {Busy(true);if (!fClient->Resume()) Busy(false);}});
    connect(fVerify,&QPushButton::clicked,this,[this] {if (fVersions->currentText().isEmpty()) return;try {MakeClient();Busy(true);fClient->QueryVersion(fVersions->currentText());}catch (const std::exception &e) {Busy(false);fResult->setPlainText(QString::fromUtf8(e.what()));}});
    connect(this,&QDialog::finished,this,[this] {if (fClient && fBusy) fClient->Cancel();});
}
void ConfigurationRepositoryWindow::Busy(bool value) {
    fBusy=value;fBrowse->setEnabled(!value);for (auto button:{fIdentity,fList,fUpload,fVerify}) button->setEnabled(!value);fResume->setEnabled(false);fExport->setEnabled(!value && fClient && !fClient->SelectionBytes().isEmpty());
    for (auto edit:{fAddress,fCredential,fId}) edit->setEnabled(!value);
    fPort->setEnabled(!value);
}
void ConfigurationRepositoryWindow::ClearClient() {
    if (fBusy) return;
    if (fClient) {fClient->Cancel();fClient->deleteLater();fClient=nullptr;}fResume->setEnabled(false);fExport->setEnabled(false);
}
void ConfigurationRepositoryWindow::MakeClient() {
    ClearClient();ConfigurationRepositoryEndpoint endpoint;endpoint.address=fAddress->text().trimmed();endpoint.port=fPort->value();endpoint.credentialFile=fCredential->text();endpoint.repositoryId=fId->text().trimmed();fClient=new ConfigurationRepositoryClient(endpoint,this);
    connect(fClient,&ConfigurationRepositoryClient::ProgressSignal,fResult,&QPlainTextEdit::setPlainText);
    connect(fClient,&ConfigurationRepositoryClient::IdentitySignal,this,[this](const QJsonObject &result,const QString &error) {Busy(false);fResult->setPlainText(error.isEmpty() ? QString::fromUtf8(QJsonDocument(result).toJson())+tr("Copy the reviewed repository_id into the ID field before upload/list.") : error);});
    connect(fClient,&ConfigurationRepositoryClient::VersionsSignal,this,[this](const QJsonObject &result,const QString &error) {Busy(false);fResult->setPlainText(error.isEmpty() ? QString::fromUtf8(QJsonDocument(result).toJson()) : error);if (error.isEmpty()) {fVersions->clear();for (const auto &entry:result["versions"].toArray()) fVersions->addItem(entry.toObject()["publication_sha256"].toString());}});
    connect(fClient,&ConfigurationRepositoryClient::FinishedSignal,this,[this](const QJsonObject &result,const QString &error) {Busy(false);fResult->setPlainText(error.isEmpty() ? QString::fromUtf8(QJsonDocument(result).toJson())+tr("Configuration registered/verified. Hardware loading is still unverified.") : error);fResume->setEnabled(!error.isEmpty() && fClient && fClient->HasUploadPlan());});
}
