#include "distributeddeploymentwindow.h"
#include "backendshutdown.h"
#include "runcontrol.h"
#include "localserviceprocesses.h"
#include "userguidewindow.h"
#include <QListWidget>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include "exportersources.h"
#include "deploymentpaths.h"
#include "existingeccconnection.h"
#include "unifiedeccwindow.h"
#include "consolelogstore.h"
#include <cmath>
#include "waveformcanvas.h"
#include <QWheelEvent>
#include <QtTest>
#include <QTemporaryDir>
#include <QStandardPaths>
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
#include "onlinereceiver.h"
#include "applicationinstance.h"
#include <QSettings>
#include "waveformviewer_testfixtures_p.h"
#include <QThread>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtEndian>
#include <QClipboard>
#include "runrecorder.h"
#include "protocol.h"
#include "mainwindow.h"
#include "ipwindow.h"
#ifdef Q_OS_LINUX
#include "hostrolewindow.h"
#include "originaldeployment.h"
#include "projectworkspace.h"
#include "servicelogpanel.h"
#include "logtimeline.h"
#include "sentryservice.h"
#include <QPlainTextEdit>
#include <QToolButton>
#include <QComboBox>
#include <QLabel>
#endif
#include "getdaq_test_support_p.h"
#include "mocknode_fixture_p.h"
#include "waveformviewer_tests_p.h"
using namespace GetDAQTestSupport;
class ProductTests:public QObject {Q_OBJECT
 QTemporaryDir fWorkspace;
private slots:
 void movedPendingActivationResolvesOnlyThroughRecordedRelocations(){
    QTemporaryDir dir;const auto old=dir.path()+"/old",middle=dir.path()+"/middle",current=dir.path()+"/current";
    for(const auto &root:{old,middle,current})Deployment::PrivateDirectory(root);
    Deployment::PrivateDirectory(current+"/jobs/activate-original");Deployment::Write(old+"/relocated-to.json",Deployment::Json({{"directory",middle}}));Deployment::Write(middle+"/relocated-to.json",Deployment::Json({{"directory",current}}));
    const QJsonObject master{{"root",current}},context{{"directory",old+"/jobs/activate-original"}};
    QCOMPARE(Deployment::ResolvePendingActivationJob(master,context),current+"/jobs/activate-original");
    QVERIFY_EXCEPTION_THROWN(Deployment::ResolvePendingActivationJob(master,QJsonObject{{"directory",dir.path()+"/unrelated/jobs/activate-original"}}),std::exception);
    Deployment::Write(middle+"/relocated-to.json",Deployment::Json({{"directory",old}}));QVERIFY_EXCEPTION_THROWN(Deployment::ResolvePendingActivationJob(master,context),std::exception);
 }
 void literalPortProcessLookupDoesNotTreatPortAsFilePath(){
    QTemporaryDir dir;const auto exe=dir.path()+"/ecc";QVERIFY(QFile::copy("/bin/sh",exe));QVERIFY(QFile::setPermissions(exe,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    QProcess process;process.start(exe,{"-c","read value","--port","18083"});QVERIFY(process.waitForStarted());
    const auto found=FindLocalArgumentValueProcesses(exe,"--port","18083");QCOMPARE(found.size(),1);QCOMPARE(found.first().pid,process.processId());QVERIFY(FindLocalArgumentValueProcesses(exe,"--port","18084").isEmpty());process.terminate();QVERIFY(process.waitForFinished());
 }
 void ownedProcessStillMatchesAfterAtomicExecutableUpdate(){
    QTemporaryDir dir;const auto exe=dir.path()+"/backend",marker=dir.path()+"/service.json";
    Deployment::Write(marker,"{}");QVERIFY(QFile::copy("/bin/sh",exe));QVERIFY(QFile::setPermissions(exe,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    QProcess process;process.start(exe,{"-c","read value","--config",marker});QVERIFY(process.waitForStarted());
    const auto found=FindLocalArgumentProcesses(exe,"--config",marker);QCOMPARE(found.size(),1);QVERIFY(SameLocalServiceProcess(found.first()));
    QVERIFY(QFile::copy("/bin/sh",exe+".new"));QVERIFY(QFile::remove(exe));QVERIFY(QFile::rename(exe+".new",exe));
    QVERIFY(SameLocalServiceProcess(found.first()));auto wrong=found.first();wrong.started+="0";QVERIFY(!SameLocalServiceProcess(wrong));process.terminate();QVERIFY(process.waitForFinished());
 }
 void busyStopIsObservedWithoutCommandReplay(){
    QTemporaryDir dir;QStringList trace;GetDAQTestFixtures::MockNode node("CoBo",&trace);node.state=5;node.transition=5;node.busyStop=true;
    auto config=GetDAQTestSupport::Config(dir.path());config.nodes.removeLast();config.simulation=false;config.nodes[0].host="127.0.0.1";config.nodes[0].eccPort=node.serverPort();config.nodes[0].sentryPort=node.serverPort();
    RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal),logs(&control,&RunControl::LogSignal);
    control.ConnectSlot(config);QTRY_VERIFY(GetDAQTestSupport::At(states,5));control.StopRunSlot();QTRY_COMPARE(trace.count("CoBo:Stop"),1);
    for(int i=0;i<5;++i){control.StopRunSlot();QTest::qWait(25);}QCOMPARE(trace.count("CoBo:Stop"),1);QVERIFY(GetDAQTestSupport::Phase(phase)!="Error");
    node.state=4;node.transition=0;QTRY_COMPARE(GetDAQTestSupport::Phase(phase),QString("Stopped (untracked run)"));QCOMPARE(trace.count("CoBo:Stop"),1);control.ShutdownSlot();
 }
 void busyStopDoesNotInventStoppedState(){
    QTemporaryDir dir;QStringList trace;GetDAQTestFixtures::MockNode node("CoBo",&trace);node.state=5;node.transition=5;node.busyStop=true;
    auto config=GetDAQTestSupport::Config(dir.path());config.nodes.removeLast();config.simulation=false;config.nodes[0].host="127.0.0.1";config.nodes[0].eccPort=node.serverPort();config.nodes[0].sentryPort=node.serverPort();config.stepTimeoutMs=300;
    RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal);control.ConnectSlot(config);QTRY_VERIFY(GetDAQTestSupport::At(states,5));control.StopRunSlot();QTRY_COMPARE(GetDAQTestSupport::Phase(phase),QString("Error"));QCOMPARE(trace.count("CoBo:Stop"),1);QVERIFY(!GetDAQTestSupport::At(states,4));control.ShutdownSlot();
 }
 void newAcquisitionReusesServicesWithoutOldMetadata(){
    auto old=RunConfig::Default();old.experiment="pedestals";old.runNumber=17;old.description="old notes";old.filePath="/tmp/old.yml";
    old.fields["Beam"]="old beam";old.autoRun.enabled=true;old.configurationBackupWarning="old warning";
    old.deploymentProject="/tmp/project/project.json";old.deploymentGroups=QStringList{QString("group-a")};old.configurationSnapshot="/tmp/release";
    old.nodes[0].prepareId="pedestals";
    const auto fresh=old.NewAcquisition(" next ");
    QCOMPARE(fresh.experiment,QString("next"));QCOMPARE(fresh.runNumber,0);QVERIFY(fresh.filePath.isEmpty());QVERIFY(fresh.description.isEmpty());
    QVERIFY(fresh.configurationBackupWarning.isEmpty());QVERIFY(!fresh.autoRun.enabled);QVERIFY(fresh.fields["Beam"].isEmpty());
    QCOMPARE(fresh.nodes.size(),old.nodes.size());QCOMPARE(fresh.nodes[0].host,old.nodes[0].host);QCOMPARE(fresh.nodes[0].prepareId,QString("pedestals"));
    QCOMPARE(fresh.deploymentProject,old.deploymentProject);QCOMPARE(fresh.deploymentGroups,old.deploymentGroups);QCOMPARE(fresh.configurationSnapshot,old.configurationSnapshot);
    QCOMPARE(fresh.configDir,old.configDir);QCOMPARE(fresh.outputDir,old.outputDir);QCOMPARE(old.runNumber,17);QCOMPARE(old.fields["Beam"],QString("old beam"));
    QTemporaryDir dir;QString error;QVERIFY2(fresh.Save(dir.filePath("new.yml"),&error),qPrintable(error));RunConfig loaded;QVERIFY(loaded.Load(dir.filePath("new.yml"),&error));
    QCOMPARE(loaded.runNumber,0);QCOMPARE(loaded.deploymentProject,old.deploymentProject);QCOMPARE(loaded.nodes[0].prepareId,QString("pedestals"));
    MainWindow window;QVERIFY(window.findChild<QAction *>("NewAcquisitionConfiguration"));
 }
 void initTestCase(){QVERIFY(fWorkspace.isValid());qputenv("GETDAQ_WORKSPACE",fWorkspace.path().toUtf8());QCoreApplication::setOrganizationName("GetDAQ-Product-Tests");QCoreApplication::setApplicationName("OriginalBackendBaseline");qRegisterMetaType<QVector<NodeStatus>>();}
 void bilingualGuideIsEmbeddedAndReadableWithoutInstalledDocuments(){
    for(const auto &language:{QString("zh"),QString("en")}){QFile pdf(":/getdaq/manual/guide-"+language+".pdf");QVERIFY(pdf.open(QIODevice::ReadOnly));QVERIFY(pdf.read(5)==QByteArray("%PDF-"));QVERIFY(pdf.size()>100000);}
    UserGuideWindow window("zh");auto *language=window.findChild<QComboBox *>("GuideLanguage");auto *chapters=window.findChild<QListWidget *>("GuideChapters");auto *text=window.findChild<QTextBrowser *>("GuideText");QVERIFY(language && chapters && text);QCOMPARE(text->document()->defaultFont().family(),QString("GetDAQ Guide Sans"));QCOMPARE(chapters->count(),20);QVERIFY(text->source().toString().startsWith("qrc:"));QVERIFY(text->toPlainText().contains(QString::fromUtf8("配置同步")));language->setCurrentIndex(language->findData("en"));QCOMPARE(chapters->count(),20);QVERIFY(text->toPlainText().contains("Start selected groups and use"));QVERIFY(text->toPlainText().contains("installation",Qt::CaseInsensitive));auto *search=window.findChild<QLineEdit *>("GuideSearch");search->setText("Node authentication required");window.findChild<QPushButton *>("GuideFindNext")->click();QCOMPARE(text->textCursor().selectedText(),QString("Node authentication required"));QVERIFY(QFile::exists(":/getdaq/waveform-3d-example.png"));
 }
 void guideMenuStaysAvailableDuringAcquisition(){
    MainWindow window;auto *chinese=window.findChild<QAction *>("UserGuideChinese"),*english=window.findChild<QAction *>("UserGuideEnglish");QVERIFY(chinese && english);QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Qt::DirectConnection,Q_ARG(QString,QString("Running")),Q_ARG(bool,true),Q_ARG(bool,true),Q_ARG(bool,true)));QVERIFY(chinese->isEnabled() && english->isEnabled());english->trigger();auto *guide=window.findChild<QDialog *>("UserGuideWindow");QVERIFY(guide && guide->isVisible());auto *language=guide->findChild<QComboBox *>("GuideLanguage");QCOMPARE(language->currentData().toString(),QString("en"));chinese->trigger();QCOMPARE(language->currentData().toString(),QString("zh"));
 }
 void initializedHostProjectFolderCanBeEditedAndSurvivesReload(){
    QTemporaryDir dir;QSettings settings;const auto cacheKey=StartupConfigurationKey()+"/slave-cache-directory",projectKey=StartupConfigurationKey()+"/distributed-project";const auto oldCache=settings.value(cacheKey),oldProject=settings.value(projectKey);
    std::unique_ptr<int,std::function<void(int *)>> restore(new int,[&](int *p){if(oldCache.isValid())settings.setValue(cacheKey,oldCache);else settings.remove(cacheKey);if(oldProject.isValid())settings.setValue(projectKey,oldProject);else settings.remove(projectKey);delete p;});
    const auto cache=dir.filePath("cache"),project=dir.filePath("project"),source=dir.filePath("source"),previous=dir.filePath("old-data"),chosen=dir.filePath("chosen-data");
    for(const auto &path:{cache,project,source,previous})Deployment::PrivateDirectory(path);
    Deployment::Write(cache+"/node.json",Deployment::Json({{"data_directory",previous}}));Deployment::Write(previous+"/keep.graw","raw data");
    for(const auto &name:{"describe-cobo0.xcfg","configure-pedestals.xcfg"})Deployment::Write(source+'/'+name,"<config/>");
    settings.setValue(cacheKey,cache);
    QJsonObject local{{"id","local"},{"name","This computer"},{"url","https://127.0.0.1:24260"},{"credential_file",project+"/nodes/local.token"},{"inventory",QJsonObject{{"data_directory",previous}}}};
    QJsonObject remote{{"id","remote"},{"name","Slave"},{"url","https://192.0.2.99:24260"},{"credential_file",project+"/nodes/remote.token"},{"inventory",QJsonObject{{"data_directory","/remote/data"}}}};
    QJsonObject group{{"id","group"},{"backend","original-soap"},{"project_mode",true},{"experiment","pedestals"},{"ecc",QJsonObject{{"node_id","local"}}},{"receivers",QJsonArray{QJsonObject{{"id","data"},{"node_id","remote"}}}},{"project_directories",QJsonObject{{"local",dir.filePath("existing-project-data")},{"remote","/remote/custom"}}}};
    Deployment::Write(project+"/project.json",Deployment::Json({{"schema",1},{"kind","getdaq.distributed-project"},{"nodes",QJsonArray{local,remote}},{"groups",QJsonArray{group}},{"configuration_source",source}}));
    auto base=RunConfig::Default();base.configDir=source;base.deploymentProject=project+"/project.json";
    {DistributedDeploymentWindow window(base);auto *folder=window.findChild<QLineEdit *>("MasterLocalRawDataRoot");QVERIFY(folder && !folder->isReadOnly());QVERIFY(window.findChild<QPushButton *>("MasterBrowseRawDataRoot")->isEnabled());QCOMPARE(folder->text(),dir.filePath("existing-project-data"));
     folder->setText(chosen);QVERIFY(QMetaObject::invokeMethod(&window,"ApplyLocalProjectFolder",Qt::DirectConnection));
     const auto saved=Deployment::Parse(Deployment::Read(project+"/project.json"));QCOMPARE(saved["local_project_directory"].toString(),chosen);const auto paths=saved["groups"].toArray()[0].toObject()["project_directories"].toObject();QCOMPARE(paths["local"].toString(),chosen);QCOMPARE(paths["remote"].toString(),QString("/remote/custom"));
     QCOMPARE(window.findChild<QTableWidget *>("DistributedHosts")->item(0,3)->text(),chosen);
     folder->setText("relative-folder");QVERIFY(QMetaObject::invokeMethod(&window,"ApplyLocalProjectFolder",Qt::DirectConnection));QCOMPARE(Deployment::Parse(Deployment::Read(project+"/project.json"))["local_project_directory"].toString(),chosen);
    }
    settings.setValue(projectKey,dir.filePath("unrelated"));DistributedDeploymentWindow reopened(base);QCOMPARE(reopened.findChild<QLineEdit *>("MasterLocalRawDataRoot")->text(),chosen);
    QCOMPARE(Deployment::Parse(Deployment::Read(cache+"/node.json"))["data_directory"].toString(),previous);QCOMPARE(Deployment::Read(previous+"/keep.graw"),QByteArray("raw data"));
 }
 void closePromptAllowsCancelOrKeepWithoutStoppingBackends(){
    QTemporaryDir dir;QSettings settings;const auto key=StartupConfigurationKey()+"/slave-cache-directory";const auto previous=settings.value(key);const bool quit=QApplication::quitOnLastWindowClosed();QApplication::setQuitOnLastWindowClosed(false);
    std::unique_ptr<int,std::function<void(int *)>> restore(new int,[&](int *p){if(previous.isValid())settings.setValue(key,previous);else settings.remove(key);QApplication::setQuitOnLastWindowClosed(quit);delete p;});settings.setValue(key,dir.filePath("no-node"));
    const auto empty=ManagedBackendExitPlan(QString(),dir.filePath("no-node"));QVERIFY(empty["targets"].toArray().isEmpty());QVERIFY(empty["errors"].toArray().isEmpty());
    QVERIFY_EXCEPTION_THROWN(StopManagedBackends(QJsonObject{{"errors",QJsonArray{"offline"}}},dir.filePath("audit")),std::exception);QVERIFY(!QFile::exists(dir.filePath("audit")));
    MainWindow window;auto config=RunConfig::Default();config.deploymentProject=dir.filePath("missing-project.json");window.SetConfig(config);window.show();
    for(const auto &choice:{QString("CancelBackendExit"),QString("KeepBackendsAndExit")}){
        bool handled=false,valid=false;QTimer timer;timer.setInterval(10);connect(&timer,&QTimer::timeout,&window,[&]{auto *prompt=window.findChild<QMessageBox *>("ManagedBackendExitPrompt");if(!prompt || !prompt->isVisible())return;auto *stop=prompt->findChild<QPushButton *>("StopBackendsAndExit"),*keep=prompt->findChild<QPushButton *>("KeepBackendsAndExit"),*cancel=prompt->findChild<QPushButton *>("CancelBackendExit");valid=stop && keep && cancel && !stop->isEnabled() && keep->isEnabled() && prompt->defaultButton()==cancel;handled=true;prompt->findChild<QPushButton *>(choice)->click();timer.stop();});timer.start();window.close();QTRY_VERIFY_WITH_TIMEOUT(handled,5000);QVERIFY(valid);
        if(choice=="CancelBackendExit"){QVERIFY(window.isVisible());QVERIFY(window.isEnabled());}else QTRY_VERIFY_WITH_TIMEOUT(!window.isVisible(),5000);
    }
 }
 void projectMonitorFollowsInstalledRuntimeWithoutChangingBoardPrograms(){
    QTemporaryDir dir;const auto old=dir.filePath("runtime-old/bin"),current=dir.filePath("runtime-current/bin");QVERIFY(QDir().mkpath(old));QVERIFY(QDir().mkpath(current));QVERIFY(QDir().mkpath(dir.filePath("site")));
    const auto executable=current+"/getdaq-sentry";QFile program(executable);QVERIFY(program.open(QIODevice::WriteOnly));program.write("#!/bin/sh\nexit 0\n");program.close();QVERIFY(program.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    QCOMPARE(Deployment::InstalledServiceProgram(old,"getdaq-sentry",old+"/getdaq-sentry"),old+"/getdaq-sentry");
    QFile selection(dir.filePath("site/gui-runtime.txt"));QVERIFY(selection.open(QIODevice::WriteOnly));selection.write("runtime-current\n");selection.close();QCOMPARE(Deployment::InstalledServiceProgram(old,"getdaq-sentry",old+"/getdaq-sentry"),executable);
    QVERIFY(QDir().mkpath(dir.filePath("bin")));const auto installed=dir.filePath("bin/getdaq-sentry");QVERIFY(QFile::copy(executable,installed));QVERIFY(QFile::setPermissions(installed,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));QVERIFY(selection.open(QIODevice::WriteOnly|QIODevice::Truncate));selection.write("installed\n");selection.close();QCOMPARE(Deployment::InstalledServiceProgram(dir.filePath("bin"),"getdaq-sentry",executable),installed);
    QVERIFY(selection.open(QIODevice::WriteOnly|QIODevice::Truncate));selection.write("../outside\n");selection.close();QVERIFY_EXCEPTION_THROWN(Deployment::InstalledServiceProgram(old,"getdaq-sentry",old+"/getdaq-sentry"),std::exception);
 }
 void newDefaultPathsUseVisibleHomeWorkspace(){
    const auto previous=qgetenv("GETDAQ_WORKSPACE");qunsetenv("GETDAQ_WORKSPACE");QCOMPARE(DefaultWorkspaceDirectory(),QDir(QDir::homePath()).filePath("GetDAQ"));const auto defaults=RunConfig::Default();QCOMPARE(defaults.outputDir,QDir(QDir::homePath()).filePath("GetDAQ/runs"));QVERIFY(!defaults.configDir.contains("/.local/"));qputenv("GETDAQ_WORKSPACE",previous);QCOMPARE(DefaultWorkspaceDirectory(),fWorkspace.path());
    ConsoleLogStore log;QVERIFY(log.OpenApplicationLog());QVERIFY(log.ApplicationLogPath().startsWith(fWorkspace.path()+"/runs/logs/sessions/"));
 }
 void sessionLogsFollowConfiguredGuiRecordsWithoutMixingSessions(){
    QTemporaryDir dir;ConsoleLogStore log;const auto original=dir.path()+"/first",selected=dir.path()+"/selected";
    QVERIFY(log.OpenApplicationLog(original));log.Append("before connection\n");QVERIFY(log.OpenConnectionLog());log.Append("before move\n");const auto old=log.ApplicationLogPath();QVERIFY(log.SetRecordsDirectory(selected));QVERIFY(log.ApplicationLogPath().startsWith(selected+"/logs/sessions/"));QVERIFY(log.ConnectionLogPath().startsWith(selected+"/logs/sessions/"));QVERIFY(!QFile::exists(old));log.Append("after move\n");
    QFile app(log.ApplicationLogPath()),connection(log.ConnectionLogPath());QVERIFY(app.open(QIODevice::ReadOnly));QVERIFY(connection.open(QIODevice::ReadOnly));QCOMPARE(app.readAll(),QByteArray("before connection\nbefore move\nafter move\n"));QCOMPARE(connection.readAll(),QByteArray("before move\nafter move\n"));
    QFile blocked(dir.path()+"/blocked");QVERIFY(blocked.open(QIODevice::WriteOnly));blocked.write("keep");blocked.close();QVERIFY(!log.SetRecordsDirectory(blocked.fileName()));log.Append("retained on failure\n");connection.seek(0);QVERIFY(connection.readAll().endsWith("retained on failure\n"));
 }
 void masterProjectRelocationPreservesPairingAndImmutableContent(){
    QTemporaryDir dir;const auto source=dir.path()+"/old",destination=dir.path()+"/visible/projects/example";Deployment::PrivateDirectory(source+"/nodes");
    Deployment::Write(source+"/nodes/node.token","same pairing secret");Deployment::Write(source+"/project.json",Deployment::Json({{"schema",1},{"nodes",QJsonArray{QJsonObject{{"id","node"},{"credential_file",source+"/nodes/node.token"}}}}}));
    const QByteArray immutable="immutable release bytes";Deployment::PrivateDirectory(source+"/repository/objects");Deployment::Write(source+"/repository/objects/blob",immutable,true);
    auto saved=RunConfig::Default();saved.deploymentProject=source+"/project.json";saved.outputDir=source+"/records";saved.configDir=source+"/xml";saved.configurationSnapshot=source+"/snapshot";saved.description="retain metadata";QString saveError;Deployment::PrivateDirectory(source+"/console");QVERIFY2(saved.Save(source+"/console/envoy.yml",&saveError),qPrintable(saveError));
    const auto pending=Deployment::Json({{"phase","pending"},{"directory",source+"/jobs/activate-original"},{"release_id",QString(64,'a')}});
    Deployment::PrivateDirectory(source+"/pending-activations");Deployment::PrivateDirectory(source+"/jobs/activate-original");Deployment::Write(source+"/pending-activations/all.json",pending);Deployment::Write(source+"/jobs/activate-original/activation-context.json",pending);
    const QByteArray request="{\"path\":\""+source.toUtf8()+"/immutable-request\"}";Deployment::Write(source+"/jobs/activate-original/request.json",request,true);
    const auto result=Deployment::RelocateMasterProject(source,destination,QString());QCOMPARE(result["directory"].toString(),destination);
    QCOMPARE(Deployment::Parse(Deployment::Read(destination+"/pending-activations/all.json"))["directory"].toString(),destination+"/jobs/activate-original");
    QCOMPARE(Deployment::Read(destination+"/jobs/activate-original/request.json"),request);
    const auto project=Deployment::Parse(Deployment::Read(destination+"/project.json"));QCOMPARE(project["nodes"].toArray()[0].toObject()["credential_file"].toString(),destination+"/nodes/node.token");QCOMPARE(Deployment::Read(destination+"/nodes/node.token"),QByteArray("same pairing secret"));QCOMPARE(Deployment::Read(destination+"/repository/objects/blob"),immutable);
    QCOMPARE(Deployment::Parse(Deployment::Read(source+"/relocated-to.json"))["directory"].toString(),destination);
    RunConfig redirected;QVERIFY(redirected.Load(source+"/console/envoy.yml",&saveError));QCOMPARE(redirected.filePath,destination+"/console/envoy.yml");QCOMPARE(redirected.deploymentProject,destination+"/project.json");QCOMPARE(redirected.outputDir,destination+"/records");QCOMPARE(redirected.configDir,destination+"/xml");QCOMPARE(redirected.configurationSnapshot,destination+"/snapshot");QCOMPARE(redirected.description,QString("retain metadata"));
    Deployment::PrivateDirectory(destination+"/console/release/group");QVERIFY(!(QFileInfo(destination+"/console/release").permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ExeGroup|QFile::ReadOther|QFile::WriteOther|QFile::ExeOther)));
    Deployment::PrivateDirectory(destination+"/console/release");
    QVERIFY_EXCEPTION_THROWN(Deployment::RelocateMasterProject(source,destination,QString()),std::exception);
 }
 void liveMasterDirectoryMovePreservesEndpointAndCredentials(){
    const auto programs=qEnvironmentVariable("GETDAQ_TEST_PROGRAMS");if(programs.isEmpty())QSKIP("Set GETDAQ_TEST_PROGRAMS to test real configuration-service relocation");
    QTemporaryDir dir;const auto source=dir.path()+"/old",destination=source+"/nested-project";
    QTcpServer port;QVERIFY(port.listen(QHostAddress::LocalHost,0));const int number=port.serverPort();port.close();
    QJsonObject before,after;std::unique_ptr<int,std::function<void(int *)>> cleanup(new int,[&](int *p){for(const auto &master:{after,before})if(!master.isEmpty()){try{Deployment::StopMaster(master);}catch(...){}}delete p;});
    before=Deployment::EnsureMaster(source,programs,"127.0.0.1",number);Deployment::Write(source+"/project.json",Deployment::Json({{"schema",1},{"nodes",QJsonArray()}}));
    const auto credential=Deployment::Read(before["admin_credential"].toString());after=Deployment::RelocateMasterProject(source,destination,programs)["master"].toObject();
    QCOMPARE(after["repository_id"],before["repository_id"]);QCOMPARE(after["certificate_sha256"],before["certificate_sha256"]);QCOMPARE(after["repository_url"],before["repository_url"]);QCOMPARE(Deployment::Read(after["admin_credential"].toString()),credential);
    QVERIFY(after["admin_credential"].toString().startsWith(destination+'/'));
 }
 void configurationSourceDoesNotDistributeNestedSoftwareState(){
    QTemporaryDir dir;const auto root=dir.path()+"/config",project=root+"/project-state",node=root+"/agent-state";Deployment::PrivateDirectory(root);Deployment::Write(root+"/CoboFormats.xcfg","<formats/>");
    Deployment::PrivateDirectory(project);Deployment::Write(project+"/project.json",Deployment::Json({{"kind","getdaq.distributed-project"}}));Deployment::Write(project+"/admin.token","private project credential");
    Deployment::PrivateDirectory(node);Deployment::Write(node+"/node.json",Deployment::Json({{"root",node},{"node_id","node"},{"credential_file",node+"/agent.token"}}));Deployment::Write(node+"/agent.token","private agent credential");
    Deployment::PrivateDirectory(root+"/resources");Deployment::Write(root+"/resources/project.json",Deployment::Json({{"kind","experiment-resource"}}));
    QMap<QString,QByteArray> files;const auto mapped=Deployment::OriginalRepository(root,"group",&files);QStringList paths;for(const auto &v:mapped)paths<<v.toObject()["path"].toString();QVERIFY(paths.contains("CoboFormats.xcfg"));QVERIFY(paths.contains("resources/project.json"));for(const auto &path:paths)QVERIFY(!path.startsWith("project-state/") && !path.startsWith("agent-state/"));
    const auto snapshot=project+"/configuration-used";Deployment::PrivateDirectory(snapshot);Deployment::Write(snapshot+"/CoboFormats.xcfg","<formats/>");files.clear();QCOMPARE(Deployment::OriginalRepository(snapshot,"group",&files).size(),1);
 }
 void projectFolderSelectionAcceptsCurrentParentAndNonemptyLocations(){
    QTemporaryDir dir;const auto source=dir.path()+"/projects/example";Deployment::PrivateDirectory(source);Deployment::Write(source+"/project.json",Deployment::Json({{"project_id","example"}}));
    QCOMPARE(Deployment::MasterProjectDestination(source,source),source);QCOMPARE(Deployment::MasterProjectDestination(source,dir.path()+"/projects"),source);QCOMPARE(Deployment::MasterProjectDestination(source,dir.path()),dir.path()+"/example");
    const auto selected=dir.path()+"/chosen";QVERIFY(QDir().mkpath(selected));QCOMPARE(Deployment::MasterProjectDestination(source,selected),selected);QVERIFY(QFile::link(selected,dir.path()+"/alias"));QCOMPARE(Deployment::MasterProjectDestination(source,dir.path()+"/alias"),selected);QFile other(selected+"/retain.txt");QVERIFY(other.open(QIODevice::WriteOnly));other.write("keep");other.close();QCOMPARE(Deployment::MasterProjectDestination(source,selected),selected+"/example");QVERIFY(QFile::exists(other.fileName()));
    QFile otherProject(selected+"/project.json");QVERIFY(otherProject.open(QIODevice::WriteOnly));otherProject.write(Deployment::Json({{"kind","getdaq.distributed-project"},{"project_id","other"}}));otherProject.close();QCOMPARE(Deployment::MasterProjectDestination(source,selected),selected+"/example");QVERIFY(!QFile::exists(selected+"/example/project.json"));
    const auto child=source+"/nested";QVERIFY(QDir().mkpath(child));QCOMPARE(Deployment::MasterProjectDestination(source,child),child);
 }
 void projectFolderRelocationAcceptsSameAndNestedDestinations(){
    QTemporaryDir dir;const auto source=dir.path()+"/project",destination=source+"/selected";Deployment::PrivateDirectory(source);const auto bytes=Deployment::Json({{"project_id","example"},{"record",source+"/file"}});Deployment::Write(source+"/project.json",bytes);Deployment::Write(source+"/file","content",true);
    const auto same=Deployment::RelocateMasterProject(source,source,QString());QVERIFY(same["unchanged"].toBool());QCOMPARE(Deployment::Read(source+"/project.json"),bytes);QVERIFY(!QFile::exists(source+"/relocated-to.json"));
    QVERIFY(QDir().mkpath(destination));QVERIFY(QFile::setPermissions(destination,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner|QFile::ReadGroup|QFile::ExeGroup|QFile::ReadOther|QFile::ExeOther));
    const auto moved=Deployment::RelocateMasterProject(source,destination,QString());QCOMPARE(moved["directory"].toString(),destination);QCOMPARE(Deployment::Read(destination+"/file"),QByteArray("content"));QCOMPARE(Deployment::Parse(Deployment::Read(destination+"/project.json"))["record"].toString(),destination+"/file");QVERIFY(!QFile::exists(destination+"/selected"));QVERIFY(!QFile::exists(destination+"/relocated-to.json"));
 }
 void dormantProjectDirectoryMoveDoesNotStartConflictingService(){
    const auto programs=qEnvironmentVariable("GETDAQ_TEST_PROGRAMS");if(programs.isEmpty())QSKIP("Set GETDAQ_TEST_PROGRAMS for real dormant master relocation checks");
    QTemporaryDir dir;auto freePort=[](){QTcpServer server;if(!server.listen(QHostAddress::LocalHost,0))return 0;return int(server.serverPort());};const int occupied=freePort();QVERIFY(occupied);int available=freePort();while(available==occupied)available=freePort();QVERIFY(available);
    const auto liveRoot=dir.path()+"/live",source=dir.path()+"/draft",destination=dir.path()+"/selected";QJsonObject live,moved;std::unique_ptr<int,std::function<void(int *)>> cleanup(new int,[&](int *p){for(const auto &m:{moved,live})if(!m.isEmpty()){try{Deployment::StopMaster(m);}catch(...){}}delete p;});
    live=Deployment::EnsureMaster(liveRoot,programs,"127.0.0.1",occupied);QVERIFY_EXCEPTION_THROWN(Deployment::EnsureMaster(source,programs,"127.0.0.1",occupied),std::exception);Deployment::Write(source+"/project.json",Deployment::Json({{"schema",1},{"nodes",QJsonArray()}}));
    const auto result=Deployment::RelocateMasterProject(source,destination,programs);QVERIFY(!result["service_running"].toBool());QCOMPARE(result["master"].toObject()["root"].toString(),destination);QVERIFY(FindLocalArgumentProcesses(programs+"/getdaq-config-service","--root",destination+"/repository").isEmpty());QCOMPARE(FindLocalArgumentProcesses(programs+"/getdaq-config-service","--root",liveRoot+"/repository").size(),1);
    moved=Deployment::EnsureMaster(destination,programs,"127.0.0.1",available);QCOMPARE(moved["port"].toInt(),available);QCOMPARE(moved["repository_id"].toString(),QString::fromLatin1(Deployment::Read(destination+"/repository/identity").trimmed()));QCOMPARE(moved["certificate_sha256"],result["master"].toObject()["certificate_sha256"]);
 }
 void missingAcquisitionProjectDoesNotOverrideSavedAvailableProject(){
    QTemporaryDir dir;const auto project=dir.path()+"/available";Deployment::PrivateDirectory(project);Deployment::Write(project+"/project.json",Deployment::Json({{"schema",1},{"kind","getdaq.distributed-project"},{"project_id","available"},{"nodes",QJsonArray()},{"groups",QJsonArray()},{"content_files",QJsonArray()}}));
    const auto key=StartupConfigurationKey()+"/distributed-project";QSettings settings;const bool had=settings.contains(key);const auto old=settings.value(key);std::unique_ptr<int,std::function<void(int *)>> restore(new int,[&](int *p){if(had)settings.setValue(key,old);else settings.remove(key);delete p;});settings.setValue(key,project);
    auto base=RunConfig::Default();base.filePath=dir.path()+"/acquisition.yml";base.deploymentProject=dir.path()+"/removed/project.json";
    DistributedDeploymentWindow window(base);auto *field=window.findChild<QLineEdit *>("DistributedProjectRoot");QVERIFY(field);QCOMPARE(field->text(),project);
 }
 void staleSavedCacheUsesVisibleDirectoryWithoutLosingExplicitCustomPaths(){
    QTemporaryDir data,workspace;const auto oldData=qgetenv("XDG_DATA_HOME"),oldWorkspace=qgetenv("GETDAQ_WORKSPACE");const bool hadData=qEnvironmentVariableIsSet("XDG_DATA_HOME");std::unique_ptr<int,std::function<void(int *)>> restore(new int,[=](int *p){if(hadData)qputenv("XDG_DATA_HOME",oldData);else qunsetenv("XDG_DATA_HOME");qputenv("GETDAQ_WORKSPACE",oldWorkspace);delete p;});qputenv("XDG_DATA_HOME",data.path().toUtf8());qputenv("GETDAQ_WORKSPACE",workspace.path().toUtf8());
    const auto legacy=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/local-node",visible=workspace.path()+"/local-node";QCOMPARE(PreferredNodeCacheDirectory(legacy),visible);QCOMPARE(PreferredNodeCacheDirectory(workspace.path()+"/custom"),workspace.path()+"/custom");
    QVERIFY(QDir().mkpath(legacy));QFile old(legacy+"/node.json");QVERIFY(old.open(QIODevice::WriteOnly));old.write("{}");old.close();QCOMPARE(PreferredNodeCacheDirectory(legacy),legacy);
    QVERIFY(QDir().mkpath(visible));QFile current(visible+"/node.json");QVERIFY(current.open(QIODevice::WriteOnly));current.write("{}");current.close();QCOMPARE(PreferredNodeCacheDirectory(legacy),visible);
 }
 void orphanedConfigurationServiceIsRetiredWithoutStoppingHealthyProject(){
    const auto programs=qEnvironmentVariable("GETDAQ_TEST_PROGRAMS");if(programs.isEmpty())QSKIP("Set GETDAQ_TEST_PROGRAMS for real management-process lifecycle tests");
    QTemporaryDir dir;const auto bin=dir.path()+"/bin",orphanRoot=dir.path()+"/orphan",healthyRoot=dir.path()+"/healthy";Deployment::PrivateDirectory(bin);
    for(const auto &name:{"getdaq-config-service","getdaq-config-repository"}){QVERIFY(QFile::copy(programs+'/'+name,bin+'/'+name));QVERIFY(QFile::setPermissions(bin+'/'+name,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));}
    auto freePort=[](){QTcpServer s;if(!s.listen(QHostAddress::LocalHost,0))return 0;return int(s.serverPort());};const int first=freePort();int second=freePort();while(second==first)second=freePort();QVERIFY(first && second);
    QJsonObject a,b;std::unique_ptr<int,std::function<void(int *)>> cleanup(new int,[&](int *p){for(const auto &m:{a,b})if(!m.isEmpty()){try{Deployment::StopMaster(m);}catch(...){}}delete p;});
    a=Deployment::EnsureMaster(orphanRoot,bin,"127.0.0.1",first);b=Deployment::EnsureMaster(healthyRoot,bin,"127.0.0.1",second);
    QVERIFY(QFile::remove(healthyRoot+"/repository.log"));QCOMPARE(CleanupOrphanedLocalServiceProcesses(bin+"/getdaq-config-service","--root"),0);
    QVERIFY(QDir(orphanRoot).removeRecursively());Deployment::PrivateDirectory(orphanRoot+"/repository");QCOMPARE(CleanupOrphanedLocalServiceProcesses(bin+"/getdaq-config-service","--root"),1);
    QVERIFY(FindLocalArgumentProcesses(bin+"/getdaq-config-service","--root",orphanRoot+"/repository").isEmpty());QCOMPARE(FindLocalArgumentProcesses(bin+"/getdaq-config-service","--root",healthyRoot+"/repository").size(),1);
    const Deployment::Endpoint endpoint{b["repository_url"].toString(),b["certificate_sha256"].toString(),Deployment::Secret(b["admin_credential"].toString())};QCOMPARE(Deployment::Get(endpoint,"/v1/identity")["repository_id"],b["repository_id"]);
 }
 void existingNodeCacheIsReusedInsteadOfCreatingAnotherIdentity(){
    QTemporaryDir dir,workspace;const auto oldData=qgetenv("XDG_DATA_HOME"),oldWorkspace=qgetenv("GETDAQ_WORKSPACE");const bool dataWasSet=qEnvironmentVariableIsSet("XDG_DATA_HOME");std::unique_ptr<int,std::function<void(int *)>> restore(new int,[=](int *p){if(dataWasSet)qputenv("XDG_DATA_HOME",oldData);else qunsetenv("XDG_DATA_HOME");qputenv("GETDAQ_WORKSPACE",oldWorkspace);delete p;});qputenv("XDG_DATA_HOME",dir.path().toUtf8());qputenv("GETDAQ_WORKSPACE",workspace.path().toUtf8());
    const auto visible=workspace.path()+"/local-node";const auto legacy=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/local-node";QCOMPARE(DefaultNodeCacheDirectory(),visible);QVERIFY(QDir().mkpath(legacy));QFile old(legacy+"/node.json");QVERIFY(old.open(QIODevice::WriteOnly));old.write("{}");old.close();QCOMPARE(DefaultNodeCacheDirectory(),legacy);QVERIFY(QDir().mkpath(visible));QFile current(visible+"/node.json");QVERIFY(current.open(QIODevice::WriteOnly));current.write("{}");current.close();QCOMPARE(DefaultNodeCacheDirectory(),visible);
 }
 void pauseResumeMenusUseObservedHardwareStateEvenWithoutLocalRun(){
    MainWindow window;auto *pause=window.findChild<QAction *>("PauseRun"),*resume=window.findChild<QAction *>("ResumeRun");QVERIFY(pause && resume);
    for(int state:{4,5,6}){NodeStatus status;status.id=0;status.state=state;status.transition=0;status.eccOnline=true;QVERIFY(QMetaObject::invokeMethod(&window,"ShowStatusSlot",Qt::DirectConnection,Q_ARG(QVector<NodeStatus>,QVector<NodeStatus>{status})));QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Qt::DirectConnection,Q_ARG(QString,QString("Monitoring")),Q_ARG(bool,true),Q_ARG(bool,false),Q_ARG(bool,false)));QCOMPARE(pause->isEnabled(),state==5);QCOMPARE(resume->isEnabled(),state==6);}
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Qt::DirectConnection,Q_ARG(QString,QString("Paused")),Q_ARG(bool,true),Q_ARG(bool,true),Q_ARG(bool,false)));QVERIFY(!pause->isEnabled() && !resume->isEnabled());
 }
 void externalRunCanPauseAndResumeWithoutCreatingLocalRecords(){
    for(bool attach:{false,true}){QTemporaryDir dir;auto c=Config(dir.path());for(auto *mock:QCoreApplication::instance()->findChildren<GetDAQTestFixtures::MockNode *>())for(const auto &node:c.nodes)if(mock->serverPort()==node.eccPort)mock->state=5;
     RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),flags(&control,&RunControl::ControlSignal),warnings(&control,&RunControl::OperationWarningSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);if(attach){control.StartRunSlot();if(!warnings.isEmpty())control.AcceptOperationWarningSlot(warnings.last()[0].toString());QTRY_VERIFY_WITH_TIMEOUT(flags.last()[3].toBool(),3000);}control.PauseSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,6),3000);QTRY_VERIFY_WITH_TIMEOUT(!flags.last()[2].toBool(),3000);QVERIFY(!QFileInfo::exists(c.RunDirectory()));control.ResumeSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);QCOMPARE(flags.last()[3].toBool(),attach);QVERIFY(!QFileInfo::exists(c.RunDirectory()));control.StopRunSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,4),3000);QTRY_VERIFY_WITH_TIMEOUT(!flags.last()[2].toBool(),3000);QVERIFY(!QFileInfo::exists(c.RunDirectory()));control.DisconnectSlot();}
 }

 void threeDimensionalWaveformsHaveAxesAndSeparation(){
    GetDAQ::WaveformCanvas canvas;canvas.SetBaselineAlignment(false);canvas.resize(900,600);QVector<GetDAQ::WaveformPlotTrace> traces;
    for(int channel:{2,7,22}){GetDAQ::WaveformPlotTrace trace;trace.channel=channel;for(int i=0;i<512;++i)trace.samples<<QPointF(i,1000+channel+40*std::sin(i/30.0));traces<<trace;}canvas.SetTraces(traces,1);canvas.SetSurface(true);canvas.show();QTest::qWait(20);const auto image=canvas.grab().toImage();QVERIFY(!image.isNull());const auto text=canvas.accessibleDescription();QVERIFY(text.contains("Time: sample bucket"));QVERIFY(text.contains("Amplitude: ADC"));QVERIFY(text.contains("Depth: channel number 2 to 22"));QVERIFY(text.contains("3 selected traces; no interpolated channels"));QVERIFY(!text.contains("ADC 0 to"));
    const int before=canvas.minimumHeight();QWheelEvent wheel(QPointF(200,200),QPointF(200,200),QPoint(),QPoint(0,120),Qt::NoButton,Qt::ControlModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(&canvas,&wheel);QVERIFY(canvas.minimumHeight()>before);canvas.SetSurface(false);QCOMPARE(canvas.minimumHeight(),360);canvas.Clear();canvas.SetSurface(true);QVERIFY(!canvas.grab().isNull());
 }
 void threeDimensionalChannelPathsDoNotFillTheirInteriors(){
    GetDAQ::WaveformCanvas canvas;canvas.SetBaselineAlignment(false);canvas.resize(900,600);QVector<GetDAQ::WaveformPlotTrace> traces;
    for(int channel:{0,1}){GetDAQ::WaveformPlotTrace trace;trace.channel=channel;for(int bucket=0;bucket<512;++bucket)trace.samples<<QPointF(bucket,1000+(channel==1?400*(1-std::abs(bucket-256)/256.0):0));traces<<trace;}
    canvas.SetTraces(traces,0);canvas.SetSurface(true);canvas.show();QTest::qWait(20);const auto image=canvas.grab().toImage();
    // This region is inside the far channel's pulse but above the shared floor.
    // It must remain background, rather than an opaque sheet hiding other traces.
    for(int y=185;y<=195;++y)for(int x=485;x<=495;++x){const auto pixel=image.pixelColor(x,y);QVERIFY(pixel.red()>252 && pixel.green()>252 && pixel.blue()>252);}
 }
 void threeDimensionalBaselinesAlignDespiteDifferentPedestalsAndPreserveRawAdc(){
    GetDAQ::WaveformCanvas canvas;canvas.resize(900,600);QVector<GetDAQ::WaveformPlotTrace> raw,equal;
    const QVector<int> pedestals{400,650,530,1200};for(int channel=0;channel<4;++channel){GetDAQ::WaveformPlotTrace a,b;a.channel=b.channel=channel;for(int bucket=0;bucket<512;++bucket){const int pulse=qMax(0,30-std::abs(bucket-240));a.samples<<QPointF(bucket,pedestals[channel]+pulse);b.samples<<QPointF(bucket,700+pulse);}raw<<a;equal<<b;}
    canvas.SetTraces(raw,0);canvas.SetSurface(true);canvas.show();QTest::qWait(20);const auto aligned=canvas.grab().toImage();QVERIFY(canvas.accessibleDescription().contains("delta ADC"));QVERIFY(canvas.accessibleDescription().contains("median baseline aligned to zero"));QCOMPARE(canvas.Traces()[1].samples[240].y(),680.0);
    canvas.SetTraces(equal,0);QCOMPARE(canvas.grab().toImage(),aligned);canvas.SetTraces(raw,0);canvas.SetBaselineAlignment(false);QVERIFY(canvas.grab().toImage()!=aligned);QVERIFY(canvas.accessibleDescription().contains("Absolute ADC"));QCOMPARE(canvas.Traces()[1].samples[240].y(),680.0);
    canvas.SetSurface(false);const auto absolute2D=canvas.grab().toImage();canvas.SetBaselineAlignment(true);QCOMPARE(canvas.grab().toImage(),absolute2D);
 }
 void previewSourcesUseTheActualExportPortAndRoundTrip(){
    QTemporaryDir dir;auto c=Config(dir.path());c.nodes[0].previewHost="192.0.2.50";c.nodes[0].previewPort=46492;QString error;QVERIFY(c.Save(dir.filePath("preview.yml"),&error));RunConfig loaded;QVERIFY(loaded.Load(dir.filePath("preview.yml"),&error));QCOMPARE(loaded.nodes[0].previewPort,46492);QCOMPARE(loaded.nodes[0].previewHost,QString("192.0.2.50"));QVERIFY(ExporterSources(loaded)[0].toObject()["endpoint"]=="192.0.2.50:46492");
    QJsonObject group{{"id","group"},{"name","Board"},{"receiver_backend","DataExporter"},{"receiver_ip","198.51.100.50"},{"export_port",46500},{"ecc",QJsonObject{{"address",c.nodes[0].host},{"port",c.nodes[0].eccPort}}},{"receivers",QJsonArray{QJsonObject{{"id","data"},{"node_id","host"},{"address","192.0.2.1"}}}}};Deployment::Write(dir.filePath("project.json"),Deployment::Json({{"groups",QJsonArray{group}}}));c.deploymentProject=dir.filePath("project.json");auto sources=ExporterSources(c);QCOMPARE(sources.first().toObject()["endpoint"].toString(),QString("198.51.100.50:46500"));QVERIFY(!sources.first().toObject()["endpoint"].toString().endsWith(":46005"));
 }
 void editedPathsShowCurrentSettingsBeforeOldPreparedPaths(){
    QJsonObject group{{"id","g"},{"experiment","pedestals"},{"project_mode",true},{"project_directories",QJsonObject{{"host","/new-data"}}},{"ecc",QJsonObject{{"node_id","host"}}},{"receivers",QJsonArray{QJsonObject{{"node_id","host"},{"id","data"}}}}};QJsonObject instance{{"group_id","g"},{"role","receiver"},{"receiver_id","data"},{"data_directory","/old-data"},{"actual_pids",QJsonArray{123}}};QJsonArray hosts{QJsonObject{{"id","host"},{"name","This computer"},{"inventory",QJsonObject{{"instances",QJsonArray{instance}},{"data_directory","/old-data"}}}}};const auto text=DeploymentPathSummary(group,hosts,"/new-source","/new-records");QVERIFY(text.contains("current setting): /new-data"));QVERIFY(text.contains("/new-data/pedestals/run_NNNN"));QVERIFY(text.contains("master, current setting): /new-source"));QVERIFY(text.contains("Folder change pending"));QVERIFY(text.contains("running folder on This computer: /old-data"));QVERIFY(text.endsWith("/new-records"));
 }
 void forgottenConnectionsStayRemovedWithoutDeletingFiles(){
    QTemporaryDir dir;auto c=Config(dir.path());QString error;QVERIFY(c.Save(dir.filePath("saved.yml"),&error));const auto path=dir.filePath("saved.yml");RememberEccConnection(path);QVERIFY(KnownEccConnectionProfiles().contains(path));ForgetEccConnection(path);QVERIFY(!KnownEccConnectionProfiles().contains(path));QVERIFY(ForgottenEccConnection(path));QVERIFY(QFileInfo::exists(path));RememberEccConnection(path);QVERIFY(KnownEccConnectionProfiles().contains(path));ForgetEccConnection(path);
 }
 void applicationLogsAreSeparateForEachGuiLaunch(){
    QTemporaryDir dir;const auto previous=qgetenv("XDG_DATA_HOME");const bool wasSet=qEnvironmentVariableIsSet("XDG_DATA_HOME");std::unique_ptr<int,std::function<void(int *)>> restore(new int,[=](int *p){if(wasSet)qputenv("XDG_DATA_HOME",previous);else qunsetenv("XDG_DATA_HOME");delete p;});qputenv("XDG_DATA_HOME",dir.path().toUtf8());ConsoleLogStore first,second;QVERIFY(first.OpenApplicationLog());first.Append("OLD GUI\n");QVERIFY(second.OpenApplicationLog());second.Append("NEW GUI\n");QVERIFY(first.ApplicationLogPath()!=second.ApplicationLogPath());QVERIFY(second.ApplicationLogPath().contains("/sessions/"));QFile current(second.ApplicationLogPath());QVERIFY(current.open(QIODevice::ReadOnly));QCOMPARE(current.readAll(),QByteArray("NEW GUI\n"));
 }
 void savedConnectionRemovalButtonForgetsTheSelectedEntry(){
    QTemporaryDir dir;auto c=Config(dir.path());QString error;const auto path=dir.filePath("saved.yml");QVERIFY(c.Save(path,&error));RememberEccConnection(path);c.filePath=path;UnifiedEccWindow window(c);auto *saved=window.findChild<QComboBox *>("SavedEccConnections");auto *remove=window.findChild<QPushButton *>("RemoveSavedEccConnection");QVERIFY(saved && remove);QVERIFY(saved->findData(path)>0);remove->click();QVERIFY(saved->findData(path)<0);QVERIFY(!KnownEccConnectionProfiles().contains(path));QVERIFY(QFileInfo::exists(path));
 }
 void restartedLogCollectionSkipsExistingOutput(){
    QTemporaryDir dir;const auto path=dir.filePath("backend.log");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("OLD SESSION\n");file.close();const auto baseline=Deployment::ReadServiceLog(path,0,{},true);QVERIFY(baseline["text"].toString().isEmpty());QVERIFY(file.open(QIODevice::Append));file.write("CURRENT SESSION\n");file.close();const auto fresh=Deployment::ReadServiceLog(path,qint64(baseline["offset"].toDouble()),baseline["generation"].toString());QCOMPARE(fresh["text"].toString(),QString("CURRENT SESSION\n"));const auto reopened=Deployment::ReadServiceLog(path,0,{},true);QVERIFY(reopened["text"].toString().isEmpty());
 }
 void masterSetupDefaultsToExpandedAndAllowsMultipleRows(){
    QTemporaryDir dir;auto c=Config(dir.path());DistributedDeploymentWindow window(c);auto *advanced=window.findChild<QCheckBox *>("DistributedAdvancedSettings");auto *table=window.findChild<QTableWidget *>("DistributedGroups");QVERIFY(advanced && advanced->isChecked());QVERIFY(table);QCOMPARE(table->selectionMode(),QAbstractItemView::ExtendedSelection);table->setRowCount(2);for(int row=0;row<2;++row){auto *item=new QTableWidgetItem(QString::number(row));item->setData(Qt::UserRole,"group"+QString::number(row));table->setItem(row,0,item);}table->selectRow(0);table->selectionModel()->select(table->model()->index(1,0),QItemSelectionModel::Select|QItemSelectionModel::Rows);QCOMPARE(table->selectionModel()->selectedRows().size(),2);
 }
 void selectedRunDoesNotRequireUnselectedStoppedGroupsOrRenumberRemoteCobo(){
    QTemporaryDir dir;auto base=Config(dir.path());base.filePath=dir.filePath("explicit.yml");base.outputDir=dir.filePath("new-records");auto makeGroup=[&](QString id,int port){return QJsonObject{{"id",id},{"name",id},{"experiment","pedestals"},{"project_mode",true},{"sync_configuration",false},{"master_repository",dir.path()},{"repository_files",QJsonArray{QJsonObject{{"path","describe-pedestals.xcfg"}},QJsonObject{{"path","configure-pedestals.xcfg"}}}},{"board_index",0},{"receiver_backend","DataExporter"},{"receiver_ip","127.0.0.1"},{"data_port",46005},{"export_port",46491},{"sentry_port",8080},{"ecc",QJsonObject{{"address","127.0.0.1"},{"port",port}}}};};const auto a=makeGroup("a",18083),b=makeGroup("b",18084);QJsonObject master{{"root",dir.path()}},preparation{{"manifest",QJsonObject{{"groups",QJsonArray{a,b}}}}};QJsonObject active{{"a-ecc",QJsonObject{{"phase","active"},{"release_id",QString(64,'a')}}},{"b-ecc",QJsonObject{{"phase","released-after-inspection"},{"release_id",QString(64,'a')}}}};Deployment::Write(dir.filePath("active-instances.json"),Deployment::Json(active));auto single=Deployment::ConsoleConfiguration(master,preparation,{},"a",base,{"a"});QCOMPARE(single.nodes.size(),1);QCOMPARE(single.deploymentGroups,QStringList{"a"});active["b-ecc"]=QJsonObject{{"phase","active"},{"release_id",QString(64,'a')}};Deployment::Write(dir.filePath("active-instances.json"),Deployment::Json(active));auto multiple=Deployment::ConsoleConfiguration(master,preparation,{},"a",base,{"a","b"});QCOMPARE(multiple.nodes.size(),2);QVERIFY(multiple.nodes[0].id!=multiple.nodes[1].id);QCOMPARE(multiple.nodes[1].boardIndex,0);QCOMPARE(multiple.outputDir,base.outputDir);QVERIFY(Protocol::EccRequest(multiple.nodes[1],multiple.experiment,"Configure").contains("CoBo[0]"));RunConfig loaded;QString error;QVERIFY(loaded.Load(multiple.filePath,&error));QCOMPARE(loaded.nodes[1].boardIndex,0);QCOMPARE(loaded.deploymentGroups.size(),2);
    Deployment::Write(dir.filePath("project.json"),Deployment::Json({{"groups",QJsonArray{a,b}}}));Deployment::Write(dir.filePath("master.json"),Deployment::Json({{"root",dir.path()}}));Deployment::Write(dir.filePath("active-instances.json"),Deployment::Json({{"b-ecc",QJsonObject{{"phase","active"},{"group_id","b"},{"node_id","unselected-unavailable"}}}}));single.syncConfiguration=true;const auto refreshed=Deployment::RefreshProjectRunConfiguration(single);QCOMPARE(refreshed.deploymentGroups,QStringList{"a"});
 }
 void slaveCanRemoveInactiveEntriesWhileLiveInstancesRemainVisible(){
    QTemporaryDir dir;QSettings settings;const auto key=StartupConfigurationKey()+"/slave-cache-directory";const auto previous=settings.value(key);std::unique_ptr<int,std::function<void(int *)>> restore(new int,[&](int *p){if(previous.isValid())settings.setValue(key,previous);else settings.remove(key);delete p;});settings.setValue(key,dir.path());HostRoleWindow window(RunConfig::Default());QJsonObject instance{{"instance_id","old-ecc"},{"release_id","release"},{"preparation_id","prepare"},{"role","ecc"},{"phase","released-after-inspection"},{"actual_pids",QJsonArray{}}};QJsonObject node{{"inventory",QJsonObject{{"instances",QJsonArray{instance}}}}};QVERIFY(QMetaObject::invokeMethod(&window,"ShowNode",Qt::DirectConnection,Q_ARG(QJsonObject,node)));auto *table=window.findChild<QTableWidget *>("SlaveCachedInstances");auto *remove=window.findChild<QPushButton *>("RemoveSlaveSavedInstance");QVERIFY(table && remove);QCOMPARE(table->rowCount(),1);table->selectRow(0);QVERIFY(remove->isEnabled());remove->click();QCOMPARE(table->rowCount(),0);instance["actual_pids"]=QJsonArray{99};node["inventory"]=QJsonObject{{"instances",QJsonArray{instance}}};QVERIFY(QMetaObject::invokeMethod(&window,"ShowNode",Qt::DirectConnection,Q_ARG(QJsonObject,node)));QCOMPARE(table->rowCount(),1);table->selectRow(0);QVERIFY(!remove->isEnabled());
 }
 void controlsRemainAvailableDuringLocalBookkeeping(){
    MainWindow window;QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Qt::DirectConnection,Q_ARG(QString,QString("Settling")),Q_ARG(bool,true),Q_ARG(bool,true),Q_ARG(bool,true)));for(const auto &name:{"StartRun","StopRun","Disconnect"}){auto *button=window.findChild<QPushButton *>(name);QVERIFY(button);QVERIFY(button->isEnabled());}
 }
 void slaveInvitationCanActuallyBeCopied(){
    QTemporaryDir dir;QSettings settings;const auto key=StartupConfigurationKey()+"/slave-cache-directory";const auto previous=settings.value(key);std::unique_ptr<int,std::function<void(int *)>> restore(new int,[&](int *p){if(previous.isValid())settings.setValue(key,previous);else settings.remove(key);delete p;});settings.setValue(key,dir.path());
    Deployment::Write(dir.filePath("pairing.json"),Deployment::Json({{"pairing_code","test-invitation"},{"expires_ms",double(QDateTime::currentMSecsSinceEpoch()+60000)},{"url","https://127.0.0.1:24260"}}));HostRoleWindow window(RunConfig::Default());QVERIFY(QMetaObject::invokeMethod(&window,"ShowNode",Qt::DirectConnection,Q_ARG(QJsonObject,QJsonObject())));auto *copy=window.findChild<QPushButton *>("CopySlaveInvitation");QVERIFY(copy && copy->isEnabled());copy->click();QVERIFY(QApplication::clipboard()->text().startsWith("getdaq-pair:"));QVERIFY(window.findChild<QPushButton *>("RenewSlavePairing"));
 }
 void operatorCanDisconnectRunningAfterWarning(){
    QTemporaryDir dir;auto c=Config(dir.path());RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),warnings(&control,&RunControl::OperationWarningSignal),flags(&control,&RunControl::ControlSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,1),3000);for(int s=2;s<=4;++s){control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,s),3000);}control.StartRunSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);control.DisconnectSlot();QCOMPARE(warnings.size(),1);QVERIFY(flags.last()[1].toBool());control.AcceptOperationWarningSlot(warnings.last()[0].toString());QVERIFY(!flags.last()[1].toBool());
 }
 void externalRunningCanBeStoppedAndPendingConfigurationIsWarning(){
    QTemporaryDir dir;auto c=Config(dir.path());for(auto *mock:QCoreApplication::instance()->findChildren<GetDAQTestFixtures::MockNode *>())for(const auto &node:c.nodes)if(mock->serverPort()==node.eccPort)mock->state=5;
    RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),flags(&control,&RunControl::ControlSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);control.StopRunSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,4),3000);QTRY_VERIFY_WITH_TIMEOUT(!flags.last()[2].toBool(),3000);control.DisconnectSlot();
 }
 void attachedRunStopDoesNotInventCatalog(){
    QTemporaryDir dir;auto c=Config(dir.path());for(auto *mock:QCoreApplication::instance()->findChildren<GetDAQTestFixtures::MockNode *>())for(const auto &node:c.nodes)if(mock->serverPort()==node.eccPort)mock->state=5;
    RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),warnings(&control,&RunControl::OperationWarningSignal),flags(&control,&RunControl::ControlSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);control.StartRunSlot();if(!warnings.isEmpty())control.AcceptOperationWarningSlot(warnings.last()[0].toString());control.StopRunSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,4),3000);QTRY_VERIFY_WITH_TIMEOUT(!flags.last()[2].toBool(),3000);QVERIFY(!QFileInfo::exists(c.RunDirectory()+"/journal.json"));control.DisconnectSlot();
 }
 void localFinalizationFailureAllowsNextRun(){
    QTemporaryDir dir;auto c=Config(dir.path());RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),finished(&control,&RunControl::RunFinishedSignal),logs(&control,&RunControl::LogSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,1),3000);for(int s=2;s<=4;++s){control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,s),3000);}control.StartRunSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);
    QDir records(c.RunDirectory());QVERIFY(records.removeRecursively());QFile blocker(c.RunDirectory());QVERIFY(blocker.open(QIODevice::WriteOnly));blocker.write("preserve");blocker.close();control.StopRunSlot();QTRY_VERIFY_WITH_TIMEOUT(finished.size()==1,7000);QVERIFY(At(states,4));bool warned=false;for(const auto &entry:logs)if(entry[0].toString().contains("records are incomplete"))warned=true;QVERIFY(warned);control.DisconnectSlot();
 }
 void onlineMemoryDecoderAndSessionCache(){
    const auto bytes=GetDAQTestInternal::MakeType1Frame();QTemporaryDir dir;QVERIFY(GetDAQTestInternal::WriteWaveformFixture(dir.filePath("frame.graw"),bytes));const auto memory=GetDAQ::GrawFrameReader::BuildIndex(bytes),file=GetDAQ::GrawFrameReader::BuildIndex(dir.filePath("frame.graw"));QVERIFY(memory.error.isEmpty());QCOMPARE(memory.frames.size(),file.frames.size());QVector<GetDAQ::GrawChannelSamples> a,b;QString error;QVERIFY(GetDAQ::GrawFrameReader::ReadChannels(bytes,memory.frames.first(),2,{7,8},&a,&error));QVERIFY(GetDAQ::GrawFrameReader::ReadChannels(dir.filePath("frame.graw"),file.frames.first(),2,{7,8},&b,&error));QCOMPARE(a.size(),b.size());
    GetDAQ::OnlineStore store;for(int i=0;i<120;++i){auto frame=bytes;GetDAQTestInternal::Put32BigEndian(&frame,22,i);store.Insert("source",frame);}QVERIFY(store.Snapshot().size()<=100);auto frame=bytes;GetDAQTestInternal::Put32BigEndian(&frame,22,0);store.Insert("source",frame);QCOMPARE(store.Snapshot().size(),1);QVERIFY(store.Statistics().contains("discarded"));
 }
 void onlineCacheEvictsOldFramesAndReceivedBytesAreOnlyACounter(){
    using namespace GetDAQTestInternal;const auto fixture=MakeType1Frame();
    {GetDAQ::OnlineStore store;for(int event=0;event<1000;++event){auto frame=fixture;Put32BigEndian(&frame,22,event);store.Insert("source",frame);QVERIFY(store.events.size()<=100);}
     QCOMPARE(store.events.size(),100);QCOMPARE(quint32(store.events.first().key),quint32(900));QCOMPARE(store.cachedFrames,100);QCOMPARE(store.cachedBytes,qint64(100)*fixture.size());QCOMPARE(store.bytes,quint64(1000)*fixture.size());QVERIFY(store.Statistics().contains("Received total"));QVERIFY(store.Statistics().contains("/ 256 MiB"));store.Reset();QVERIFY(store.Snapshot().isEmpty());QCOMPARE(store.cachedBytes,qint64(0));QCOMPARE(store.cachedFrames,0);QCOMPARE(store.bytes,quint64(1000)*fixture.size());}
    {GetDAQ::OnlineStore store;QByteArray frame(32,0);Put24BigEndian(&frame,1,frame.size());Put16BigEndian(&frame,5,1);frame[7]=5;Put16BigEndian(&frame,8,28);Put16BigEndian(&frame,10,4);Put32BigEndian(&frame,12,1);Put32BigEndian(&frame,22,1);
     for(int item=0;item<4097;++item){Put32BigEndian(&frame,28,item);store.Insert("source",frame);QVERIFY(store.cachedFrames<=4096);if(item==4095)QCOMPARE(store.cachedFrames,4096);}QVERIFY(store.events.isEmpty());QCOMPARE(store.cachedBytes,qint64(0));QVERIFY(store.dropped>0);QCOMPARE(store.errors,quint64(0));}
    {GetDAQ::OnlineStore store;auto frame=fixture;frame.resize(4*1024*1024);Put24BigEndian(&frame,1,frame.size()/256);
     for(int event=0;event<160;++event){Put32BigEndian(&frame,22,event);store.Insert("source",frame);QVERIFY(store.cachedBytes<=256LL*1024*1024);QVERIFY(store.events.size()<=100);}
     QCOMPARE(store.errors,quint64(0));QCOMPARE(store.cachedBytes,256LL*1024*1024);QCOMPARE(store.cachedFrames,64);QCOMPARE(store.bytes,640ULL*1024*1024);QCOMPARE(quint32(store.events.first().key),quint32(96));store.Reset();QVERIFY(store.events.isEmpty());QCOMPARE(store.cachedBytes,qint64(0));QCOMPARE(store.cachedFrames,0);}
 }
 void onlineReceiverHandlesSplitTcpFramesAndMalformedLength(){
    QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));auto store=std::make_shared<GetDAQ::OnlineStore>();auto *worker=new GetDAQ::OnlineReceiver(store);QThread thread;worker->moveToThread(&thread);connect(&thread,&QThread::finished,worker,&QObject::deleteLater);thread.start();
    std::unique_ptr<int,std::function<void(int *)>> cleanup(new int,[&](int *p){QMetaObject::invokeMethod(worker,"Stop",Qt::BlockingQueuedConnection);thread.quit();thread.wait();delete p;});
    const auto endpoint=QString("127.0.0.1:%1").arg(server.serverPort());QMetaObject::invokeMethod(worker,"Connect",Qt::QueuedConnection,Q_ARG(QStringList,QStringList{endpoint}));QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(),3000);auto *socket=server.nextPendingConnection();const auto frame=GetDAQTestInternal::MakeType1Frame();QByteArray header(8,0);qToLittleEndian<quint64>(frame.size(),reinterpret_cast<uchar *>(header.data()));socket->write(header.left(3));socket->flush();QTest::qWait(20);socket->write(header.mid(3)+frame.left(15));socket->flush();QTest::qWait(20);socket->write(frame.mid(15));socket->flush();QTRY_COMPARE_WITH_TIMEOUT(store->Snapshot().size(),1,3000);qToLittleEndian<quint64>(65ULL*1024*1024,reinterpret_cast<uchar *>(header.data()));socket->write(header);socket->flush();QTRY_VERIFY_WITH_TIMEOUT(store->Statistics().contains("Errors 1"),3000);
 }
 void yamlAndRetiredProfileGuard(){QTemporaryDir dir;auto c=Config(dir.path());QString error;QVERIFY(c.Save(dir.filePath("envoy.yml"),&error));QFile f(dir.filePath("envoy.yml"));QVERIFY(f.open(QIODevice::ReadOnly));const auto bytes=f.readAll();QVERIFY(!bytes.contains("native_ecc"));QVERIFY(!bytes.contains("simulation"));RunConfig loaded;QVERIFY(loaded.Load(f.fileName(),&error));QCOMPARE(loaded.nodes.size(),2);QCOMPARE(loaded.configDir,c.configDir);QCOMPARE(loaded.outputDir,c.outputDir);c.nativeEcc=true;QVERIFY(!c.Validate(&error));QVERIFY(error.contains("removed"));c.nativeEcc=false;c.simulation=true;QVERIFY(!c.Validate(&error));}
 void remoteReceiverProtocol(){NodeConfig node;node.id=8;node.host="192.0.2.16";node.routerHost="198.51.100.16";node.routerPort=46005;node.exporterPort=0;auto body=Protocol::EccRequest(node,"pedestals","Configure");QVERIFY(body.contains("DataRouter"));QVERIFY(body.contains("198.51.100.16"));QVERIFY(body.contains("46005"));QVERIFY(!body.contains("DataExporter"));QVERIFY(body.contains("CoBo[8]"));}
 void fullSoapLifecycle(){QTemporaryDir dir;auto c=Config(dir.path());RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),finished(&control,&RunControl::RunFinishedSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,1),3000);control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,2),3000);control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,3),3000);control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,4),3000);control.StartRunSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);control.PauseSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,6),3000);control.ResumeSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);control.StopRunSlot();QTRY_VERIFY_WITH_TIMEOUT(finished.size()==1,7000);QVERIFY(At(states,4));QJsonObject journal;QString error;QVERIFY(RunRecorder::ReadJournal(c,&journal,&error));QCOMPARE(journal["phase"].toString(),QString("Complete"));control.DisconnectSlot();}
 void missingBackupIsWarning(){QTemporaryDir dir;auto c=Config(dir.path());RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),finished(&control,&RunControl::RunFinishedSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,1),3000);for(int s=2;s<=4;++s){control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,s),3000);}control.StartRunSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);QVERIFY(QFile::remove(c.configDir+"/describe-"+c.nodes[0].ConfigId("describe",c.experiment)+".xcfg"));control.StopRunSlot();QTRY_VERIFY_WITH_TIMEOUT(finished.size()==1,7000);QVERIFY(At(states,4));QVERIFY(QFileInfo::exists(c.RunDirectory()+"/configuration-backup-warning.json"));control.DisconnectSlot();}
 void sameRunFileModesPreserveNumberAndScope(){
    QTemporaryDir dir;auto c=Config(dir.path());QString error;QVERIFY(RunRecorder::Reserve(c,&error));QFile marker(c.RunDirectory()+"/old.graw");QVERIFY(marker.open(QIODevice::WriteOnly));marker.write("old");marker.close();
    c.existingRunPolicy="append";c.description="new acquisition";QVERIFY2(RunRecorder::Reserve(c,true,&error),qPrintable(error));QVERIFY(QFile::exists(c.RunDirectory()+"/old.graw"));QCOMPARE(c.runNumber,0);QCOMPARE(QDir(c.RunDirectory()).entryList({"records-*"},QDir::Dirs).size(),1);RunConfig saved;QVERIFY(saved.Load(c.RunDirectory()+"/run.yml",&error));QCOMPARE(saved.runNumber,0);QCOMPARE(saved.existingRunPolicy,QString("append"));
    QFile outside(dir.filePath("outside-keep"));QVERIFY(outside.open(QIODevice::WriteOnly));outside.write("outside");outside.close();QVERIFY(QFile::link(outside.fileName(),c.RunDirectory()+"/outside-link"));
    const auto other=c.RunDirectory()+"-other";QVERIFY(QDir().mkpath(other));QFile otherFile(other+"/keep");QVERIFY(otherFile.open(QIODevice::WriteOnly));otherFile.write("other");otherFile.close();
    c.existingRunPolicy="overwrite";QVERIFY2(RunRecorder::Reserve(c,true,&error),qPrintable(error));QVERIFY(!QFile::exists(c.RunDirectory()+"/old.graw"));QVERIFY(QDir(c.RunDirectory()).entryList({"records-*"},QDir::Dirs).isEmpty());QVERIFY(QFile::exists(outside.fileName()));QVERIFY(QFile::exists(otherFile.fileName()));QJsonObject journal;QVERIFY(RunRecorder::ReadJournal(c,&journal,&error));QVERIFY(journal["previous_run_removed"].toBool());QVERIFY(!journal.contains("replacement_backup"));
 }
 void sameRunDialogHasThreeChoicesWithoutRenumbering(){
    QTemporaryDir dir;auto c=Config(dir.path());QString error;QVERIFY(RunRecorder::Reserve(c,&error));
    for(const auto &choice:{QString("CancelExistingRun"),QString("KeepAndAddRunFiles"),QString("OverwriteExistingRun")}){
        MainWindow window;window.SetConfig(c);QSignalSpy starts(&window,&MainWindow::StartSignal),metadata(&window,&MainWindow::UpdateRunMetadataSignal);bool handled=false,valid=false;
        QTimer timer;timer.setInterval(10);connect(&timer,&QTimer::timeout,&window,[&]{auto *prompt=window.findChild<QMessageBox *>("ExistingRunFileChoice");if(!prompt || !prompt->isVisible())return;auto *cancel=prompt->findChild<QPushButton *>("CancelExistingRun");valid=prompt->findChild<QPushButton *>("OverwriteExistingRun") && prompt->findChild<QPushButton *>("KeepAndAddRunFiles") && prompt->defaultButton()==cancel;handled=true;prompt->findChild<QPushButton *>(choice)->click();timer.stop();});timer.start();QVERIFY(QMetaObject::invokeMethod(&window,"on_StartRun_clicked",Qt::DirectConnection));QVERIFY(handled && valid);
        if(choice=="CancelExistingRun"){QCOMPARE(starts.size(),0);QCOMPARE(metadata.size(),0);}else{QCOMPARE(starts.size(),1);QVERIFY(starts[0][0].toBool());const auto submitted=qvariant_cast<RunConfig>(metadata[0][0]);QCOMPARE(submitted.runNumber,c.runNumber);QCOMPARE(submitted.RunDirectory(),c.RunDirectory());QCOMPARE(submitted.existingRunPolicy,choice=="OverwriteExistingRun"?QString("overwrite"):QString("append"));}
    }
 }
 void sameRunExplicitModesFinishThroughSentry(){
    for(const auto &policy:{QString("append"),QString("overwrite")}){QTemporaryDir dir;auto c=Config(dir.path());QString error;QVERIFY(RunRecorder::Reserve(c,&error));c.existingRunPolicy=policy;RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),finished(&control,&RunControl::RunFinishedSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,1),3000);for(int state=2;state<=4;++state){control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,state),3000);}control.StartRunSlot(true,true);QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);control.StopRunSlot();QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,7000);QJsonObject journal;QVERIFY(RunRecorder::ReadJournal(c,&journal,&error));QCOMPARE(journal["phase"].toString(),QString("Complete"));QCOMPARE(journal["existing_run_policy"].toString(),policy);control.DisconnectSlot();}
 }
 void oldSentryUsesItsOriginalCatalogInterface(){
    QTemporaryDir dir;auto c=Config(dir.path());QString error;QVERIFY(RunRecorder::Reserve(c,&error));c.existingRunPolicy="append";
    GetDAQTestFixtures::MockNode *receiver=nullptr;for(auto *mock:QCoreApplication::instance()->findChildren<GetDAQTestFixtures::MockNode *>())if(mock->serverPort()==c.nodes[0].sentryPort){receiver=mock;mock->setProperty("legacy-file-policy",true);}QVERIFY(receiver);
    RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),finished(&control,&RunControl::RunFinishedSignal),logs(&control,&RunControl::LogSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,1),3000);for(int state=2;state<=4;++state){control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,state),3000);}control.StartRunSlot(true,true);QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);control.StopRunSlot();QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,7000);QCOMPARE(receiver->catalogCount,1);QCOMPARE(receiver->replacementCatalogCount,1);QCOMPARE(receiver->replacementFinalizeCount,1);QVERIFY(At(states,4));control.DisconnectSlot();
 }
 void confirmedFileChoiceTargetsRequestedRunAfterUnfinishedBookkeeping(){
    QTemporaryDir dir;auto first=Config(dir.path()),chosen=first;chosen.runNumber=1;QString error;QVERIFY(RunRecorder::Reserve(chosen,&error));QFile oldChosen(chosen.RunDirectory()+"/old-selected");QVERIFY(oldChosen.open(QIODevice::WriteOnly));oldChosen.write("selected");oldChosen.close();
    RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),warnings(&control,&RunControl::OperationWarningSignal),finished(&control,&RunControl::RunFinishedSignal);control.ConnectSlot(first);QTRY_VERIFY_WITH_TIMEOUT(At(states,1),3000);for(int state=2;state<=4;++state){control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,state),3000);}control.StartRunSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);QFile oldFirst(first.RunDirectory()+"/keep-first");QVERIFY(oldFirst.open(QIODevice::WriteOnly));oldFirst.write("first");oldFirst.close();
    for(auto *mock:QCoreApplication::instance()->findChildren<GetDAQTestFixtures::MockNode *>())for(const auto &node:first.nodes)if(mock->serverPort()==node.eccPort)mock->state=4;QTRY_VERIFY_WITH_TIMEOUT(At(states,4),3000);
    chosen.existingRunPolicy="overwrite";control.StartRunSlot(true,true,chosen);QTRY_VERIFY_WITH_TIMEOUT(!warnings.isEmpty(),3000);control.AcceptOperationWarningSlot(warnings.last()[0].toString());QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);QVERIFY(QFile::exists(oldFirst.fileName()));QVERIFY(!QFile::exists(oldChosen.fileName()));RunConfig actual;QVERIFY(actual.Load(chosen.RunDirectory()+"/run.yml",&error));QCOMPARE(actual.runNumber,1);control.StopRunSlot();QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,7000);control.DisconnectSlot();
 }
 void legacyCleanupWarningDoesNotLockStoppedRun(){
    QTemporaryDir dir;auto c=Config(dir.path());QString error;QVERIFY(RunRecorder::Reserve(c,&error));c.existingRunPolicy="append";
    for(auto *mock:QCoreApplication::instance()->findChildren<GetDAQTestFixtures::MockNode *>())if(mock->serverPort()==c.nodes[0].sentryPort){mock->setProperty("legacy-file-policy",true);mock->setProperty("legacy-no-cleanup",true);}
    RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),finished(&control,&RunControl::RunFinishedSignal),flags(&control,&RunControl::ControlSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,1),3000);for(int state=2;state<=4;++state){control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,state),3000);}control.StartRunSlot(true,true);QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);control.StopRunSlot();QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,7000);QVERIFY(At(states,4));QVERIFY(!flags.last()[2].toBool());QVERIFY(!flags.last()[3].toBool());QJsonObject journal;QVERIFY(RunRecorder::ReadJournal(c,&journal,&error));QCOMPARE(journal["phase"].toString(),QString("CompleteWithWarnings"));QVERIFY(journal.contains("legacy_cleanup_warning"));control.DisconnectSlot();
 }
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
 void originalWindowAndNodeOptions(){MainWindow window;QVERIFY(!window.findChild<QAction *>("OriginalGetController"));QVERIFY(window.findChild<QAction *>("DirectCoBoInformation"));auto *table=window.findChild<QTableWidget *>("StatusTable");QVERIFY(table);QCOMPARE(table->columnCount(),13);for(auto *action:window.findChildren<QAction *>())QVERIFY2(!action->objectName().startsWith("Native"),qPrintable(action->objectName()));auto c=RunConfig::Default();IPWindow nodes(c);auto *settings=nodes.findChild<QTableWidget *>("NodeSettingsTable");QVERIFY(settings);QCOMPARE(settings->columnCount(),16);QVERIFY(!nodes.findChild<QCheckBox *>("UseNativeWholeSystemControl"));for(const auto &name:{"LoadConfig","SaveConfig","Nodes"}){auto *b=window.findChild<QPushButton *>(name);QVERIFY(b);QCOMPARE(b->width(),172);}}
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
 void unsynchronizedRuntimeBackupIsExplicitlyUnverified(){
    QTemporaryDir dir;auto config=Config(dir.path());config.configurationBackupWarning="Runtime configuration has not been verified against source";QString error;QVERIFY(RunRecorder::Reserve(config,&error));QCOMPARE(RunRecorder::Finish(config,1.0),QString());QVERIFY(QFileInfo::exists(config.RunDirectory()+"/configuration-backup-warning.json"));QVERIFY(!QFileInfo::exists(config.RunDirectory()+"/backup.complete"));
 }
 void configurationSourceDeduplicatesWholeFolderAcrossGroups(){
    QTemporaryDir dir;for(const auto &name:{"describe-pedestals.xcfg","configure-pedestals.xcfg","resource.dat"}){QFile file(dir.filePath(name));QVERIFY(file.open(QIODevice::WriteOnly));file.write(QByteArray(name).endsWith(".dat")?QByteArray("resource"):QByteArray("<config/>"));}
    QMap<QString,QByteArray> files;const auto first=Deployment::OriginalRepository(dir.path(),"one",&files);const int count=files.size();const auto second=Deployment::OriginalRepository(dir.path(),"two",&files);QCOMPARE(files.size(),count);QCOMPARE(first,second);
 }
 void projectConfigurationSyncPreservesDataAndDefersActiveChanges(){
    QTemporaryDir dir;const auto project=dir.filePath("project");QVERIFY(QDir().mkpath(project));QVERIFY(QFile::setPermissions(project,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner|QFile::ReadGroup|QFile::ExeGroup));
    QFile raw(project+"/keep.graw");QVERIFY(raw.open(QIODevice::WriteOnly));raw.write("raw-data");raw.close();
    QJsonObject group;QMap<QString,QByteArray> files;auto configure=[&](const QByteArray &value){files["settings"]=value;files["resource"]=QByteArray();group["repository_files"]=QJsonArray{QJsonObject{{"file","settings"},{"path","config.xml"},{"sha256",Deployment::Sha(value)}},QJsonObject{{"file","resource"},{"path","subdir/empty.dat"},{"sha256",Deployment::Sha(QByteArray())}}};};
    configure("<config old='1'/>");auto result=Deployment::SyncWorkspace(project,group,files,Deployment::Sha("one"),true,true);QCOMPARE(result["state"].toString(),QString("applied"));
    configure("<config new='2'/>");result=Deployment::SyncWorkspace(project,group,files,Deployment::Sha("two"),true,false);QCOMPARE(result["state"].toString(),QString("pending"));QCOMPARE(Deployment::Read(project+"/config.xml"),QByteArray("<config old='1'/>"));
    result=Deployment::SyncWorkspace(project,group,files,Deployment::Sha("two"),true,true);QCOMPARE(result["state"].toString(),QString("applied"));QCOMPARE(Deployment::Read(project+"/config.xml"),files["settings"]);QCOMPARE(Deployment::Read(project+"/keep.graw"),QByteArray("raw-data"));QVERIFY(QFileInfo::exists(project+"/subdir/empty.dat"));QVERIFY(!QFileInfo::exists(project+"/.getdaq/pending-configuration.json"));
    configure("<disabled/>");Deployment::SyncWorkspace(project,group,files,Deployment::Sha("pending-disabled"),true,false);QVERIFY(QFileInfo::exists(project+"/.getdaq/pending-configuration.json"));result=Deployment::SyncWorkspace(project,group,files,Deployment::Sha("three"),false,true);QCOMPARE(result["state"].toString(),QString("disabled"));QVERIFY(!QFileInfo::exists(project+"/.getdaq/pending-configuration.json"));QCOMPARE(Deployment::Read(project+"/config.xml"),QByteArray("<config new='2'/>"));
 }
 void sharedSentryArchivesOnceForTwoEccs(){
    QTemporaryDir dir;auto c=RunConfig::Default();c.nodes={c.nodes[0],c.nodes[1]};c.outputDir=dir.path();c.configDir=dir.filePath("configs");c.pollMs=60;c.timeoutMs=300;c.catalogTimeoutMs=600;c.stepTimeoutMs=2000;c.settleMs=20;AttachMockNodes(c);MakeFiles(c);c.nodes[1].sentryHost=c.nodes[0].host;c.nodes[1].sentryPort=c.nodes[0].sentryPort;
    RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),done(&control,&RunControl::RunFinishedSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(status,1),3000);for(int state=2;state<=4;++state){control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(status,state),3000);}control.StartRunSlot();QTRY_VERIFY_WITH_TIMEOUT(At(status,5),3000);QTest::qWait(100);control.StopRunSlot();QTRY_VERIFY_WITH_TIMEOUT(done.size()==1,7000);
    int catalogs=0;for(auto *mock:QCoreApplication::instance()->findChildren<GetDAQTestFixtures::MockNode *>())if(mock->serverPort()==c.nodes[0].sentryPort || mock->serverPort()==c.nodes[1].eccPort)catalogs+=mock->catalogCount;QCOMPARE(catalogs,1);control.DisconnectSlot();
 }
 void sentryReportsPerBoardAndProjectTotals(){
    QTemporaryDir dir;auto write=[&](const QString &name,int size){QFile file(dir.filePath(name));QVERIFY(file.open(QIODevice::WriteOnly));file.write(QByteArray(size,'x'));};write("CoBo0_AsAd0_time.graw",100);write("CoBo1_AsAd1_time.graw",200);write("unassigned.graw",50);
    SentryConfig config;config.dataDir=dir.path();config.checkProcess=false;const auto result=SentryFiles::Status(config);QCOMPARE(result.code,200);QCOMPARE(result.json["data_written_bytes"].toString(),QString("350"));const auto sources=result.json["per_source"].toObject();QCOMPARE(sources["0"].toObject()["data_written_bytes"].toString(),QString("100"));QCOMPARE(sources["1"].toObject()["data_path_files"].toInt(),1);
 }
 void combinedLogSnapshotOrdersChunksAndKeepsClearHistory(){
    LogTimeline timeline;timeline.Append("EVENT LOG","first event",1000);timeline.AppendChunk("ecc","ECC LOG | host1","partial",false,1000);timeline.AppendChunk("data","DATA LOG | host2","connection established\n",false,1000);timeline.AppendChunk("ecc","ECC LOG | host1"," line\nnext",false,900);
    const auto before=timeline.ExportText();QVERIFY(before.indexOf("first event")<before.indexOf("connection established"));QVERIFY(before.indexOf("connection established")<before.indexOf("partial line"));QVERIFY(before.contains("partial line at snapshot"));QVERIFY(before.contains("local observation time"));
    timeline.Append("EVENT LOG","after snapshot",2000);QVERIFY(!before.contains("after snapshot"));QVERIFY(timeline.ExportText().contains("after snapshot"));
    auto *events=new QPlainTextEdit;events->setPlainText("persistent event history");ServiceLogPanel panel(events);auto *clear=panel.findChild<QToolButton *>("ClearLog");QVERIFY(clear);clear->click();QVERIFY(panel.VisibleText().isEmpty());QVERIFY(panel.CombinedText().contains("persistent event history"));
    LogTimeline bounded(24);bounded.Append("EVENT LOG","12345678",1);bounded.Append("ECC LOG","abcdefgh",2);QVERIFY(!bounded.ExportText().contains("12345678"));QVERIFY(bounded.ExportText().contains("discarded older records: 1"));
 }
 void logCollectorReadsBackendSourcesWhileEventTabIsSelected(){
    QTemporaryDir dir;const auto path=dir.filePath("project.json");QJsonObject host{{"id","host1"},{"name","Computer"}};
    QJsonObject group{{"id","group1"},{"name","Boards"},{"ecc",QJsonObject{{"node_id","host1"}}},{"receivers",QJsonArray{QJsonObject{{"id","data"},{"node_id","host1"}}}}};Deployment::Write(path,Deployment::Json({{"nodes",QJsonArray{host}},{"groups",QJsonArray{group}}}));
    auto *events=new QPlainTextEdit;events->setPlainText("event page stays selected");ServiceLogPanel panel(events);panel.SetProject(path);QVERIFY(QMetaObject::invokeMethod(&panel,"Poll",Qt::DirectConnection));QTRY_VERIFY_WITH_TIMEOUT(panel.CombinedText().contains("ECC LOG"),3000);QTRY_VERIFY_WITH_TIMEOUT(panel.CombinedText().contains("DATA LOG"),3000);QCOMPARE(panel.VisibleText(),QString("event page stays selected"));
 }
 void serviceLogHeaderFitsTextAndRefreshesNewProjectInstances(){
    auto *events=new QPlainTextEdit;ServiceLogPanel panel(events);auto *button=panel.findChild<QToolButton *>("ShowEventLog");QVERIFY(button);QVERIFY(button->height()>=button->fontMetrics().height()+6);QVERIFY(button->minimumWidth()>=button->fontMetrics().horizontalAdvance(button->text())+24);
    QTemporaryDir dir;const auto path=dir.filePath("project.json");panel.SetProject(path);panel.findChild<QToolButton *>("ShowEccLog")->click();auto *instances=panel.findChild<QComboBox *>("ServiceLogInstance");QCOMPARE(instances->count(),0);
    QJsonObject host{{"id","host1"},{"name","Computer"}};QJsonObject group{{"id","group1"},{"name","Boards"},{"ecc",QJsonObject{{"node_id","host1"}}}};
    Deployment::Write(path,Deployment::Json({{"nodes",QJsonArray{host}},{"groups",QJsonArray{group}}}));QVERIFY(QMetaObject::invokeMethod(&panel,"Poll",Qt::DirectConnection));QCOMPARE(instances->count(),1);QCOMPARE(instances->currentData().toJsonObject()["instance_id"].toString(),QString("group1-ecc"));
 }
 void receiverWaitingSpinnerIsCompactWithoutHidingErrors(){
    const QString waiting=" Waiting for client connection .       \r Waiting for client connection ..        \r";
    const QString error="ERROR: disconnected\n";
    const auto shown=ServiceLogPanel::PresentLog(waiting+error+waiting);
    QCOMPARE(shown.count("Waiting for incoming data connection"),2);QVERIFY(shown.contains(error));QVERIFY(!shown.contains("connection .."));
    QCOMPARE(ServiceLogPanel::PresentLog("connected\nframes received\n"),QString("connected\nframes received\n"));
    QCOMPARE(ServiceLogPanel::PresentLog(" Waiting for client connec"),QString(" Waiting for client connec"));
    QCOMPARE(ServiceLogPanel::PresentLog(QString(" Waiting for client connec")+"tion .\r"+waiting).count("Waiting for incoming data connection"),1);
 }
 void serviceLogTabsPreserveEventTextAndHandleRotation(){
    QPlainTextEdit *events=new QPlainTextEdit;events->appendPlainText("event history");ServiceLogPanel panel(events);QCOMPARE(panel.VisibleText(),QString("event history"));panel.findChild<QToolButton *>("ShowEccLog")->click();QVERIFY(!panel.VisibleText().contains("event history"));panel.findChild<QToolButton *>("ShowEventLog")->click();QCOMPARE(panel.VisibleText(),QString("event history"));
    QTemporaryDir dir;const auto path=dir.filePath("ecc.log");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("first\n");file.close();auto log=Deployment::ReadServiceLog(path,0,QString());QCOMPARE(log["text"].toString(),QString("first\n"));QCOMPARE(log["snapshot_size"].toInt(),6);QVERIFY(log["offset"].toDouble()<=log["snapshot_size"].toDouble());QVERIFY(file.open(QIODevice::Append));file.write("second\n");file.close();auto next=Deployment::ReadServiceLog(path,qint64(log["offset"].toDouble()),log["generation"].toString());QCOMPARE(next["text"].toString(),QString("second\n"));QVERIFY(QFile::rename(path,path+".old"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("rotated\n");file.close();next=Deployment::ReadServiceLog(path,6,log["generation"].toString());QVERIFY(next["reset"].toBool());QCOMPARE(next["text"].toString(),QString("rotated\n"));
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
