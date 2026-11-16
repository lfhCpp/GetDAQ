#include "hardwaredeployment.h"
#include "protocol.h"
#include "eccclient.h"
#include "runrecorder_p.h"
#include "runcontrol.h"
#include "unifiedeccwindow.h"
#include "existingeccconnection.h"
#include "localserviceprocesses.h"
#include "localdeploymentservices.h"
#include "applicationinstance.h"
#include "earlywarningrecovery.h"
#include "hardwaresetupwindow.h"
#include "localserviceswindow.h"
#include "localbackendregistry.h"
#include "backendpresets.h"
#include "distributeddeploymentwindow.h"
#include "hostrolewindow.h"
#include "deploymentcontroller.h"
#include <QSettings>
#include <QSet>
#include <QtConcurrent/QtConcurrentRun>
#include <QTcpSocket>
#include <QStandardPaths>
#include <QLabel>
#include <QComboBox>
#include <QTimer>
#include "ipwindow.h"
#include "nativestatusprojection.h"
#include "nativereceiverfinalizationpolicy.h"
#include <QSpinBox>
#include <QTcpServer>
#include <csignal>
#include <QtTest>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <stdexcept>

class HardwareSetupTests:public QObject {
    Q_OBJECT
    QTemporaryDir fTemp;
    QString fConfigs;
    QString Root() const {return QStringLiteral(GETDAQ_SETUP_SOURCE_ROOT);}
    HardwareDeploymentSetup Setup(const QString &name) {
        HardwareDeploymentSetup s;s.experimentFile=QDir(fConfigs).filePath("configure-ExternalTrigger-2CoBo_1Mutant.xcfg");
        s.profileFile=Root()+"/config/experiment/system-profile.external-trigger.example.json";
        s.programDirectory=qEnvironmentVariable("GETDAQ_SETUP_BIN");s.deploymentDirectory=fTemp.filePath(name);s.dataDirectory=s.deploymentDirectory+"/data";s.callbackAddress="127.0.0.1";
        s.boards=HardwareSetupBoards(CompileHardwareSetupExperiment(s.experimentFile,s.profileFile));
        for(int i=0;i<s.boards.size();++i){auto &b=s.boards[i];b.address=QStringLiteral("192.0.2.%1").arg(10+i);b.controlPort=46000+i;b.senderPort=46100+i;b.nodeObject=b.mutant?"MutantNode":"CoBoNode";b.nodeName=b.id;b.receiverAddress="127.0.0.1";b.descriptionFile=QDir(fConfigs).filePath(b.mutant?"hardwareDescription_Mutant.xcfg":"hardwareDescription_fullCoBoStandAlone.xcfg");}
        return s;
    }
    HardwareDeploymentFiles CheckedSetup(const QString &name,int offset,int boardOffset=-1,int receiverOffset=-1) {
        auto s=Setup(name);if(boardOffset<0)boardOffset=offset;if(receiverOffset<0)receiverOffset=offset;
        QTcpServer ecc,node;if(!ecc.listen(QHostAddress::LocalHost,0) || !node.listen(QHostAddress::LocalHost,0))throw std::runtime_error("Cannot reserve test control ports");
        s.eccPort=ecc.serverPort();s.supervisorPort=node.serverPort();ecc.close();node.close();s.callbackPort=30000+offset*100;
        for(int i=0;i<s.boards.size();++i){s.boards[i].address=QString("192.0.2.%1").arg(50+boardOffset*10+i);s.boards[i].dataPort=31000+receiverOffset*20+2*i;s.boards[i].statusPort=s.boards[i].dataPort+1;}
        auto files=WriteHardwareDeployment(s,RunConfig::Default());QProcess check;
        check.start(s.programDirectory+"/getdaq-supervisor",{"--config",files.supervisorFile,"--plan-config"});
        if(!check.waitForFinished(15000) || check.exitCode())throw std::runtime_error("Supervisor plan check failed");
        auto remote=files.ecc["remote_receivers"].toObject();auto endpoints=remote["endpoints"].toArray();auto endpoint=endpoints[0].toObject();endpoint["plan_sha256"]=HardwareSetupSha256(check.readAllStandardOutput());endpoints[0]=endpoint;remote["endpoints"]=endpoints;files.ecc["remote_receivers"]=remote;
        WriteHardwareSetupFile(files.eccFile,QJsonDocument(files.ecc).toJson());
        check.start(s.programDirectory+"/getdaq-ecc-server",{"--config",files.eccFile,"--plan-config"});
        if(!check.waitForFinished(15000) || check.exitCode())throw std::runtime_error("ECC plan check failed");
        const auto identity=QJsonDocument::fromJson(check.readAllStandardOutput()).object();files.console.nativeClient.snapshotSha256=identity["snapshot_sha256"].toString();files.console.nativeClient.inputBindingSha256=identity["input_binding_sha256"].toString();
        QString error;if(!files.console.Save(files.console.filePath,&error))throw std::runtime_error(error.toStdString());return files;
    }
private slots:
    void legacyCatalogFailureClosesWithWarningAndAllowsNextRun() {
        QTemporaryDir directory;QVERIFY(directory.isValid());QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost,0));int state=4,catalogCalls=0,startCalls=0,cleanupCalls=0,incomingFiles=0;bool failCatalog=true;
        connect(&server,&QTcpServer::newConnection,this,[&]{while(server.hasPendingConnections()) {
            auto *socket=server.nextPendingConnection();socket->setParent(&server);auto bytes=std::make_shared<QByteArray>();
            connect(socket,&QTcpSocket::readyRead,socket,[&,socket,bytes]{
                *bytes+=socket->readAll();const int split=bytes->indexOf("\r\n\r\n");if(split<0 || socket->property("replied").toBool())return;
                int length=0;for(const auto &line:bytes->left(split).split('\n'))if(line.toLower().startsWith("content-length:"))length=line.mid(15).trimmed().toInt();
                if(bytes->size()<split+4+length)return;
                socket->setProperty("replied",true);int code=200;QByteArray body;
                if(bytes->startsWith("GET /status "))body=QJsonDocument(QJsonObject{{"disk","disk"},{"process","dataRouter"},{"data_path","/incoming"},{"data_written_gb",0.0},{"data_path_files",incomingFiles},{"disk_avail_gb",10.0},{"disk_total_gb",20.0}}).toJson();
                else if(bytes->startsWith("POST /catalog ")){++catalogCalls;if(failCatalog){code=500;body="{\"error\":\"archive destination unavailable\"}";}else {incomingFiles=0;body=QJsonDocument(QJsonObject{{"disk","disk"},{"process","dataRouter"},{"data_path","/incoming"},{"data_written_gb",0.0},{"data_path_files",0},{"disk_avail_gb",10.0},{"disk_total_gb",20.0}}).toJson();}}
                else if(bytes->startsWith("POST /catalog/finalize ")){++cleanupCalls;body="{\"replacement_cleanup_complete\":true}";}
                else {if(bytes->contains("<ecc:Start>")){state=5;++startCalls;}if(bytes->contains("<ecc:Stop>"))state=4;
                    body=QString("<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body><ResponseState><ErrorCode>0</ErrorCode><State>%1</State><Transition>0</Transition></ResponseState></s:Body></s:Envelope>").arg(state).toUtf8();}
                socket->write("HTTP/1.1 "+QByteArray::number(code)+" Result\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();
            });connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        }});
        auto config=RunConfig::Default();config.simulation=false;config.pollMs=50;config.timeoutMs=500;config.stepTimeoutMs=2000;config.catalogTimeoutMs=1000;config.settleMs=0;
        config.outputDir=directory.filePath("runs");config.configDir=directory.filePath("config");QVERIFY(QDir().mkpath(config.configDir));NodeConfig node;node.host="127.0.0.1";node.eccPort=node.sentryPort=server.serverPort();config.nodes={node};
        RunControl control;QSignalSpy phases(&control,&RunControl::ControlSignal),observations(&control,&RunControl::StatusSignal),finished(&control,&RunControl::RunFinishedSignal);
        control.ConnectSlot(config);QTRY_VERIFY_WITH_TIMEOUT(!observations.isEmpty() && qvariant_cast<QVector<NodeStatus>>(observations.last()[0])[0].sentryOnline,2000);
        control.StartRunSlot(false);QTRY_VERIFY_WITH_TIMEOUT(phases.last()[0].toString()=="Running",2000);control.StopRunSlot();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,3000);QCOMPARE(finished[0][0].toInt(),1);QCOMPARE(phases.last()[0].toString(),QString("Stopped (archive warning)"));QVERIFY(!phases.last()[3].toBool());
        QString error;QJsonObject journal;QVERIFY(RunRecorder::ReadJournal(config,&journal,&error));QCOMPARE(journal["phase"].toString(),QString("CompleteWithWarnings"));QCOMPARE(journal["catalog"].toObject()["0"].toString(),QString("unknown"));
        QVERIFY(QFile::exists(QDir(config.RunDirectory()).filePath("catalog-warning.json")));QVERIFY(!RunRecorder::Reserve(config,false,&error));
        QTRY_VERIFY_WITH_TIMEOUT(qvariant_cast<QVector<NodeStatus>>(observations.last()[0])[0].sentryOnline,2000);
        control.StartRunSlot(false);QTRY_VERIFY_WITH_TIMEOUT(phases.last()[0].toString()=="Running",2000);QCOMPARE(startCalls,2);QCOMPARE(catalogCalls,1);
        control.StopRunSlot();QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,3000);QCOMPARE(catalogCalls,2);
        incomingFiles=1;QTRY_COMPARE_WITH_TIMEOUT(qvariant_cast<QVector<NodeStatus>>(observations.last()[0])[0].files,1,2000);
        control.StartRunSlot(false);QTest::qWait(80);QCOMPARE(startCalls,2);
        control.UpdateRunMetadataSlot(config);QTRY_VERIFY_WITH_TIMEOUT(qvariant_cast<QVector<NodeStatus>>(observations.last()[0])[0].sentryOnline,2000);
        control.StartRunSlot(true);QTRY_VERIFY_WITH_TIMEOUT(phases.last()[0].toString()=="Running",2000);QCOMPARE(startCalls,3);
        QVERIFY(RunRecorder::ReadJournal(config,&journal,&error));QCOMPARE(journal["replacement_backup_policy"].toString(),QString("retain"));
        QVERIFY(journal["same_run_existing_incoming_acknowledged"].toBool());QCOMPARE(journal["existing_incoming_data"].toArray()[0].toObject()["files"].toInt(),1);
        const auto retained=QDir(QFileInfo(config.RunDirectory()).absolutePath()).filePath(journal["retained_previous_run"].toString());QVERIFY(QFile::exists(retained+"/catalog-warning.json"));
        failCatalog=false;control.StopRunSlot();QTRY_COMPARE_WITH_TIMEOUT(finished.size(),3,3000);QCOMPARE(cleanupCalls,0);QVERIFY(QFile::exists(retained+"/catalog-warning.json"));
        QVERIFY(RunRecorder::ReadJournal(config,&journal,&error));QVERIFY(journal["replacement_cleanup_skipped"].toBool());control.DisconnectSlot();
    }
    void confirmedLegacyReplacementRetainsPartialRecords() {
        QTemporaryDir directory;QVERIFY(directory.isValid());auto config=RunConfig::Default();config.outputDir=directory.path();
        QVERIFY(QDir().mkpath(config.RunDirectory()));WriteHardwareSetupFile(QDir(config.RunDirectory()).filePath("old-record.txt"),"preserve this partial record");
        QString error;QVERIFY(!RunRecorder::Reserve(config,false,&error));QVERIFY(QFile::exists(QDir(config.RunDirectory()).filePath("old-record.txt")));
        QVERIFY2(RunRecorder::Reserve(config,true,&error),qPrintable(error));QJsonObject journal;QVERIFY(RunRecorder::ReadJournal(config,&journal,&error));
        QCOMPARE(journal["replacement_backup_policy"].toString(),QString("retain"));const auto old=QDir(QFileInfo(config.RunDirectory()).absolutePath()).filePath(journal["retained_previous_run"].toString());
        QVERIFY(QFile::exists(old+"/old-record.txt"));journal["phase"]="Complete";QVERIFY(RunRecorder::WriteJournal(config,journal,&error));QVERIFY(RunRecorder::FinalizeReplacement(config,&error));QVERIFY(QFile::exists(old+"/old-record.txt"));
    }
    void sentryRateUsesByteCountersAndResetsOnCounterDecrease() {
        auto config=RunConfig::Default();config.simulation=false;config.pollMs=3600000;
        NodeConfig node;node.host="127.0.0.1";node.eccPort=1;node.sentryPort=1;config.nodes={node};
        RunControl control;QSignalSpy observations(&control,&RunControl::StatusSignal);control.ConnectSlot(config);
        NodeStatus sample;sample.disk="disk";sample.dataPath="/data";sample.rawFileBytesKnown=true;sample.rawFileBytes=100;sample.writtenGB=0;
        auto reply=[&]{return QMetaObject::invokeMethod(&control,"SentryReplySlot",Qt::DirectConnection,Q_ARG(int,0),Q_ARG(bool,false),Q_ARG(NodeStatus,sample),Q_ARG(QString,QString()));};
        QVERIFY(reply());QTest::qWait(20);sample.rawFileBytes=1100;QVERIFY(reply());
        auto current=qvariant_cast<QVector<NodeStatus>>(observations.last()[0])[0];QVERIFY(current.sentryRateKnown);QVERIFY(current.rateMB>0);
        QTest::qWait(20);sample.rawFileBytes=10;QVERIFY(reply());current=qvariant_cast<QVector<NodeStatus>>(observations.last()[0])[0];QVERIFY(!current.sentryRateKnown);QCOMPARE(current.rateMB,0.0);
        control.ShutdownSlot();
    }
    void sentryStatusContinuesDuringCatalog() {
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost,0));
        connect(&server,&QTcpServer::newConnection,this,[&]{while(server.hasPendingConnections()){
            auto *socket=server.nextPendingConnection();socket->setParent(&server);auto bytes=std::make_shared<QByteArray>();
            connect(socket,&QTcpSocket::readyRead,socket,[socket,bytes]{*bytes+=socket->readAll();if(!bytes->contains("\r\n\r\n") || socket->property("scheduled").toBool())return;socket->setProperty("scheduled",true);
                const bool catalog=bytes->startsWith("POST /catalog ");
                QTimer::singleShot(catalog?600:0,socket,[socket,catalog]{
                    const QJsonObject status{{"disk","disk"},{"process","dataRouter"},{"data_path","/data"},{"data_written_gb",0.0},{"data_written_bytes",catalog?"0":"1000"},{"data_path_files",catalog?0:1},{"disk_avail_gb",10.0},{"disk_total_gb",20.0}};
                    const auto body=QJsonDocument(status).toJson();socket->write("HTTP/1.1 200 OK\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();});});
            connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        }});
        auto config=RunConfig::Default();config.simulation=false;config.timeoutMs=200;config.catalogTimeoutMs=2000;NodeConfig node;node.host="127.0.0.1";node.sentryPort=server.serverPort();
        qRegisterMetaType<NodeStatus>();SentryClient client(node,config);QSignalSpy replies(&client,&SentryClient::ReplySignal);
        QVERIFY(client.Send(true));QVERIFY(!client.Send(true));QVERIFY(client.Send());QVERIFY(!client.Send());
        QTRY_COMPARE_WITH_TIMEOUT(replies.size(),1,400);QVERIFY(!replies[0][1].toBool());QVERIFY(replies[0][3].toString().isEmpty());
        QCOMPARE(qvariant_cast<NodeStatus>(replies[0][2]).rawFileBytes,quint64(1000));
        QTRY_COMPARE_WITH_TIMEOUT(replies.size(),2,1500);QVERIFY(replies[1][1].toBool());QVERIFY(replies[1][3].toString().isEmpty());
    }
    void stoppedPendingFinalizationCanDisconnectAndRecoverWithoutCommands() {
        QTemporaryDir directory;QVERIFY(directory.isValid());QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost,0));int state=4;bool respond=true;QStringList requests;
        connect(&server,&QTcpServer::newConnection,this,[&]{while(server.hasPendingConnections()){
            auto *socket=server.nextPendingConnection();socket->setParent(&server);auto bytes=std::make_shared<QByteArray>();
            connect(socket,&QTcpSocket::readyRead,socket,[&,socket,bytes]{*bytes+=socket->readAll();if(!bytes->contains("</SOAP-ENV:Envelope>"))return;
                requests.append(QString::fromUtf8(*bytes));bytes->clear();if(!respond)return;
                const auto body=QString("<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body><ResponseState><ErrorCode>0</ErrorCode><State>%1</State><Transition>0</Transition></ResponseState></s:Body></s:Envelope>").arg(state).toUtf8();
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();});connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        }});
        auto config=RunConfig::Default();config.simulation=false;config.pollMs=50;config.timeoutMs=100;config.stepTimeoutMs=1000;
        config.outputDir=directory.filePath("runs");config.configDir=directory.filePath("config");QVERIFY(QDir().mkpath(config.configDir));QVERIFY(QDir().mkpath(config.RunDirectory()));
        NodeConfig node;node.host="127.0.0.1";node.eccPort=server.serverPort();config.nodes={node};QString error;QVERIFY(config.Save(QDir(config.RunDirectory()).filePath("run.yml"),&error));
        QJsonObject journal{{"phase","Error"},{"error","Local finalization failed: Configuration backup: missing file"},{"catalog",QJsonObject{{"0","complete"}}}};
        QVERIFY(RunRecorder::WriteJournal(config,journal,&error));RunControl control;QSignalSpy phases(&control,&RunControl::ControlSignal);control.ConnectSlot(config);
        QTest::qWait(150);control.RecoverSlot();QTRY_VERIFY_WITH_TIMEOUT(phases.last()[0].toString()=="Stopped; finalization pending",1500);
        state=5;QTRY_VERIFY_WITH_TIMEOUT(phases.last()[0].toString()=="Finalization pending; inspect ECC state",1500);control.DisconnectSlot();QVERIFY(phases.last()[1].toBool());
        state=4;QTRY_VERIFY_WITH_TIMEOUT(phases.last()[0].toString()=="Stopped; finalization pending",1500);
        respond=false;QTRY_VERIFY_WITH_TIMEOUT(phases.last()[0].toString()=="Finalization pending; inspect ECC state",1500);control.DisconnectSlot();QVERIFY(phases.last()[1].toBool());
        respond=true;QTRY_VERIFY_WITH_TIMEOUT(phases.last()[0].toString()=="Stopped; finalization pending",1500);control.DisconnectSlot();QVERIFY(!phases.last()[1].toBool());QVERIFY(!phases.last()[3].toBool());
        QVERIFY(RunRecorder::ReadJournal(config,&journal,&error));QCOMPARE(journal["phase"].toString(),QString("Finalization pending (disconnected)"));QVERIFY(journal["local_finalization_failed"].toBool());QCOMPARE(journal["catalog"].toObject()["0"].toString(),QString("complete"));
        control.ConnectSlot(config);QTest::qWait(150);control.RecoverSlot();QTRY_VERIFY_WITH_TIMEOUT(phases.last()[0].toString()=="Stopped; finalization pending",1500);
        control.FinalizeSlot();QTRY_VERIFY_WITH_TIMEOUT(!phases.last()[3].toBool(),3000);
        QCOMPARE(phases.last()[0].toString(),QString("Complete (configuration backup warning)"));
        QVERIFY(RunRecorder::ReadJournal(config,&journal,&error));QCOMPARE(journal["phase"].toString(),QString("Complete"));QCOMPARE(journal["configuration_backup_status"].toString(),QString("missing"));
        QVERIFY(!QFile::exists(QDir(config.RunDirectory()).filePath("backup.complete")));QVERIFY(QFile::exists(QDir(config.RunDirectory()).filePath("run.csv")));control.DisconnectSlot();QVERIFY(!phases.last()[1].toBool());
        for(const auto &request:requests)QVERIFY(request.contains("ecc:GetState"));
        journal["phase"]="Error";journal["catalog"]=QJsonObject{{"0","unknown"}};journal.remove("local_finalization_failed");journal.remove("error");
        QVERIFY(RunRecorder::WriteJournal(config,journal,&error));control.ConnectSlot(config);QTest::qWait(150);control.RecoverSlot();
        QTRY_VERIFY_WITH_TIMEOUT(phases.last()[0].toString()=="Stopped; catalog inspection required",1500);
        control.FinalizeSlot();QTRY_VERIFY_WITH_TIMEOUT(!phases.last()[3].toBool(),3000);control.DisconnectSlot();QVERIFY(!phases.last()[1].toBool());
        QVERIFY(RunRecorder::ReadJournal(config,&journal,&error));QCOMPARE(journal["phase"].toString(),QString("CompleteWithWarnings"));QCOMPARE(journal["catalog"].toObject()["0"].toString(),QString("unknown"));
        for(const auto &request:requests)QVERIFY(request.contains("ecc:GetState"));
    }
    void legacyPrepareBackupUsesConfigureStorage() {
        QTemporaryDir directory;QVERIFY(directory.isValid());auto config=RunConfig::Default();config.simulation=false;
        config.configDir=directory.filePath("configs");config.outputDir=directory.filePath("runs");config.experiment="pedestals";
        NodeConfig node;node.id=0;node.host="127.0.0.1";node.prepareId="prepare-selection";node.configureId="configure-selection";config.nodes={node};
        QVERIFY(QDir().mkpath(config.configDir));QVERIFY(QDir().mkpath(config.RunDirectory()));
        for(const auto &name:{"describe-cobo0.xcfg","configure-prepare-selection.xcfg","configure-configure-selection.xcfg"})WriteHardwareSetupFile(QDir(config.configDir).filePath(name),"<Setup/>");
        QString error;QVERIFY2(RunRecorderInternal::SnapshotConfiguration(config,&error),qPrintable(error));
        QFile manifest(QDir(config.RunDirectory()).filePath("configuration/manifest.json"));QVERIFY(manifest.open(QIODevice::ReadOnly));
        const auto roots=QJsonDocument::fromJson(manifest.readAll()).object()["roots"].toArray();QCOMPARE(roots.size(),3);
        QCOMPARE(roots[1].toObject()["kind"].toString(),QString("prepare"));QVERIFY(roots[1].toObject()["source"].toString().endsWith("configure-prepare-selection.xcfg"));
        config.outputDir=directory.filePath("missing-runs");QVERIFY(QDir().mkpath(config.RunDirectory()));QVERIFY(QFile::remove(QDir(config.configDir).filePath("describe-cobo0.xcfg")));
        QVERIFY(!RunRecorderInternal::SnapshotConfiguration(config,&error));QVERIFY(error.contains("describe, node 0, ID cobo0"));QVERIFY(!QFile::exists(QDir(config.RunDirectory()).filePath("backup.complete")));
        QVERIFY(RunRecorder::Finish(config,1).isEmpty());QVERIFY(QFile::exists(QDir(config.RunDirectory()).filePath("configuration-backup-warning.json")));
        WriteHardwareSetupFile(QDir(config.configDir).filePath("describe-cobo0.xcfg"),"<Setup>");
        QVERIFY(!RunRecorder::Finish(config,1).isEmpty()); // Corrupt XML is not a missing-file warning.
        QVERIFY(QFile::remove(QDir(config.configDir).filePath("describe-cobo0.xcfg")));
        WriteHardwareSetupFile(QDir(config.configDir).filePath("describe-cobo0.xcfg"),"<Setup/>");
        QVERIFY(RunRecorder::Finish(config,1).isEmpty());QVERIFY(QFile::exists(QDir(config.RunDirectory()).filePath("backup.complete")));
        QVERIFY(!QFile::exists(QDir(config.RunDirectory()).filePath("configuration-backup-warning.json")));
    }
    void legacyEccTransitionsUseLongerDeadlineWithoutRetry() {
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost,0));int requests=0;
        connect(&server,&QTcpServer::newConnection,this,[&]{while(server.hasPendingConnections()){
            auto *socket=server.nextPendingConnection();socket->setParent(&server);
            connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{socket->readAll();if(socket->property("scheduled").toBool())return;socket->setProperty("scheduled",true);++requests;
                QTimer::singleShot(350,socket,[socket]{if(socket->state()!=QAbstractSocket::ConnectedState)return;
                    const QByteArray body="<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body><ResponseState><ErrorCode>0</ErrorCode><State>3</State><Transition>0</Transition></ResponseState></s:Body></s:Envelope>";
                    socket->write("HTTP/1.1 200 OK\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();});});
            connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        }});
        auto config=RunConfig::Default();config.simulation=false;config.timeoutMs=100;config.stepTimeoutMs=1500;
        NodeConfig node;node.host="127.0.0.1";node.eccPort=server.serverPort();
        EccClient client(node,config);QSignalSpy replies(&client,&EccClient::ReplySignal);
        QVERIFY(client.Send("Prepare"));QTRY_COMPARE_WITH_TIMEOUT(replies.size(),1,2500);QVERIFY(replies[0][4].toString().isEmpty());QCOMPARE(requests,1);
        QVERIFY(client.Send("GetState"));QTRY_COMPARE_WITH_TIMEOUT(replies.size(),2,1500);QVERIFY(replies[1][4].toString().contains("timed out"));QCOMPARE(requests,2);
        config.stepTimeoutMs=150;EccClient shortDeadline(node,config);QSignalSpy failed(&shortDeadline,&EccClient::ReplySignal);
        QVERIFY(shortDeadline.Send("Prepare"));QTRY_COMPARE_WITH_TIMEOUT(failed.size(),1,1500);QVERIFY(failed[0][4].toString().contains("unknown"));QTest::qWait(400);QCOMPARE(requests,3);
    }
    void newerGuiReusesSavedNodeAndMasterRuntime() {
        const auto installed=QCoreApplication::applicationDirPath();if(!QFile::exists(installed+"/getdaq-node-agent"))QSKIP("Stage backend programs beside tests");const auto root=fTemp.filePath("versioned-install"),oldPrograms=root+"/runtime-old/bin",newPrograms=root+"/runtime-new/bin",nodeRoot=root+"/node",masterRoot=root+"/project";QVERIFY(QDir().mkpath(oldPrograms));QVERIFY(QDir().mkpath(newPrograms));for(const auto &name:{"getdaq-node-agent","getdaq-config-repository","getdaq-config-service"}){QVERIFY(QFile::copy(installed+'/'+name,oldPrograms+'/'+name));QVERIFY(QFile::copy(installed+'/'+name,newPrograms+'/'+name));}
        struct Cleanup{QString node,old;QJsonObject master;~Cleanup(){try{if(!master.isEmpty())Deployment::StopMaster(master);}catch(...){}for(const auto &p:FindLocalServiceProcesses(old+"/getdaq-node-agent",node+"/node.json"))if(SameLocalServiceProcess(p))::kill(p.pid,SIGTERM);QTest::qWait(250);}} cleanup{nodeRoot,oldPrograms,{}};
        QTcpServer nodePort,masterPort;QVERIFY(nodePort.listen(QHostAddress::LocalHost,0));QVERIFY(masterPort.listen(QHostAddress::LocalHost,0));const int n=nodePort.serverPort(),m=masterPort.serverPort();nodePort.close();masterPort.close();Deployment::EnsureLocalNode(nodeRoot,oldPrograms,"127.0.0.1",n,nodeRoot+"/data");const auto before=FindLocalServiceProcesses(oldPrograms+"/getdaq-node-agent",nodeRoot+"/node.json");QCOMPARE(before.size(),1);const auto reused=Deployment::EnsureLocalNode(nodeRoot,newPrograms,"127.0.0.1",n,nodeRoot+"/data");QCOMPARE(reused["inventory"].toObject()["program_directory"].toString(),oldPrograms);QCOMPARE(FindLocalServiceProcesses(oldPrograms+"/getdaq-node-agent",nodeRoot+"/node.json")[0].pid,before[0].pid);
        cleanup.master=Deployment::EnsureMaster(masterRoot,oldPrograms,"127.0.0.1",m);const auto masterBefore=FindLocalArgumentProcesses(oldPrograms+"/getdaq-config-service","--root",masterRoot+"/repository");QCOMPARE(masterBefore.size(),1);const auto masterAgain=Deployment::EnsureMaster(masterRoot,newPrograms,"127.0.0.1",m);QCOMPARE(masterAgain["repository_id"],cleanup.master["repository_id"]);QCOMPARE(FindLocalArgumentProcesses(oldPrograms+"/getdaq-config-service","--root",masterRoot+"/repository")[0].pid,masterBefore[0].pid);
    }
    void simpleExistingEccDetectsSoapNamespacesAndSavesOneProfile() {
        for(const auto &ns:{QString("urn:ecc"),QString("urn:http://localhost:8083/")}){
            QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost,0));QStringList requests;
            connect(&server,&QTcpServer::newConnection,this,[&,ns]{while(server.hasPendingConnections()){auto *socket=server.nextPendingConnection();socket->setParent(&server);auto bytes=std::make_shared<QByteArray>();auto replied=std::make_shared<bool>(false);connect(socket,&QTcpSocket::readyRead,&server,[&,ns,socket,bytes,replied]{*bytes+=socket->readAll();const int split=bytes->indexOf("\r\n\r\n");if(split<0 || *replied)return;int length=0;for(const auto &line:bytes->left(split).split('\n'))if(line.toLower().startsWith("content-length:"))length=line.mid(15).trimmed().toInt();if(bytes->size()<split+4+length)return;*replied=true;requests.append(QString::fromUtf8(*bytes));int code=404;QByteArray body="{}";if(bytes->startsWith("POST ")){const bool matches=bytes->contains(("xmlns:ecc=\""+ns+"\"").toUtf8());code=matches?200:500;body=matches?QString("<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:ecc=\"%1\"><s:Body><ecc:ResponseState><ErrorCode>0</ErrorCode><ErrorMessage/><State>1</State><Transition>0</Transition></ecc:ResponseState></s:Body></s:Envelope>").arg(ns).toUtf8():QByteArray("<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body><s:Fault><faultcode>s:Client</faultcode><faultstring>namespace not recognized</faultstring></s:Fault></s:Body></s:Envelope>");}socket->write("HTTP/1.1 "+QByteArray::number(code)+" Result\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();});connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);}});
            UnifiedEccWindow window(RunConfig::Default());window.show();window.findChild<QLineEdit *>("ExistingEccName")->setText("Existing ECC");window.findChild<QLineEdit *>("ExistingEccAddress")->setText("127.0.0.1");window.findChild<QSpinBox *>("ExistingEccPort")->setValue(server.serverPort());auto *check=window.findChild<QPushButton *>("CheckExistingEcc");auto *use=window.findChild<QPushButton *>("UseExistingEcc");check->click();QTRY_VERIFY_WITH_TIMEOUT(check->isEnabled(),12000);QVERIFY2(use->isEnabled(),qPrintable(window.findChild<QLabel *>("ExistingEccStatus")->text()));QVERIFY(window.findChild<QLabel *>("ExistingEccStatus")->text().contains("Idle"));if(ns=="urn:ecc" && !qEnvironmentVariable("GETDAQ_UNIFIED_SCREENSHOT_DIR").isEmpty()){QDir().mkpath(qEnvironmentVariable("GETDAQ_UNIFIED_SCREENSHOT_DIR"));window.grab().save(qEnvironmentVariable("GETDAQ_UNIFIED_SCREENSHOT_DIR")+"/connect-existing.png");}
            window.findChild<QLineEdit *>("ExistingEccName")->setText("Renamed ECC");QVERIFY(!use->isEnabled());check->click();QTRY_VERIFY_WITH_TIMEOUT(use->isEnabled(),12000);use->click();QCOMPARE(window.result(),int(QDialog::Accepted));const auto config=window.Configuration();QVERIFY(!config.nativeEcc);QCOMPARE(config.nodes[0].soapNamespace,ns);QCOMPARE(config.nodes[0].eccPort,int(server.serverPort()));QCOMPARE(config.nodes[0].routerPort,0);RunConfig loaded;QString error;QVERIFY2(loaded.Load(config.filePath,&error),qPrintable(error));QCOMPARE(loaded.eccName,QString("Renamed ECC"));QCOMPARE(loaded.nodes[0].soapNamespace,ns);QVERIFY(KnownEccConnectionProfiles().contains(config.filePath));for(const auto &request:requests){QVERIFY(!request.contains("Prepare"));QVERIFY(!request.contains("Configure"));QVERIFY(!request.contains("<ecc:Start"));}
        }
    }
    void simpleExistingEccWaitCanBeClosed() {
        QTcpServer silent;QVERIFY(silent.listen(QHostAddress::LocalHost,0));connect(&silent,&QTcpServer::newConnection,this,[&]{while(silent.hasPendingConnections())silent.nextPendingConnection()->setParent(&silent);});UnifiedEccWindow window(RunConfig::Default());window.show();window.findChild<QLineEdit *>("ExistingEccAddress")->setText("127.0.0.1");window.findChild<QSpinBox *>("ExistingEccPort")->setValue(silent.serverPort());window.findChild<QPushButton *>("CheckExistingEcc")->click();QVERIFY(window.Busy());window.RequestClose();QTRY_VERIFY_WITH_TIMEOUT(!window.isVisible(),3000);QVERIFY(!window.Busy());QVERIFY(!window.findChild<QPushButton *>("UseExistingEcc")->isEnabled());
    }
    void soapNamespaceIsExplicitAndSurvivesSaving() {
        auto config=RunConfig::Default();config.nodes.resize(1);config.simulation=false;config.nodes[0].host="192.168.40.16";config.nodes[0].eccPort=8083;config.nodes[0].soapNamespace="urn:ecc";QString error;const auto path=fTemp.filePath("soap-ecc.yml");QVERIFY2(config.Save(path,&error),qPrintable(error));RunConfig loaded;QVERIFY2(loaded.Load(path,&error),qPrintable(error));QCOMPARE(loaded.nodes[0].soapNamespace,QString("urn:ecc"));QVERIFY(Protocol::EccRequest(loaded.nodes[0],loaded.experiment,"GetState").contains("xmlns:ecc=\"urn:ecc\""));NodeConfig legacy;QVERIFY(Protocol::EccRequest(legacy,"Exp","GetState").contains("urn:http://localhost:8083/"));loaded.nodes[0].soapNamespace="unknown";QVERIFY(!loaded.Validate(&error));
    }
    void unresponsiveAgentStartupHasOneBoundedWait() {
        const auto programs=QCoreApplication::applicationDirPath(),root=fTemp.filePath("stalled-agent");if(!QFileInfo(programs+"/getdaq-node-agent").isExecutable())QSKIP("Stage current node agent beside tests");
        QTcpServer port;QVERIFY(port.listen(QHostAddress::LocalHost,0));const int selectedPort=port.serverPort();port.close();
        Deployment::EnsureLocalNode(root,programs,"127.0.0.1",selectedPort,root+"/data");const auto processes=FindLocalServiceProcesses(programs+"/getdaq-node-agent",root+"/node.json");QCOMPARE(processes.size(),1);const auto process=processes[0];
        struct Cleanup{LocalServiceProcess process;~Cleanup(){if(SameLocalServiceProcess(process)){::kill(process.pid,SIGCONT);::kill(process.pid,SIGTERM);}QTest::qWait(250);}} cleanup{process};
        QVERIFY(!::kill(process.pid,SIGSTOP));QElapsedTimer elapsed;elapsed.start();QString failure;try{Deployment::EnsureLocalNode(root,programs,"127.0.0.1",selectedPort,root+"/data");}catch(const std::exception &error){failure=QString::fromUtf8(error.what());}
        QVERIFY(!failure.isEmpty());QVERIFY2(elapsed.elapsed()<14000,qPrintable(QString::number(elapsed.elapsed())));QVERIFY(!::kill(process.pid,SIGCONT));
    }
    void localWaitCancellationStopsOnlyItsOfflineHelper() {
        auto canceled=std::make_shared<std::atomic_bool>(false);auto future=QtConcurrent::run([canceled]{Deployment::OperationScope operation(canceled);try{Deployment::Run("/bin/sleep",{"10"},15000);return QString("unexpected success");}catch(const std::exception &error){return QString::fromUtf8(error.what());}});
        QTest::qWait(200);QElapsedTimer elapsed;elapsed.start();canceled->store(true);QTRY_VERIFY_WITH_TIMEOUT(future.isFinished(),3000);QVERIFY(future.result().contains("Local waiting stopped"));QVERIFY(elapsed.elapsed()<3000);
    }
    void masterWaitCanBeStoppedAndClosedWithPairingIntentPreserved() {
        const auto programs=QCoreApplication::applicationDirPath(),root=fTemp.filePath("cancel-master");if(!QFileInfo(programs+"/getdaq-config-service").isExecutable())QSKIP("Stage configuration service beside tests");
        struct Cleanup{QString root;~Cleanup(){try{if(QFile::exists(root+"/master.json"))Deployment::StopMaster(Deployment::Parse(Deployment::Read(root+"/master.json")));}catch(...){}}} cleanup{root};
        DistributedDeploymentWindow window(RunConfig::Default());window.show();window.findChild<QLineEdit *>("DistributedProjectRoot")->setText(root);window.findChild<QLineEdit *>("DistributedMasterAddress")->setText("127.0.0.1");QTcpServer port;QVERIFY(port.listen(QHostAddress::LocalHost,0));for(auto *spin:window.findChildren<QSpinBox *>())spin->setValue(port.serverPort());port.close();window.findChild<QPushButton *>("StartDistributedMaster")->click();QTRY_VERIFY_WITH_TIMEOUT(!window.Busy(),15000);QVERIFY(QFile::exists(root+"/master.json"));
        QTcpServer silent;QVERIFY(silent.listen(QHostAddress::LocalHost,0));QJsonObject invitation{{"schema",1},{"node_id","silent-node"},{"url",QString("https://127.0.0.1:%1").arg(silent.serverPort())},{"certificate_sha256",QString(64,'0')},{"pairing_code","test-only-unused"}};const auto code="getdaq-pair:"+QString::fromLatin1(Deployment::Json(invitation).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals));window.findChild<QLineEdit *>("DistributedPairingInvitation")->setText(code);
        auto *pair=window.findChild<QPushButton *>("PairDistributedHost");pair->click();QTRY_VERIFY_WITH_TIMEOUT(silent.hasPendingConnections(),5000);silent.nextPendingConnection()->setParent(&silent);QVERIFY(window.Busy());auto *stop=window.findChild<QPushButton *>("StopDeploymentWaiting");QVERIFY(stop->isEnabled());QElapsedTimer elapsed;elapsed.start();stop->click();QTRY_VERIFY_WITH_TIMEOUT(!window.Busy(),3000);QVERIFY(elapsed.elapsed()<3000);QVERIFY(window.isVisible());QVERIFY(QFile::exists(root+"/pending-pairings/silent-node.json"));QVERIFY(window.findChild<QPlainTextEdit *>("DistributedDeploymentLog")->toPlainText().contains("Local waiting stopped"));
        pair->click();QTRY_VERIFY_WITH_TIMEOUT(silent.hasPendingConnections(),5000);silent.nextPendingConnection()->setParent(&silent);window.RequestClose();QTRY_VERIFY_WITH_TIMEOUT(!window.isVisible(),3000);QVERIFY(!window.Busy());
        DistributedDeploymentWindow reopened(RunConfig::Default());reopened.OpenProject(root);QCOMPARE(reopened.findChild<QLineEdit *>("DistributedPairingInvitation")->text(),code);
    }
    void suggestedPortsSurviveGroupRemovalAndRespectHostScope() {
        HardwareDeploymentSetup setup;setup.boards.resize(2);for(int i=0;i<2;++i){setup.boards[i].id=QString::number(i);setup.boards[i].receivesData=true;}
        const QJsonArray nodes{QJsonObject{{"id","host-a"},{"url","https://192.0.2.1:24000"}},QJsonObject{{"id","host-b"},{"url","https://192.0.2.2:24260"}}};
        const QJsonObject remaining{{"id","remaining-after-removal"},{"ecc",QJsonObject{{"node_id","host-a"},{"port",25000},{"callback_port",24100}}},{"policies",QJsonArray{QJsonObject(),QJsonObject()}},{"receivers",QJsonArray{QJsonObject{{"node_id","host-a"},{"port",24002},{"sources",QJsonArray{QJsonObject{{"data_port",24300},{"status_port",24301}},QJsonObject{{"data_port",24302},{"status_port",24303}}}}}}}};
        const auto suggested=Deployment::SuggestGroupPorts(setup,"host-a","host-a",QJsonArray{remaining},nodes);
        QCOMPARE(suggested.eccPort,24001);QCOMPARE(suggested.supervisorPort,24003);QCOMPARE(suggested.callbackPort,24102);
        QSet<int> assigned{suggested.eccPort,suggested.supervisorPort,suggested.callbackPort,suggested.callbackPort+1};
        for(const auto &board:suggested.boards){QVERIFY(!assigned.contains(board.dataPort));assigned.insert(board.dataPort);QVERIFY(!assigned.contains(board.statusPort));assigned.insert(board.statusPort);QVERIFY(board.dataPort>24303);QVERIFY(board.statusPort>24303);}
        const auto otherHost=Deployment::SuggestGroupPorts(setup,"host-b","host-b",QJsonArray{remaining},nodes);QCOMPARE(otherHost.eccPort,24000);QCOMPARE(otherHost.boards[0].dataPort,24300);
        setup.boards[1].receiverNodeId="host-b";const auto split=Deployment::SuggestGroupPorts(setup,"host-a","host-a",QJsonArray{remaining},nodes);QVERIFY(split.supervisorPort!=split.eccPort);QCOMPARE(split.boards[0].receiverNodeId,QString("host-a"));QCOMPARE(split.boards[1].receiverNodeId,QString("host-b"));QCOMPARE(split.boards[1].dataPort,24300);
    }
    void slaveRoleStartsWithoutExperimentAndReusesAgent() {
        const auto program=QCoreApplication::applicationDirPath()+"/getdaq-node-agent";
        if(!QFileInfo(program).isExecutable())QSKIP("Stage node agent beside this test executable");
        QSettings().setValue(StartupConfigurationKey()+"/machine-role","master");
        const auto nodeRoot=fTemp.filePath("chosen-slave-cache");
        struct Cleanup {QString program,root;~Cleanup(){for(const auto &p:FindLocalServiceProcesses(program,root+"/node.json"))if(SameLocalServiceProcess(p))::kill(p.pid,SIGTERM);QSettings().setValue(StartupConfigurationKey()+"/machine-role","master");QTest::qWait(250);}} cleanup{program,nodeRoot};
        HostRoleWindow window(RunConfig::Default());window.show();
        auto *role=window.findChild<QComboBox *>("MachineRoleSelector");role->setCurrentIndex(1);QCOMPARE(HostRoleWindow::SavedRole(),QString("slave"));
        QVERIFY(window.findChild<QLineEdit *>("SlaveCacheDirectory")->isVisible());QVERIFY(window.findChild<QLineEdit *>("SlaveDataDirectory")->isVisible());window.findChild<QLineEdit *>("SlaveCacheDirectory")->setText(nodeRoot);window.findChild<QLineEdit *>("SlaveDataDirectory")->setText(fTemp.filePath("chosen-slave-data"));auto *start=window.findChild<QPushButton *>("StartSlaveAgent");auto *address=window.findChild<QLineEdit *>("SlaveManagementAddress");address->setText("127.0.0.1");
        auto *port=window.findChild<QSpinBox *>("SlaveManagementPort");QTcpServer probe;QVERIFY(probe.listen(QHostAddress::LocalHost,0));port->setValue(probe.serverPort());probe.close();
        start->click();auto *invitation=window.findChild<QLineEdit *>("SlavePairingInvitation");
        QTRY_VERIFY_WITH_TIMEOUT(start->isEnabled(),20000);
        QVERIFY2(invitation->text().startsWith("getdaq-pair:"),qPrintable(window.findChild<QLabel *>("SlaveAgentStatus")->text()));
        QVERIFY(QFile::exists(nodeRoot+"/node.json"));QVERIFY(!QFile::exists(nodeRoot+"/project.json"));QCOMPARE(QSettings().value(StartupConfigurationKey()+"/slave-cache-directory").toString(),nodeRoot);QCOMPARE(Deployment::Parse(Deployment::Read(nodeRoot+"/node.json"))["data_directory"].toString(),fTemp.filePath("chosen-slave-data"));
        const auto before=FindLocalServiceProcesses(program,nodeRoot+"/node.json");QCOMPARE(before.size(),1);
        start->click();QTRY_VERIFY_WITH_TIMEOUT(start->isEnabled(),20000);
        const auto after=FindLocalServiceProcesses(program,nodeRoot+"/node.json");QCOMPARE(after.size(),1);QCOMPARE(after[0].pid,before[0].pid);
        role->setCurrentIndex(0);QCOMPARE(HostRoleWindow::SavedRole(),QString("master"));
        // Selecting a different GUI role must not kill the independent agent.
        QVERIFY(SameLocalServiceProcess(before[0]));
    }
    void masterAutomaticallyJoinsLocalAndRunsCachedGroups() {
        const auto programs=QCoreApplication::applicationDirPath();
        if(!QFileInfo(programs+"/getdaq-config-service").isExecutable())QSKIP("Stage current backend programs beside tests");
        const auto projectRoot=fTemp.filePath("simple-master"),nodeRoot=QSettings().value(StartupConfigurationKey()+"/slave-cache-directory",QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/local-node").toString();
        struct Cleanup {QString programs,root,node;~Cleanup(){try{if(QFile::exists(root+"/master.json"))Deployment::StopMaster(Deployment::Parse(Deployment::Read(root+"/master.json")));}catch(...){}for(const auto &name:QDir(node+"/instances").entryList({"*.json"},QDir::Files)){try{const auto instance=Deployment::Parse(Deployment::Read(node+"/instances/"+name));for(const auto &p:FindLocalServiceProcesses(instance["program"].toString(),instance["config_file"].toString()))if(SameLocalServiceProcess(p))::kill(p.pid,SIGTERM);}catch(...){}}for(const auto &p:FindLocalServiceProcesses(programs+"/getdaq-node-agent",node+"/node.json"))if(SameLocalServiceProcess(p))::kill(p.pid,SIGTERM);QTest::qWait(250);}} cleanup{programs,projectRoot,nodeRoot};
        DistributedDeploymentWindow window(RunConfig::Default());window.show();window.findChild<QLineEdit *>("DistributedProjectRoot")->setText(projectRoot);window.findChild<QLineEdit *>("DistributedMasterAddress")->setText("127.0.0.1");
        QTcpServer probe;QVERIFY(probe.listen(QHostAddress::LocalHost,0));
        const auto masterPort=probe.serverPort();probe.close();for(auto *spin:window.findChildren<QSpinBox *>())spin->setValue(masterPort);
        auto *join=window.findChild<QPushButton *>("InitializeLocalNode");join->click();auto *hosts=window.findChild<QTableWidget *>("DistributedHosts");
        QTRY_VERIFY_WITH_TIMEOUT(hosts->rowCount()==1 || window.findChild<QPlainTextEdit *>("DistributedDeploymentLog")->toPlainText().contains(QStringLiteral("Operation incomplete")),30000);
        QCOMPARE(hosts->rowCount(),1);QVERIFY(QFile::exists(nodeRoot+"/master.json"));
        join->click();QTRY_VERIFY_WITH_TIMEOUT(join->isEnabled(),30000);QCOMPARE(hosts->rowCount(),1);
        bool directBoardSetup=false;QTimer::singleShot(0,&window,[&]{auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());directBoardSetup=qobject_cast<HardwareSetupWindow *>(dialog)!=nullptr;if(dialog)QMetaObject::invokeMethod(dialog,"reject",Qt::DirectConnection);});window.findChild<QPushButton *>("AddDistributedGroup")->click();QVERIFY(directBoardSetup);
        const auto master=Deployment::Parse(Deployment::Read(projectRoot+"/master.json"));const auto project=Deployment::Parse(Deployment::Read(projectRoot+"/project.json"));const auto nodes=project["nodes"].toArray();const auto node=nodes[0].toObject();
        QMap<QString,QByteArray> files;QList<QTcpServer *> reservations;QList<int> ports;
        for(int i=0;i<10;++i){auto *socket=new QTcpServer(this);QVERIFY(socket->listen(QHostAddress::LocalHost,0));ports.append(socket->serverPort());reservations.append(socket);}for(auto *socket:reservations)socket->close();
        auto group=[&](const QString &id,int index){HardwareDeploymentSetup setup;setup.experimentFile=Root()+"/config/backend-presets/pedestals-single-cobo.xcfg";setup.profileFile=Root()+"/config/backend-presets/pedestals-profile.json";setup.callbackAddress="127.0.0.1";setup.eccPort=ports[index*5];setup.supervisorPort=ports[index*5+1];setup.callbackPort=ports[index*5+2];setup.boards=HardwareSetupBoards(CompileHardwareSetupExperiment(setup.experimentFile,setup.profileFile));for(auto &b:setup.boards){b.address=QString("192.0.2.%1").arg(230+index);b.nodeObject="CoBo";b.nodeName="Hardware";b.controlPort=46001;b.senderPort=46002;b.receiverAddress="127.0.0.1";b.dataPort=ports[index*5+3];b.statusPort=ports[index*5+4];b.descriptionFile=Root()+"/config/backend-presets/cobo-hardware.xcfg";}return Deployment::CompileGroup(setup,id,node["id"].toString(),"127.0.0.1",QJsonArray{QJsonObject{{"id","data"},{"node_id",node["id"]},{"address","127.0.0.1"},{"port",setup.supervisorPort},{"sender_ids",QJsonArray{"0"}}}},&files,files);};
        const auto first=group("group1",0);const QJsonArray hostIds{QJsonObject{{"id",node["id"]}}};
        const auto bundle1=Deployment::BuildBundle("simple-project",hostIds,QJsonArray{first},files);const auto prepared1=Deployment::PrepareProject(master,nodes,bundle1,{},"group1");const auto active1=Deployment::ActivateProject(master,nodes,prepared1,{},"group1");
        const auto config=Deployment::ConsoleConfiguration(master,prepared1,active1,"group1",RunConfig::Default());QVERIFY(config.nativeEcc);QVERIFY(config.nativeClient.inputBindingSha256.size()==64);
        UnifiedEccWindow nativeConnection(config);nativeConnection.show();nativeConnection.findChild<QPushButton *>("CheckExistingEcc")->click();QTRY_VERIFY_WITH_TIMEOUT(nativeConnection.findChild<QPushButton *>("CheckExistingEcc")->isEnabled(),15000);QVERIFY2(nativeConnection.findChild<QPushButton *>("UseExistingEcc")->isEnabled(),qPrintable(nativeConnection.findChild<QLabel *>("ExistingEccStatus")->text()));nativeConnection.findChild<QPushButton *>("UseExistingEcc")->click();QVERIFY(nativeConnection.Configuration().nativeEcc);QCOMPARE(nativeConnection.Configuration().nativeClient.snapshotSha256,config.nativeClient.snapshotSha256);QCOMPARE(nativeConnection.Configuration().nativeRecordDir,config.nativeRecordDir);
        auto unauthorized=QtConcurrent::run([&]{try{CheckExistingEcc("Unpaired",config.nativeClient.address,config.nativeClient.port,RunConfig::Default());return QString();}catch(const std::exception &error){return QString::fromUtf8(error.what());}});QTRY_VERIFY_WITH_TIMEOUT(unauthorized.isFinished(),10000);QVERIFY(unauthorized.result().contains("authorization"));
        auto wrong=config;wrong.nativeClient.snapshotSha256=QString(64,'0');auto mismatch=QtConcurrent::run([wrong]{try{CheckExistingEcc("Wrong identity",wrong.nativeClient.address,wrong.nativeClient.port,wrong);return QString();}catch(const std::exception &error){return QString::fromUtf8(error.what());}});QTRY_VERIFY_WITH_TIMEOUT(mismatch.isFinished(),10000);QVERIFY(mismatch.result().contains("snapshot differs"));
        const auto before=Deployment::NodeInventory(node)["instances"].toArray();QMap<QString,QJsonArray> pids;for(const auto &value:before){const auto instance=value.toObject();pids[instance["instance_id"].toString()]=instance["actual_pids"].toArray();QCOMPARE(instance["actual_pids"].toArray().size(),1);}
        const auto second=group("group2",1);const auto bundle2=Deployment::BuildBundle("simple-project",hostIds,QJsonArray{first,second},files);const auto prepared2=Deployment::PrepareProject(master,nodes,bundle2,{},"group2");Deployment::ActivateProject(master,nodes,prepared2,{},"group2");
        for(const auto &value:Deployment::NodeInventory(node)["instances"].toArray()){const auto instance=value.toObject();if(instance["group_id"]=="group1")QCOMPARE(instance["actual_pids"].toArray(),pids[instance["instance_id"].toString()]);}
        const auto preparedAgain=Deployment::PrepareProject(master,nodes,bundle2,{},"group1");const auto reused=Deployment::ActivateProject(master,nodes,preparedAgain,{},"group1");const auto reusedConfig=Deployment::ConsoleConfiguration(master,preparedAgain,reused,"group1",RunConfig::Default());QCOMPARE(reusedConfig.nativeClient.inputBindingSha256,config.nativeClient.inputBindingSha256);
        auto savedProject=project;savedProject["project_id"]="simple-project";savedProject["groups"]=QJsonArray{first,second};QJsonArray content;for(auto it=files.begin();it!=files.end();++it){const auto sha=Deployment::Sha(it.value());Deployment::Write(projectRoot+"/content/"+sha,it.value(),true);content.append(QJsonObject{{"id",it.key()},{"sha256",sha},{"bytes",it.value().size()}});}savedProject["content_files"]=content;Deployment::Write(projectRoot+"/project.json",Deployment::Json(savedProject));
        DistributedDeploymentWindow daily(RunConfig::Default());daily.OpenProject(projectRoot);daily.show();QVERIFY(!daily.findChild<QCheckBox *>("DistributedAdvancedSettings")->isChecked());
        const auto agents=FindLocalServiceProcesses(programs+"/getdaq-node-agent",nodeRoot+"/node.json");QCOMPARE(agents.size(),1);
        {const auto agent=agents[0];struct Resume{LocalServiceProcess process;~Resume(){if(SameLocalServiceProcess(process))::kill(process.pid,SIGCONT);}} resume{agent};QVERIFY(!::kill(agent.pid,SIGSTOP));daily.findChild<QPushButton *>("StartAndUseDistributedGroup")->click();auto *dailyLog=daily.findChild<QPlainTextEdit *>("DistributedDeploymentLog");QTRY_VERIFY_WITH_TIMEOUT(dailyLog->toPlainText().contains("Preparing receiving host"),15000);QVERIFY(daily.Busy());daily.findChild<QPushButton *>("StopDeploymentWaiting")->click();QTRY_VERIFY_WITH_TIMEOUT(!daily.Busy(),3000);QVERIFY(daily.isVisible());QVERIFY(dailyLog->toPlainText().contains("Local waiting stopped"));}
        for(const auto &value:Deployment::NodeInventory(node)["instances"].toArray()){const auto instance=value.toObject();if(instance["group_id"]=="group1")QCOMPARE(instance["actual_pids"].toArray(),pids[instance["instance_id"].toString()]);}
        daily.findChild<QPushButton *>("StartAndUseDistributedGroup")->click();QTRY_VERIFY_WITH_TIMEOUT(daily.result()==QDialog::Accepted || (!daily.Busy() && daily.findChild<QPlainTextEdit *>("DistributedDeploymentLog")->toPlainText().contains(QStringLiteral("Operation incomplete"))),60000);QVERIFY2(daily.result()==QDialog::Accepted,qPrintable(daily.findChild<QPlainTextEdit *>("DistributedDeploymentLog")->toPlainText()));QCOMPARE(daily.Configuration().nativeClient.inputBindingSha256,config.nativeClient.inputBindingSha256);
        const auto observation=Deployment::ObserveDeployment(master,nodes);QVERIFY(!observation["hosts"].toArray().isEmpty());
        auto edited=second;auto eccSettings=edited["ecc"].toObject();QTcpServer callbackProbe;QVERIFY(callbackProbe.listen(QHostAddress::LocalHost,0));eccSettings["callback_port"]=int(callbackProbe.serverPort());callbackProbe.close();edited["ecc"]=eccSettings;
        const auto bundle3=Deployment::BuildBundle("simple-project",hostIds,QJsonArray{first,edited},files);const auto prepared3=Deployment::PrepareProject(master,nodes,bundle3,{},"group2");
        bool changeRefused=false;try{Deployment::ActivateProject(master,nodes,prepared3,{},"group2");}catch(const std::exception &){changeRefused=true;}QVERIFY(changeRefused);
        bool refused=false;try{Deployment::ReleaseTerminatedGroup(master,nodes,"group2");}catch(const std::exception &){refused=true;}QVERIFY(refused);
        Deployment::StopGroup(master,nodes,prepared2,"group2",true);
        const QString deniedId="inspection-without-confirmation";
        Deployment::Post(Deployment::NodeEndpoint(node),"/v1/operations",QJsonObject{{"schema",1},{"operation_id",deniedId},{"action","release-terminated"},{"release_id",prepared2["release_id"]},{"group_id","group2"},{"role","ecc"},{"accepted_repair",false}});
        QTRY_VERIFY_WITH_TIMEOUT(Deployment::Get(Deployment::NodeEndpoint(node),"/v1/operations/"+deniedId)["phase"]=="failed",3000);
        const auto released=Deployment::ReleaseTerminatedGroup(master,nodes,"group2");QCOMPARE(released["phase"].toString(),QString("released-after-inspection"));
        for(const auto &value:released["receipts"].toArray()){const auto receipt=value.toObject();QVERIFY(Deployment::DeploymentInstanceReleased(receipt));QVERIFY(!receipt["stopped"].toBool());QVERIFY(receipt["inspection_release"].toObject()["operator_confirmed_stopped"].toBool());}
        Deployment::ActivateProject(master,nodes,prepared3,{},"group2");
        Deployment::StopGroup(master,nodes,prepared3,"group2",false);
        Deployment::StopGroup(master,nodes,preparedAgain,"group1",false);
        for(const auto &value:Deployment::NodeInventory(node)["instances"].toArray())QVERIFY(value.toObject()["actual_pids"].toArray().isEmpty());
    }
    void distributedDraftAssignsBoardsToActualReceivingHosts() {
        HardwareSetupWindow window(RunConfig::Default());window.EnableProjectDraftMode();
        const QJsonArray hosts={QJsonObject{{"id","host-a"},{"name","A"},{"url","https://192.0.2.80:24260"},{"inventory",QJsonObject{{"addresses",QJsonArray{"192.0.2.80"}}}}},QJsonObject{{"id","host-b"},{"name","B"},{"url","https://192.0.2.81:24260"},{"inventory",QJsonObject{{"addresses",QJsonArray{"192.0.2.81"}}}}}};
        window.SetReceivingHosts(hosts,"host-a");auto *table=window.findChild<QTableWidget *>("HardwareBoardTable");
        auto *selection=qobject_cast<QComboBox *>(table->cellWidget(0,10));QVERIFY(selection);selection->setCurrentIndex(selection->findData("host-b"));
        const auto draft=window.DraftSetup();QCOMPARE(draft.boards[0].receiverNodeId,QString("host-b"));QCOMPARE(draft.boards[0].receiverAddress,QString("192.0.2.81"));
        window.findChild<QCheckBox *>("SetupStopped")->setChecked(true);window.findChild<QPushButton *>("SetupUse")->click();QCOMPARE(window.result(),int(QDialog::Accepted));
    }
    void distributedWindowObservesAndSelectsRealPreparedProject() {
        const auto root=qEnvironmentVariable("GETDAQ_DISTRIBUTED_GUI_PROJECT");if(root.isEmpty())QSKIP("Set a private integration project for the real async GUI flow");
        DistributedDeploymentWindow window(RunConfig::Default());window.OpenProject(root);window.show();
        auto *log=window.findChild<QPlainTextEdit *>("DistributedDeploymentLog");auto *groups=window.findChild<QTableWidget *>("DistributedGroups");QVERIFY(groups->rowCount()>0);
        window.findChild<QPushButton *>("ObserveDistributedRelease")->click();
        QTRY_VERIFY_WITH_TIMEOUT(log->toPlainText().contains("PIDs=") || log->toPlainText().contains(QStringLiteral("Operation incomplete")),30000);
        QVERIFY2(log->toPlainText().contains("Idle"),qPrintable(log->toPlainText()));
        window.findChild<QPushButton *>("UseDistributedGroup")->click();QCOMPARE(window.result(),int(QDialog::Accepted));QVERIFY(window.Configuration().nativeEcc);QVERIFY(window.Configuration().nativeClient.inputBindingSha256.size()==64);
    }
    void emptyBackendWindowOffersSetupInsteadOfDisabledStart() {
        LocalServicesWindow window(RunConfig::Default());
        auto *start=window.findChild<QPushButton *>("BackendAction0");
        QVERIFY(start);QVERIFY(start->isEnabled());
        QVERIFY(window.findChild<QPlainTextEdit *>("ServiceLaunchLog")->toPlainText().contains("No saved local backend configuration"));
        bool opened=false;
        QTimer::singleShot(0,&window,[&]{
            auto *setup=qobject_cast<HardwareSetupWindow *>(QApplication::activeModalWidget());
            opened=setup!=nullptr;if(setup)QMetaObject::invokeMethod(setup,"reject",Qt::DirectConnection);
            else if(auto *modal=QApplication::activeModalWidget())modal->close();
        });
        start->click();QVERIFY(opened);QVERIFY(start->isEnabled());
        QVERIFY(window.Configuration().filePath.isEmpty());
    }
    void initTestCase() {
        QVERIFY(fTemp.isValid());qputenv("XDG_DATA_HOME",fTemp.filePath("app-data").toUtf8());QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,fTemp.filePath("settings"));fConfigs=fTemp.filePath("configs");QVERIFY(QDir().mkpath(fConfigs));
        const QString from=Root()+"/source/get/GetBench-software-release-20210208/data/config";
        QDirIterator it(from,QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
        while(it.hasNext()){it.next();QString dest=QDir(fConfigs).filePath(QDir(from).relativeFilePath(it.filePath()));if(it.fileInfo().isDir())QVERIFY(QDir().mkpath(dest));else QVERIFY(QFile::copy(it.filePath(),dest));}
    }
    void generatesBoundMultiBoardInputs() {
        if(qEnvironmentVariableIsEmpty("GETDAQ_SETUP_BIN"))QSKIP("Set GETDAQ_SETUP_BIN to a full installed bin directory");
        const auto s=Setup("valid-site");const auto files=WriteHardwareDeployment(s,RunConfig::Default());
        QCOMPARE(s.boards.size(),3);QCOMPARE(files.console.nodes.size(),2); // one active CoBo, one MuTaNT
        const auto receiver=QJsonDocument::fromJson(ReadHardwareSetupFile(files.supervisorFile)).object();const auto plan=receiver["receiver_plan"].toObject();
        const auto sources=plan["sources"].toArray();QCOMPARE(sources.size(),2);
        QCOMPARE(sources[0].toObject()["id"].toString(),s.boards[0].id);
        QCOMPARE(sources[1].toObject()["id"].toString(),s.boards[2].id);
        QVERIFY(files.console.nodes[1].mutant);QCOMPARE(files.console.nodes[1].nativeSenderId,s.boards[2].id);
        QVERIFY(!QFileInfo::exists(files.directory+"/state/node"));QVERIFY(!QFileInfo::exists(files.directory+"/state/ecc"));QVERIFY(!QFileInfo::exists(files.eccFile));
        QCOMPARE(ReadHardwareSetupFile(files.directory+"/credentials/ecc.token").trimmed().size(),64);
        QCOMPARE(QFileInfo(files.directory+"/credentials/ecc.token").permissions() & (QFile::ReadGroup|QFile::ReadOther|QFile::WriteGroup|QFile::WriteOther),QFile::Permissions());
        QVERIFY_EXCEPTION_THROWN(WriteHardwareDeployment(s,RunConfig::Default()),std::runtime_error);
        // Real production parsers validate the generated plans without any board connection.
        QProcess check;check.start(s.programDirectory+"/getdaq-supervisor",{"--config",files.supervisorFile,"--plan-config"});QVERIFY(check.waitForFinished(15000));QCOMPARE(check.exitCode(),0);const auto planBytes=check.readAllStandardOutput();
        auto ecc=files.ecc;auto remote=ecc["remote_receivers"].toObject();auto endpoints=remote["endpoints"].toArray();auto endpoint=endpoints[0].toObject();endpoint["plan_sha256"]=HardwareSetupSha256(planBytes);endpoints[0]=endpoint;remote["endpoints"]=endpoints;ecc["remote_receivers"]=remote;
        WriteHardwareSetupFile(files.eccFile,QJsonDocument(ecc).toJson());
        check.start(s.programDirectory+"/getdaq-ecc-server",{"--config",files.eccFile,"--plan-config"});QVERIFY(check.waitForFinished(15000));const auto diagnostic=check.readAllStandardError();QVERIFY2(check.exitCode()==0,diagnostic.constData());
        const auto identity=QJsonDocument::fromJson(check.readAllStandardOutput()).object();QVERIFY(!identity["snapshot_sha256"].toString().isEmpty());QVERIFY(!identity["hardware_commands_sent"].toBool());
        QVERIFY(!QFileInfo::exists(files.directory+"/state/node"));QVERIFY(!QFileInfo::exists(files.directory+"/state/ecc"));
    }
    void editedBoardsMapToFilesAndReopen() {
        if(qEnvironmentVariableIsEmpty("GETDAQ_SETUP_BIN"))QSKIP("Set GETDAQ_SETUP_BIN to a full installed bin directory");
        auto s=Setup("edited-boards");
        // Remove disabled CoBo and MuTaNT, then duplicate the reviewed active CoBo.
        const auto original=s.boards[0];s.boards={original};
        auto extra=original;extra.id="AddedCoBo";extra.address="192.0.2.55";extra.dataPort=23021;extra.statusPort=23022;
        s.boards.append(extra);s.eccPort=24200;s.supervisorPort=23200;
        auto files=WriteHardwareDeployment(s,RunConfig::Default());
        const auto artifact=CompileHardwareSetupExperiment(files.directory+"/experiment.json",QString());
        QCOMPARE(artifact.settings.cobos.size(),std::size_t(2));QVERIFY(artifact.settings.mutants.empty());
        QCOMPARE(QString::fromStdString(artifact.settings.cobos[1].id),extra.id);
        GetDAQ::SystemExperimentSettings first,second;
        first.cobos.push_back(artifact.settings.cobos[0]);second.cobos.push_back(artifact.settings.cobos[1]);
        second.cobos[0].id=first.cobos[0].id;
        QCOMPARE(GetDAQ::EncodeSystemExperimentSettings(first),GetDAQ::EncodeSystemExperimentSettings(second));
        const auto topology=QJsonDocument::fromJson(ReadHardwareSetupFile(files.directory+"/topology.json")).object();
        QVERIFY(topology["cobos"].toArray()[1].toObject()["node_proxy"].toString().contains(extra.address));
        const auto supervisor=QJsonDocument::fromJson(ReadHardwareSetupFile(files.supervisorFile)).object();
        QCOMPARE(supervisor["port"].toInt(),23200);
        const auto sources=supervisor["receiver_plan"].toObject()["sources"].toArray();
        QCOMPARE(sources.size(),2);QCOMPARE(sources[1].toObject()["data_port"].toInt(),23021);
        QCOMPARE(files.console.nodes[1].host,extra.address);QCOMPARE(files.console.nodes[1].routerPort,23021);
        WriteHardwareSetupFile(files.eccFile,QJsonDocument(files.ecc).toJson());
        HardwareSetupWindow editor(files.console);
        const auto table=editor.findChild<QTableWidget *>("HardwareBoardTable");
        QCOMPARE(table->rowCount(),2);QCOMPARE(table->item(1,2)->text(),extra.address);
        QCOMPARE(editor.findChild<QSpinBox *>("SetupEccPort")->value(),24200);
        QVERIFY(editor.findChild<QPushButton *>("SetupUse")->isEnabled()==false);
        auto restored=ReadHardwareDeploymentSetup(files.directory);
        QCOMPARE(restored.boards.size(),2);QCOMPARE(restored.boards[1].address,extra.address);
        QCOMPARE(restored.boards[1].templateId,original.id);QCOMPARE(restored.eccPort,24200);
        // Reopening uses the saved template artifact and can regenerate without the original .xcfg/profile.
        restored.programDirectory=s.programDirectory;restored.deploymentDirectory=fTemp.filePath("edited-roundtrip");
        const auto again=WriteHardwareDeployment(restored,RunConfig::Default());
        QCOMPARE(ReadHardwareSetupFile(again.directory+"/experiment.json").contains("AddedCoBo"),true);
        QVERIFY(!QFileInfo::exists(files.directory+"/state/ecc"));
        QVERIFY(QFile::rename(files.directory+"/setup-inputs.json",files.directory+"/setup-inputs.saved"));
        const auto imported=ReadHardwareDeploymentSetup(files.directory);
        QCOMPARE(imported.boards.size(),2);QCOMPARE(imported.boards[1].address,extra.address);
        QCOMPARE(imported.boards[1].dataPort,23021);QCOMPARE(imported.callbackAddress,s.callbackAddress);
    }
    void duplicateBoardIdsAreRejectedBeforeWriting() {
        auto s=Setup("duplicate-ids");s.boards[1].id=s.boards[0].id;
        QVERIFY_EXCEPTION_THROWN(WriteHardwareDeployment(s,RunConfig::Default()),std::runtime_error);
        QVERIFY(!QFileInfo::exists(s.deploymentDirectory));
    }
    void rejectsConflictsAndNonlocalReceiverBeforeWriting() {
        auto s=Setup("bad-site");s.boards[0].receiverAddress="192.0.2.254";
        QVERIFY_EXCEPTION_THROWN(WriteHardwareDeployment(s,RunConfig::Default()),std::runtime_error);QVERIFY(!QFileInfo::exists(s.deploymentDirectory));
        s.boards[0].receiverAddress="127.0.0.1";s.boards[0].dataPort=s.eccPort;
        QVERIFY_EXCEPTION_THROWN(WriteHardwareDeployment(s,RunConfig::Default()),std::runtime_error);QVERIFY(!QFileInfo::exists(s.deploymentDirectory));
        s.boards[0].dataPort=23001;s.boards[0].nodeObject="REPLACE_COBO_NODE_OBJECT";
        QVERIFY_EXCEPTION_THROWN(WriteHardwareDeployment(s,RunConfig::Default()),std::runtime_error);QVERIFY(!QFileInfo::exists(s.deploymentDirectory));
    }
    void mutantSourceParticipatesInStatusBinding() {
        RunConfig c=RunConfig::Default();c.nodes.clear();NodeConfig co;co.id=0;co.nativeSenderId="C0";co.nativeAgentId="A";c.nodes.append(co);NodeConfig mu;mu.id=11;mu.mutant=true;mu.nativeSenderId="M0";mu.nativeAgentId="A";c.nodes.append(mu);
        GetDAQ::EccControlSystemStatus system;system.controller.state=4;system.nodes.resize(1);system.nodes[0].agentId="A";
        GetDAQ::EccControlReceiverStatus one;one.id="C0";system.nodes[0].receivers.push_back(one);one.id="M0";system.nodes[0].receivers.push_back(one);
        const auto rows=BuildNativeSystemStatusRows(c,system,0,false);QCOMPARE(rows.size(),2);QVERIFY(rows[1].sentryOnline);
        system.nodes[0].state="stopped";
        for(auto &receiver:system.nodes[0].receivers){receiver.reaped=true;receiver.sealVerified=true;receiver.exitCode=0;}
        QVERIFY(GetDAQ::NativeReceiverFinalizationError(system,c,false).isEmpty());
        system.nodes[0].receivers[1].id="unexpected";QVERIFY_EXCEPTION_THROWN(BuildNativeSystemStatusRows(c,system,0,false),std::runtime_error);
    }
    void guiGeneratesAndStartsVerifiedServices() {
        if(qEnvironmentVariableIsEmpty("GETDAQ_SETUP_BIN"))QSKIP("Set GETDAQ_SETUP_BIN for local services");
        const auto s=Setup("gui-launch");HardwareSetupWindow window(RunConfig::Default());
        const QMap<QString,QString> fields={{"SetupExperiment",s.experimentFile},{"SetupProfile",s.profileFile},{"SetupPrograms",s.programDirectory},{"SetupRoot",s.deploymentDirectory},{"SetupData",s.dataDirectory},{"SetupCallback","127.0.0.1"}};
        for(auto it=fields.begin();it!=fields.end();++it)window.findChild<QLineEdit *>(it.key())->setText(it.value());
        QTcpServer eccPort,nodePort;QVERIFY(eccPort.listen(QHostAddress::LocalHost,0));QVERIFY(nodePort.listen(QHostAddress::LocalHost,0));
        window.findChild<QSpinBox *>("SetupEccPort")->setValue(eccPort.serverPort());window.findChild<QSpinBox *>("SetupNodePort")->setValue(nodePort.serverPort());eccPort.close();nodePort.close();
        window.findChild<QPushButton *>("SetupLoadBoards")->click();
        auto *table=window.findChild<QTableWidget *>("HardwareBoardTable");
        for(int i=0;i<s.boards.size();++i){const auto &b=s.boards[i];table->item(i,2)->setText(b.address);table->item(i,3)->setText(QString::number(b.controlPort));table->item(i,4)->setText(b.descriptionFile);table->item(i,5)->setText(b.receiverAddress);table->item(i,8)->setText(b.nodeObject);}
        window.findChild<QCheckBox *>("SetupStopped")->setChecked(true);
        window.findChild<QPushButton *>("SetupGenerate")->click();
        auto *launch=window.findChild<QPushButton *>("SetupLaunch");auto *log=window.findChild<QPlainTextEdit *>("HardwareSetupLog");
        QTRY_VERIFY_WITH_TIMEOUT(launch->isEnabled() || log->toPlainText().contains("Setup incomplete"),20000);
        QVERIFY2(launch->isEnabled(),qPrintable(log->toPlainText()));
        RunConfig saved;QString problem;QVERIFY2(saved.Load(s.deploymentDirectory+"/envoy.yml",&problem),qPrintable(problem));QVERIFY(saved.nativeEcc);
        QVERIFY(!QFileInfo::exists(s.deploymentDirectory+"/state/ecc"));
        launch->click();
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(s.deploymentDirectory+"/service-processes.json") || log->toPlainText().contains("Setup incomplete"),15000);
        // Always stop only these test-owned detached processes, even if an assertion later fails.
        struct TestServices {QVector<int> pids;~TestServices(){for(int pid:pids)if(pid>0)::kill(pid,SIGTERM);QTest::qWait(250);}} cleanup;
        if(QFileInfo::exists(s.deploymentDirectory+"/service-processes.json")){
            const auto pids=QJsonDocument::fromJson(ReadHardwareSetupFile(s.deploymentDirectory+"/service-processes.json")).object();cleanup.pids={pids["supervisor_pid"].toInt(),pids["ecc_pid"].toInt()};
            QCOMPARE(ReadHardwareSetupFile(s.deploymentDirectory+"/getdaq-supervisor.pid").trimmed().toLongLong(),qint64(cleanup.pids[0]));
            QCOMPARE(ReadHardwareSetupFile(s.deploymentDirectory+"/getdaq-ecc-server.pid").trimmed().toLongLong(),qint64(cleanup.pids[1]));
            const auto matches=FindLocalServiceProcesses(s.programDirectory+"/getdaq-ecc-server",s.deploymentDirectory+"/ecc.json");
            QCOMPARE(matches.size(),1);QCOMPARE(matches[0].pid,qint64(cleanup.pids[1]));
            QVERIFY(QFile::remove(s.deploymentDirectory+"/getdaq-ecc-server.pid"));
            QCOMPARE(FindLocalServiceProcesses(s.programDirectory+"/getdaq-ecc-server",s.deploymentDirectory+"/ecc.json").size(),1);
            auto reused=matches[0];reused.started+="changed";QVERIFY(!SameLocalServiceProcess(reused));
            QVERIFY(FindLocalServiceProcesses(s.programDirectory+"/getdaq-ecc-server",s.deploymentDirectory+"/supervisor.json").isEmpty());
        }
        QTRY_VERIFY_WITH_TIMEOUT(log->toPlainText().contains("ECC and supervisor identities verified") || log->toPlainText().contains("Setup incomplete"),20000);
        QVERIFY2(log->toPlainText().contains("ECC and supervisor identities verified"),qPrintable(log->toPlainText()));
        QCOMPARE(QDir(s.deploymentDirectory+"/state/ecc/operations").entryList(QDir::Files|QDir::NoDotAndDotDot).size(),0);
        LocalDeploymentServices managed(saved,s.programDirectory);
        QSignalSpy ready(&managed,&LocalDeploymentServices::Ready),failed(&managed,&LocalDeploymentServices::Failed),stopped(&managed,&LocalDeploymentServices::Stopped);
        managed.EnsureStarted();QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty() || !failed.isEmpty(),20000);
        QVERIFY2(failed.isEmpty(),failed.isEmpty()?"":qPrintable(failed[0][0].toString()));QCOMPARE(ready.size(),1);
        QCOMPARE(FindLocalServiceProcesses(s.programDirectory+"/getdaq-ecc-server",s.deploymentDirectory+"/ecc.json")[0].pid,qint64(cleanup.pids[1]));
        managed.StopSafely();QTRY_VERIFY_WITH_TIMEOUT(!stopped.isEmpty() || !failed.isEmpty(),15000);
        QVERIFY2(failed.isEmpty(),failed.isEmpty()?"":qPrintable(failed[0][0].toString()));QCOMPARE(stopped.size(),1);
        QVERIFY(FindLocalServiceProcesses(s.programDirectory+"/getdaq-supervisor",s.deploymentDirectory+"/supervisor.json").isEmpty());
        ready.clear();stopped.clear();managed.EnsureStarted();
        QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty() || !failed.isEmpty(),20000);
        for(const auto &name:{QString("getdaq-ecc-server"),QString("getdaq-supervisor")})
            for(const auto &process:FindLocalServiceProcesses(s.programDirectory+"/"+name,s.deploymentDirectory+(name=="getdaq-ecc-server"?"/ecc.json":"/supervisor.json")))cleanup.pids.append(int(process.pid));
        QVERIFY2(failed.isEmpty(),failed.isEmpty()?"":qPrintable(failed[0][0].toString()));QCOMPARE(ready.size(),1);
        managed.StopSafely();QTRY_VERIFY_WITH_TIMEOUT(!stopped.isEmpty() || !failed.isEmpty(),15000);
        // The persisted backend deliberately requires reconciliation after restart.
        // Identity readiness must never silently clear that hold or terminate it as a clean Run.
        QVERIFY(stopped.isEmpty());QCOMPARE(failed.size(),1);QVERIFY(failed[0][0].toString().contains("confirmed stopped/released"));
        failed.clear();managed.StopExplicitly();QTRY_VERIFY_WITH_TIMEOUT(!stopped.isEmpty() || !failed.isEmpty(),15000);
        QVERIFY2(failed.isEmpty(),failed.isEmpty()?"":qPrintable(failed[0][0].toString()));QCOMPARE(stopped.size(),1);
        QCOMPARE(QDir(s.deploymentDirectory+"/state/ecc/operations").entryList(QDir::Files|QDir::NoDotAndDotDot).size(),0);
        window.findChild<QPushButton *>("SetupUse")->click();QCOMPARE(window.result(),int(QDialog::Accepted));
    }
    void parallelDraftSuggestsIndependentPathsAndPorts() {
        if(qEnvironmentVariableIsEmpty("GETDAQ_SETUP_BIN"))QSKIP("Set GETDAQ_SETUP_BIN for local files");
        const auto saved=CheckedSetup("parallel-draft",4);HardwareSetupWindow editor(saved.console);
        const auto previous=editor.findChild<QLineEdit *>("SetupRoot")->text();
        editor.findChild<QPushButton *>("SetupNewParallelInstance")->click();
        QVERIFY(editor.findChild<QLineEdit *>("SetupRoot")->text()!=previous);
        QVERIFY(editor.findChild<QLineEdit *>("SetupData")->text()!=ReadHardwareDeploymentSetup(saved.directory).dataDirectory);
        const auto table=editor.findChild<QTableWidget *>("HardwareBoardTable");QCOMPARE(table->rowCount(),3);
        for(int row=0;row<table->rowCount();++row)QVERIFY(table->item(row,2)->text().isEmpty());
        QVERIFY(editor.findChild<QSpinBox *>("SetupEccPort")->value()!=saved.console.nativeClient.port);
        QVERIFY(!editor.findChild<QPushButton *>("SetupUse")->isEnabled());
    }
    void reopenedSetupStartsSavedServicesWithoutRegeneration() {
        if(qEnvironmentVariableIsEmpty("GETDAQ_SETUP_BIN"))QSKIP("Set GETDAQ_SETUP_BIN for local services");
        const auto files=CheckedSetup("reopened-launch",8);
        const auto programs=qEnvironmentVariable("GETDAQ_SETUP_BIN");
        struct Cleanup {QString programs,directory;~Cleanup(){for(const auto &name:{QString("getdaq-ecc-server"),QString("getdaq-supervisor")})for(const auto &p:FindLocalServiceProcesses(programs+'/'+name,directory+(name=="getdaq-ecc-server"?"/ecc.json":"/supervisor.json")))if(SameLocalServiceProcess(p))::kill(p.pid,SIGTERM);QTest::qWait(250);}} cleanup{programs,files.directory};
        const auto originalEcc=ReadHardwareSetupFile(files.eccFile);
        HardwareSetupWindow window(files.console);
        auto *launch=window.findChild<QPushButton *>("SetupLaunch");
        auto *log=window.findChild<QPlainTextEdit *>("HardwareSetupLog");
        QVERIFY(launch->isEnabled());
        QVERIFY(!window.findChild<QCheckBox *>("SetupStopped")->isChecked());
        launch->click();
        QTRY_VERIFY_WITH_TIMEOUT(log->toPlainText().contains("Saved setup: ECC and data service identities verified") || log->toPlainText().contains("Setup incomplete"),20000);
        QVERIFY2(log->toPlainText().contains("Saved setup: ECC and data service identities verified"),qPrintable(log->toPlainText()));
        QCOMPARE(ReadHardwareSetupFile(files.eccFile),originalEcc);
        const auto ecc=FindLocalServiceProcesses(programs+"/getdaq-ecc-server",files.eccFile);
        QCOMPARE(ecc.size(),1);
        HardwareSetupWindow reopened(files.console);
        auto *again=reopened.findChild<QPushButton *>("SetupLaunch");QVERIFY(again->isEnabled());again->click();
        auto *againLog=reopened.findChild<QPlainTextEdit *>("HardwareSetupLog");
        QTRY_VERIFY_WITH_TIMEOUT(againLog->toPlainText().contains("Saved setup: ECC and data service identities verified") || againLog->toPlainText().contains("Setup incomplete"),20000);
        QVERIFY2(againLog->toPlainText().contains("Saved setup: ECC and data service identities verified"),qPrintable(againLog->toPlainText()));
        QCOMPARE(FindLocalServiceProcesses(programs+"/getdaq-ecc-server",files.eccFile)[0].pid,ecc[0].pid);
        auto *table=reopened.findChild<QTableWidget *>("HardwareBoardTable");table->item(0,2)->setText("192.0.2.240");
        again->click();QVERIFY(againLog->toPlainText().contains("confirm exclusive control"));
        QCOMPARE(ReadHardwareSetupFile(files.eccFile),originalEcc);
        QCOMPARE(QDir(files.directory+"/state/ecc/operations").entryList(QDir::Files|QDir::NoDotAndDotDot).size(),0);
    }
    void newSetupStartReportsMissingFieldsAndStartsAfterChecks() {
        if(qEnvironmentVariableIsEmpty("GETDAQ_SETUP_BIN"))QSKIP("Set GETDAQ_SETUP_BIN for local services");
        const auto s=Setup("start-new-directly");HardwareSetupWindow window(RunConfig::Default());
        auto *launch=window.findChild<QPushButton *>("SetupLaunch");auto *log=window.findChild<QPlainTextEdit *>("HardwareSetupLog");
        window.findChild<QLineEdit *>("SetupExperiment")->clear();
        QVERIFY(launch->isEnabled());launch->click();QVERIFY(log->toPlainText().contains("Fill in:"));QVERIFY(launch->isEnabled());
        const QMap<QString,QString> fields={{"SetupExperiment",s.experimentFile},{"SetupProfile",s.profileFile},{"SetupPrograms",s.programDirectory},{"SetupRoot",s.deploymentDirectory},{"SetupData",s.dataDirectory},{"SetupCallback","127.0.0.1"}};
        for(auto it=fields.begin();it!=fields.end();++it)window.findChild<QLineEdit *>(it.key())->setText(it.value());
        QTcpServer ecc,node;QVERIFY(ecc.listen(QHostAddress::LocalHost,0));QVERIFY(node.listen(QHostAddress::LocalHost,0));
        window.findChild<QSpinBox *>("SetupEccPort")->setValue(ecc.serverPort());window.findChild<QSpinBox *>("SetupNodePort")->setValue(node.serverPort());ecc.close();node.close();
        window.findChild<QPushButton *>("SetupLoadBoards")->click();auto *table=window.findChild<QTableWidget *>("HardwareBoardTable");
        for(int i=0;i<s.boards.size();++i){const auto &b=s.boards[i];table->item(i,2)->setText(b.address);table->item(i,3)->setText(QString::number(b.controlPort));table->item(i,4)->setText(b.descriptionFile);table->item(i,5)->setText(b.receiverAddress);table->item(i,8)->setText(b.nodeObject);}
        launch->click();QVERIFY(log->toPlainText().contains("confirm exclusive control"));QVERIFY(launch->isEnabled());
        window.findChild<QCheckBox *>("SetupStopped")->setChecked(true);launch->click();
        struct Cleanup {QString programs,directory;~Cleanup(){for(const auto &name:{QString("getdaq-ecc-server"),QString("getdaq-supervisor")})for(const auto &p:FindLocalServiceProcesses(programs+'/'+name,directory+(name=="getdaq-ecc-server"?"/ecc.json":"/supervisor.json")))if(SameLocalServiceProcess(p))::kill(p.pid,SIGTERM);QTest::qWait(250);}} cleanup{s.programDirectory,s.deploymentDirectory};
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QPushButton *>("SetupUse")->isEnabled() || log->toPlainText().contains("Setup incomplete"),25000);
        QVERIFY2(window.findChild<QPushButton *>("SetupUse")->isEnabled(),qPrintable(log->toPlainText()));QVERIFY(launch->isEnabled());
        QCOMPARE(QDir(s.deploymentDirectory+"/state/ecc/operations").entryList(QDir::Files|QDir::NoDotAndDotDot).size(),0);
        launch->click();QTRY_VERIFY_WITH_TIMEOUT(log->toPlainText().contains("Saved setup: ECC and data service identities verified") || log->toPlainText().contains("Setup incomplete"),20000);
        QVERIFY2(log->toPlainText().contains("Saved setup: ECC and data service identities verified"),qPrintable(log->toPlainText()));
        QCOMPARE(FindLocalServiceProcesses(s.programDirectory+"/getdaq-ecc-server",s.deploymentDirectory+"/ecc.json").size(),1);
    }
    void builtinPresetMakesNetworkOnlySetupPossible() {
        const auto preset=BuiltinPedestalsBackendSetup();QCOMPARE(preset.boards.size(),1);QVERIFY(!preset.boards[0].mutant);
        const auto artifact=CompileHardwareSetupExperiment(preset.experimentFile,preset.profileFile);
        QCOMPARE(artifact.settings.cobos.size(),std::size_t(1));QVERIFY(artifact.settings.mutants.empty());
        HardwareSetupWindow window(RunConfig::Default());window.show();QTest::qWait(20);
        QVERIFY(!window.findChild<QLineEdit *>("SetupExperiment")->isVisible());
        QVERIFY(!window.findChild<QLineEdit *>("SetupProfile")->isVisible());
        QVERIFY(window.findChild<QLineEdit *>("SetupData")->isVisible());
        QVERIFY(window.findChild<QLineEdit *>("SetupCallback")->isVisible());
        QCOMPARE(window.findChild<QTableWidget *>("HardwareBoardTable")->rowCount(),1);
        if(!qEnvironmentVariableIsEmpty("GETDAQ_SIMPLE_SETUP_SCREENSHOT"))window.grab().save(qEnvironmentVariable("GETDAQ_SIMPLE_SETUP_SCREENSHOT"));
        if(qEnvironmentVariableIsEmpty("GETDAQ_SETUP_BIN"))return;
        const QString programs=qEnvironmentVariable("GETDAQ_SETUP_BIN"),directory=fTemp.filePath("builtin-start");
        window.findChild<QLineEdit *>("SetupPrograms")->setText(programs);window.findChild<QLineEdit *>("SetupRoot")->setText(directory);window.findChild<QLineEdit *>("SetupData")->setText(directory+"/data");window.findChild<QLineEdit *>("SetupCallback")->setText("127.0.0.1");
        auto *table=window.findChild<QTableWidget *>("HardwareBoardTable");table->item(0,2)->setText("192.0.2.230");table->item(0,5)->setText("127.0.0.1");
        QTcpServer ecc,node;QVERIFY(ecc.listen(QHostAddress::LocalHost,0));QVERIFY(node.listen(QHostAddress::LocalHost,0));window.findChild<QSpinBox *>("SetupEccPort")->setValue(ecc.serverPort());window.findChild<QSpinBox *>("SetupNodePort")->setValue(node.serverPort());ecc.close();node.close();
        window.findChild<QCheckBox *>("SetupStopped")->setChecked(true);
        struct Cleanup {QString programs,directory;~Cleanup(){for(const auto &name:{QString("getdaq-ecc-server"),QString("getdaq-supervisor")})for(const auto &p:FindLocalServiceProcesses(programs+'/'+name,directory+(name=="getdaq-ecc-server"?"/ecc.json":"/supervisor.json")))if(SameLocalServiceProcess(p))::kill(p.pid,SIGTERM);QTest::qWait(250);}} cleanup{programs,directory};
        window.findChild<QPushButton *>("SetupLaunch")->click();auto *log=window.findChild<QPlainTextEdit *>("HardwareSetupLog");
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QPushButton *>("SetupUse")->isEnabled() || log->toPlainText().contains("Setup incomplete"),25000);
        QVERIFY2(window.findChild<QPushButton *>("SetupUse")->isEnabled(),qPrintable(log->toPlainText()));
        QCOMPARE(QDir(directory+"/state/ecc/operations").entryList(QDir::Files|QDir::NoDotAndDotDot).size(),0);
    }
    void oneManagerStartsTwoBackendInstancesWithoutAnotherGui() {
        if(qEnvironmentVariableIsEmpty("GETDAQ_SETUP_BIN"))QSKIP("Set GETDAQ_SETUP_BIN for local services");
        const auto first=CheckedSetup("manager-a",10),second=CheckedSetup("manager-b",11);RememberLocalBackend(second.console.filePath);
        const auto programs=qEnvironmentVariable("GETDAQ_SETUP_BIN");
        struct Cleanup {QString programs;QStringList dirs;~Cleanup(){for(const auto &dir:dirs)for(const auto &name:{QString("getdaq-ecc-server"),QString("getdaq-supervisor")})for(const auto &p:FindLocalServiceProcesses(programs+'/'+name,dir+(name=="getdaq-ecc-server"?"/ecc.json":"/supervisor.json")))if(SameLocalServiceProcess(p))::kill(p.pid,SIGTERM);QTest::qWait(250);}} cleanup{programs,{first.directory,second.directory}};
        LocalServicesWindow window(first.console);auto *instances=window.findChild<QComboBox *>("BackendInstances");
        auto *start=window.findChild<QPushButton *>("BackendAction0");auto *log=window.findChild<QPlainTextEdit *>("ServiceLaunchLog");
        QVERIFY(instances->findData(second.console.filePath)>=0);
        start->click();QTRY_VERIFY_WITH_TIMEOUT(start->isEnabled(),20000);
        QVERIFY2(log->toPlainText().contains("ECC and data service identities verified"),qPrintable(log->toPlainText()));
        log->clear();instances->setCurrentIndex(instances->findData(second.console.filePath));QCOMPARE(window.Configuration().filePath,second.console.filePath);
        start->click();QTRY_VERIFY_WITH_TIMEOUT(start->isEnabled(),20000);
        QVERIFY2(log->toPlainText().contains("ECC and data service identities verified"),qPrintable(log->toPlainText()));
        for(const auto &dir:{first.directory,second.directory})for(const auto &name:{QString("getdaq-ecc-server"),QString("getdaq-supervisor")})QCOMPARE(FindLocalServiceProcesses(programs+'/'+name,dir+(name=="getdaq-ecc-server"?"/ecc.json":"/supervisor.json")).size(),1);
        window.findChild<QPushButton *>("UseBackendInstance")->click();QCOMPARE(window.result(),int(QDialog::Accepted));QCOMPARE(window.Configuration().filePath,second.console.filePath);
    }
    void parallelBackendsRemainIndependent() {
        if(qEnvironmentVariableIsEmpty("GETDAQ_SETUP_BIN"))QSKIP("Set GETDAQ_SETUP_BIN for local services");
        const auto a=CheckedSetup("parallel-a",0),b=CheckedSetup("parallel-b",1),conflict=CheckedSetup("parallel-overlap",2,0),portConflict=CheckedSetup("parallel-port-overlap",3,3,0);
        const auto programs=qEnvironmentVariable("GETDAQ_SETUP_BIN");
        struct Cleanup {QString programs;QStringList directories;~Cleanup(){for(const auto &dir:directories)for(const auto &name:{QString("getdaq-ecc-server"),QString("getdaq-supervisor")})for(const auto &p:FindLocalServiceProcesses(programs+'/'+name,dir+(name=="getdaq-ecc-server"?"/ecc.json":"/supervisor.json")))if(SameLocalServiceProcess(p))::kill(p.pid,SIGTERM);QTest::qWait(250);}} cleanup{programs,{a.directory,b.directory,conflict.directory,portConflict.directory}};
        LocalDeploymentServices first(a.console,programs),second(b.console,programs),blocked(conflict.console,programs);
        QSignalSpy ar(&first,&LocalDeploymentServices::Ready),af(&first,&LocalDeploymentServices::Failed),as(&first,&LocalDeploymentServices::Stopped);
        QSignalSpy br(&second,&LocalDeploymentServices::Ready),bf(&second,&LocalDeploymentServices::Failed),bs(&second,&LocalDeploymentServices::Stopped);
        QSignalSpy cr(&blocked,&LocalDeploymentServices::Ready),cf(&blocked,&LocalDeploymentServices::Failed),cs(&blocked,&LocalDeploymentServices::Stopped);
        first.EnsureStarted();QTRY_VERIFY_WITH_TIMEOUT(!ar.isEmpty()||!af.isEmpty(),20000);QVERIFY2(af.isEmpty(),af.isEmpty()?"":qPrintable(af[0][0].toString()));QCOMPARE(ar.size(),1);
        second.EnsureStarted();QTRY_VERIFY_WITH_TIMEOUT(!br.isEmpty()||!bf.isEmpty(),20000);QVERIFY2(bf.isEmpty(),bf.isEmpty()?"":qPrintable(bf[0][0].toString()));QCOMPARE(br.size(),1);
        const auto peer=FindLocalServiceProcesses(programs+"/getdaq-ecc-server",b.eccFile);QCOMPARE(peer.size(),1);
        const auto dataPeer=FindLocalServiceProcesses(programs+"/getdaq-supervisor",b.supervisorFile);QCOMPARE(dataPeer.size(),1);
        blocked.EnsureStarted();QTRY_VERIFY_WITH_TIMEOUT(!cr.isEmpty()||!cf.isEmpty(),20000);QVERIFY(cr.isEmpty());QVERIFY(cf[0][0].toString().contains("Another ECC already controls"));
        blocked.StopExplicitly();QTRY_COMPARE_WITH_TIMEOUT(cs.size(),1,10000);
        LocalDeploymentServices badPort(portConflict.console,programs);QSignalSpy pf(&badPort,&LocalDeploymentServices::Failed);
        badPort.EnsureStarted();QTRY_COMPARE_WITH_TIMEOUT(pf.size(),1,10000);QVERIFY(pf[0][0].toString().contains("Local port"));
        QVERIFY(FindLocalServiceProcesses(programs+"/getdaq-supervisor",portConflict.supervisorFile).isEmpty());
        QVERIFY(SameLocalServiceProcess(peer[0]));QVERIFY(SameLocalServiceProcess(dataPeer[0]));
        first.StopSafely();QTRY_VERIFY_WITH_TIMEOUT(!as.isEmpty()||!af.isEmpty(),15000);QCOMPARE(as.size(),1);
        QVERIFY(SameLocalServiceProcess(peer[0]));QVERIFY(SameLocalServiceProcess(dataPeer[0]));
        second.StopSafely();QTRY_VERIFY_WITH_TIMEOUT(!bs.isEmpty()||!bf.isEmpty(),15000);QCOMPARE(bs.size(),1);
    }
    void acceptedWarningPolicyIsScopedAndSaved() {
        if(qEnvironmentVariableIsEmpty("GETDAQ_SETUP_BIN"))QSKIP("Set GETDAQ_SETUP_BIN for files");
        auto setup=Setup("accepted-warning");setup.acceptLegacyDelayReadback=true;
        const auto files=WriteHardwareDeployment(setup,RunConfig::Default());
        const auto policy=QJsonDocument::fromJson(ReadHardwareSetupFile(files.directory+"/readback-warning-policy.json")).object();
        QCOMPARE(policy["kind"].toString(),QString("getdaq.read-data-delay-warning"));QVERIFY(policy["accepted"].toBool());QCOMPARE(policy["accepted_readback"].toString(),QString("255"));
        QCOMPARE(policy["targets"].toArray().size(),2);QVERIFY(policy["targets"].toArray()[0].toObject()["node_proxy"].toString().contains(setup.boards[0].address));
        QVERIFY(ReadHardwareDeploymentSetup(files.directory).acceptLegacyDelayReadback);
    }
    void onlyOriginalEarlyDelayFailureCanUseIdleRecovery() {
        const auto digest=QString(64,'a');
        QJsonObject controller{{"state",2},{"transition",0},{"error",2},{"outcome_unknown",true},{"needs_reconciliation",true},{"operation_id","old-prepare"},{"message","delay mismatch"}};
        QJsonObject data{{"needs_reconciliation",false},{"controller_scope_sha256",digest},{"controller_epoch",digest},{"message",""},{"nodes",QJsonArray()}};
        QJsonObject status{{"schema",1},{"service","getdaq-ecc-server"},{"instance_id",digest},{"snapshot_sha256",digest},{"operation_active",false},{"operation_id",""},{"client_epoch",digest},{"control_needs_reconciliation",true},{"message",""},{"controller",controller},{"data",data}};
        auto fixture=[&](const QString &name,const QString &reg,bool subscribed) {
            const auto root=fTemp.filePath(name),audit=root+"/audit",co=audit+"/operations/old-prepare/prepare/cobo-0";QDir().mkpath(co);
            const auto topology=root+"/topology.json";
            WriteHardwareSetupFile(topology,QJsonDocument(QJsonObject{{"cobos",QJsonArray{QJsonObject{{"id","0"},{"enabled",true},{"node_proxy","HwNode:tcp -h 192.0.2.10 -p 46001"}}}},{"mutants",QJsonArray()}}).toJson());
            WriteHardwareSetupFile(root+"/ecc.json",QJsonDocument(QJsonObject{{"audit_root",audit},{"topology",QJsonObject{{"path",topology}}}}).toJson());
            QJsonObject write{{"operation","write"},{"device","ctrl"},{"register",reg},{"field","AGET0"},{"value","14"},{"mask","255"}};auto expect=write;expect["operation"]="expect";
            auto lines=[](const QJsonArray &rows){QByteArray out;for(const auto &row:rows)out+=QJsonDocument(row.toObject()).toJson(QJsonDocument::Compact)+'\n';return out;};
            WriteHardwareSetupFile(co+"/before-subscription.jsonl",lines({QJsonObject{{"phase","intent"},{"steps",QJsonArray{write,expect}}},QJsonObject{{"step",0},{"phase","acknowledged"}},QJsonObject{{"step",1},{"phase","mismatch"},{"actual","255"}}}));
            QJsonArray alarms{QJsonObject{{"phase","listener_prepared"}}};if(subscribed)alarms.append(QJsonObject{{"phase","subscribed"}});
            WriteHardwareSetupFile(co+"/alarms.jsonl",lines(alarms));
            WriteHardwareSetupFile(co+"/prepare.jsonl",lines({QJsonObject{{"phase","intent"}},QJsonObject{{"phase","preflight_complete"}},QJsonObject{{"phase","stopped"},{"full_system_prepared",false}}}));
            WriteHardwareSetupFile(audit+"/operations/old-prepare/prepare/system.jsonl",lines({QJsonObject{{"phase","failed"}}}));return root;
        };
        const auto good=fixture("early-delay","readDataDelay",false);
        const auto proof=ValidateEarlyDelayWarningEvidence(good,QJsonDocument(status).toJson());QCOMPARE(proof["actual"].toString(),QString("255"));QCOMPARE(proof["original_files"].toObject().size(),4);
        const auto critical=fixture("critical-stop","pipeCtrl",false);QVERIFY_EXCEPTION_THROWN(ValidateEarlyDelayWarningEvidence(critical,QJsonDocument(status).toJson()),std::runtime_error);
        const auto subscribed=fixture("late-subscription","readDataDelay",true);QVERIFY_EXCEPTION_THROWN(ValidateEarlyDelayWarningEvidence(subscribed,QJsonDocument(status).toJson()),std::runtime_error);
        status["operation_active"]=true;status["operation_id"]="in-progress";QVERIFY_EXCEPTION_THROWN(ValidateEarlyDelayWarningEvidence(good,QJsonDocument(status).toJson()),std::runtime_error);
        status["operation_active"]=false;status["operation_id"]="";controller["state"]=1;controller["error"]=0;controller["outcome_unknown"]=false;controller["operation_id"]="";status["controller"]=controller;
        WriteHardwareSetupFile(good+"/audit/ecc.jsonl","{\"phase\":\"known_idle_checkpoint\"}\n");
        const auto idle=ValidateEarlyDelayWarningEvidence(good,QJsonDocument(status).toJson());QCOMPARE(idle["kind"].toString(),QString("getdaq.empty-idle-baseline"));QVERIFY(!idle["requires_compatibility_warning"].toBool());
    }
    void receiverProcessRequiresMatchingPlan() {
        const auto root=fTemp.filePath("receiver-scope"),exe=root+"/runtime-old/bin/getdaq-receiver";
        QVERIFY(QDir().mkpath(QFileInfo(exe).absolutePath()));QVERIFY(QFile::copy("/usr/bin/python3",exe));
        const auto output=root+"/data/session";QVERIFY(QDir().mkpath(output));
        const QString configuration(64,'a'),artifact(64,'b');
        QJsonObject plan{{"configuration_sha256",configuration},{"artifact_sha256",artifact},{"sources",QJsonArray{QJsonObject{{"id","cobo0"},{"bind","127.0.0.1"},{"data_port",23001},{"output_root",root+"/data"}}}}};
        const auto config=root+"/supervisor.json";WriteHardwareSetupFile(config,QJsonDocument(QJsonObject{{"receiver_plan",plan}}).toJson());
        QProcess process;process.start(exe,{"-c","import time;time.sleep(30)","--output",output,"--bind","127.0.0.1","--port","23001","--sender-id","cobo0","--configuration-sha256",configuration,"--artifact-sha256",artifact});
        QVERIFY(process.waitForStarted());QTRY_COMPARE(FindLocalReceiverProcesses(exe,config).size(),1);
        plan["artifact_sha256"]=QString(64,'c');const auto other=root+"/another-supervisor.json";WriteHardwareSetupFile(other,QJsonDocument(QJsonObject{{"receiver_plan",plan}}).toJson());
        QVERIFY(FindLocalReceiverProcesses(exe,other).isEmpty());
        process.terminate();QVERIFY(process.waitForFinished(3000));
    }
    void siteEnvironmentUsesOnlyAllowedDefaults() {
        const auto root=fTemp.filePath("site-environment");QVERIFY(QDir().mkpath(root+"/site"));
        struct Restore {bool present=qEnvironmentVariableIsSet("GETDAQ_COBO_IPV4");QByteArray value=qgetenv("GETDAQ_COBO_IPV4");~Restore(){if(present)qputenv("GETDAQ_COBO_IPV4",value);else qunsetenv("GETDAQ_COBO_IPV4");}} restore;
        const auto file=root+"/site/runtime-environment.json";WriteHardwareSetupFile(file,"{\"GETDAQ_COBO_IPV4\":\"192.0.2.8\"}");QString error;
        qputenv("GETDAQ_COBO_IPV4","192.0.2.9");QVERIFY(LoadLocalSiteEnvironment(root,&error));QCOMPARE(qgetenv("GETDAQ_COBO_IPV4"),QByteArray("192.0.2.9"));
        qunsetenv("GETDAQ_COBO_IPV4");QVERIFY(LoadLocalSiteEnvironment(root,&error));QCOMPARE(qgetenv("GETDAQ_COBO_IPV4"),QByteArray("192.0.2.8"));
        QVERIFY(QFile::remove(file));WriteHardwareSetupFile(file,"{\"PATH\":\"bad\"}");const auto path=qgetenv("PATH");
        QVERIFY(!LoadLocalSiteEnvironment(root,&error));QCOMPARE(qgetenv("PATH"),path);
    }
    void binaryRaisesExistingInstance() {
        QWidget window;ApplicationInstance first(fTemp.filePath("single-gui"));QVERIFY2(first.Acquire(),qPrintable(first.Error()));first.SetWindow(&window);
        ApplicationInstance duplicate(fTemp.filePath("single-gui"));QVERIFY(!duplicate.Acquire());QVERIFY(duplicate.Error().isEmpty());
        QTRY_VERIFY(window.isVisible());
        ApplicationInstance separate(fTemp.filePath("another-installation"));QVERIFY(separate.Acquire());
        ApplicationInstance otherConfig(fTemp.filePath("single-gui"));QVERIFY(otherConfig.Acquire(fTemp.filePath("b.yml")));
        QVERIFY(!first.Acquire(fTemp.filePath("b.yml")));QVERIFY(first.Error().isEmpty());
        ApplicationInstance stillFirst(fTemp.filePath("single-gui"));QVERIFY(!stillFirst.Acquire());
        QVERIFY(otherConfig.Acquire(fTemp.filePath("c.yml")));
        ApplicationInstance released(fTemp.filePath("single-gui"));QVERIFY(released.Acquire(fTemp.filePath("b.yml")));
    }
    void findsEarlierRuntimeWithoutPidRecord() {
        const auto root=fTemp.filePath("runtime-scope");
        const auto old=root+"/runtime-old/bin/getdaq-ecc-server",current=root+"/runtime-new/bin/getdaq-ecc-server";
        QVERIFY(QDir().mkpath(QFileInfo(old).absolutePath()));QVERIFY(QDir().mkpath(QFileInfo(current).absolutePath()));
        QVERIFY(QFile::copy("/usr/bin/python3",old));QVERIFY(QFile::copy("/usr/bin/python3",current));
        const auto config=root+"/ecc.json";WriteHardwareSetupFile(config,"{}");
        QProcess process;process.start(old,{"-c","import time; time.sleep(30)","--config",config});
        QVERIFY(process.waitForStarted());
        QTRY_COMPARE(FindLocalServiceProcesses(current,config).size(),1);
        const auto match=FindLocalServiceProcesses(current,config)[0];QCOMPARE(match.pid,process.processId());
        QVERIFY(SameLocalServiceProcess(match));auto changed=match;changed.started+="different";QVERIFY(!SameLocalServiceProcess(changed));
        process.terminate();QVERIFY(process.waitForFinished(3000));QVERIFY(!SameLocalServiceProcess(match));
    }
    void guiEntryAndBoardMapping() {
        IPWindow parent(RunConfig::Default());QVERIFY(parent.findChild<QPushButton *>("OpenHardwareSetup"));
        HardwareSetupWindow window(RunConfig::Default());const auto s=Setup("gui-site");
        window.findChild<QLineEdit *>("SetupExperiment")->setText(s.experimentFile);window.findChild<QLineEdit *>("SetupProfile")->setText(s.profileFile);
        window.findChild<QPushButton *>("SetupLoadBoards")->click();const auto table=window.findChild<QTableWidget *>("HardwareBoardTable");QCOMPARE(table->rowCount(),3);
        QVERIFY(table->item(1,1)->text().contains("disabled"));QVERIFY(table->item(2,1)->text().contains("mutant"));
        QVERIFY(table->item(2,5)->flags()&Qt::ItemIsEditable);
        window.findChild<QPushButton *>("SetupGenerate")->click();QVERIFY(window.findChild<QPlainTextEdit *>("HardwareSetupLog")->toPlainText().contains("exclusive control"));
        QVERIFY(!QFileInfo::exists(s.deploymentDirectory));
        if(!qEnvironmentVariableIsEmpty("GETDAQ_SETUP_SCREENSHOT")){window.show();QTest::qWait(100);window.grab().save(qEnvironmentVariable("GETDAQ_SETUP_SCREENSHOT"));}
    }
};
QTEST_MAIN(HardwareSetupTests)
#include "test_hardwaresetup.moc"
