#include "ipwindow.h"
#include "hardwaresetupwindow.h"
#include "localserviceswindow.h"
#include "coboconnectionwindow.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QGroupBox>
#include <QDir>
#include <QUuid>
#include <QScrollArea>

IPWindow::IPWindow(const RunConfig &config, QWidget *parent) : QDialog(parent), fConfig(config)
{
    setWindowTitle(tr("Nodes and paths")); resize(960, 560);
    QVBoxLayout *layout = new QVBoxLayout(this);
    fLegacyTopologyNote=new QLabel(tr("Verify the prefilled CoBo/MuTaNT hosts and ports against the actual lab network before leaving simulation. The default addresses are not hardware-validated; update them in this table for your deployment."),this);
    fLegacyTopologyNote->setObjectName("LegacyTopologyWarning");fLegacyTopologyNote->setWordWrap(true);
    fLegacyTopologyNote->setStyleSheet("QLabel { background-color: #fff4cc; color: #513b00; padding: 8px; border: 1px solid #d3b35f; }");
    fNativeTopologyNote=new QLabel(tr("Native whole-system mode connects to one ECC control service. Its fixed service configuration supplies board proxies and topology; the per-node ECC host columns below are hidden and are not used."),this);
    fNativeTopologyNote->setObjectName("NativeTopologySourceNote");fNativeTopologyNote->setWordWrap(true);
    fNativeTopologyNote->setStyleSheet("QLabel { background-color: #e8f1fa; color: #17324d; padding: 8px; border: 1px solid #9bb8d2; }");
    layout->addWidget(fLegacyTopologyNote);layout->addWidget(fNativeTopologyNote);
    auto *addresses=new QGroupBox(tr("Current hardware network"),this);auto *addressForm=new QFormLayout(addresses);
    auto *boardIp=new QLineEdit(qEnvironmentVariable("GETDAQ_COBO_IPV4","192.168.40.64"),addresses);boardIp->setObjectName("CurrentBoardIPv4");boardIp->setReadOnly(true);addressForm->addRow(tr("CoBo IPv4 (from ECC topology)"),boardIp);
    auto *receiverIp=new QLineEdit(qEnvironmentVariable("GETDAQ_RECEIVER_IPV4","192.168.50.10"),addresses);receiverIp->setObjectName("CurrentReceiverIPv4");receiverIp->setReadOnly(true);addressForm->addRow(tr("Receiver IPv4 (this computer)"),receiverIp);
    layout->addWidget(addresses);
    auto *hardwareSetup=new QPushButton(tr("Set up board and receiver addresses…"),this);
    hardwareSetup->setObjectName("OpenHardwareSetup");
    layout->addWidget(hardwareSetup);
    auto *directCoBo=new QPushButton(tr("Connect CoBo directly (read-only)…"),this);directCoBo->setObjectName("OpenDirectCoBo");layout->addWidget(directCoBo);
    connect(directCoBo,&QPushButton::clicked,this,[this]{CoBoConnectionWindow window(this);window.exec();});
    auto *services=new QPushButton(tr("Start independent ECC / data services…"),this);services->setObjectName("OpenLocalServices");layout->addWidget(services);
    connect(services,&QPushButton::clicked,this,[this]{LocalServicesWindow window(fConfig,this);if(window.exec()==QDialog::Accepted && window.Configuration().filePath!=fConfig.filePath){fConfig=window.Configuration();accept();}});
    connect(hardwareSetup,&QPushButton::clicked,this,[this] {
        HardwareSetupWindow window(fConfig,this);
        if(window.exec()!=QDialog::Accepted)return;
        fConfig=window.Configuration();
        accept(); // Reopen Run Setup from the parent with the fully checked config.
    });
    fLegacyTopologyNote->setVisible(!config.nativeEcc);fNativeTopologyNote->setVisible(config.nativeEcc);
    fNodes = new QTableWidget(0, 16, this);
    fNodes->setObjectName("NodeSettingsTable");
    fNodes->setHorizontalHeaderLabels({"ID", "Role (cobo/mutant)", "Name", "ECC host", "ECC port", "Sentry port", "Router port (0=off)", "Legacy ECC target port (0=off)", "Router host", "Legacy ECC target host", "Describe ID", "Prepare ID", "Configure ID", "Sentry host", "Native sender ID", "Native agent ID"});
    fNodes->horizontalHeaderItem(6)->setToolTip(tr("Primary board-to-receiver data connection. The receiver may separately provide a downstream --export-port for analysis clients."));
    fNodes->horizontalHeaderItem(7)->setToolTip(tr("Optional second board-to-target data connection from legacy ECC configurations. Leave 0 for the normal single-ingress setup; this is not the receiver's downstream --export-port."));
    fNodes->horizontalHeaderItem(8)->setToolTip(tr("Optional host override for the primary board-to-receiver connection; blank uses ECC host."));
    fNodes->horizontalHeaderItem(9)->setToolTip(tr("Optional host override for the legacy second board-to-target connection."));
    fNodes->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(fNodes);
    for (const NodeConfig &node : config.nodes) AddRow(node);
    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *add = new QPushButton(tr("Add CoBo"), this);
    QPushButton *remove = new QPushButton(tr("Remove selected"), this);
    buttons->addWidget(add); buttons->addWidget(remove); buttons->addStretch(); layout->addLayout(buttons);
    connect(add, &QPushButton::clicked, this, &IPWindow::AddNodeSlot);
    connect(remove, &QPushButton::clicked, this, &IPWindow::RemoveNodeSlot);
    QFormLayout *form = new QFormLayout;
    fConfigDir = new QLineEdit(config.configDir, this); fOutputDir = new QLineEdit(config.outputDir, this);
    form->addRow(tr("GET configuration directory"), fConfigDir); form->addRow(tr("Local run records directory"), fOutputDir);
    auto spin = [this, form](const QString &name, int value, int minimum) {
        QSpinBox *box = new QSpinBox(this); box->setRange(minimum, 3600000); box->setValue(value); box->setSuffix(" ms"); form->addRow(name, box); return box;
    };
    fPoll = spin(tr("Status polling"), config.pollMs, 50);
    fTimeout = spin(tr("HTTP timeout"), config.timeoutMs, 100);
    fCatalogTimeout = spin(tr("Catalog timeout (server normally waits 30 s)"), config.catalogTimeoutMs, 100);
    fStepTimeout = spin(tr("Transition deadline"), config.stepTimeoutMs, 100);
    fSettle = spin(tr("Post-stop settling time"), config.settleMs, 0);
    QWidget *settings=new QWidget(this);
    QVBoxLayout *settingsLayout=new QVBoxLayout(settings);
    settingsLayout->addLayout(form);
    QGroupBox *native=new QGroupBox(tr("Whole-system native ECC (one control endpoint)"),this);
    QFormLayout *nativeForm=new QFormLayout(native); fNative=new QCheckBox(tr("Use native whole-system control"),native); fNative->setObjectName("UseNativeWholeSystemControl"); fNative->setChecked(config.nativeEcc); nativeForm->addRow(fNative);
    fNativeCatalog=new QCheckBox(tr("Require data-host Run catalog before Complete (fixed roots must be enabled)"),native); fNativeCatalog->setChecked(config.nativeCatalog); nativeForm->addRow(fNativeCatalog);
    fNativeAlarms=new QCheckBox(tr("Require and display ECC alarm cache (/v1/alarms)"),native); fNativeAlarms->setChecked(config.nativeClient.alarmStatusEnabled); nativeForm->addRow(fNativeAlarms);
    fNativeReceivers=new QCheckBox(tr("Observe receiver processes"),native); fNativeReceivers->setChecked(config.nativeClient.receiverStatusEnabled); nativeForm->addRow(fNativeReceivers);
    fNativeSources=new QCheckBox(tr("Observe source state (read-only)"),native);fNativeSources->setChecked(config.nativeClient.sourceStatusEnabled);nativeForm->addRow(fNativeSources);
    fNativeReceiverFailure=new QCheckBox(tr("Show receiver failure stop results"),native);fNativeReceiverFailure->setChecked(config.nativeClient.receiverFailureStatusEnabled);nativeForm->addRow(fNativeReceiverFailure);
    auto edit=[native,nativeForm](const QString &label,const QString &value) { auto *field=new QLineEdit(value,native); nativeForm->addRow(label,field); return field; };
    auto number=[native,nativeForm](const QString &label,int value,int low,int high) { auto *field=new QSpinBox(native); field->setRange(low,high); field->setValue(value); nativeForm->addRow(label,field); return field; };
    fNativeHost=edit(tr("Control IPv4"),config.nativeClient.address); fNativePort=number(tr("Control port"),config.nativeClient.port ? config.nativeClient.port : 24000,1,65535);
    fNativeService=edit(tr("Expected service ID"),config.nativeClient.serviceId); fNativeClient=edit(tr("Client ID"),config.nativeClient.clientId);
    fNativeCredential=edit(tr("Private credential file (value is never shown)"),config.nativeClient.credentialFile);
    QString epoch=config.nativeClient.clientEpoch;
    if (epoch.isEmpty()) { epoch=QUuid::createUuid().toString(QUuid::WithoutBraces)+QUuid::createUuid().toString(QUuid::WithoutBraces); epoch.remove('-'); }
    fNativeEpoch=edit(tr("Persistent client epoch"),epoch);
    fNativeSnapshot=edit(tr("Reviewed snapshot SHA256"),config.nativeClient.snapshotSha256); fNativeInput=edit(tr("Reviewed fixed input SHA256"),config.nativeClient.inputBindingSha256);
    fNativeRecords=edit(tr("Private control record root"),config.nativeRecordDir.isEmpty() ? QDir(config.outputDir).filePath("native-control-records") : config.nativeRecordDir);
    fNativeTimeout=number(tr("Native HTTP timeout (ms)"),config.nativeClient.timeoutMs,100,60000); fNativeStatusAge=number(tr("Maximum status age (ms)"),config.nativeClient.statusMaxAgeMs,100,3600000);
    settingsLayout->addWidget(native);
    QScrollArea *scroll=new QScrollArea(this); scroll->setObjectName("NodeSettingsScroll"); scroll->setWidgetResizable(true); scroll->setWidget(settings);
    layout->addWidget(scroll,2);
    const auto mode=[this,native](bool enabled) {
        fLegacyTopologyNote->setVisible(!enabled);fNativeTopologyNote->setVisible(enabled);
        fNativeCatalog->setEnabled(enabled);
        for (int col=3; col<=13; ++col) fNodes->setColumnHidden(col,enabled);
        fNodes->setColumnHidden(14,!enabled); fNodes->setColumnHidden(15,!enabled);
        for (auto *field:native->findChildren<QLineEdit *>()) field->setEnabled(enabled);
        for (auto *field:native->findChildren<QSpinBox *>()) field->setEnabled(enabled);
    };
    connect(fNative,&QCheckBox::toggled,this,mode); mode(config.nativeEcc);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, this, &IPWindow::ApplySlot);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
void IPWindow::AddRow(const NodeConfig &n)
{
    const int row = fNodes->rowCount(); fNodes->insertRow(row);
    const QStringList text = {QString::number(n.id), n.mutant ? "mutant" : "cobo", n.name, n.host,
        QString::number(n.eccPort), QString::number(n.sentryPort), QString::number(n.routerPort), QString::number(n.exporterPort),
        n.routerHost, n.exporterHost, n.describeId, n.prepareId, n.configureId, n.sentryHost, n.nativeSenderId, n.nativeAgentId};
    for (int i = 0; i < text.size(); ++i) fNodes->setItem(row, i, new QTableWidgetItem(text[i]));
}
void IPWindow::AddNodeSlot()
{
    NodeConfig node;
    for (int i = 0; i < fNodes->rowCount(); ++i) node.id = qMax(node.id, fNodes->item(i, 0)->text().toInt() + 1);
    node.name = QString("CoBo %1").arg(node.id); node.host = "127.0.0.1"; AddRow(node);
}
void IPWindow::RemoveNodeSlot() { if (fNodes->currentRow() >= 0) fNodes->removeRow(fNodes->currentRow()); }
void IPWindow::ApplySlot()
{
    RunConfig config = fConfig;
    config.nodes.clear();
    for (int row = 0; row < fNodes->rowCount(); ++row) {
        auto text = [this, row](int col) { return fNodes->item(row, col)->text().trimmed(); };
        const QString role = text(1);
        if (role != "mutant" && role != "cobo") { QMessageBox::warning(this, tr("Configuration"), tr("Role must be cobo or mutant.")); return; }
        NodeConfig n;
        bool ok;
        n.id = text(0).toInt(&ok);
        if (!ok) { QMessageBox::warning(this, tr("Configuration"), tr("Invalid node ID.")); return; }
        // 保留未在该表显示的协议兼容选项。
        for (const NodeConfig &previous : fConfig.nodes) if (previous.id == n.id) { n = previous; break; }
        n.mutant = role == "mutant"; n.name = text(2); n.host = text(3);
        n.eccPort = text(4).toInt(); n.sentryPort = text(5).toInt(); n.routerPort = text(6).toInt(); n.exporterPort = text(7).toInt();
        for (int col = 4; col <= 7; ++col) {
            text(col).toInt(&ok);
            if (!ok) { QMessageBox::warning(this, tr("Configuration"), tr("Ports must be integers.")); return; }
        }
        n.routerHost = text(8); n.exporterHost = text(9);
        n.describeId = text(10); n.prepareId = text(11); n.configureId = text(12);
        n.sentryHost = text(13);
        n.nativeSenderId=text(14); n.nativeAgentId=text(15);
        config.nodes.append(n);
    }
    config.configDir = fConfigDir->text(); config.outputDir = fOutputDir->text();
    config.pollMs = fPoll->value(); config.timeoutMs = fTimeout->value(); config.stepTimeoutMs = fStepTimeout->value(); config.settleMs = fSettle->value();
    config.catalogTimeoutMs = fCatalogTimeout->value();
    config.nativeCatalog=fNative->isChecked() && fNativeCatalog->isChecked();
    config.nativeClient.alarmStatusEnabled=fNative->isChecked() && fNativeAlarms->isChecked();
    config.nativeClient.receiverStatusEnabled=fNative->isChecked() && fNativeReceivers->isChecked();
    config.nativeClient.sourceStatusEnabled=fNative->isChecked() && fNativeSources->isChecked();
    config.nativeClient.receiverFailureStatusEnabled=fNative->isChecked() && fNativeReceiverFailure->isChecked();
    config.nativeEcc=fNative->isChecked();
    if (config.nativeEcc) {
        config.simulation=false; config.nativeClient.address=fNativeHost->text().trimmed(); config.nativeClient.port=fNativePort->value(); config.nativeClient.serviceId=fNativeService->text().trimmed();
        config.nativeClient.clientId=fNativeClient->text().trimmed(); config.nativeClient.credentialFile=QDir::cleanPath(fNativeCredential->text().trimmed());
        config.nativeClient.clientEpoch=fNativeEpoch->text().trimmed(); config.nativeClient.snapshotSha256=fNativeSnapshot->text().trimmed(); config.nativeClient.inputBindingSha256=fNativeInput->text().trimmed();
        config.nativeRecordDir=QDir::cleanPath(fNativeRecords->text().trimmed()); config.nativeClient.timeoutMs=fNativeTimeout->value(); config.nativeClient.statusMaxAgeMs=fNativeStatusAge->value();
    }
    QString error;
    if (!config.Validate(&error)) { QMessageBox::warning(this, tr("Configuration"), error); return; }
    fConfig = config; accept();
}
