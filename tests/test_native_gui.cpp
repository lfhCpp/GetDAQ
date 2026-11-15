#include "source_observation_fixture.h"
#include "receiver_failure_fixture.h"
#include "runcontrol.h"
#include "runrecorder.h"
#include "eccclientstore.h"
#include "httpserver.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
namespace {
class Fixture : public QObject {
public:
    QTemporaryDir directory;
    HttpServer server;
    RunConfig config;
    int state=1,cycle=0,statusDelayMs=0,catalogPostDelayMs=0;
    bool catalogPostPending=false,catalogStatusHeld=false;
    bool dataStopped=false,failSeal=false,dropStart=false,catalogued=false,failReplacementCatalog=false;
    QString failedReplacementCatalogId;
    bool unusedReceiver=false,iceSourceEnded=false;
    bool alarmHeld=false;
    bool holdStatusResponse=false;
    QPointer<HttpConnection> heldStatusConnection;
    QByteArray heldStatusBody;
    QByteArray receiverFailureBody;
    QString receiverNodeInstance=QString(64,'5');
    bool receiverFailureAvailable=true;
    int receiverFailureDelayMs=0;
    bool configurationAvailable=true;
    bool receiverAvailable=true,receiverBusy=false;
    bool automaticMetrics=false,metricsUnavailable=false,wrongMetricSession=false;
    quint64 metricBytes=0;
    QString latestOperation;
    int receiverDelayMs=0;
    int sourceDelayMs=0;
    QString sourceState="running",sourcePhase="available",sourceSession,sourceOperation;
    int receiverExitCode=1;
    QString receiverState="running";
    bool configurationRecordedBeforeStart=true;
    bool maintenanceEnabled=false,dropMaintenanceReply=false;
    QString maintenancePhase="complete";
    int maintenanceResultState=3;
    QString repositoryId=QString(64,'9');
    QByteArray token=QByteArray(64,'f');
    QStringList trace;
    QMap<QString,int> commandPosts;
    QString instance=QString(64,'a'),received="24576",frames="6",files="1";
    QMap<QString,GetDAQ::EccControlOperation> operations;
    Fixture() {
        if (!server.listen(QHostAddress::LocalHost,0)) throw std::runtime_error("Native GUI fixture listen");
        config=RunConfig::Default(); config.simulation=false; config.nativeEcc=true; config.experiment="NativeTest";
        config.outputDir=directory.filePath("runs"); config.configDir=directory.filePath("unused-legacy-config"); config.nativeRecordDir=directory.filePath("records");
        config.pollMs=60; config.timeoutMs=500; config.stepTimeoutMs=3000;
        auto &client=config.nativeClient; client.address="127.0.0.1"; client.port=server.serverPort(); client.serviceId="test-ecc"; client.clientId="test-console";
        client.clientEpoch=QString(64,'d'); client.snapshotSha256=QString(64,'b'); client.inputBindingSha256=QString(64,'c'); client.timeoutMs=500;
        client.credentialFile=directory.filePath("credential"); QFile credential(client.credentialFile);
        if (!credential.open(QIODevice::WriteOnly) || credential.write(token+'\n')!=65) throw std::runtime_error("Fixture credential");
        credential.close(); credential.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
        config.nodes.resize(2); for (int i=0;i<2;++i) { auto &node=config.nodes[i]; node.id=i; node.mutant=false; node.nativeSenderId=QString("C%1").arg(i); node.nativeAgentId=QString("A%1").arg(i); }
        connect(&server,&HttpServer::RequestSignal,this,[this](const QString &method,const QString &path,const QByteArray &body,HttpConnection *connection) {
            if (connection->RequestHeader("authorization")!="Bearer "+token) { connection->Reply(401,"{}"); return; }
            if (method=="GET" && path=="/v1/identity") { connection->Reply(200,Identity()); return; }
            if (method=="GET" && path=="/v1/cobo-maintenance-targets" && maintenanceEnabled) {
                GetDAQ::EccControlMaintenanceTargets targets;
                targets.instanceId=instance.toStdString();targets.snapshotSha256=config.nativeClient.snapshotSha256.toStdString();
                targets.configurationSha256=std::string(64,'1');targets.artifactSha256=std::string(64,'2');
                targets.cobos.push_back({"C0","CoBo0"});targets.cobos.push_back({"C1","CoBo1"});
                connection->Reply(200,QByteArray::fromStdString(GetDAQ::EncodeEccControlMaintenanceTargets(targets)));return;
            }
            if(method=="GET" && path=="/v1/sources") {
                std::string rows;
                const auto session=sourceSession.isEmpty() ? QString(64,QChar(cycle%10+'0')) : sourceSession;
                if(cycle)for(int i=0;i<2;++i) {if(!rows.empty())rows+=',';rows+=SourceObservationFixture::Row("C"+std::to_string(i),session.toStdString(),sourceState.toStdString(),sourcePhase.toStdString());}
                auto response=QByteArray::fromStdString(SourceObservationFixture::Root(rows,instance.toStdString(),config.nativeClient.snapshotSha256.toStdString()));
                response.replace("\"controller_operation_id\":\"\"",QByteArray("\"controller_operation_id\":")+QByteArray::fromStdString(GetDAQ::EncodeJsonString(sourceOperation.toStdString())));
                if(sourceDelayMs) {const QPointer<HttpConnection> target(connection);QTimer::singleShot(sourceDelayMs,this,[target,response] {if(target)target->Reply(200,response);});}
                else connection->Reply(200,response);
                return;
            }
            if(method=="GET" && path.startsWith("/v1/receiver-file-bytes/")) {
                if(!automaticMetrics || metricsUnavailable){connection->Reply(404,"{}");return;}
                const auto source=QString::fromUtf8(QByteArray::fromHex(path.mid(QString("/v1/receiver-file-bytes/").size()).toLatin1()));
                const int index=source=="C0" ? 0 : source=="C1" ? 1 : -1;
                if(index<0){connection->Reply(404,"{}");return;}
                if(state==5)metricBytes+=1000000;
                QJsonObject metrics{{"agent_id",QString("A%1").arg(index)},{"plan_sha256",QString(64,'4')},{"instance_id",receiverNodeInstance},
                    {"source_id",source},{"session_id",wrongMetricSession ? QString(64,'9') : QString(64,QChar(cycle%10+'0'))},{"pid",100+index},
                    {"available",true},{"accepted_file_bytes",QString::number(metricBytes)},{"observed_at_unix_ms",QString::number(QDateTime::currentMSecsSinceEpoch())}};
                connection->Reply(200,QJsonDocument(QJsonObject{{"schema",1},{"kind","getdaq.ecc-receiver-file-bytes"},{"instance_id",instance},
                    {"snapshot_sha256",config.nativeClient.snapshotSha256},{"data_binding_sha256",QString(64,'3')},{"events",metrics}}).toJson(QJsonDocument::Compact));return;
            }
            if (method=="GET" && path=="/v1/receivers") {
                const auto response=ReceiverObservation();
                if (receiverDelayMs) {const QPointer<HttpConnection> target(connection); QTimer::singleShot(receiverDelayMs,this,[target,response] {if(target) target->Reply(200,response);});}
                else connection->Reply(200,response);
                return;
            }
            if (method=="GET" && path=="/v1/configuration") {
                if (!configurationAvailable) {connection->Reply(404,"{}");return;}
                GetDAQ::EccControlConfiguration c;c.instanceId=instance.toStdString();c.snapshotSha256=config.nativeClient.snapshotSha256.toStdString();c.inputBindingSha256=config.nativeClient.inputBindingSha256.toStdString();
                c.configurationSha256=std::string(64,'1');c.artifactSha256=std::string(64,'2');c.hasSelection=true;c.selection.registrationSha256=std::string(64,'3');
                auto &r=c.selection.registration;r.repositoryId=repositoryId.toStdString();r.publicationSha256=r.effectiveSha256=r.snapshotSha256=std::string(64,'4');
                r.configurationSha256=c.configurationSha256;r.artifactSha256=c.artifactSha256;r.manifestFile="/fixture-config/version/publication.json";
                connection->Reply(200,QByteArray::fromStdString(GetDAQ::EncodeEccControlConfiguration(c)));return;
            }
            if (method=="GET" && path=="/v1/receiver-failure") {
                auto response=receiverFailureBody.isEmpty() ? ReceiverFailureFixture::Waiting(instance,config.nativeClient.snapshotSha256) : receiverFailureBody;
                if (!receiverFailureAvailable) {connection->Reply(503,"{}");return;}
                if (receiverFailureDelayMs) {const QPointer<HttpConnection> target(connection);QTimer::singleShot(receiverFailureDelayMs,this,[target,response] {if(target)target->Reply(200,response);});}
                else connection->Reply(200,response);
                return;
            }
            if (method=="GET" && path=="/v1/alarms") {
                QJsonObject node{{"id","C0"},{"subscription",2},{"received","9007199254740993"},{"durable","9007199254740993"},{"lost","0"},{"invalid","0"},{"pending",1},
                    {"alarm_mask",alarmHeld ? 1 : 0},{"stop_requested",alarmHeld},{"queue_overflow",false},{"audit_failed",false},{"budget_exceeded",false}};
                QJsonObject observation{{"controller_state",state},{"controller_outcome_unknown",false},{"controller_operation_id",""},{"stop_requested",alarmHeld},{"nodes",QJsonArray{node}}};
                QJsonObject alarm{{"schema",1},{"kind","getdaq.ecc-alarms"},{"instance_id",instance},{"snapshot_sha256",config.nativeClient.snapshotSha256},{"enabled",true},{"latched",alarmHeld},
                    {"operation_id",alarmHeld ? "internal-alarm-fixture" : ""},{"phase",alarmHeld ? "complete" : "watching"},{"observation",observation}};
                connection->Reply(200,QJsonDocument(alarm).toJson(QJsonDocument::Compact)); return;
            }
            if (method=="GET" && path=="/v1/status") {
                const auto status=Status();
                if(holdStatusResponse){heldStatusConnection=connection;heldStatusBody=status;return;}
                if (catalogPostPending) {
                    catalogStatusHeld=true;
                    QTimer::singleShot(catalogPostDelayMs*2,connection,[connection,status] { connection->Reply(200,status); });
                } else if (statusDelayMs && state==5) QTimer::singleShot(statusDelayMs,connection,[connection,status] { connection->Reply(200,status); });
                else connection->Reply(200,status);
                return;
            }
            if (method=="GET" && path.startsWith("/v1/operations/")) {
                const auto id=path.mid(QString("/v1/operations/").size()); if (!operations.contains(id)) { connection->Reply(404,"{}"); return; }
                connection->Reply(200,QByteArray::fromStdString(GetDAQ::EncodeEccControlOperation(operations[id]))); return;
            }
            if (method!="POST" || path!="/v1/operations") { connection->Reply(404,"{}"); return; }
            const auto request=GetDAQ::DecodeEccControlRequest(body.toStdString()); const auto id=QString::fromStdString(request.operationId);
            ++commandPosts[QString::fromStdString(request.command)];
            if (request.command=="start" && config.nativeClient.configurationStatusEnabled) {
                QJsonObject journal;QString error;
                configurationRecordedBeforeStart=configurationRecordedBeforeStart && RunRecorder::ReadJournal(config,&journal,&error) && !journal.value("native_configuration").toString().isEmpty();
            }
            const auto apply=[this,request,id,connection] {
            catalogPostPending=false;
            if (!operations.contains(id)) {
                const auto command=QString::fromStdString(request.command); trace.append(command);if(automaticMetrics)latestOperation=id;
                const bool maintenance=QStringList({"reprepare-cobo","configure-cobo","set-cobo-led","blink-cobo-leds",
                    "flip-cobo-led","pulse-cobo-led","trigger-asad-pulser"}).contains(command);
                if (maintenance) {
                    if(!maintenanceEnabled || state!=3 || request.coboId!="C1")
                        throw std::runtime_error("Fixture maintenance requires selected C1 in Prepared");
                } else if (command=="describe") state=2; else if (command=="prepare") state=3;
                else if (command=="configure") { state=4; ++cycle; dataStopped=false; catalogued=false; if(automaticMetrics){metricBytes=0;received="0";frames="0";files="0";} }
                else if (command=="start" || command=="resume") state=5;
                else if (command=="pause") state=6; else if (command=="stop") state=4;
                else if (command=="breakup") { dataStopped=true; if (!failSeal) state=3; }
                else if (command=="reset") --state;
                else if (command=="catalog") {
                    if (state!=3 || !dataStopped) throw std::runtime_error("Fixture catalog requires stopped/Prepared");
                    if(request.catalogReplaceExisting && failReplacementCatalog) {failedReplacementCatalogId=id;catalogued=false;}
                    else catalogued=true;
                } else if(command=="resume-catalog-replacement") {
                    if(request.failedOperationId!=failedReplacementCatalogId.toStdString() || !failReplacementCatalog)
                        throw std::runtime_error("Fixture recovery must bind the exact failed replacement");
                    failReplacementCatalog=false;catalogued=true;
                }
                GetDAQ::EccControlOperation operation; operation.request=request; operation.requestKey=GetDAQ::EccControlRequestKey(request);
                operation.phase=maintenance ? maintenancePhase.toStdString() : command=="breakup" && failSeal ? "failed" :
                    command=="catalog" && request.catalogReplaceExisting && failReplacementCatalog ? "failed" : "complete";
                if(maintenance && operation.phase=="failed")operation.message="Fixture selected CoBo maintenance failed";
                if(operation.phase=="failed" && command=="catalog")operation.message="Fixture failed the replacement catalog before publication";
                GetDAQ::EccControlControllerStatus status; status.state=unsigned(state); status.operationId=request.operationId; status.outcomeUnknown=command=="breakup" && failSeal; status.needsReconciliation=status.outcomeUnknown;
                if(maintenance)status.state=unsigned(maintenanceResultState);
                operation.resultJson=GetDAQ::EncodeEccControlControllerStatus(status); operations[id]=operation;
            }
            if (dropStart && request.command=="start") { connection->findChild<QTcpSocket *>()->disconnectFromHost(); return; }
            if (dropMaintenanceReply && request.command=="flip-cobo-led") { connection->findChild<QTcpSocket *>()->disconnectFromHost(); return; }
            connection->Reply(202,QByteArray::fromStdString(GetDAQ::EncodeEccControlOperation(operations[id])));
            };
            if (request.command=="catalog" && catalogPostDelayMs) {
                catalogPostPending=true; QTimer::singleShot(catalogPostDelayMs,connection,apply);
            } else apply();
        });
    }
    QByteArray Identity() const {
        QJsonObject identity{{"schema",1},{"service","getdaq-ecc-server"},{"service_id",config.nativeClient.serviceId},{"client_id",config.nativeClient.clientId},
            {"instance_id",instance},{"snapshot_sha256",config.nativeClient.snapshotSha256},{"input_binding_sha256",config.nativeClient.inputBindingSha256},
            {"configuration_sha256",QString(64,'1')},{"artifact_sha256",QString(64,'2')},{"data_binding_sha256",QString(64,'3')}};
        return QJsonDocument(identity).toJson(QJsonDocument::Compact);
    }
    QByteArray Status() const {
        GetDAQ::EccControlControllerStatus controller; controller.state=unsigned(state);if(automaticMetrics)controller.operationId=latestOperation.toStdString();  controller.outcomeUnknown=controller.needsReconciliation=failSeal&&dataStopped;
        QJsonArray nodes;
        for (int i=0;i<2;++i) {
            QJsonArray receivers; const auto session=QString(64,QChar(cycle%10+'0'));
            if (cycle>0) receivers.append(QJsonObject{{"id",QString("C%1").arg(i)},{"session_id",session},{"output_directory",QString("/fixture-data/session-%1/C%2").arg(cycle).arg(i)},
                {"pid",100+i},{"exit_code",dataStopped?0:-1},{"reaped",dataStopped},{"seal_verified",dataStopped&&!failSeal},{"received_bytes",received},{"frames",frames},{"files",files}});
            if (unusedReceiver && !receivers.isEmpty()) {auto receiver=receivers[0].toObject();receiver["source_unused_confirmed"]=true;receivers[0]=receiver;}
            if (iceSourceEnded && !receivers.isEmpty()) {auto receiver=receivers[0].toObject();receiver["source_end_confirmed"]=true;receivers[0]=receiver;}
            QJsonObject life{{"schema",1},{"state",cycle==0?"fresh":dataStopped?"stopped":"ready"},{"needs_reconciliation",failSeal&&dataStopped},
                {"plan_sha256",QString(64,'4')},{"session_id",cycle?session:QString()},{"message",""},{"receivers",receivers}};
            QJsonObject node{{"agent_id",QString("A%1").arg(i)},{"instance_id",receiverNodeInstance},{"plan_sha256",QString(64,'4')},{"lifecycle",life}};
            if (catalogued) {
                GetDAQ::EccControlCatalogReceipt receipt; receipt.operationId="catalog-node-"+std::to_string(i); receipt.requestKey=receipt.manifestSha256=std::string(64,'6');
                receipt.directory="/fixture-catalog/Test/run_0000/node-"+std::to_string(i); receipt.linkedFiles=1;
                node["catalog"]=QJsonDocument::fromJson(QByteArray::fromStdString(GetDAQ::EncodeEccControlCatalogReceipt(receipt))).object();
            }
            nodes.append(node);
        }
        QJsonObject data{{"needs_reconciliation",failSeal&&dataStopped},{"controller_scope_sha256",QString(64,'6')},{"controller_epoch",QString(64,'7')},{"message",""},{"nodes",nodes}};
        QJsonObject status{{"schema",1},{"service","getdaq-ecc-server"},{"instance_id",instance},{"snapshot_sha256",config.nativeClient.snapshotSha256},{"operation_active",false},
            {"operation_id",""},{"client_epoch",trace.isEmpty()?QString():config.nativeClient.clientEpoch},{"control_needs_reconciliation",failSeal&&dataStopped},{"message",""},
            {"controller",QJsonDocument::fromJson(QByteArray::fromStdString(GetDAQ::EncodeEccControlControllerStatus(controller))).object()},{"data",data}};
        return QJsonDocument(status).toJson(QJsonDocument::Compact);
    }
    QByteArray ReceiverObservation() {
        const auto cached=QJsonDocument::fromJson(Status()).object().value("data").toObject().value("nodes").toArray(); QJsonArray nodes;
        for (const auto &entry:cached) {
            const auto original=entry.toObject(); const auto life=original.value("lifecycle").toObject(); QJsonArray processes;
            if (!receiverBusy) for (const auto &child:life.value("receivers").toArray()) {
                const auto r=child.toObject(); const auto state=r.value("reaped").toBool() ? "reaped" : receiverState;
                const int code=state=="reaped" ? r.value("exit_code").toInt() : state=="exited" ? receiverExitCode : -1;
                processes.append(QJsonObject{{"id",r.value("id")},{"pid",r.value("pid")},{"state",state},{"exit_code",code},{"signal",0},
                    {"message",state=="unavailable" ? "Fixture child ownership unavailable" : ""}});
            }
            QJsonObject observation{{"schema",1},{"busy",receiverBusy},{"observed_at_unix_ms","18446744073709551615"},{"lifecycle",life},{"receivers",processes}};
            nodes.append(QJsonObject{{"agent_id",original.value("agent_id")},{"plan_sha256",original.value("plan_sha256")},
                {"instance_id",receiverAvailable ? original.value("instance_id") : QJsonValue("")},{"available",receiverAvailable},
                {"message",receiverAvailable ? "" : "Fixture node offline"},{"observation",receiverAvailable ? QJsonValue(observation) : QJsonValue()}});
        }
        return QJsonDocument(QJsonObject{{"schema",1},{"kind","getdaq.ecc-receivers"},{"instance_id",instance},
            {"snapshot_sha256",config.nativeClient.snapshotSha256},{"data_binding_sha256",QString(64,'3')},{"nodes",nodes}}).toJson(QJsonDocument::Compact);
    }
};
bool At(const QSignalSpy &spy,int state) {
    if (spy.isEmpty()) return false;
    const auto rows=qvariant_cast<QVector<NodeStatus>>(spy.last()[0]); if (rows.size()!=2) return false;
    for (const auto &row:rows) if (!row.eccOnline || row.state!=state) return false;
    return true;
}
bool Ready(const QSignalSpy &status,const QSignalSpy &phase,int state) {
    return At(status,state) && !phase.isEmpty() && phase.last()[1].toBool() && !phase.last()[2].toBool();
}
}
class TestNativeGui : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { qRegisterMetaType<QVector<NodeStatus>>(); }
    void selectedCoBoResultsNeverEnterRunFinalization_data() {
        QTest::addColumn<QString>("command");QTest::addColumn<QString>("outcome");
        for(const auto &command:QStringList({"reprepare-cobo","configure-cobo","set-cobo-led","blink-cobo-leds",
                "flip-cobo-led","pulse-cobo-led","trigger-asad-pulser"}))
            QTest::newRow(qPrintable(command)) << command << QString("complete");
        QTest::newRow("failed-configure") << QString("configure-cobo") << QString("failed");
        QTest::newRow("configure-did-not-retain-prepared") << QString("configure-cobo") << QString("wrong-state");
        QTest::newRow("lost-non-idempotent-flip-reply") << QString("flip-cobo-led") << QString("lost-reply");
    }
    void selectedCoBoResultsNeverEnterRunFinalization() {
        QFETCH(QString,command);QFETCH(QString,outcome);
        Fixture f;f.state=3;f.maintenanceEnabled=true;
        f.maintenancePhase=outcome=="failed" ? "failed" : "complete";
        f.maintenanceResultState=outcome=="wrong-state" ? 4 : 3;
        f.dropMaintenanceReply=outcome=="lost-reply";
        RunControl control;
        QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal),
            targets(&control,&RunControl::CoBoMaintenanceTargetsSignal),results(&control,&RunControl::CoBoMaintenanceResultSignal),
            done(&control,&RunControl::RunFinishedSignal);
        control.ConnectSlot(f.config);
        QTRY_VERIFY(!targets.isEmpty() && !targets.last()[0].toByteArray().isEmpty());
        QTRY_VERIFY(Ready(status,phase,3));
        const QString snapshot=f.config.nativeClient.snapshotSha256;
        if(command=="reprepare-cobo")control.ReprepareCoBoSlot("C1",f.instance,snapshot);
        else if(command=="configure-cobo")control.ConfigureCoBoSlot("C1",f.instance,snapshot);
        else if(command=="set-cobo-led")control.SetCoBoLedSlot("C1",0,0,1,f.instance,snapshot);
        else if(command=="blink-cobo-leds")control.BlinkCoBoLedsSlot("C1",f.instance,snapshot);
        else if(command=="flip-cobo-led")control.FlipCoBoLedSlot("C1",2,f.instance,snapshot);
        else if(command=="pulse-cobo-led")control.PulseCoBoLedSlot("C1",2,25,f.instance,snapshot);
        else control.TriggerAsAdPulserSlot("C1",-100,f.instance,snapshot);
        QTRY_COMPARE(f.trace.count(command),1);
        if(outcome=="failed" || outcome=="wrong-state") {
            QTRY_VERIFY(!results.isEmpty() && !results.last()[1].toString().isEmpty());
            if(outcome=="wrong-state")QVERIFY(results.last()[0].toByteArray().isEmpty());
            else QVERIFY(!results.last()[0].toByteArray().isEmpty());
        } else {
            QTRY_VERIFY(!results.isEmpty() && !results.last()[0].toByteArray().isEmpty() && results.last()[1].toString().isEmpty());
        }
        QTRY_VERIFY(!phase.last()[2].toBool());
        QCOMPARE(f.trace,QStringList({command}));QCOMPARE(f.state,3);QCOMPARE(done.size(),0);
        QCOMPARE(f.commandPosts.value(command),1);
        QVERIFY(!phase.last()[3].toBool());QVERIFY(!QFileInfo::exists(f.config.RunDirectory()));
        QCOMPARE(f.operations.size(),1);
        const auto operation=f.operations.first();
        control.ShutdownSlot();
        EccClientStore store(f.config.nativeRecordDir,f.config.nativeClient);
        GetDAQ::EccControlRequest saved;
        QVERIFY(store.FindRequest(operation.request.operationId,&saved));
        QCOMPARE(QString::fromStdString(saved.command),command);QCOMPARE(saved.coboId,std::string("C1"));
        QVERIFY(store.Pending().empty());
    }
    void retainedMaintenanceIntentIsObservedWithoutSubmittingAgain() {
        Fixture f;f.state=3;f.maintenanceEnabled=true;
        GetDAQ::EccControlOperation observed;
        auto &request=observed.request;
        request.command="flip-cobo-led";request.coboId="C1";request.ledFlipId=2;
        request.operationId="retained-original-led-flip";
        request.instanceId=f.instance.toStdString();request.snapshotSha256=f.config.nativeClient.snapshotSha256.toStdString();
        request.clientId=f.config.nativeClient.clientId.toStdString();request.clientEpoch=f.config.nativeClient.clientEpoch.toStdString();
        observed.requestKey=GetDAQ::EccControlRequestKey(request);observed.phase="complete";
        GetDAQ::EccControlControllerStatus result;result.state=3;result.operationId=request.operationId;
        observed.resultJson=GetDAQ::EncodeEccControlControllerStatus(result);
        {
            EccClientStore store(f.config.nativeRecordDir,f.config.nativeClient);QString error;
            QVERIFY2(store.WriteIntent(GetDAQ::EncodeEccControlRequest(request),&error),qPrintable(error));
            QCOMPARE(store.Pending().size(),std::size_t(1));
        }
        f.operations[QString::fromStdString(request.operationId)]=observed;
        RunControl control;
        QSignalSpy replies(&control,&RunControl::CoBoMaintenanceResultSignal),done(&control,&RunControl::RunFinishedSignal);
        control.ConnectSlot(f.config);
        QTRY_VERIFY(!replies.isEmpty() && !replies.last()[0].toByteArray().isEmpty());
        QCOMPARE(replies.last()[1].toString(),QString());
        QCOMPARE(GetDAQ::DecodeEccControlOperation(replies.last()[0].toByteArray().toStdString()).request.operationId,request.operationId);
        QVERIFY(f.trace.isEmpty());QVERIFY(f.commandPosts.isEmpty());QCOMPARE(done.size(),0);QVERIFY(!QFileInfo::exists(f.config.RunDirectory()));
        control.ShutdownSlot();
        EccClientStore store(f.config.nativeRecordDir,f.config.nativeClient);
        QVERIFY(store.Pending().empty());
    }
    void manualProgressSurvivesReadPolling_data() {
        QTest::addColumn<bool>("fault");
        QTest::newRow("healthy-once") << false;
        QTest::newRow("changed-state-cancels") << true;
    }
    void manualProgressSurvivesReadPolling() {
        QFETCH(bool,fault);Fixture f;f.state=2;f.config.pollMs=200;f.config.nativeClient.timeoutMs=5000;
        RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal),logs(&control,&RunControl::LogSignal);
        control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,2));
        f.holdStatusResponse=true;QTRY_VERIFY(!f.heldStatusConnection.isNull());
        control.ProgressSlot();control.ProgressSlot();
        QVERIFY(f.trace.isEmpty());
        bool queued=false;for(const auto &row:logs)queued=queued || row[0].toString().contains("Manual Progress waiting for current metadata");
        QVERIFY2(queued,"The manual click was silently lost during a read-only status request");QVERIFY(phase.last()[2].toBool());
        f.holdStatusResponse=false;
        if(fault){f.failSeal=f.dataStopped=true;f.heldStatusConnection->Reply(200,f.Status());
            QTRY_VERIFY(At(status,2));QTRY_VERIFY(!phase.last()[2].toBool());QCOMPARE(f.trace.count("prepare"),0);
            bool canceled=false;for(const auto &row:logs)canceled=canceled || row[0].toString().contains("Pending manual Progress canceled");QVERIFY(canceled);
        } else {f.heldStatusConnection->Reply(200,f.heldStatusBody);QTRY_COMPARE(f.trace.count("prepare"),1);QTRY_VERIFY(Ready(status,phase,3));}
        control.ShutdownSlot();
    }
    void receiverFailureCacheKeepsRunIncompleteWithoutCommands() {
        Fixture f;f.config.nativeClient.receiverFailureStatusEnabled=true;
        RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal),finished(&control,&RunControl::RunFinishedSignal);
        control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,1));
        control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,2));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,3));
        control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,4));control.StartRunSlot();QTRY_VERIFY(Ready(status,phase,5));
        const auto before=f.trace;f.receiverFailureBody=ReceiverFailureFixture::Cache(f.Status(),f.ReceiverObservation());
        const auto rows=[&] {return status.isEmpty() ? QVector<NodeStatus>() : qvariant_cast<QVector<NodeStatus>>(status.last()[0]);};
        QTRY_VERIFY(rows().size()==2 && rows()[0].nativeReceiverFailureHeld && rows()[0].nativeReceiverFailureAffected);
        QCOMPARE(rows()[0].nativeReceiverFailurePhase,QString("complete"));QVERIFY(!rows()[1].nativeReceiverFailureAffected);
        QVERIFY(rows()[0].nativeReceiverFailureDetail.contains("exit 7"));QVERIFY(rows()[0].nativeReceiverFailureDetail.contains("current retained session"));
        QJsonObject journal;QString error;QTRY_VERIFY(RunRecorder::ReadJournal(f.config,&journal,&error) && !journal.value("native_receiver_failure_cache").toString().isEmpty());
        QVERIFY(journal.value("phase").toString()!="Complete");QVERIFY(journal.value("duration_incomplete").toBool());
        const auto cached=GetDAQ::DecodeEccControlReceiverFailureStatus(journal.value("native_receiver_failure_cache").toString().toStdString());
        QVERIFY(cached.hasEvidence && cached.latched);QCOMPARE(finished.size(),0);QVERIFY(!QFileInfo::exists(QDir(f.config.RunDirectory()).filePath("run.csv")));
        control.StartRunSlot();control.StopRunSlot();QTest::qWait(150);QCOMPARE(f.trace,before);
        f.receiverFailureAvailable=false;
        QTRY_VERIFY(!rows()[0].nativeReceiverFailureObserved && !rows()[0].nativeReceiverFailureDetail.isEmpty());
        f.receiverFailureAvailable=true;f.receiverFailureBody.clear();QTest::qWait(150);
        control.StartRunSlot();QCOMPARE(f.trace,before);QCOMPARE(finished.size(),0);control.ShutdownSlot();
    }
    void receiverFailureCacheExpiresAndPreservesHistoricalSessionMeaning() {
        Fixture f;f.config.nativeClient.receiverFailureStatusEnabled=true;f.config.nativeClient.statusMaxAgeMs=100;
        RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal);
        control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,1));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,2));
        control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,3));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,4));
        f.receiverFailureBody=ReceiverFailureFixture::Cache(f.Status(),f.ReceiverObservation());const auto before=f.trace;
        const auto rows=[&] {return status.isEmpty() ? QVector<NodeStatus>() : qvariant_cast<QVector<NodeStatus>>(status.last()[0]);};
        QTRY_VERIFY(rows().size()==2 && rows()[0].nativeReceiverFailureAffected);
        f.receiverFailureDelayMs=450;
        QTRY_VERIFY(!rows()[0].nativeReceiverFailureObserved && rows()[0].nativeReceiverFailureDetail.contains("stale"));
        f.receiverFailureDelayMs=0;f.receiverNodeInstance=QString(64,'9');
        QTRY_VERIFY(rows()[0].nativeReceiverFailureObserved && !rows()[0].nativeReceiverFailureAffected && rows()[0].nativeReceiverFailureDetail.contains("historical session"));
        f.receiverNodeInstance=QString(64,'5');++f.cycle;
        QTRY_VERIFY(rows()[0].nativeReceiverFailureObserved && !rows()[0].nativeReceiverFailureAffected && rows()[0].nativeReceiverFailureDetail.contains("historical session"));
        QCOMPARE(f.trace,before);control.ShutdownSlot();
    }
    void unusedReceiverRemainsVisibleWithoutAutomaticCommands() {
        Fixture f;RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal);
        control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,1));
        control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,2));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,3));
        control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,4));const auto before=f.trace;
        f.dataStopped=true;f.state=3;f.unusedReceiver=true;f.received=f.frames=f.files="0";
        const auto rows=[&] {return status.isEmpty() ? QVector<NodeStatus>() : qvariant_cast<QVector<NodeStatus>>(status.last()[0]);};
        QTRY_VERIFY(rows().size()==2 && rows()[0].nativeSourceUnusedConfirmed && rows()[1].nativeSourceUnusedConfirmed);
        QVERIFY(rows()[0].nativeSealVerified && rows()[0].nativeReaped);QCOMPARE(rows()[0].nativeReceivedBytes,QString("0"));
        QCOMPARE(f.trace,before);control.ShutdownSlot();
    }
    void iceSourceEndRemainsDistinctWithoutAutomaticCommands() {
        Fixture f;RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal);
        control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,1));
        control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,2));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,3));
        control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,4));const auto before=f.trace;
        f.dataStopped=true;f.state=3;f.iceSourceEnded=true;
        const auto rows=[&] {return status.isEmpty() ? QVector<NodeStatus>() : qvariant_cast<QVector<NodeStatus>>(status.last()[0]);};
        QTRY_VERIFY(rows().size()==2 && rows()[0].nativeSourceEndConfirmed && rows()[1].nativeSourceEndConfirmed);
        QVERIFY(rows()[0].nativeSealVerified && rows()[0].nativeReaped && !rows()[0].nativeSourceUnusedConfirmed);
        QCOMPARE(f.trace,before);control.ShutdownSlot();
    }
    void sourceObservationShowsUnknownAndUnavailableWithoutCommands() {
        Fixture f;f.config.nativeClient.sourceStatusEnabled=true;
        RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal);
        control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,1));
        control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,2));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,3));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,4));
        const auto rows=[&] {return status.isEmpty() ? QVector<NodeStatus>() : qvariant_cast<QVector<NodeStatus>>(status.last()[0]);};
        QTRY_VERIFY(rows().size()==2 && rows()[0].nativeSourceObserved && rows()[0].nativeSourceState=="running");
        const auto commands=f.trace;f.sourceOperation="previous-breakup";
        QTRY_VERIFY(rows()[0].nativeSourceError.contains("controller operation") && !rows()[0].nativeSourceObserved);QCOMPARE(f.trace,commands);
        f.sourceOperation.clear();QTRY_VERIFY(rows()[0].nativeSourceObserved);
        f.sourceState="unknown";QTRY_VERIFY(rows()[0].nativeSourceFault);QCOMPARE(f.trace,commands);
        f.sourcePhase="unavailable";QTRY_VERIFY(!rows()[0].nativeSourceObserved);QCOMPARE(f.trace,commands);
        f.sourcePhase="available";f.sourceState="running";f.sourceSession=QString(64,'9');
        QTRY_VERIFY(rows()[0].nativeSourceError.contains("session") && !rows()[0].nativeSourceObserved);QCOMPARE(f.trace,commands);
        control.ShutdownSlot();
    }
    void sourceObservationAgeExpiresWhileReadIsPending() {
        Fixture f;f.config.nativeClient.sourceStatusEnabled=true;f.config.nativeClient.statusMaxAgeMs=100;
        RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal);
        control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,1));
        control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,2));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,3));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,4));
        const auto rows=[&] {return status.isEmpty() ? QVector<NodeStatus>() : qvariant_cast<QVector<NodeStatus>>(status.last()[0]);};
        QTRY_VERIFY(rows().size()==2 && rows()[0].nativeSourceObserved);
        const auto commands=f.trace;f.sourceDelayMs=400;
        QTRY_VERIFY(rows()[0].nativeSourceError.contains("stale") && !rows()[0].nativeSourceObserved);
        QCOMPARE(f.trace,commands);QCOMPARE(f.state,4);control.ShutdownSlot();
    }
    void receiverObservationShowsFailureBusyAndUnavailableWithoutCommands() {
        Fixture f;f.config.nativeClient.receiverStatusEnabled=true;
        RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal);
        control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,1));
        control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,2));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,3));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,4));
        const auto rows=[&] {return status.isEmpty() ? QVector<NodeStatus>() : qvariant_cast<QVector<NodeStatus>>(status.last()[0]);};
        QTRY_VERIFY(rows().size()==2 && rows()[0].nativeReceiverObserved && rows()[0].process.contains("Running"));
        const auto commands=f.trace;f.receiverState="exited";
        QTRY_VERIFY(rows()[0].nativeReceiverFault && rows()[0].process.contains("exit 1"));QCOMPARE(f.state,4);QCOMPARE(f.trace,commands);
        f.receiverExitCode=0;QTRY_VERIFY(!rows()[0].nativeReceiverFault && rows()[0].process.contains("Finish/seal pending"));QCOMPARE(f.trace,commands);
        f.receiverBusy=true;QTRY_VERIFY(!rows()[0].nativeReceiverObserved && rows()[0].process.contains("busy"));QCOMPARE(f.trace,commands);
        f.receiverBusy=false;f.receiverAvailable=false;
        QTRY_VERIFY(rows()[0].nativeReceiverError.contains("offline") && !rows()[0].nativeReceiverObserved);QCOMPARE(f.trace,commands);
        f.receiverAvailable=true;f.receiverState="running";QTRY_VERIFY(rows()[0].nativeReceiverObserved && !rows()[0].nativeReceiverFault);
        control.ShutdownSlot();
    }
    void receiverObservationAgeExpiresWhileReadIsPending() {
        Fixture f;f.config.nativeClient.receiverStatusEnabled=true;f.config.nativeClient.statusMaxAgeMs=100;
        RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal);
        control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,1));
        const auto rows=[&] {return status.isEmpty() ? QVector<NodeStatus>() : qvariant_cast<QVector<NodeStatus>>(status.last()[0]);};
        QTRY_VERIFY(rows().size()==2 && rows()[0].process.contains("No owned receiver"));
        f.receiverDelayMs=400;
        QTRY_VERIFY(rows()[0].nativeReceiverError.contains("stale") && !rows()[0].nativeReceiverObserved);
        QVERIFY(f.trace.isEmpty());control.ShutdownSlot();
    }
    void configurationRoundtripAndInvalidMapping() {
        Fixture f; f.config.nativeClient.alarmStatusEnabled=true;f.config.nativeClient.configurationStatusEnabled=true;f.config.nativeClient.sourceStatusEnabled=true;f.config.nativeClient.receiverFailureStatusEnabled=true; QString error; QVERIFY2(f.config.Validate(&error),qPrintable(error)); const auto file=f.directory.filePath("native.yml"); QVERIFY(f.config.Save(file,&error));
        RunConfig loaded; QVERIFY2(loaded.Load(file,&error),qPrintable(error)); QVERIFY(loaded.nativeEcc); QVERIFY(loaded.nativeClient.alarmStatusEnabled); QCOMPARE(loaded.nativeClient.clientEpoch,f.config.nativeClient.clientEpoch); QCOMPARE(loaded.nodes[1].nativeSenderId,QString("C1"));
        QVERIFY(loaded.nativeClient.configurationStatusEnabled);
        QVERIFY(loaded.nativeClient.sourceStatusEnabled);QVERIFY(loaded.nativeClient.receiverFailureStatusEnabled);
        loaded.nodes[1].nativeSenderId="C0"; QVERIFY(!loaded.Validate(&error)); loaded=f.config; loaded.simulation=true; QVERIFY(!loaded.Validate(&error));
    }
    void startupConfigurationIsBoundBeforeStartAndSavedWithRun() {
        Fixture f;f.config.nativeClient.configurationStatusEnabled=true;RunControl control;
        QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal),configuration(&control,&RunControl::ConfigurationSignal),done(&control,&RunControl::RunFinishedSignal);
        control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,1));QTRY_VERIFY(!configuration.isEmpty());QVERIFY(configuration.last()[1].toString().isEmpty());
        control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,2));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,3));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,4));
        control.StartRunSlot();QTRY_VERIFY(Ready(status,phase,5));QJsonObject journal;QString error;QVERIFY(RunRecorder::ReadJournal(f.config,&journal,&error));
        const auto recorded=journal.value("native_configuration").toString().toUtf8();QVERIFY(!recorded.isEmpty());QVERIFY(f.configurationRecordedBeforeStart);
        const auto accepted=GetDAQ::DecodeEccControlConfiguration(recorded.toStdString());QCOMPARE(QString::fromStdString(accepted.selection.registration.repositoryId),f.repositoryId);QCOMPARE(f.trace.count("start"),1);
        control.StopRunSlot();QTRY_COMPARE(done.size(),1);QFile saved(QDir(f.config.RunDirectory()).filePath("ecc-configuration.json"));QVERIFY(saved.open(QIODevice::ReadOnly));QCOMPARE(saved.readAll(),recorded);
        control.ShutdownSlot();
    }
    void missingRequiredConfigurationDoesNotReserveOrStartRun() {
        Fixture f;f.configurationAvailable=false;f.config.nativeClient.configurationStatusEnabled=true;RunControl control;
        QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal),logs(&control,&RunControl::LogSignal);
        control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,1));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,2));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,3));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,4));
        control.StartRunSlot();QTest::qWait(100);QCOMPARE(f.trace.count("start"),0);QVERIFY(!QFileInfo::exists(f.config.RunDirectory()));QVERIFY(!logs.isEmpty());control.ShutdownSlot();
    }
    void localRecoveryRejectsChangedStartupSelection() {
        Fixture f;f.config.nativeClient.configurationStatusEnabled=true;
        {
            RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal);
            control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,1));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,2));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,3));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,4));
            control.StartRunSlot();QTRY_VERIFY(Ready(status,phase,5));control.ShutdownSlot();
        }
        f.repositoryId=QString(64,'8');RunControl recovered;QSignalSpy status(&recovered,&RunControl::StatusSignal),phase(&recovered,&RunControl::ControlSignal),logs(&recovered,&RunControl::LogSignal);
        recovered.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,5));recovered.RecoverSlot();QVERIFY(!phase.last()[3].toBool());
        QCOMPARE(f.trace.count("start"),1);QVERIFY(logs.last()[0].toString().contains("configuration record differs"));recovered.ShutdownSlot();
    }
    void alarmHeldIsVisibleAndNeverCompletesRun() {
        Fixture f; f.config.nativeClient.alarmStatusEnabled=true; RunControl control;
        QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal),logs(&control,&RunControl::LogSignal),done(&control,&RunControl::RunFinishedSignal);
        control.ConnectSlot(f.config); QTRY_VERIFY(Ready(status,phase,1)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,2));
        control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,3)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,4));
        control.StartRunSlot(); QTRY_VERIFY(Ready(status,phase,5)); f.alarmHeld=true; f.state=4;
        QTRY_VERIFY(!phase.isEmpty() && phase.last()[0].toString()=="Native requires inspection");
        bool exact=false; for (const auto &entry:logs) exact=exact || entry[0].toString().contains("9007199254740993"); QVERIFY(exact);
        QJsonObject journal; QString error; QVERIFY(RunRecorder::ReadJournal(f.config,&journal,&error));
        QVERIFY(journal.value("phase").toString()!="Complete"); QVERIFY(!journal.value("native_alarm_cache").toString().isEmpty());
        const auto observed=GetDAQ::DecodeEccControlAlarmStatus(journal.value("native_alarm_cache").toString().toStdString()); QVERIFY(observed.latched);
        control.FinalizeSlot(); QTest::qWait(150); QCOMPARE(done.size(),0); QCOMPARE(f.trace.count("breakup"),0);
        QVERIFY(!QFileInfo::exists(QDir(f.config.RunDirectory()).filePath("run.csv"))); control.ShutdownSlot();
    }
    void automaticNativeTimeAndFileCycles() {
        for(const auto condition:{AutoRunSettings::Time,AutoRunSettings::FileSize,AutoRunSettings::Either}) {
            Fixture f;f.automaticMetrics=true;f.received="0";f.frames="0";f.files="0";
            f.config.nativeClient.receiverStatusEnabled=true;f.config.autoRun.enabled=true;
            f.config.autoRun.condition=condition;f.config.autoRun.action=AutoRunSettings::NextRun;
            f.config.autoRun.durationMs=300;f.config.autoRun.fileBytes=1500000;f.config.autoRun.maximumRuns=2;
            RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal),done(&control,&RunControl::RunFinishedSignal);
            QElapsedTimer monotonic;monotonic.start();qint64 completedAt=-1,continuedAt=-1;
            connect(&control,&RunControl::RunFinishedSignal,&control,[&](int){if(completedAt<0)completedAt=monotonic.elapsed();});
            connect(&control,&RunControl::LogSignal,&control,[&](const QString &message){if(message.startsWith("Automatic Run: starting Run"))continuedAt=monotonic.elapsed();});
            control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,1));
            for(int state=2;state<=4;++state){control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,state));}
            control.StartRunSlot();QTRY_VERIFY(Ready(status,phase,5));
            auto edited=f.config.autoRun;edited.enabled=false;control.ConfigureAutoRunSlot(edited);
            QTRY_COMPARE(done.count(),2);
            QVERIFY(completedAt>=0 && continuedAt-completedAt>=3000);
            QCOMPARE(f.trace,QStringList({"describe","prepare","configure","start","stop","breakup","configure","start","stop","breakup"}));
            QTest::qWait(500);QCOMPARE(done.count(),2);control.DisconnectSlot();control.ShutdownSlot();
        }
    }
    void automaticNativeEnableDuringRunUsesSavedConditions() {
        Fixture f;f.config.autoRun.enabled=false;f.config.autoRun.durationMs=1000;
        RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal),done(&control,&RunControl::RunFinishedSignal),automatic(&control,&RunControl::AutoRunStatusSignal);
        control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,1));
        for(int state=2;state<=4;++state){control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,state));}
        control.StartRunSlot();QTRY_VERIFY(Ready(status,phase,5));
        control.SetAutoRunEnabledSlot(true);QTRY_VERIFY(automatic.last()[1].toBool());QTest::qWait(400);
        control.SetAutoRunEnabledSlot(false);QTRY_VERIFY(!automatic.last()[1].toBool());QTest::qWait(1200);
        QCOMPARE(f.commandPosts.value("stop"),0);QCOMPARE(done.count(),0);
        auto edited=f.config.autoRun;edited.durationMs=100;control.ConfigureAutoRunSlot(edited); // rejected; thresholds remain locked
        control.SetAutoRunEnabledSlot(true);QTRY_VERIFY(automatic.last()[1].toBool());QTest::qWait(400);
        QCOMPARE(f.commandPosts.value("stop"),0);QTRY_COMPARE(done.count(),1);
        QCOMPARE(f.commandPosts.value("stop"),1);control.DisconnectSlot();control.ShutdownSlot();
    }
    void automaticNativeMissingMetricsOrWrongSession() {
        for(const bool wrong:{false,true}) {
            Fixture f;f.automaticMetrics=true;f.received="0";f.frames="0";f.files="0";
            f.metricsUnavailable=!wrong;f.wrongMetricSession=wrong;f.config.nativeClient.receiverStatusEnabled=true;
            f.config.autoRun.enabled=true;f.config.autoRun.condition=AutoRunSettings::FileSize;f.config.autoRun.fileBytes=1;
            RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal),done(&control,&RunControl::RunFinishedSignal),automation(&control,&RunControl::AutoRunStatusSignal),logs(&control,&RunControl::LogSignal);
            control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,1));
            for(int state=2;state<=4;++state){control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,state));}
            control.StartRunSlot();QTRY_VERIFY(Ready(status,phase,5));
            QTRY_VERIFY(!automation.isEmpty() && automation.last()[0].toString().contains("raw-file size unavailable"));
            QTest::qWait(400);QCOMPARE(f.commandPosts.value("stop"),0);QCOMPARE(done.count(),0);
            QTRY_VERIFY(Ready(status,phase,5));control.StopRunSlot();QTRY_COMPARE(done.count(),1);control.DisconnectSlot();control.ShutdownSlot();
        }
    }
    void automaticNativeExternalPauseResumeAndStop() {
        Fixture f;f.automaticMetrics=true;f.config.autoRun.enabled=true;f.config.autoRun.durationMs=1000;f.config.autoRun.action=AutoRunSettings::NextRun;
        RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal),done(&control,&RunControl::RunFinishedSignal),automation(&control,&RunControl::AutoRunStatusSignal);
        control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,1));
        for(int state=2;state<=4;++state){control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,state));}
        control.StartRunSlot();QTRY_VERIFY(Ready(status,phase,5));QTest::qWait(250);
        f.state=6;f.latestOperation="mcp-pause";QTRY_VERIFY(At(status,6));QTest::qWait(1100);QCOMPARE(f.commandPosts.value("stop"),0);
        f.state=5;f.latestOperation="mcp-resume";QTRY_VERIFY(At(status,5));QTest::qWait(400);QCOMPARE(f.commandPosts.value("stop"),0);
        f.state=4;f.latestOperation="mcp-stop";QTRY_VERIFY(At(status,4));
        QTRY_VERIFY(!automation.isEmpty() && !automation.last()[1].toBool());QTest::qWait(1100);
        QCOMPARE(f.commandPosts.value("stop"),0);QCOMPARE(f.commandPosts.value("start"),1);
        QTRY_VERIFY(Ready(status,phase,4));control.FinalizeSlot();QTRY_COMPARE(done.count(),1);control.DisconnectSlot();control.ShutdownSlot();
    }
    void wholeSystemOnceAndSealBeforeRunRecord() {
        Fixture f; f.config.nativeClient.alarmStatusEnabled=true;
        RunControl control; QSignalSpy status(&control,&RunControl::StatusSignal), done(&control,&RunControl::RunFinishedSignal), phase(&control,&RunControl::ControlSignal);
        control.ConnectSlot(f.config); QTRY_VERIFY(Ready(status,phase,1)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,2)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,3)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,4));
        control.StartRunSlot(); QTRY_VERIFY(Ready(status,phase,5)); control.PauseSlot(); QTRY_VERIFY(Ready(status,phase,6)); control.ResumeSlot(); QTRY_VERIFY(Ready(status,phase,5)); control.StopRunSlot(); QTRY_COMPARE(done.size(),1);
        QCOMPARE(f.trace,QStringList({"describe","prepare","configure","start","pause","resume","stop","breakup"}));
        QJsonObject journal; QString error; QVERIFY(RunRecorder::ReadJournal(f.config,&journal,&error)); QCOMPARE(journal["phase"].toString(),QString("Complete"));
        QVERIFY(QFileInfo::exists(QDir(f.config.RunDirectory()).filePath("receiver-status.json"))); QVERIFY(QFileInfo::exists(QDir(f.config.RunDirectory()).filePath("run.csv")));
        QVERIFY(!QFileInfo::exists(QDir(f.config.RunDirectory()).filePath("configuration"))); // 新模式不把旧本机配置镜像冒充实际加载工件。
        control.DisconnectSlot(); control.ShutdownSlot();
    }
    void lostStartReplyObservedAndSealFailureNotComplete() {
        Fixture f; f.dropStart=true; RunControl control; QSignalSpy status(&control,&RunControl::StatusSignal), done(&control,&RunControl::RunFinishedSignal), phase(&control,&RunControl::ControlSignal);
        control.ConnectSlot(f.config); QTRY_VERIFY(Ready(status,phase,1)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,2)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,3)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,4));
        control.StartRunSlot(); QTRY_VERIFY(Ready(status,phase,5) && phase.last()[0].toString()=="Running"); QCOMPARE(f.trace.count("start"),1);
        QCOMPARE(f.commandPosts.value("start"),1);
        f.failSeal=true; control.StopRunSlot();
        QTRY_VERIFY(f.dataStopped); QTest::qWait(100); QCOMPARE(done.size(),0);
        QJsonObject journal; QString error; QVERIFY(RunRecorder::ReadJournal(f.config,&journal,&error)); QVERIFY(journal["phase"].toString()!="Complete");
        QTRY_VERIFY(!phase.last()[2].toBool()); control.StopProductionAfterFailureSlot(); QTRY_COMPARE(f.trace.count("stop-production-after-failure"),1);
        QTRY_VERIFY(phase.last()[0].toString().contains("UNKNOWN retained") && !phase.last()[2].toBool());
        const auto before=f.trace; control.ResumeSlot(); control.StartRunSlot(); control.ProgressSlot(); QCOMPARE(f.trace,before); control.ShutdownSlot();
    }
    void completedOperationWaitsForCurrentStatusBeforeEnablingControl() {
        Fixture f; f.statusDelayMs=250; f.config.pollMs=1000;
        RunControl control; QSignalSpy status(&control,&RunControl::StatusSignal), phase(&control,&RunControl::ControlSignal);
        control.ConnectSlot(f.config); QTRY_VERIFY(Ready(status,phase,1)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,2)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,3)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,4));
        control.StartRunSlot(); QTRY_VERIFY(f.trace.contains("start"));
        QVERIFY(phase.last()[2].toBool()); control.StopRunSlot(); QCOMPARE(f.trace.count("stop"),0);
        QTRY_VERIFY(Ready(status,phase,5)); control.PauseSlot(); QTRY_VERIFY(Ready(status,phase,6)); QCOMPARE(f.trace.count("pause"),1);
        control.ShutdownSlot();
    }
    void dataHostCatalogIsRequiredBeforeRunComplete() {
        Fixture f; f.config.nativeCatalog=true; f.catalogPostDelayMs=250; f.config.nativeClient.timeoutMs=1000;
        RunControl control; QSignalSpy status(&control,&RunControl::StatusSignal), phase(&control,&RunControl::ControlSignal), done(&control,&RunControl::RunFinishedSignal);
        control.ConnectSlot(f.config); QTRY_VERIFY(Ready(status,phase,1)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,2)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,3)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,4));
        control.StartRunSlot(); QTRY_VERIFY(Ready(status,phase,5)); control.StopRunSlot(); QTRY_COMPARE(done.size(),1);
        QVERIFY(f.catalogued); QVERIFY(f.catalogStatusHeld);
        QVERIFY2(f.trace==QStringList({"describe","prepare","configure","start","stop","breakup","catalog"}),qPrintable(f.trace.join(", ")));
        QVERIFY(QFileInfo::exists(QDir(f.config.RunDirectory()).filePath("data-catalogs.json")));
        bool bound=false; for (const auto &operation:f.operations) if (operation.request.command=="catalog") { QCOMPARE(QString::fromStdString(operation.request.catalogExperiment),f.config.experiment); QCOMPARE(operation.request.catalogRunNumber,0U); bound=true; }
        QVERIFY(bound); control.ShutdownSlot();
    }
    void nativeConfirmedReplacementIsJournaledAndUsesCatalogReplacementIntent() {
        Fixture f; f.config.nativeCatalog=true;
        QString error; QVERIFY2(RunRecorder::Reserve(f.config,&error),qPrintable(error));
        QVERIFY(RunRecorder::Finish(f.config,12.0).isEmpty());
        QJsonObject oldJournal; QVERIFY(RunRecorder::ReadJournal(f.config,&oldJournal,&error)); oldJournal["phase"]="Complete";
        QVERIFY2(RunRecorder::WriteJournal(f.config,oldJournal,&error),qPrintable(error));
        RunControl control; QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal),done(&control,&RunControl::RunFinishedSignal);
        control.ConnectSlot(f.config); QTRY_VERIFY(Ready(status,phase,1)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,2));
        control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,3)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,4));
        control.StartRunSlot(true); QTRY_VERIFY(Ready(status,phase,5)); control.StopRunSlot(); QTRY_COMPARE(done.size(),1);
        bool replacementCatalog=false;
        for(const auto &operation:f.operations) if(operation.request.command=="catalog") replacementCatalog=operation.request.catalogReplaceExisting;
        QVERIFY(replacementCatalog);
        QJsonObject journal; QVERIFY(RunRecorder::ReadJournal(f.config,&journal,&error));
        QCOMPARE(journal.value("phase").toString(),QString("Complete"));
        QVERIFY(journal.value("replacement_backup").toString().isEmpty());
        control.ShutdownSlot();
    }
    void failedNativeReplacementCanBeExplicitlyResumedThroughRunControl() {
        Fixture f;f.config.nativeCatalog=true;f.failReplacementCatalog=true;
        QString error;QVERIFY2(RunRecorder::Reserve(f.config,&error),qPrintable(error));
        QVERIFY(RunRecorder::Finish(f.config,12.0).isEmpty());
        QJsonObject oldJournal;QVERIFY(RunRecorder::ReadJournal(f.config,&oldJournal,&error));oldJournal["phase"]="Complete";
        QVERIFY2(RunRecorder::WriteJournal(f.config,oldJournal,&error),qPrintable(error));
        RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal),
            done(&control,&RunControl::RunFinishedSignal),recovery(&control,&RunControl::CatalogRecoveryStateSignal);
        control.ConnectSlot(f.config);QTRY_VERIFY(Ready(status,phase,1));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,2));
        control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,3));control.ProgressSlot();QTRY_VERIFY(Ready(status,phase,4));
        control.StartRunSlot(true);QTRY_VERIFY(Ready(status,phase,5));control.StopRunSlot();
        QTRY_VERIFY(!recovery.isEmpty() && recovery.last()[0].toBool());
        const QString failedId=recovery.last()[1].toString();QVERIFY(!failedId.isEmpty());QCOMPARE(failedId,f.failedReplacementCatalogId);
        QCOMPARE(done.size(),0);QVERIFY(phase.last()[3].toBool());
        control.ResumeCatalogReplacementSlot(failedId,"gui-test-catalog-resume");
        QTRY_COMPARE(done.size(),1);QCOMPARE(f.trace.count("catalog"),1);QCOMPARE(f.trace.count("resume-catalog-replacement"),1);
        QJsonObject journal;QVERIFY(RunRecorder::ReadJournal(f.config,&journal,&error));
        QCOMPARE(journal.value("native_catalog_failed_operation_id").toString(),failedId);
        QCOMPARE(journal.value("native_catalog_recovery_operation_id").toString(),QString("gui-test-catalog-resume"));
        QVERIFY(journal.value("native_catalog_recovery_complete").toBool());
        QCOMPARE(journal.value("phase").toString(),QString("Complete"));control.ShutdownSlot();
    }
    void nativeRunCompletionPersistsNextRunNumber() {
        Fixture f;
        f.config.filePath=f.directory.filePath("operator-config.yml");
        QString error; QVERIFY2(f.config.Save(f.config.filePath,&error),qPrintable(error));
        RunControl control; QSignalSpy status(&control,&RunControl::StatusSignal), phase(&control,&RunControl::ControlSignal), done(&control,&RunControl::RunFinishedSignal);
        control.ConnectSlot(f.config); QTRY_VERIFY(Ready(status,phase,1)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,2));
        control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,3)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,4));
        control.StartRunSlot(); QTRY_VERIFY(Ready(status,phase,5)); control.StopRunSlot(); QTRY_COMPARE(done.size(),1);
        QCOMPARE(done.last().at(0).toInt(),1);
        RunConfig reopened; QVERIFY2(reopened.Load(f.config.filePath,&error),qPrintable(error)); QCOMPARE(reopened.runNumber,1);
        control.ShutdownSlot();
    }
    void largeCountersStayExactInNativeStatus() {
        Fixture f; f.received="18446744073709551615"; f.frames="9007199254740993"; f.files="9007199254740993";
        RunControl control; QSignalSpy status(&control,&RunControl::StatusSignal), phase(&control,&RunControl::ControlSignal);
        control.ConnectSlot(f.config); QTRY_VERIFY(Ready(status,phase,1)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,2)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,3)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,4));
        const auto row=qvariant_cast<QVector<NodeStatus>>(status.last()[0]).first(); QCOMPARE(row.nativeReceivedBytes,f.received); QCOMPARE(row.nativeFrames,f.frames); QCOMPARE(row.nativeFiles,f.files);
        control.ShutdownSlot();
    }
    void interruptedGuiRebindsSameRunWithoutReplayingStart() {
        Fixture f;
        {
            RunControl control; QSignalSpy status(&control,&RunControl::StatusSignal), phase(&control,&RunControl::ControlSignal);
            control.ConnectSlot(f.config); QTRY_VERIFY(Ready(status,phase,1)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,2)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,3)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,4));
            control.StartRunSlot(); QTRY_VERIFY(Ready(status,phase,5)); control.ShutdownSlot();
        }
        RunControl recovered; QSignalSpy status(&recovered,&RunControl::StatusSignal), phase(&recovered,&RunControl::ControlSignal), done(&recovered,&RunControl::RunFinishedSignal);
        recovered.ConnectSlot(f.config); QTRY_VERIFY(Ready(status,phase,5)); recovered.RecoverSlot();
        QVERIFY(phase.last()[3].toBool()); QVERIFY(phase.last()[0].toString().contains("timing incomplete")); QCOMPARE(f.trace.count("start"),1);
        recovered.StopRunSlot(); QTRY_COMPARE(done.size(),1);
        QJsonObject journal; QString error; QVERIFY(RunRecorder::ReadJournal(f.config,&journal,&error)); QVERIFY(journal["duration_incomplete"].toBool()); QCOMPARE(journal["phase"].toString(),QString("Complete"));
        QCOMPARE(f.trace.count("start"),1); recovered.ShutdownSlot();
    }
    void recoveryRejectsChangedReceiverSessionOrEccInstance() {
        for (int change=0;change<2;++change) {
            Fixture f;
            {
                RunControl control; QSignalSpy status(&control,&RunControl::StatusSignal), phase(&control,&RunControl::ControlSignal);
                control.ConnectSlot(f.config); QTRY_VERIFY(Ready(status,phase,1)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,2)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,3)); control.ProgressSlot(); QTRY_VERIFY(Ready(status,phase,4));
                control.StartRunSlot(); QTRY_VERIFY(Ready(status,phase,5)); control.ShutdownSlot();
            }
            if (change==0) ++f.cycle; else f.instance=QString(64,'9');
            RunControl recovered; QSignalSpy status(&recovered,&RunControl::StatusSignal), phase(&recovered,&RunControl::ControlSignal), logs(&recovered,&RunControl::LogSignal), done(&recovered,&RunControl::RunFinishedSignal);
            recovered.ConnectSlot(f.config); QTRY_VERIFY(Ready(status,phase,5)); const auto before=f.trace; recovered.RecoverSlot();
            QVERIFY(!phase.last()[3].toBool()); QCOMPARE(done.size(),0); QCOMPARE(f.trace,before); QVERIFY(!logs.isEmpty());
            QVERIFY(logs.last()[0].toString().contains(change==0 ? "session/PID/dataset differs" : "instance/snapshot differs")); recovered.ShutdownSlot();
        }
    }
};
QTEST_GUILESS_MAIN(TestNativeGui)
#include "test_native_gui.moc"
