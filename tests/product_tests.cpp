#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QAction>
#include <QCheckBox>
#include <QTableWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QMessageBox>
#include <QTimer>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include "runconfig.h"
#include "runcontrol.h"
#include "runrecorder.h"
#include "protocol.h"
#include "mainwindow.h"
#include "ipwindow.h"
#ifdef Q_OS_LINUX
#include "hostrolewindow.h"
#include "originaldeployment.h"
#include <QComboBox>
#include <QLabel>
#endif
#include "getdaq_test_support_p.h"
#include "mocknode_fixture_p.h"
#include "waveformviewer_tests_p.h"
using namespace GetDAQTestSupport;
class ProductTests:public QObject {Q_OBJECT
private slots:
 void initTestCase(){QCoreApplication::setOrganizationName("GetDAQ-Product-Tests");QCoreApplication::setApplicationName("OriginalBackendBaseline");qRegisterMetaType<QVector<NodeStatus>>();}
 void yamlAndRetiredProfileGuard(){QTemporaryDir dir;auto c=Config(dir.path());QString error;QVERIFY(c.Save(dir.filePath("envoy.yml"),&error));QFile f(dir.filePath("envoy.yml"));QVERIFY(f.open(QIODevice::ReadOnly));const auto bytes=f.readAll();QVERIFY(!bytes.contains("native_ecc"));QVERIFY(!bytes.contains("simulation"));RunConfig loaded;QVERIFY(loaded.Load(f.fileName(),&error));QCOMPARE(loaded.nodes.size(),2);QCOMPARE(loaded.configDir,c.configDir);QCOMPARE(loaded.outputDir,c.outputDir);c.nativeEcc=true;QVERIFY(!c.Validate(&error));QVERIFY(error.contains("removed"));c.nativeEcc=false;c.simulation=true;QVERIFY(!c.Validate(&error));}
 void remoteReceiverProtocol(){NodeConfig node;node.id=8;node.host="192.0.2.16";node.routerHost="198.51.100.16";node.routerPort=46005;node.exporterPort=0;auto body=Protocol::EccRequest(node,"pedestals","Configure");QVERIFY(body.contains("DataRouter"));QVERIFY(body.contains("198.51.100.16"));QVERIFY(body.contains("46005"));QVERIFY(!body.contains("DataExporter"));QVERIFY(body.contains("CoBo[8]"));}
 void fullSoapLifecycle(){QTemporaryDir dir;auto c=Config(dir.path());RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),finished(&control,&RunControl::RunFinishedSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,1),3000);control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,2),3000);control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,3),3000);control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,4),3000);control.StartRunSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);control.PauseSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,6),3000);control.ResumeSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);control.StopRunSlot();QTRY_VERIFY_WITH_TIMEOUT(finished.size()==1,7000);QVERIFY(At(states,4));QJsonObject journal;QString error;QVERIFY(RunRecorder::ReadJournal(c,&journal,&error));QCOMPARE(journal["phase"].toString(),QString("Complete"));control.DisconnectSlot();}
 void missingBackupIsWarning(){QTemporaryDir dir;auto c=Config(dir.path());RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),finished(&control,&RunControl::RunFinishedSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,1),3000);for(int s=2;s<=4;++s){control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,s),3000);}control.StartRunSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);QVERIFY(QFile::remove(c.configDir+"/describe-"+c.nodes[0].ConfigId("describe",c.experiment)+".xcfg"));control.StopRunSlot();QTRY_VERIFY_WITH_TIMEOUT(finished.size()==1,7000);QVERIFY(At(states,4));QVERIFY(QFileInfo::exists(c.RunDirectory()+"/configuration-backup-warning.json"));control.DisconnectSlot();}
 void sameRunRequiresExplicitReplacement(){QTemporaryDir dir;auto c=Config(dir.path());QString error;QVERIFY(RunRecorder::Reserve(c,&error));QVERIFY(!RunRecorder::Reserve(c,false,&error));QVERIFY(RunRecorder::Reserve(c,true,&error));QJsonObject journal;QVERIFY(RunRecorder::ReadJournal(c,&journal,&error));QVERIFY(!journal["replacement_backup"].toString().isEmpty());}
 void newRunWithIncomingFilesRequiresSeparateConfirmation(){
    QTemporaryDir dir;auto c=Config(dir.path());
    for(auto *mock:QCoreApplication::instance()->findChildren<GetDAQTestFixtures::MockNode *>())
        if(mock->serverPort()==c.nodes[0].eccPort)mock->generatedBytes=1234;
    RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),confirmation(&control,&RunControl::IncomingFilesConfirmationSignal),finished(&control,&RunControl::RunFinishedSignal);
    control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,1),3000);
    for(int s=2;s<=4;++s){control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,s),3000);}
    control.StartRunSlot();QCOMPARE(confirmation.size(),1);QVERIFY(!confirmation[0][0].toBool());QVERIFY(confirmation[0][1].toString().contains("/incoming"));
    QVERIFY(!QFileInfo::exists(c.RunDirectory()));QVERIFY(At(states,4));
    control.StartRunSlot(false,true);QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);
    QJsonObject journal;QString error;QVERIFY(RunRecorder::ReadJournal(c,&journal,&error));
    QVERIFY(journal["existing_incoming_acknowledged"].toBool());QVERIFY(!journal.contains("replacement_operation_id"));QVERIFY(!journal.contains("replacement_backup"));QVERIFY(!journal.contains("same_run_existing_incoming_acknowledged"));
    control.StopRunSlot();QTRY_VERIFY_WITH_TIMEOUT(finished.size()==1,7000);control.DisconnectSlot();
 }
 void acceptingIncomingDoesNotAuthorizeRunReplacement(){
    QTemporaryDir dir;auto c=Config(dir.path());QString error;QVERIFY(RunRecorder::Reserve(c,&error));
    RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,1),3000);
    for(int s=2;s<=4;++s){control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,s),3000);}
    control.StartRunSlot(false,true);QVERIFY(At(states,4));QVERIFY(QFileInfo::exists(c.RunDirectory()+"/journal.json"));
    control.DisconnectSlot();
 }
 void incomingFilesDialogCanCancelAndThenStart(){
    QTemporaryDir dir;auto c=Config(dir.path());
    for(auto *mock:QCoreApplication::instance()->findChildren<GetDAQTestFixtures::MockNode *>())
        if(mock->serverPort()==c.nodes[0].eccPort)mock->generatedBytes=1234;
    MainWindow window;auto *table=window.findChild<QTableWidget *>("StatusTable");QVERIFY(table);
    QSignalSpy starts(&window,&MainWindow::StartSignal);window.ConnectSignal(c);
    QTRY_COMPARE_WITH_TIMEOUT(table->item(0,1)->text(),QString("Idle"),3000);
    for(const auto &state:{"Described","Prepared","Ready"}){window.ProgressSignal(-1);QTRY_COMPARE_WITH_TIMEOUT(table->item(0,1)->text(),QString(state),3000);}
    bool accept=false;int dialogs=0;QTimer click;
    connect(&click,&QTimer::timeout,&window,[&]{
        auto *dialog=window.findChild<QMessageBox *>("IncomingFilesConfirmation");if(!dialog || !dialog->isVisible())return;
        ++dialogs;QVERIFY(dialog->text().contains("does not mean"));
        if(!accept){dialog->reject();return;}
        for(auto *button:dialog->buttons())if(button->text()=="Start and retain files"){button->click();return;}
    });click.start(10);
    window.StartSignal(false,false);QTRY_COMPARE_WITH_TIMEOUT(dialogs,1,3000);
    QCOMPARE(starts.size(),1);QVERIFY(!QFileInfo::exists(c.RunDirectory()));
    accept=true;window.StartSignal(false,false);QTRY_COMPARE_WITH_TIMEOUT(table->item(0,1)->text(),QString("Running"),3000);
    QCOMPARE(dialogs,2);QCOMPARE(starts.size(),3);QVERIFY(starts.last()[1].toBool());
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QPushButton *>("StopRun")->isEnabled(),3000);
    window.StopSignal();QTRY_COMPARE_WITH_TIMEOUT(table->item(0,1)->text(),QString("Ready"),3000);
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(c.RunDirectory()+"/run.csv"),7000);window.DisconnectSignal();
 }
 void originalWindowAndNodeOptions(){MainWindow window;QVERIFY(!window.findChild<QAction *>("OriginalGetController"));QVERIFY(window.findChild<QAction *>("DirectCoBoInformation"));auto *table=window.findChild<QTableWidget *>("StatusTable");QVERIFY(table);QCOMPARE(table->columnCount(),13);for(auto *action:window.findChildren<QAction *>())QVERIFY2(!action->objectName().startsWith("Native"),qPrintable(action->objectName()));auto c=RunConfig::Default();IPWindow nodes(c);auto *settings=nodes.findChild<QTableWidget *>("NodeSettingsTable");QVERIFY(settings);QCOMPARE(settings->columnCount(),14);QVERIFY(!nodes.findChild<QCheckBox *>("UseNativeWholeSystemControl"));for(const auto &name:{"LoadConfig","SaveConfig","Nodes"}){auto *b=window.findChild<QPushButton *>(name);QVERIFY(b);QCOMPARE(b->width(),172);}}
#ifdef Q_OS_LINUX
 void managedPrepareUsesConfigureRepositoryFile(){
    QJsonObject group{{"backend","original-soap"},{"experiment","pedestals"}};QMap<QString,QByteArray> files;QJsonArray entries;
    auto add=[&](const QString &id,const QString &path){files[id]="<config/>";entries.append(QJsonObject{{"file",id},{"path",path},{"sha256",Deployment::Sha(files[id])}});};
    add("description","describe-cobo0.xcfg");add("experiment","configure-pedestals.xcfg");group["repository_files"]=entries;
    Deployment::ValidateOriginalFiles(group,files); // No prepare-pedestals file exists.
    group["prepare_id"]="alignment";add("alignment","configure-alignment.xml");group["repository_files"]=entries;Deployment::ValidateOriginalFiles(group,files);
    group["prepare_id"]="missing";QVERIFY_EXCEPTION_THROWN(Deployment::ValidateOriginalFiles(group,files),std::exception);
 }
 void reusingManagedGroupPreservesLocalFolderChoices(){
    QTemporaryDir dir;auto base=RunConfig::Default();base.outputDir=dir.filePath("chosen-records");
    QJsonObject group{{"id","group1"},{"name","ECC"},{"backend","original-soap"},{"experiment","pedestals"},{"board_index",0},{"master_repository",dir.filePath("source-configs")},{"receiver_ip","127.0.0.1"},{"data_port",46005},{"sentry_port",8080},{"ecc",QJsonObject{{"address","127.0.0.1"},{"port",8083}}}};
    QJsonArray entries;for(const auto &name:{"describe-cobo0.xcfg","configure-pedestals.xcfg"})entries.append(QJsonObject{{"path",name}});group["repository_files"]=entries;
    QJsonObject master{{"root",dir.filePath("master")}},active{{"release_id",Deployment::Sha("release")}};
    auto config=Deployment::OriginalConsole(master,group,active,base);QCOMPARE(config.outputDir,base.outputDir);
    config.configDir=dir.filePath("local-mirror");config.outputDir=dir.filePath("other-records");QString error;QVERIFY(config.Save(config.filePath,&error));
    auto restored=Deployment::OriginalConsole(master,group,active,base);QCOMPARE(restored.configDir,config.configDir);QCOMPARE(restored.outputDir,config.outputDir);
 }
 void slaveRefreshKeepsControlsAndObservedPaths(){
    HostRoleWindow window(RunConfig::Default());auto *role=window.findChild<QComboBox *>("MachineRoleSelector");QVERIFY(role);role->setCurrentIndex(1);
    auto *start=window.findChild<QPushButton *>("StartSlaveAgent");auto *table=window.findChild<QTableWidget *>("SlaveCachedInstances");QVERIFY(start);QVERIFY(table);
    QJsonObject instance{{"instance_id","group-data"},{"role","receiver"},{"phase","stopped"},{"data_directory","/data/group-data"},{"working_directory","/data/group-data"}};
    QJsonObject node{{"inventory",QJsonObject{{"instances",QJsonArray{instance}}}}};
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowNode",Qt::DirectConnection,Q_ARG(QJsonObject,node)));QCOMPARE(table->columnCount(),6);
    auto *item=table->item(0,5);QCOMPARE(item->text(),QString("/data/group-data"));
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowNode",Qt::DirectConnection,Q_ARG(QJsonObject,node)));QCOMPARE(table->item(0,5),item);
    QVERIFY(QMetaObject::invokeMethod(&window,"ObserveSlave",Qt::DirectConnection));QVERIFY(role->isEnabled());QVERIFY(start->isEnabled());
    auto *status=window.findChild<QLabel *>("SlaveAgentStatus");QTRY_VERIFY_WITH_TIMEOUT(status->text().contains("unreachable"),3000);QCOMPARE(table->item(0,5),item);QCOMPARE(item->text(),QString("/data/group-data"));role->setCurrentIndex(0);
 }
#endif
 void nodePathsSeparateRawFromGuiRecords(){
    auto c=RunConfig::Default();NodeStatus status;status.id=0;status.dataPath="/receiver/raw-folder";status.sentryOnline=true;
    IPWindow window(c,nullptr,{status});auto *path=window.findChild<QLabel *>("ObservedReceiverDataPaths");QVERIFY(path);QVERIFY(path->text().contains("/receiver/raw-folder"));
    QVERIFY(!window.findChild<QPushButton *>("OpenLocalServices"));QVERIFY(window.findChild<QPushButton *>("OpenHardwareSetup"));
 }
 void localPathsCanBeSelectedSavedAndUsed(){
    QTemporaryDir dir;auto c=Config(dir.path());c.filePath=dir.filePath("envoy.yml");QString error;QVERIFY(c.Save(c.filePath,&error));
    const auto configPath=dir.filePath("chosen-configs"),recordsPath=dir.filePath("chosen-records");QVERIFY(QDir().mkpath(configPath));
    IPWindow window(c);auto *config=window.findChild<QLineEdit *>("LocalConfigurationDirectory");auto *records=window.findChild<QLineEdit *>("LocalRunRecordsDirectory");QVERIFY(config);QVERIFY(records);QVERIFY(!config->isReadOnly());QVERIFY(!records->isReadOnly());
    QVERIFY(window.findChild<QPushButton *>("BrowseLocalConfigurationDirectory"));QVERIFY(window.findChild<QPushButton *>("BrowseLocalRunRecordsDirectory"));
    config->setText(configPath);records->setText(recordsPath);window.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();QCOMPARE(window.result(),int(QDialog::Accepted));
    RunConfig restored;QVERIFY(restored.Load(c.filePath,&error));QCOMPARE(restored.configDir,configPath);QCOMPARE(restored.outputDir,recordsPath);
    QVERIFY(RunRecorder::Reserve(restored,&error));QVERIFY(QFileInfo::exists(restored.RunDirectory()+"/run.yml"));QVERIFY(!QFileInfo::exists(c.RunDirectory()));
    IPWindow canceled(restored);canceled.findChild<QLineEdit *>("LocalRunRecordsDirectory")->setText(dir.filePath("canceled-records"));canceled.reject();
    RunConfig unchanged;QVERIFY(unchanged.Load(c.filePath,&error));QCOMPARE(unchanged.outputDir,recordsPath);
 }
 void waveformRawFrames(){GetDAQTestInternal::RunWaveformViewerIndexesRawAndLayeredFrames();}
 void waveformCompactFrames(){GetDAQTestInternal::RunWaveformViewerDecodesCompactCoboFrames();}
 void waveformTruncation(){GetDAQTestInternal::RunWaveformViewerRejectsTruncatedMfmInput();}
 void waveformChannelExport(){GetDAQTestInternal::RunWaveformViewerExportsSelectedSamplesAsCsv();}
 void waveformNavigation(){GetDAQTestInternal::RunWaveformViewerNavigatesAndWrapsFrames();}
 void waveformPulseMetrics(){GetDAQTestInternal::RunWaveformViewerComputesLegacyPulseMetrics();}
};
QTEST_MAIN(ProductTests)
#include "product_tests.moc"
