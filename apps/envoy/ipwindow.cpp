#include "ipwindow.h"
#ifdef Q_OS_LINUX
#include "hostrolewindow.h"
#endif
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
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>

IPWindow::IPWindow(const RunConfig &config, QWidget *parent, const QVector<NodeStatus> &status) : QDialog(parent), fConfig(config)
{
    setWindowTitle(tr("Nodes and paths")); resize(960, 560);
    QVBoxLayout *layout = new QVBoxLayout(this);
    fLegacyTopologyNote=new QLabel(tr("ECC host and port identify the computer running SOAP ECC. CoBo IPs and electronics settings belong in the original ECC configuration files. Router host and port are the CoBo data destination."),this);
    fLegacyTopologyNote->setObjectName("LegacyTopologyWarning");fLegacyTopologyNote->setWordWrap(true);
    fLegacyTopologyNote->setStyleSheet("QLabel { background-color: #fff4cc; color: #513b00; padding: 8px; border: 1px solid #d3b35f; }");
    layout->addWidget(fLegacyTopologyNote);
    auto *addresses=new QGroupBox(tr("Current hardware network"),this);auto *addressForm=new QFormLayout(addresses);
    auto *boardIp=new QLineEdit(qEnvironmentVariable("GETDAQ_COBO_IPV4","192.168.40.64"),addresses);boardIp->setObjectName("CurrentBoardIPv4");boardIp->setText(tr("See original ECC XML/XCFG files"));boardIp->setReadOnly(true);addressForm->addRow(tr("CoBo IPv4 (from ECC topology)"),boardIp);
    auto *receiverIp=new QLineEdit(qEnvironmentVariable("GETDAQ_RECEIVER_IPV4","192.168.50.10"),addresses);receiverIp->setObjectName("CurrentReceiverIPv4");receiverIp->setText(config.nodes.isEmpty()?QString():config.nodes[0].routerHost);receiverIp->setReadOnly(true);addressForm->addRow(tr("Receiver IPv4 (data host)"),receiverIp);
    layout->addWidget(addresses);
    auto *hardwareSetup=new QPushButton(tr("Configure and manage ECC / data services…"),this);
    hardwareSetup->setObjectName("OpenHardwareSetup");
    layout->addWidget(hardwareSetup);
    const auto originalServices=[this]{
#ifdef Q_OS_LINUX
        HostRoleWindow window(fConfig,this);if(window.exec()==QDialog::Accepted){fConfig=window.Configuration();accept();}
#else
        QMessageBox::information(this,tr("Original backends"),tr("Start the bundled getEccSoapServer and DataExporter programs, then enter their endpoints below. Automatic host management currently requires Linux."));
#endif
    };
    connect(hardwareSetup,&QPushButton::clicked,this,originalServices);
    fNodes = new QTableWidget(0, 14, this);
    fNodes->setObjectName("NodeSettingsTable");
    fNodes->setHorizontalHeaderLabels({"ID", "Role (cobo/mutant)", "Name", "ECC host", "ECC port", "Sentry port", "Router port (0=off)", "Legacy ECC target port (0=off)", "Router host", "Legacy ECC target host", "Describe ID", "Prepare ID", "Configure ID", "Sentry host"});
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
    fConfigDir->setObjectName("LocalConfigurationDirectory");
    fOutputDir->setObjectName("LocalRunRecordsDirectory");
    fConfigDir->setToolTip(tr("Local files used by the configuration editor and Run backup. Existing remote ECC reads its own --config-repo-url directory / NFS mount; this field does not upload files. Managed services receive a cache from the master."));
    fOutputDir->setToolTip(tr("Local run.yml, journal.json, CSV and configuration backups. Raw .graw files remain on the receiver host in the directory reported by Sentry."));
    auto directoryRow=[this](QLineEdit *edit,const QString &name,const QString &title){
        auto *row=new QWidget(this);auto *line=new QHBoxLayout(row);line->setContentsMargins(0,0,0,0);line->addWidget(edit,1);
        auto *browse=new QPushButton(tr("Browse…"),row);browse->setObjectName(name);line->addWidget(browse);
        connect(browse,&QPushButton::clicked,this,[this,edit,title]{
            const auto path=QFileDialog::getExistingDirectory(this,title,edit->text());
            if(!path.isEmpty())edit->setText(QDir::cleanPath(path));
        });return row;
    };
    form->addRow(tr("Configuration source folder (master)"), directoryRow(fConfigDir,"BrowseLocalConfigurationDirectory",tr("Choose local XML/XCFG directory")));
    form->addRow(tr("GUI Run records root (not raw data)"), directoryRow(fOutputDir,"BrowseLocalRunRecordsDirectory",tr("Choose GUI Run records root")));
    auto *sync=new QCheckBox(tr("Sync configuration to project folders before starting services"),this);sync->setObjectName("SyncConfigurationToProjects");sync->setChecked(config.syncConfiguration);form->addRow(sync);
    auto *pathHelp=new QLabel(tr("Type a path or Browse. Save and apply writes both paths to the current YAML; a new setup gets its own saved YAML automatically. Saving Nodes does not transfer XML/XCFG files. Use Tools > Sync project configuration, or start project services with sync enabled. Existing files are not moved and external ECC / receiver directories are not changed."),this);
    pathHelp->setWordWrap(true);form->addRow(pathHelp);
    QStringList observedPaths;
    for(const auto &entry:status)if(!entry.dataPath.isEmpty())
        observedPaths.append(tr("Node %1: %2%3").arg(entry.id).arg(entry.dataPath,entry.sentryOnline?QString():tr(" (last observed; offline / stale)")));
    auto *rawPath=new QLabel(observedPaths.isEmpty()?tr("Not observed yet. Connect, then check the receiver Data path column. This folder belongs to the receiver host, not the GUI."):observedPaths.join('\n'),this);
    rawPath->setObjectName("ObservedReceiverDataPaths");rawPath->setWordWrap(true);rawPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("Raw .graw folder (from Sentry)"),rawPath);

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
    QScrollArea *scroll=new QScrollArea(this); scroll->setObjectName("NodeSettingsScroll"); scroll->setWidgetResizable(true); scroll->setWidget(settings);
    layout->addWidget(scroll,2);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(tr("Save and apply"));
    layout->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, this, &IPWindow::ApplySlot);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
void IPWindow::AddRow(const NodeConfig &n)
{
    const int row = fNodes->rowCount(); fNodes->insertRow(row);
    const QStringList text = {QString::number(n.id), n.mutant ? "mutant" : "cobo", n.name, n.host,
        QString::number(n.eccPort), QString::number(n.sentryPort), QString::number(n.routerPort), QString::number(n.exporterPort),
        n.routerHost, n.exporterHost, n.describeId, n.prepareId, n.configureId, n.sentryHost};
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
        config.nodes.append(n);
    }
    const QDir base(fConfig.filePath.isEmpty()?QDir::currentPath():QFileInfo(fConfig.filePath).absolutePath());
    config.configDir = QDir::cleanPath(base.absoluteFilePath(fConfigDir->text().trimmed()));
    config.outputDir = QDir::cleanPath(base.absoluteFilePath(fOutputDir->text().trimmed()));
    if(fConfigDir->text().trimmed().isEmpty() || fOutputDir->text().trimmed().isEmpty()) {
        QMessageBox::warning(this,tr("Configuration"),tr("Choose both the local configuration directory and the GUI Run records directory."));return;
    }
    config.syncConfiguration=findChild<QCheckBox *>("SyncConfigurationToProjects")->isChecked();
    config.pollMs = fPoll->value(); config.timeoutMs = fTimeout->value(); config.stepTimeoutMs = fStepTimeout->value(); config.settleMs = fSettle->value();
    config.catalogTimeoutMs = fCatalogTimeout->value();
    QString error;
    if (!config.Validate(&error)) { QMessageBox::warning(this, tr("Configuration"), error); return; }
    if(config.filePath.isEmpty()) {
        const auto directory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/connections/"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        if(!QDir().mkpath(directory)){QMessageBox::warning(this,tr("Configuration"),tr("Cannot create the settings directory: %1").arg(directory));return;}
        config.filePath=directory+"/envoy.yml";
    }
    if(!config.Save(config.filePath,&error)){QMessageBox::warning(this,tr("Configuration"),tr("Cannot save settings: %1").arg(error));return;}
    fConfig = config; accept();
}
