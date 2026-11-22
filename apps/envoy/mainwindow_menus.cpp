#include <QTimer>
#include "coboconnectionwindow.h"
#include "mainwindow.h"
#include "userguidewindow.h"
#ifdef Q_OS_LINUX
#include "distributeddeploymentwindow.h"
#endif
#ifdef Q_OS_LINUX
#include "hostrolewindow.h"
#endif
#include "ui_mainwindow.h"
#include "offlinedatatoolswindow.h"
#include "runcontrol.h"
#include "consolepresentation.h"
#include <QAbstractItemView>
#include <QAction>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QProcess>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

void MainWindow::InitializeMenus()
{
    QMenu *configurationMenu=new QMenu(tr("Configuration"),this);
    auto *newConfiguration=configurationMenu->addAction(tr("New configuration…"));
    newConfiguration->setObjectName("NewAcquisitionConfiguration");
    connect(newConfiguration,&QAction::triggered,this,&MainWindow::NewConfiguration);
    configurationMenu->addSeparator();
    const auto manageBackends=[this]{
#ifdef Q_OS_LINUX
        if(fConnected || fBusy){ShowLogSlot(tr("Disconnect before changing backend service groups."));return;}
        HostRoleWindow window(fConfig,this);
        if(window.exec()==QDialog::Accepted && AdoptConfiguration(window.Configuration())){RememberStartupConfiguration();QTimer::singleShot(0,this,[this]{if(!fConnected && !fBusy)on_Connect_clicked();});}
#elif defined(Q_OS_WIN)
        QMessageBox::information(this,tr("Native Windows backends"),tr("Build getEccSoapServer.exe and DataExporter.exe with GETDAQ_BUILD_BACKENDS=1 in the Qt Creator qmake arguments, or use tools/build-windows-backends.py. CMake, Python 3 and matching MinGW are required. Start the services with scripts/start-windows-backends.cmd, then configure their ECC / router endpoints in Nodes / Paths. See docs/WINDOWS_BACKENDS.md. Automatic Master / Slave deployment management requires Linux."));
#else
        QMessageBox::information(this,tr("Original backends"),tr("Use the bundled getEccSoapServer and DataExporter programs, then configure their endpoints in Nodes / Paths. Automatic Master / Slave service management currently requires Linux."));
#endif
    };
    auto *instances=configurationMenu->addAction(tr("ECC / data service instances…"));instances->setObjectName("ManageBackendInstances");
    connect(instances,&QAction::triggered,this,manageBackends);
#ifdef Q_OS_LINUX
    const auto openHostRole=[this]{
        if(fConnected || fBusy){ShowLogSlot(tr("Disconnect before changing this computer's role or active ECC group."));return;}
        HostRoleWindow window(fConfig,this);
        if(window.exec()==QDialog::Accepted && AdoptConfiguration(window.Configuration())){RememberStartupConfiguration();QTimer::singleShot(0,this,[this]{if(!fConnected && !fBusy)on_Connect_clicked();});}
        if(auto *button=findChild<QPushButton *>("MachineRoleButton"))button->setText(HostRoleWindow::ButtonText());
    };

#endif
    QMenu *toolsMenu=new QMenu(tr("Tools"),this);
    QMenu *runMenu=new QMenu(tr("Run"),this);
    QMenu *recoveryMenu=new QMenu(tr("Recovery and safety"),this);
    QHBoxLayout *menuLayout=findChild<QHBoxLayout *>(QStringLiteral("HeaderMenuLayout"));
    const auto addMenuControl=[this,menuLayout](const QString &objectName,const QString &label,QMenu *menu) {
        if(!menuLayout)return;
        auto *button=new QToolButton(this);
        button->setObjectName(objectName);
        button->setText(label);
        button->setToolTip(menu->title());
        button->setAccessibleName(menu->title());
        button->setPopupMode(QToolButton::InstantPopup);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
        button->setMinimumHeight(28);
        menu->setStyleSheet(ConsolePresentation::MenuStyleSheet());
        button->setMenu(menu);
        menuLayout->addWidget(button);
    };
    addMenuControl(QStringLiteral("MenuConfiguration"),tr("Configuration"),configurationMenu);
    addMenuControl(QStringLiteral("MenuTools"),tr("Tools"),toolsMenu);
    addMenuControl(QStringLiteral("MenuRun"),tr("Run"),runMenu);
    addMenuControl(QStringLiteral("MenuRecovery"),tr("Recovery"),recoveryMenu);
#ifdef Q_OS_LINUX
    auto *role=new QPushButton(HostRoleWindow::ButtonText(),ui->SetupPanel);role->setObjectName("MachineRoleButton");role->setMinimumSize(120,36);role->setMaximumWidth(172);role->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);role->setToolTip(tr("Master / Slave: keep source configuration on the master; receive and cache it on slaves."));role->setAccessibleName(tr("Master / Slave"));ui->ConfigLayout->insertWidget(ui->ConfigLayout->indexOf(ui->Nodes)+1,role);for(auto *button:{ui->LoadConfig,ui->SaveConfig,ui->Nodes,role}){button->ensurePolished();button->setFixedSize(172,36);}connect(role,&QPushButton::clicked,this,openHostRole);
    QTimer::singleShot(0,this,[this]{HostRoleWindow::ResumeSlaveAgent(this,[this](const QString &message){ShowLogSlot(message);});});
#endif
    fWaveformViewerAction=toolsMenu->addAction(tr("Waveform viewer…"));
    fWaveformViewerAction->setObjectName("OfflineWaveformViewer");
    fWaveformViewerAction->setToolTip(tr("Offline .graw/.gmat viewer. Close its window before starting a Run."));
    connect(fWaveformViewerAction,&QAction::triggered,this,&MainWindow::OpenWaveformViewer);
    fDataToolsAction=toolsMenu->addAction(tr("Data file tools…"));
    fDataToolsAction->setObjectName("OfflineDataTools");
    fDataToolsAvailable=OfflineDataToolsWindow::HasAvailableTool();
    fDataToolsAction->setEnabled(fDataToolsAvailable);
    fDataToolsAction->setToolTip(tr("Inspect, merge and convert completed files; optional FITS / ROOT tools require installation."));
    connect(fDataToolsAction,&QAction::triggered,this,&MainWindow::OpenDataTools);
    connect(toolsMenu,&QMenu::aboutToShow,this,[this]{
        fDataToolsAvailable=OfflineDataToolsWindow::HasAvailableTool();RefreshOfflineToolsGate();
    });
    auto *syncConfiguration=toolsMenu->addAction(tr("Sync project configuration…"));syncConfiguration->setObjectName("SyncProjectConfigurationTool");
#ifdef Q_OS_LINUX
    connect(syncConfiguration,&QAction::triggered,this,[this]{DistributedDeploymentWindow window(fConfig,this);window.show();window.OpenSyncDialog();window.exec();});
#else
    syncConfiguration->setEnabled(false);
#endif
    auto *onlineWaveform=toolsMenu->addAction(tr("Online waveform viewer…"));onlineWaveform->setObjectName("OnlineWaveformViewer");
    connect(onlineWaveform,&QAction::triggered,this,[this]{auto program=QCoreApplication::applicationDirPath()+"/getdaq-online-waveform-viewer";
#ifdef Q_OS_WIN
    program+=".exe";
#endif
    const auto choices=OnlineSourcesFile();if(choices.isEmpty()){ShowLogSlot(tr("Cannot prepare the current DataExporter choices."));return;}if(!QProcess::startDetached(program,{"--sources-file",choices},QDir::homePath()))ShowLogSlot(tr("Cannot start the independent online viewer. Install getdaq-online-waveform-viewer beside GetDAQ."));});
    auto *exporterRefresh=new QTimer(this);connect(exporterRefresh,&QTimer::timeout,this,[this]{if(!fOnlineSourcesPath.isEmpty())OnlineSourcesFile();});exporterRefresh->start(2000);

    QMenu *logsMenu=toolsMenu->addMenu(tr("Logs"));
    logsMenu->setObjectName("ToolsLogsMenu");
    QAction *exportLog=logsMenu->addAction(tr("Export combined logs…"));
    exportLog->setObjectName("ExportVisibleLog");
    exportLog->setToolTip(tr("Save a chronological snapshot of collected Event, ECC, Data and Sentry logs; clearing the display does not delete export history."));
    connect(exportLog,&QAction::triggered,this,&MainWindow::ExportVisibleLog);
    auto *localServices=toolsMenu->addAction(tr("ECC / data service manager…"));localServices->setObjectName("StartLocalServices");
    connect(localServices,&QAction::triggered,this,manageBackends);
    auto configuration = configurationMenu->addAction(tr("GET configuration files..."));
    configurationMenu->addSeparator();auto *guide=configurationMenu->addMenu(tr("User guide"));guide->setObjectName("UserGuideMenu");
    auto *chinese=guide->addAction(QString::fromUtf8("中文 (Chinese)"));chinese->setObjectName("UserGuideChinese");auto *english=guide->addAction(tr("English"));english->setObjectName("UserGuideEnglish");
    connect(chinese,&QAction::triggered,this,[this]{OpenUserGuide(this,"zh");});connect(english,&QAction::triggered,this,[this]{OpenUserGuide(this,"en");});
    auto *boardInfo=toolsMenu->addAction(tr("CoBo information (direct, read-only)…"));
    boardInfo->setObjectName("DirectCoBoInformation");
    connect(boardInfo,&QAction::triggered,this,[this]{CoBoConnectionWindow window(this);window.exec();});
    InitializeRecoveryAndSafetyMenus(configurationMenu,runMenu,recoveryMenu);
    connect(configuration, &QAction::triggered, this, [this]() {
        if(fConnected || fActive || fBusy || !ui->Connect->isEnabled()) {
            QMessageBox::information(this,tr("Configuration editor"),tr("Disconnect before editing configuration files."));
            return;
        }
        QString editor=QCoreApplication::applicationDirPath()+QStringLiteral("/getdaq-cconfig-editor");
#ifdef Q_OS_WIN
        editor+=QStringLiteral(".exe");
#endif
        if(!QFileInfo(editor).isExecutable()) {
            QMessageBox::warning(this,tr("Configuration editor"),tr("The bundled original configuration editor is missing: %1\nRebuild or reinstall GetDAQ with getdaq-cconfig-editor.").arg(editor));
            return;
        }
        QString directory=fConfig.configDir.isEmpty()?RunConfig::Default().configDir:fConfig.configDir;
        directory=QDir::cleanPath(QFileInfo(directory).absoluteFilePath());
        // A fresh, unsaved setup may create its private default workspace on demand.
        if(!QFileInfo(directory).isDir() && fConfig.filePath.isEmpty() && directory==RunConfig::Default().configDir)
            QDir().mkpath(directory);
        if(!QFileInfo(directory).isDir() || !QFileInfo(directory).isReadable()) {
            const QString previous=QSettings().value("configuration-editor/last-directory").toString();
            const QString initial=QFileInfo(previous).isDir()?previous:RunConfig::Default().configDir;
            directory=QFileDialog::getExistingDirectory(this,tr("Choose configuration directory"),initial);
            if(directory.isEmpty())return;
            if(!ReadConfig())return;
            fConfig.configDir=QDir::cleanPath(directory);
            ShowLogSlot(tr("Configuration directory selected. Save YAML to retain this setting: %1").arg(directory));
        }
        QSettings().setValue("configuration-editor/last-directory",directory);
        if(!QProcess::startDetached(editor,{QStringLiteral("--path"),directory},directory))
            QMessageBox::warning(this,tr("Configuration editor"),tr("Could not start the bundled configuration editor."));
    });
}
