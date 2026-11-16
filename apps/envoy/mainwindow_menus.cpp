#include "mainwindow.h"
#include "localserviceswindow.h"
#ifdef Q_OS_LINUX
#include "hostrolewindow.h"
#endif
#include "coboconnectionwindow.h"
#include "ui_mainwindow.h"
#include "offlinedatatoolswindow.h"
#include "runcontrol.h"
#include "consolepresentation.h"
#include "configwindow.h"
#include "configurationreadbackwindow.h"
#include "plantransitionwindow.h"
#include "cobomaintenancewindow.h"
#include "hardwarestatewindow.h"
#include "transferwindow.h"
#include "coldhandoffwindow.h"
#include "failedhandoffwindow.h"
#include "scanplanwindow.h"
#include "eccprotocol.h"
#include <QAbstractItemView>
#include <QAction>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
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
    const auto manageBackends=[this]{LocalServicesWindow window(fConfig,this);if(window.exec()==QDialog::Accepted && window.Configuration().filePath!=fConfig.filePath){if(fConnected || fBusy){ShowLogSlot(tr("Backend instance saved. Disconnect the current acquisition before switching the instance used by Run Setup."));return;}if(AdoptConfiguration(window.Configuration()))RememberStartupConfiguration();}};
    auto *instances=configurationMenu->addAction(tr("ECC / data service instances…"));instances->setObjectName("ManageBackendInstances");
    connect(instances,&QAction::triggered,this,manageBackends);
#ifdef Q_OS_LINUX
    const auto openHostRole=[this]{
        if(fConnected || fBusy){ShowLogSlot(tr("Disconnect before changing this computer's role or active ECC group."));return;}
        HostRoleWindow window(fConfig,this);
        if(window.exec()==QDialog::Accepted && AdoptConfiguration(window.Configuration()))RememberStartupConfiguration();
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
    QMenu *logsMenu=toolsMenu->addMenu(tr("Logs"));
    logsMenu->setObjectName("ToolsLogsMenu");
    QAction *exportLog=logsMenu->addAction(tr("Export visible log…"));
    exportLog->setObjectName("ExportVisibleLog");
    exportLog->setToolTip(tr("Save the log text currently shown in the operation log panel."));
    connect(exportLog,&QAction::triggered,this,&MainWindow::ExportVisibleLog);
    auto *directCoBo=toolsMenu->addAction(tr("Connect CoBo directly (read-only)…"));directCoBo->setObjectName("ConnectDirectCoBoAction");connect(directCoBo,&QAction::triggered,this,[this]{CoBoConnectionWindow window(this);window.exec();});
    auto *localServices=toolsMenu->addAction(tr("ECC / data service manager…"));localServices->setObjectName("StartLocalServices");
    connect(localServices,&QAction::triggered,this,manageBackends);
    auto configuration = configurationMenu->addAction(tr("GET configuration files..."));
    auto originalController=toolsMenu->addAction(tr("GET Controller…"));
    originalController->setObjectName(QStringLiteral("OriginalGetController"));
    connect(originalController,&QAction::triggered,this,[this] {
        if(fConnected || fActive || fBusy || !ui->Connect->isEnabled()) {
            QMessageBox::information(this,tr("GET Controller"),tr("Disconnect GetDAQ before opening GET Controller to control hardware."));
            return;
        }
        const QString executable=QCoreApplication::applicationDirPath()+QStringLiteral("/getdaq-getcontroller");
        if(!QFileInfo(executable).isExecutable()) {
            QMessageBox::warning(this,tr("GET Controller"),tr("The bundled original GET Controller is missing: %1\nBuild or install the original GetController component.").arg(executable));
            return;
        }
        const QString formats=QCoreApplication::applicationDirPath()+QStringLiteral("/../share/get-bench/format/CoboFormats.xcfg");
        if(!QFileInfo(formats).isReadable()) {
            QMessageBox::warning(this,tr("GET Controller"),tr("GET frame formats are missing: %1").arg(formats));
            return;
        }
        if(!QProcess::startDetached(executable,{QStringLiteral("--format"),QFileInfo(formats).absoluteFilePath()},QFileInfo(fConfig.configDir).absoluteFilePath()))
            QMessageBox::warning(this,tr("GET Controller"),tr("Could not start the bundled GET Controller."));
    });
    auto scanPlans=toolsMenu->addAction(tr("Scan plans and runtime checks..."));scanPlans->setObjectName("NativeOfflineScanPlans");
    connect(scanPlans,&QAction::triggered,this,[this]{
        if(fScanPlanWindow){fScanPlanWindow->show();fScanPlanWindow->raise();fScanPlanWindow->activateWindow();return;}
        auto *window=new ScanPlanWindow(this);window->SetNativeCatalogEnabled(fConfig.nativeCatalog);fScanPlanWindow=window;
        window->setAttribute(Qt::WA_DeleteOnClose);
        connect(window,&ScanPlanWindow::QueryScanRuntimeIdentityRequested,
                fRunControl,&RunControl::QueryScanRuntimeIdentitySlot);
        connect(window,&ScanPlanWindow::QueryScanEventCountBaselineRequested,
                fRunControl,&RunControl::QueryScanEventCountBaselineSlot);
        connect(window,&ScanPlanWindow::ExecuteScanPointLifecycleOperationRequested,
                fRunControl,&RunControl::ExecuteScanPointLifecycleOperationSlot);
        connect(window,&ScanPlanWindow::ObservePendingScanLifecycleOperationRequested,
                fRunControl,&RunControl::ObservePendingScanLifecycleOperationSlot);
        connect(window,&ScanPlanWindow::StartScanPointRequested,this,[this,window](const QString &journal){
            if(!RequireOfflineToolsClosed()){window->ScanPointRunStatus(tr("Close the offline tool windows before starting a scan Run."),false);return;}
            emit StartScanPointSignal(journal);
        });
        connect(fRunControl,&RunControl::ScanRuntimeIdentityBoundSignal,
                window,&ScanPlanWindow::RuntimeIdentityQueryFinished);
        connect(fRunControl,&RunControl::ScanEventCountBaselineBoundSignal,
                window,&ScanPlanWindow::EventBaselineQueryFinished);
        connect(fRunControl,&RunControl::ScanPointRunStatusSignal,
                window,&ScanPlanWindow::ScanPointRunStatus);
        window->show();
    });
    auto hardwareState=toolsMenu->addAction(tr("Live hardware control bits..."));hardwareState->setObjectName("NativeHardwareState");
    connect(hardwareState,&QAction::triggered,this,[this]{auto window=new HardwareStateWindow(fConfig.nativeClient,this);window->setAttribute(Qt::WA_DeleteOnClose);window->show();});
    auto transfers=toolsMenu->addAction(tr("Native data-host copy tasks..."));transfers->setObjectName("NativeTransferTasks");
    connect(transfers,&QAction::triggered,this,[this]{auto window=new TransferWindow(this);window->setAttribute(Qt::WA_DeleteOnClose);window->show();});
    fReadbacksInfo=configurationMenu->addAction(tr("Configure register readbacks..."));fReadbacksInfo->setObjectName("NativeConfigureReadbacksInfo");
    connect(fReadbacksInfo,&QAction::triggered,this,[this] {
        if(!fReadbacksView){
            fReadbacksView=new ConfigurationReadbackWindow(this);
            connect(fReadbacksView,&ConfigurationReadbackWindow::InspectSignal,this,&MainWindow::InspectConfigureReadbacksSignal);
            connect(fReadbacksView,&QObject::destroyed,this,[this]{fReadbacksView=nullptr;});
        }
        fReadbacksView->Bind(fConfig.nativeClient.snapshotSha256,fOwnershipGeneration,fOwnershipConnected && fConfig.nativeEcc);
        fReadbacksView->show();fReadbacksView->raise();emit InspectConfigureReadbacksSignal(fOwnershipGeneration);
    });
    fOwnershipClock.start();
    fOwnershipInfo=toolsMenu->addAction(tr("ECC host board reservations..."));
    fOwnershipInfo->setObjectName("NativeOwnershipInfo");
    connect(fOwnershipInfo,&QAction::triggered,this,[this] {
        if (!fOwnershipView) {
            auto dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setWindowTitle(tr("ECC host board reservations"));dialog->resize(900,520);
            auto layout=new QVBoxLayout(dialog);
            auto note=new QLabel(tr("Reservations protect cooperating ECC services using one directory on the same host. They do not prove board-global control, hardware loading, or that acquisition has stopped."),dialog);
            note->setWordWrap(true);layout->addWidget(note);
            fOwnershipView=new QTableWidget(dialog);fOwnershipView->setObjectName("NativeOwnershipTable");
            fOwnershipView->setColumnCount(2);fOwnershipView->setHorizontalHeaderLabels({tr("Item"),tr("Observation")});
            fOwnershipView->setEditTriggers(QAbstractItemView::NoEditTriggers);
            fOwnershipView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
            fOwnershipView->horizontalHeader()->setStretchLastSection(true);layout->addWidget(fOwnershipView);
            auto refresh=new QPushButton(tr("Refresh observation"),dialog);layout->addWidget(refresh);
            connect(refresh,&QPushButton::clicked,this,[this] {if (fOwnershipInfo->isEnabled()) emit InspectOwnershipSignal(fOwnershipGeneration);});
            auto timer=new QTimer(dialog);timer->setInterval(2000);
            connect(timer,&QTimer::timeout,this,[this] {RenderOwnership();if (fOwnershipInfo->isEnabled()) emit InspectOwnershipSignal(fOwnershipGeneration);});timer->start();
            connect(dialog,&QObject::destroyed,this,[this] {fOwnershipView=nullptr;});dialog->show();
        } else {fOwnershipView->window()->show();fOwnershipView->window()->raise();}
        RenderOwnership();emit InspectOwnershipSignal(fOwnershipGeneration);
    });
    auto maintenance=toolsMenu->addAction(tr("Selected CoBo maintenance..."));maintenance->setObjectName("NativeCoBoMaintenance");
    connect(maintenance,&QAction::triggered,this,[this] { OpenCoBoMaintenance(); });
    auto planTransition=toolsMenu->addAction(tr("Review receiver plan transition..."));planTransition->setObjectName("NativePlanTransition");
    connect(planTransition,&QAction::triggered,this,[this] {
        if(!fPlanTransition) {
            fPlanTransition=new PlanTransitionWindow(this);fPlanTransition->setObjectName("NativePlanTransitionWindow");
            connect(fPlanTransition,&PlanTransitionWindow::SubmitSignal,this,&MainWindow::RetireDataPlansSignal);
            connect(fPlanTransition,&PlanTransitionWindow::InspectSignal,this,&MainWindow::InspectPlanRetirementReceiptSignal);
            connect(fPlanTransition,&PlanTransitionWindow::CompleteSignal,this,&MainWindow::CompletePlanRetirementSignal);
            connect(fPlanTransition,&PlanTransitionWindow::InspectCompletionSignal,this,&MainWindow::InspectPlanRetirementCompletionSignal);
        }
        fPlanTransition->SetControlState(fPlanCanSubmit,fPlanCanInspect,fPlanStatus,fPlanIdentity,fPlanReason);
        fPlanTransition->SetCompletionState(fPlanCanComplete,fPlanCanObserveCompletion,fPlanOriginalRequest,fPlanOriginalStatus,fPlanCompletionRequest,fPlanCompletionPhase,fPlanCompletionReason);
        if(!fPlanReceipt.isEmpty())fPlanTransition->ShowResult(fPlanReceipt,fPlanRequest,fPlanError);
        fPlanTransition->show();fPlanTransition->raise();
    });
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
        const QString directory=QFileInfo(fConfig.configDir).absoluteFilePath();
        if(!QFileInfo(directory).isDir()) {
            QMessageBox::warning(this,tr("Configuration editor"),tr("Configuration directory does not exist: %1").arg(directory));
            return;
        }
        if(!QProcess::startDetached(editor,{QStringLiteral("--path"),directory},directory))
            QMessageBox::warning(this,tr("Configuration editor"),tr("Could not start the bundled configuration editor."));
    });
}
