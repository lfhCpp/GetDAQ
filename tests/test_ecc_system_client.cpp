#include "eccsystemclient.h"
#include "configuration_readback_fixture.h"
#include "receiver_failure_fixture.h"
#include "source_observation_fixture.h"
#include "plan_transition_fixture.h"
#include "cobo_maintenance_fixture.h"
#include "httpserver.h"
#include "eccclientstore.h"
#include "eccclientstore_recoverypolicy.h"
#include <QtTest>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
namespace {
class Fixture : public QObject
{
public:
    QTemporaryDir directory;
    HttpServer server;
    EccSystemClientOptions options;
    QByteArray token = QByteArray(64, 'f'); // 本机测试凭据，不能访问实际服务。
    QString instance = QString(64, 'a');
    int posts = 0, commands = 0, scanIdentityPosts = 0;
    bool dropReply = false, dropObservationOnce = false, complete = false, unknown = false, persistedBeforePost = true;
    bool reconcile = false, active = false;
    unsigned int recoveryReplyState=2;
    QString ownerEpoch;
    bool alarmHeld=false,alarmInstanceMismatch=false;
    QString heldMetadataPath;QPointer<HttpConnection> heldMetadataConnection;QByteArray heldMetadataBody;
    void MetadataReply(const QString &path,HttpConnection *connection,const QByteArray &body) {
        if(path==heldMetadataPath){heldMetadataConnection=connection;heldMetadataBody=body;}else connection->Reply(200,body);
    }
    QByteArray configurationOverride;
    QByteArray ownershipOverride;
    QByteArray readbacksOverride=QByteArray::fromStdString(ConfigurationReadbackFixture::Cache());
    QByteArray receiverOverride;
    QByteArray receiverEventsOverride;
    QByteArray sourceOverride;
    QByteArray receiverFailureOverride;
    QByteArray statusOverride,handoffOverride;
    QByteArray scanIdentityOverride;
    QByteArray maintenanceOverride=CoBoMaintenanceFixture::Targets();
    int handoffGets=0,receiverEventsGets=0;
    QString receiverEventsPath;
    bool configurationSelected=false;
    QString repositoryId=QString(64,'9');
    QMap<QString, GetDAQ::EccControlRequest> requests;
    Fixture() {
        if (!server.listen(QHostAddress::LocalHost, 0)) throw std::runtime_error("Cannot bind fixture");
        options.address = "127.0.0.1"; options.port = server.serverPort(); options.timeoutMs = 300;
        options.serviceId = "test-ecc"; options.clientId = "test-console"; options.clientEpoch = QString(64, 'd');
        options.snapshotSha256 = QString(64, 'b'); options.inputBindingSha256 = QString(64, 'c'); options.credentialFile = directory.filePath("credential");
        WriteCredential(token);
        connect(&server, &HttpServer::RequestSignal, this, [this](const QString &method, const QString &path, const QByteArray &body, HttpConnection *connection) {
            if (connection->RequestHeader("authorization") != "Bearer " + token) { connection->Reply(401, "{}"); return; }
            if (method == "GET" && path == "/v1/identity") {
                QJsonObject identity{{"schema", 1}, {"service", "getdaq-ecc-server"}, {"service_id", options.serviceId}, {"client_id", options.clientId},
                    {"instance_id", instance}, {"snapshot_sha256", options.snapshotSha256}, {"input_binding_sha256", options.inputBindingSha256},
                    {"configuration_sha256", QString(64, '1')}, {"artifact_sha256", QString(64, '2')}, {"data_binding_sha256", QString(64, '3')}};
                connection->Reply(200, QJsonDocument(identity).toJson(QJsonDocument::Compact)); return;
            }
            if (method == "GET" && path == "/v1/status") {
                if(!statusOverride.isEmpty()){MetadataReply(path,connection,statusOverride);return;}
                GetDAQ::EccControlControllerStatus state;
                QJsonObject data{{"needs_reconciliation", false}, {"controller_scope_sha256", QString(64, '5')}, {"controller_epoch", QString(64, '6')}, {"message", ""}, {"nodes", QJsonArray()}};
                QJsonObject status{{"schema", 1}, {"service", "getdaq-ecc-server"}, {"instance_id", instance}, {"snapshot_sha256", options.snapshotSha256}, {"operation_active", active},
                    {"operation_id", active ? "external-operation" : ""}, {"client_epoch", ownerEpoch}, {"control_needs_reconciliation", reconcile}, {"message", ""},
                    {"controller", QJsonDocument::fromJson(QByteArray::fromStdString(GetDAQ::EncodeEccControlControllerStatus(state))).object()}, {"data", data}};
                MetadataReply(path,connection,QJsonDocument(status).toJson(QJsonDocument::Compact)); return;
            }
            if (method=="GET" && path=="/v1/ownership") {
                connection->Reply(200,ownershipOverride);return;
            }
            if(method=="GET" && path=="/v1/configure-readbacks"){connection->Reply(200,readbacksOverride);return;}
            if (method=="GET" && path=="/v1/configuration") {
                GetDAQ::EccControlConfiguration c;c.instanceId=instance.toStdString();c.snapshotSha256=options.snapshotSha256.toStdString();c.inputBindingSha256=options.inputBindingSha256.toStdString();
                c.configurationSha256=std::string(64,'1');c.artifactSha256=std::string(64,'2');c.hasSelection=configurationSelected;
                if (c.hasSelection) {
                    auto &s=c.selection;s.registrationSha256=std::string(64,'0');auto &r=s.registration;r.repositoryId=repositoryId.toStdString();
                    r.publicationSha256=r.effectiveSha256=r.snapshotSha256=std::string(64,'3');r.configurationSha256=c.configurationSha256;r.artifactSha256=c.artifactSha256;r.manifestFile="/repository/version/publication.json";
                }
                connection->Reply(200,configurationOverride.isEmpty() ? QByteArray::fromStdString(GetDAQ::EncodeEccControlConfiguration(c)) : configurationOverride);return;
            }
            if(method=="GET" && path=="/v1/sources") {
                const auto value=QByteArray::fromStdString(SourceObservationFixture::Root(SourceObservationFixture::Row("C0",std::string(64,'d')),instance.toStdString(),options.snapshotSha256.toStdString()));
                connection->Reply(200,sourceOverride.isEmpty() ? value : sourceOverride);return;
            }
            if (method=="GET" && path=="/v1/receivers") {
                const QJsonObject observation{{"schema",1},{"kind","getdaq.ecc-receivers"},{"instance_id",instance},
                    {"snapshot_sha256",options.snapshotSha256},{"data_binding_sha256",QString(64,'3')},{"nodes",QJsonArray()}};
                connection->Reply(200,receiverOverride.isEmpty() ? QJsonDocument(observation).toJson(QJsonDocument::Compact) : receiverOverride);return;
            }
            if (method=="GET" && path.startsWith("/v1/receiver-events/")) {
                ++receiverEventsGets;receiverEventsPath=path;
                QJsonObject receiver{{"agent_id","receiver-0"},{"plan_sha256",QString(64,'7')},
                    {"instance_id",QString(64,'6')},{"source_id","C0"},{"session_id",QString(64,'8')},
                    {"pid",4242},{"available",true},{"acquisition_events","9007199254740993"},
                    {"observed_at_unix_ms","1700000000000"}};
                QJsonObject observation{{"schema",1},{"kind","getdaq.ecc-receiver-events"},
                    {"instance_id",instance},{"snapshot_sha256",options.snapshotSha256},
                    {"data_binding_sha256",QString(64,'3')},{"events",receiver}};
                connection->Reply(200,receiverEventsOverride.isEmpty() ?
                    QJsonDocument(observation).toJson(QJsonDocument::Compact) : receiverEventsOverride);return;
            }
            if (method=="GET" && path=="/v1/receiver-failure") {
                MetadataReply(path,connection,receiverFailureOverride.isEmpty() ? ReceiverFailureFixture::Waiting(instance,options.snapshotSha256) : receiverFailureOverride);return;
            }
            if (method=="GET" && path=="/v1/alarms") {
                QJsonObject node{{"id","C0"},{"subscription",2},{"received","9007199254740993"},{"durable","9007199254740993"},{"lost","0"},{"invalid","0"},
                    {"pending",1},{"alarm_mask",alarmHeld ? 1 : 0},{"stop_requested",alarmHeld},{"queue_overflow",false},{"audit_failed",false},{"budget_exceeded",false}};
                QJsonObject observation{{"controller_state",5},{"controller_outcome_unknown",false},{"controller_operation_id","start"},{"stop_requested",alarmHeld},{"nodes",QJsonArray{node}}};
                QJsonObject alarm{{"schema",1},{"kind","getdaq.ecc-alarms"},{"instance_id",alarmInstanceMismatch ? QString(64,'9') : instance},{"snapshot_sha256",options.snapshotSha256},
                    {"enabled",true},{"latched",alarmHeld},{"operation_id",alarmHeld ? "internal-alarm-fixture" : ""},{"phase",alarmHeld ? "complete" : "watching"},{"observation",observation}};
                MetadataReply(path,connection,QJsonDocument(alarm).toJson(QJsonDocument::Compact)); return;
            }
            if(method=="GET" && path=="/v1/cobo-maintenance-targets") {MetadataReply(path,connection,maintenanceOverride);return;}
            if(method=="GET" && path=="/v1/control-handoff") {
                ++handoffGets;
                for(const auto &request:requests)if(request.command=="retire-data-plans" && complete) {
                    MetadataReply(path,connection,handoffOverride.isEmpty() ? PlanTransitionFixture::Receipt(request) : handoffOverride);return;
                }
                connection->Reply(503,"{}");return;
            }
            if(method=="POST" && path=="/v1/scan-runtime-identity") {
                ++scanIdentityPosts;
                const auto scanRequest=GetDAQ::DecodeScanRuntimePointIdentityRequest(body.toStdString());
                const auto expected=ScanExpectation(scanRequest);
                connection->Reply(200,scanIdentityOverride.isEmpty()
                    ? QByteArray::fromStdString(GetDAQ::EncodeScanRuntimePointIdentityResponse(ScanResponse(expected)))
                    : scanIdentityOverride);
                return;
            }
            GetDAQ::EccControlRequest request;
            if (method == "POST" && path == "/v1/operations") {
                ++posts; request = GetDAQ::DecodeEccControlRequest(body.toStdString());
                QFile intent(directory.filePath("intent.json")); persistedBeforePost = persistedBeforePost && intent.open(QIODevice::ReadOnly) && intent.readAll() == body;
                const auto id = QString::fromStdString(request.operationId);
                if (!requests.contains(id)) { requests[id] = request; ++commands; }
                if (dropReply) { connection->findChild<QTcpSocket *>()->disconnectFromHost(); return; }
            } else if (method == "GET" && path.startsWith("/v1/operations/")) {
                const auto id = path.mid(QString("/v1/operations/").size());
                if (!requests.contains(id)) { connection->Reply(404, "{}"); return; }
                if (dropObservationOnce) { dropObservationOnce=false; connection->findChild<QTcpSocket *>()->disconnectFromHost(); return; }
                request = requests[id];
            } else { connection->Reply(404, "{}"); return; }
            GetDAQ::EccControlOperation operation; operation.request = request; operation.requestKey = GetDAQ::EccControlRequestKey(request); operation.phase = complete ? "complete" : "accepted";
            if (complete) { GetDAQ::EccControlControllerStatus status; status.state = request.command=="retire-data-plans" ? 1 : (request.command.find("reconcile-")==0 || request.command=="release-stopped-alarm") ? recoveryReplyState : request.command=="describe" ? 2 : request.command=="prepare" ? 3 : request.command=="configure" ? 4 : 5; status.operationId = request.operationId; status.outcomeUnknown = unknown; status.needsReconciliation = unknown;
                if(request.command=="retire-data-plans")status.needsReconciliation=true;
                operation.resultJson = GetDAQ::EncodeEccControlControllerStatus(status); }
            connection->Reply(method == "POST" ? 202 : 200, QByteArray::fromStdString(GetDAQ::EncodeEccControlOperation(operation)));
        });
    }
    void WriteCredential(const QByteArray &bytes) {
        QFile file(options.credentialFile); if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(bytes + '\n') != 65) throw std::runtime_error("Fixture credential write");
        file.close(); if (!file.setPermissions(QFile::ReadOwner | QFile::WriteOwner)) throw std::runtime_error("Fixture credential permissions");
    }
    EccSystemClient::IntentWriter Writer() {
        return [this](const std::string &bytes, QString *error) {
            QSaveFile file(directory.filePath("intent.json")); const auto body = QByteArray::fromStdString(bytes);
            const bool ok = file.open(QIODevice::WriteOnly) && file.write(body) == body.size() && file.commit();
            if (!ok) *error = "Test intent write failed";
            return ok;
        };
    }
    GetDAQ::EccControlRequest Saved() {
        QFile file(directory.filePath("intent.json")); if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Fixture saved request missing");
        return GetDAQ::DecodeEccControlRequest(file.readAll().toStdString());
    }
    static std::string Hash(char value) { return std::string(64,value); }
    GetDAQ::ScanRuntimeIdentityExpectation ScanExpectation(
        const GetDAQ::ScanRuntimePointIdentityRequest &request) const {
        GetDAQ::ScanRuntimeIdentityExpectation expected;
        expected.executionSnapshotSha256=request.executionSnapshotSha256;
        expected.pointIndex=request.pointIndex;
        expected.serviceId=options.serviceId.toStdString();
        expected.clientId=options.clientId.toStdString();
        expected.instanceId=instance.toStdString();
        expected.snapshotSha256=options.snapshotSha256.toStdString();
        expected.inputBindingSha256=options.inputBindingSha256.toStdString();
        expected.configurationSha256=request.configurationSha256;
        expected.artifactSha256=request.artifactSha256;
        expected.dataBindingSha256=Hash('3');
        expected.dataScopeSha256=Hash('4');
        expected.dataEpoch=Hash('5');
        expected.dataNodes.push_back({"receiver-0",Hash('6'),Hash('7')});
        return expected;
    }
    static GetDAQ::ScanRuntimePointIdentityResponse ScanResponse(
        const GetDAQ::ScanRuntimeIdentityExpectation &expected) {
        GetDAQ::ScanRuntimePointIdentityResponse response;
        response.expectation=expected;
        response.identityJson="{\"schema\":1,\"service\":\"getdaq-ecc-server\",\"service_id\":\""+expected.serviceId+
            "\",\"client_id\":\""+expected.clientId+"\",\"instance_id\":\""+expected.instanceId+
            "\",\"snapshot_sha256\":\""+expected.snapshotSha256+"\",\"input_binding_sha256\":\""+expected.inputBindingSha256+
            "\",\"configuration_sha256\":\""+expected.configurationSha256+"\",\"artifact_sha256\":\""+expected.artifactSha256+
            "\",\"data_binding_sha256\":\""+expected.dataBindingSha256+"\"}";
        const auto &node=expected.dataNodes.front();
        const std::string lifecycle="{\"schema\":1,\"state\":\"fresh\",\"needs_reconciliation\":false,\"plan_sha256\":\""+
            node.planSha256+"\",\"session_id\":\"\",\"message\":\"\",\"receivers\":[]}";
        const std::string statusNode="{\"agent_id\":\""+node.agentId+"\",\"instance_id\":\""+node.instanceId+
            "\",\"plan_sha256\":\""+node.planSha256+"\",\"lifecycle\":"+lifecycle+"}";
        response.statusJson="{\"schema\":1,\"service\":\"getdaq-ecc-server\",\"instance_id\":\""+expected.instanceId+
            "\",\"snapshot_sha256\":\""+expected.snapshotSha256+"\",\"operation_active\":false,\"operation_id\":\"\",\"client_epoch\":\"\","
            "\"control_needs_reconciliation\":false,\"message\":\"\",\"controller\":{\"state\":1,\"transition\":0,\"error\":0,"
            "\"outcome_unknown\":false,\"needs_reconciliation\":false,\"operation_id\":\"\",\"message\":\"\"},\"data\":{"
            "\"needs_reconciliation\":false,\"controller_scope_sha256\":\""+expected.dataScopeSha256+"\",\"controller_epoch\":\""+
            expected.dataEpoch+"\",\"message\":\"\",\"nodes\":["+statusNode+"]}}";
        const std::string receiverNode="{\"agent_id\":\""+node.agentId+"\",\"plan_sha256\":\""+node.planSha256+
            "\",\"instance_id\":\""+node.instanceId+"\",\"available\":true,\"message\":\"\",\"observation\":{\"schema\":1,"
            "\"busy\":false,\"observed_at_unix_ms\":\"1000\",\"lifecycle\":"+lifecycle+",\"receivers\":[]}}";
        response.receiversJson="{\"schema\":1,\"kind\":\"getdaq.ecc-receivers\",\"instance_id\":\""+expected.instanceId+
            "\",\"snapshot_sha256\":\""+expected.snapshotSha256+"\",\"data_binding_sha256\":\""+expected.dataBindingSha256+
            "\",\"nodes\":["+receiverNode+"]}";
        return response;
    }
};

class InProcessScanRecoveryFixture
{
public:
    QTemporaryDir directory;
    EccSystemClientOptions options;
    GetDAQ::EccControlRequest request;
    bool dropNextOperationGet=true;
    bool alarmLatched=false;

    InProcessScanRecoveryFixture() {
        if (!directory.isValid()) throw std::runtime_error("Cannot create in-process recovery fixture directory");
        options.address="127.0.0.1";options.port=1;options.timeoutMs=1000;
        options.serviceId="test-ecc";options.clientId="test-console";options.clientEpoch=QString(64,'d');
        options.snapshotSha256=QString(64,'b');options.inputBindingSha256=QString(64,'c');
        options.credentialFile=directory.filePath("credential");
        QFile credential(options.credentialFile);
        if(!credential.open(QIODevice::WriteOnly) || credential.write(QByteArray(64,'f'))!=64 || !credential.setPermissions(QFile::ReadOwner|QFile::WriteOwner))
            throw std::runtime_error("Cannot create in-process recovery credential");
        request.operationId="scan-configure-observation";request.command="configure";
        request.instanceId=QString(64,'a').toStdString();request.snapshotSha256=options.snapshotSha256.toStdString();
        request.clientId=options.clientId.toStdString();request.clientEpoch=options.clientEpoch.toStdString();
    }

    QByteArray Status() const {
        QJsonObject status=QJsonDocument::fromJson(PlanTransitionFixture::Status()).object();
        QJsonObject controller=status.value("controller").toObject();
        controller["state"]=4;controller["operation_id"]=QString::fromStdString(request.operationId);
        controller["outcome_unknown"]=false;controller["needs_reconciliation"]=false;
        status["controller"]=controller;status["operation_id"]=QString::fromStdString(request.operationId);
        status["operation_active"]=false;status["control_needs_reconciliation"]=false;
        return QJsonDocument(status).toJson(QJsonDocument::Compact);
    }

    QByteArray Operation(bool unknown=false) const {
        GetDAQ::EccControlOperation operation;operation.request=request;
        operation.requestKey=GetDAQ::EccControlRequestKey(request);operation.phase=unknown ? "unknown" : "complete";
        if(unknown)return QByteArray::fromStdString(GetDAQ::EncodeEccControlOperation(operation));
        GetDAQ::EccControlControllerStatus state;state.state=4;state.operationId=request.operationId;
        operation.resultJson=GetDAQ::EncodeEccControlControllerStatus(state);
        return QByteArray::fromStdString(GetDAQ::EncodeEccControlOperation(operation));
    }

    QByteArray Alarms() const {
        const QJsonObject observation{{"controller_state",4},{"controller_outcome_unknown",false},
            {"controller_operation_id",QString::fromStdString(request.operationId)},
            {"stop_requested",alarmLatched},{"nodes",QJsonArray()}};
        const QJsonObject alarm{{"schema",1},{"kind","getdaq.ecc-alarms"},{"instance_id",QString(64,'a')},
            {"snapshot_sha256",options.snapshotSha256},{"enabled",true},{"latched",alarmLatched},
            {"operation_id",alarmLatched ? QString("internal-alarm-fixture") : QString()},
            {"phase",alarmLatched ? QString("complete") : QString("watching")},{"observation",observation}};
        return QJsonDocument(alarm).toJson(QJsonDocument::Compact);
    }

    HttpClient::Transport Transport() {
        return [this](const QString &method,const QUrl &url,const QByteArray &,
                      const QByteArray &,const QMap<QByteArray,QByteArray> &headers) {
            HttpClientResponse response;
            if(headers.value("Authorization")!="Bearer "+QByteArray(64,'f')) {
                response.error="synthetic transport received an unexpected credential";return response;
            }
            if(method!="GET") {response.error="unexpected synthetic HTTP method";return response;}
            if(url.path()=="/v1/identity") response.body=PlanTransitionFixture::Identity();
            else if(url.path()=="/v1/status") response.body=Status();
            else if(url.path()=="/v1/alarms") response.body=Alarms();
            else if(url.path()=="/v1/operations/"+QString::fromStdString(request.operationId)) {
                if(dropNextOperationGet) {dropNextOperationGet=false;response.body=Operation(true);}
                else response.body=Operation();
            } else response.error="unexpected synthetic HTTP path";
            return response;
        };
    }
};
}
class TestEccSystemClient : public QObject
{
    Q_OBJECT
    void FreshStatus(EccSystemClient &client) {
        QSignalSpy status(&client, &EccSystemClient::StatusSignal);
        QVERIFY(client.QueryStatus()); QTRY_COMPARE(status.size(), 1); QVERIFY2(status.last()[1].toString().isEmpty(), qPrintable(status.last()[1].toString()));
    }
private slots:
    void scanRuntimeIdentityRequiresFullFrozenDeploymentExpectation() {
        Fixture f; EccSystemClient client(f.options,f.Writer());
        QSignalSpy identities(&client,&EccSystemClient::IdentitySignal);
        QSignalSpy observations(&client,&EccSystemClient::ScanRuntimeIdentitySignal);
        QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identities.size(),1); QVERIFY(identities.last()[0].toBool());
        GetDAQ::ScanRuntimePointIdentityRequest request;
        request.executionSnapshotSha256=Fixture::Hash('8');
        request.pointIndex=0;
        request.configurationSha256=Fixture::Hash('1');
        request.artifactSha256=Fixture::Hash('2');
        const auto expected=f.ScanExpectation(request);
        auto wrongRequest=request; wrongRequest.artifactSha256=Fixture::Hash('3');
        QVERIFY(!client.QueryScanRuntimeIdentity(wrongRequest,expected));
        QTRY_COMPARE(observations.size(),1);
        QVERIFY(observations.last()[0].toByteArray().isEmpty());
        QVERIFY(observations.last()[1].toString().contains("differs from request"));
        QCOMPARE(f.scanIdentityPosts,0);

        QVERIFY(client.QueryScanRuntimeIdentity(request,expected));
        QTRY_COMPARE(observations.size(),2);
        QVERIFY2(observations.last()[1].toString().isEmpty(),qPrintable(observations.last()[1].toString()));
        const auto response=GetDAQ::DecodeScanRuntimePointIdentityResponse(
            observations.last()[0].toByteArray().toStdString());
        QCOMPARE(response.expectation.dataNodes.size(),std::size_t(1));
        QCOMPARE(f.scanIdentityPosts,1);

        auto wrong=expected; wrong.dataScopeSha256=Fixture::Hash('9');
        QVERIFY(client.QueryScanRuntimeIdentity(request,wrong));
        QTRY_COMPARE(observations.size(),3);
        QVERIFY(observations.last()[0].toByteArray().isEmpty());
        QVERIFY(observations.last()[1].toString().contains("complete frozen deployment expectation"));
        QCOMPARE(f.scanIdentityPosts,2);

        QCOMPARE(f.scanIdentityPosts,2);
    }

    void selectedMaintenanceBindsMetadataAndPersistsBeforePost() {
        Fixture f;f.statusOverride=CoBoMaintenanceFixture::Status();EccSystemClient client(f.options,f.Writer());QSignalSpy targets(&client,&EccSystemClient::MaintenanceTargetsSignal);
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal);QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);QVERIFY(client.CanSubmit("start"));
        QVERIFY(!client.SubmitCoBoMaintenance("C0","no-review"));QCOMPARE(f.posts,0);
        QVERIFY(client.QueryMaintenanceTargets());QTRY_COMPARE(targets.size(),1);QVERIFY(targets.last()[1].toString().isEmpty());
        QVERIFY(!client.SubmitCoBoMaintenance("inactive","bad-target"));QCOMPARE(f.posts,0);
        QVERIFY(client.SubmitCoBoMaintenance("C1","selected-maintenance"));QTRY_COMPARE(f.posts,1);QVERIFY(f.persistedBeforePost);
        QCOMPARE(QString::fromStdString(f.requests["selected-maintenance"].coboId),QString("C1"));
        QVERIFY(!client.SubmitCoBoMaintenance("C0","duplicate-intent"));QCOMPARE(f.posts,1);
    }
    void selectedLedSubmissionPersistsExactPreparedTargetBeforePost() {
        Fixture f;f.statusOverride=CoBoMaintenanceFixture::Status();EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),targets(&client,&EccSystemClient::MaintenanceTargetsSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);QVERIFY(client.CanSubmit("set-cobo-led"));
        QVERIFY(!client.SubmitCoBoLed("C0",3,2,3,"led-before-review"));QCOMPARE(f.posts,0);
        QVERIFY(client.QueryMaintenanceTargets());QTRY_COMPARE(targets.size(),1);QVERIFY(targets.last()[1].toString().isEmpty());
        QVERIFY(!client.SubmitCoBoLed("inactive",0,0,0,"led-inactive"));QCOMPARE(f.posts,0);
        QVERIFY(client.SubmitCoBoLed("C0",3,2,3,"led-selected"));QTRY_COMPARE(f.posts,1);QVERIFY(f.persistedBeforePost);
        const auto request=f.requests["led-selected"];
        QCOMPARE(QString::fromStdString(request.command),QString("set-cobo-led"));
        QCOMPARE(QString::fromStdString(request.coboId),QString("C0"));
        QCOMPARE(request.ledAsadId,3u);QCOMPARE(request.ledType,2u);QCOMPARE(request.ledState,3u);
        QVERIFY(!client.SubmitCoBoLed("C0",3,2,3,"led-duplicate"));QCOMPARE(f.posts,1);
    }
    void selectedMaintenanceRejectsForeignTargetsAndWrongState() {
        Fixture f;f.statusOverride=CoBoMaintenanceFixture::Status(4);EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal);QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);QVERIFY(client.CanSubmit("start"));QVERIFY(!client.CanSubmit("reprepare-cobo"));
        auto body=QJsonDocument::fromJson(f.maintenanceOverride).object();body["instance_id"]=QString(64,'9');f.maintenanceOverride=QJsonDocument(body).toJson(QJsonDocument::Compact);
        QSignalSpy targets(&client,&EccSystemClient::MaintenanceTargetsSignal);QVERIFY(client.QueryMaintenanceTargets());QTRY_COMPARE(targets.size(),1);
        QVERIFY(!targets.last()[1].toString().isEmpty());QVERIFY(targets.last()[0].toByteArray().isEmpty());QVERIFY(!client.SubmitCoBoMaintenance("C0","foreign"));QCOMPARE(f.posts,0);
    }
    void maintenanceStoreRequiresKnownPreparedAndRetainsTarget() {
        Fixture f;const auto root=f.directory.filePath("maintenance-records");auto operation=CoBoMaintenanceFixture::Complete();QString error;
        {EccClientStore store(root,f.options);QVERIFY(store.WriteIntent(GetDAQ::EncodeEccControlRequest(operation.request),&error));
         QVERIFY(store.Observe(GetDAQ::EncodeEccControlOperation(operation),&error));}
        EccClientStore reopened(root,f.options);QVERIFY(reopened.Pending().empty());QVERIFY(!reopened.NeedsReconciliation());
        auto next=operation;next.request.operationId="next-maintenance";next.request.coboId="C1";next.requestKey=GetDAQ::EccControlRequestKey(next.request);
        QVERIFY(reopened.WriteIntent(GetDAQ::EncodeEccControlRequest(next.request),&error));
        GetDAQ::EccControlControllerStatus wrong;wrong.state=4;next.resultJson=GetDAQ::EncodeEccControlControllerStatus(wrong);
        QVERIFY(!reopened.Observe(GetDAQ::EncodeEccControlOperation(next),&error));
    }
    void catalogReplacementResumePolicyRequiresExactKnownPreparedFailure() {
        EccSystemClientOptions options;options.clientId="client";options.clientEpoch=QString(64,'d');options.snapshotSha256=QString(64,'b');
        GetDAQ::EccControlRequest request;request.operationId="replace-failed";request.command="catalog";
        request.instanceId=std::string(64,'a');request.clientId="client";request.clientEpoch=options.clientEpoch.toStdString();
        request.snapshotSha256=options.snapshotSha256.toStdString();request.catalogReplaceExisting=true;
        GetDAQ::EccControlOperation observed;observed.request=request;observed.phase="failed";
        GetDAQ::EccControlControllerStatus status;status.state=3;observed.resultJson=GetDAQ::EncodeEccControlControllerStatus(status);
        const QString id=QString::fromStdString(request.operationId);
        QMap<QString,GetDAQ::EccControlRequest> requests;requests.insert(id,request);
        QMap<QString,GetDAQ::EccControlOperation> observations;observations.insert(id,observed);
        QSet<QString> holds;
        QVERIFY(EccClientStoreRecoveryPolicy::CanResumeCatalogReplacement(request.operationId,requests,observations,holds,
            request.instanceId,options,true));
        QVERIFY(!EccClientStoreRecoveryPolicy::CanResumeCatalogReplacement(request.operationId,requests,observations,holds,
            request.instanceId,options,false));
        holds.insert("independent-failure");
        QVERIFY(!EccClientStoreRecoveryPolicy::CanResumeCatalogReplacement(request.operationId,requests,observations,holds,
            request.instanceId,options,true));
        holds.clear();
        auto unknown=status;unknown.outcomeUnknown=true;
        observed.resultJson=GetDAQ::EncodeEccControlControllerStatus(unknown);observations[id]=observed;
        QVERIFY(!EccClientStoreRecoveryPolicy::CanResumeCatalogReplacement(request.operationId,requests,observations,holds,
            request.instanceId,options,true));
    }
    void reviewedPlanRetirementPersistsAndHoldsEveryNewCommand() {
        Fixture f;f.statusOverride=PlanTransitionFixture::Status();f.complete=true;
        EccSystemClient client(f.options,f.Writer());QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),operations(&client,&EccSystemClient::OperationSignal),receipts(&client,&EccSystemClient::ControlHandoffSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);
        QVERIFY(client.SubmitPlanRetirement(PlanTransitionFixture::Successor(),PlanTransitionFixture::Targets(),"reviewed-retire"));
        QTRY_COMPARE(operations.size(),1);QCOMPARE(operations.last()[1].toString(),QString("complete"));QCOMPARE(f.posts,1);QCOMPARE(f.commands,1);QVERIFY(f.persistedBeforePost);QVERIFY(!client.Pending());
        const auto request=f.requests.value("reviewed-retire");QCOMPARE(QString::fromStdString(request.successorPlanFile),QString("/host/reviewed/successor.json"));
        for(const auto &command:{"start","stop","retire-data-plans","stop-production-after-failure","abort-failed-receivers"})QVERIFY(!client.CanSubmit(command));
        f.statusOverride=PlanTransitionFixture::Status(true);FreshStatus(client);QVERIFY(!client.CanSubmit("start"));
        const auto original=GetDAQ::DecodeEccControlSystemStatus(PlanTransitionFixture::Status(true).toStdString());
        QVERIFY(client.QueryControlHandoff(request,original));QTRY_COMPARE(receipts.size(),1);QVERIFY(receipts.last()[1].toString().isEmpty());QCOMPARE(receipts.last()[0].toByteArray(),PlanTransitionFixture::Receipt(request));QCOMPARE(f.handoffGets,1);
        auto independent=original;independent.controller.outcomeUnknown=true;QVERIFY_EXCEPTION_THROWN(GetDAQ::ValidateEccPlanRetirementReceipt(PlanTransitionFixture::Receipt(request).toStdString(),request,independent),std::invalid_argument);
        independent=original;independent.controller.error=1;QVERIFY_EXCEPTION_THROWN(GetDAQ::ValidateEccPlanRetirementReceipt(PlanTransitionFixture::Receipt(request).toStdString(),request,independent),std::invalid_argument);
        independent=original;independent.dataNeedsReconciliation=false;QVERIFY_EXCEPTION_THROWN(GetDAQ::ValidateEccPlanRetirementReceipt(PlanTransitionFixture::Receipt(request).toStdString(),request,independent),std::invalid_argument);
        auto wrong=request;wrong.instanceId=std::string(64,'6');QVERIFY(!client.QueryControlHandoff(wrong,original));QCOMPARE(f.handoffGets,1);
        auto bad=QJsonDocument::fromJson(PlanTransitionFixture::Receipt(request)).object();bad["successor_epoch"]=QString(64,'9');f.handoffOverride=QJsonDocument(bad).toJson(QJsonDocument::Compact);
        QVERIFY(client.QueryControlHandoff(request,original));QTRY_COMPARE(receipts.size(),3);QVERIFY(receipts.last()[0].toByteArray().isEmpty());QVERIFY(!receipts.last()[1].toString().isEmpty());QCOMPARE(f.posts,1);
        auto current=QJsonDocument::fromJson(PlanTransitionFixture::Status(true)).object();auto controller=current["controller"].toObject();controller["outcome_unknown"]=true;current["controller"]=controller;
        f.statusOverride=QJsonDocument(current).toJson(QJsonDocument::Compact);FreshStatus(client);
        const auto frozen=GetDAQ::DecodeEccControlSystemStatus(PlanTransitionFixture::Status().toStdString());
        QVERIFY(!client.QueryControlHandoff(request,frozen));QCOMPARE(receipts.size(),4);QCOMPARE(f.handoffGets,2);QCOMPARE(f.posts,1);
    }
    void retirementRecordReopensAndReceiptCannotBeOverwritten() {
        Fixture f;const auto root=f.directory.filePath("records");const auto request=PlanTransitionFixture::Request();QString error,path;
        GetDAQ::EccControlControllerStatus status;status.state=1;status.operationId="old-reset";status.needsReconciliation=true;
        GetDAQ::EccControlOperation operation;operation.request=request;operation.requestKey=GetDAQ::EccControlRequestKey(request);operation.phase="complete";operation.resultJson=GetDAQ::EncodeEccControlControllerStatus(status);
        {
            EccClientStore store(root,f.options);QVERIFY2(store.WriteIntent(GetDAQ::EncodeEccControlRequest(request),&error),qPrintable(error));QVERIFY(store.Observe(GetDAQ::EncodeEccControlOperation(operation),&error));
            QVERIFY(store.SavePlanRetirementReceipt(request,GetDAQ::DecodeEccControlSystemStatus(PlanTransitionFixture::Status(true).toStdString()),PlanTransitionFixture::Receipt(request).toStdString(),&path,&error));
            auto another=request;another.operationId="another-retirement";QVERIFY(!store.WriteIntent(GetDAQ::EncodeEccControlRequest(another),&error));
        }
        EccClientStore reopened(root,f.options);GetDAQ::EccControlRequest retained;bool complete=false;QVERIFY(reopened.PlanRetirement(&retained,&complete));QVERIFY(complete);QCOMPARE(GetDAQ::EncodeEccControlRequest(retained),GetDAQ::EncodeEccControlRequest(request));
        QVERIFY(reopened.SavePlanRetirementReceipt(request,GetDAQ::DecodeEccControlSystemStatus(PlanTransitionFixture::Status(true).toStdString()),PlanTransitionFixture::Receipt(request).toStdString(),&path,&error));
        QFile changed(path);QVERIFY(changed.open(QIODevice::WriteOnly|QIODevice::Truncate));changed.write("changed");changed.close();
        QVERIFY(!reopened.SavePlanRetirementReceipt(request,GetDAQ::DecodeEccControlSystemStatus(PlanTransitionFixture::Status(true).toStdString()),PlanTransitionFixture::Receipt(request).toStdString(),&path,&error));QVERIFY(reopened.NeedsReconciliation());
    }
    void partialRetirementContextSurvivesAndCompletionPreservesOriginalFailure() {
        Fixture f;const auto root=f.directory.filePath("partial-records");const auto original=PlanTransitionFixture::Request();
        const auto completion=PlanTransitionFixture::Completion(original);QString error,path;
        {
            EccClientStore store(root,f.options);
            QVERIFY2(store.WriteIntent(GetDAQ::EncodeEccControlRequest(original),&error,PlanTransitionFixture::Status().toStdString()),qPrintable(error));
            QVERIFY(!store.CanCompletePlanRetirement());
            QVERIFY(store.Observe(PlanTransitionFixture::Operation(original,"failed"),&error));QVERIFY(store.CanCompletePlanRetirement());
            auto wrong=completion;wrong.failedOperationId="different-parent";QVERIFY(!store.WriteIntent(GetDAQ::EncodeEccControlRequest(wrong),&error));
            QVERIFY(store.WriteIntent(GetDAQ::EncodeEccControlRequest(completion),&error));QVERIFY(!store.CanCompletePlanRetirement());
            QVERIFY(!store.WriteIntent(GetDAQ::EncodeEccControlRequest(PlanTransitionFixture::Completion(original,"duplicate")),&error));
            QVERIFY(store.Observe(PlanTransitionFixture::Operation(completion,"failed"),&error));QVERIFY(store.CanCompletePlanRetirement());
        }
        EccClientStore reopened(root,f.options);QVERIFY(reopened.CanCompletePlanRetirement());std::string context,phase;
        QVERIFY(reopened.PlanRetirementContext(&context));QCOMPARE(context,PlanTransitionFixture::Status().toStdString());
        GetDAQ::EccControlRequest last;QVERIFY(reopened.PlanRetirementCompletion(&last,&phase));QCOMPARE(phase,std::string("failed"));
        const auto next=PlanTransitionFixture::Completion(original,"second-completion");QVERIFY(reopened.WriteIntent(GetDAQ::EncodeEccControlRequest(next),&error));
        QVERIFY(reopened.Observe(PlanTransitionFixture::Operation(next,"complete"),&error));QVERIFY(!reopened.CanCompletePlanRetirement());
        bool complete=false;QVERIFY(reopened.PlanRetirement(&last,&complete));QVERIFY(complete);QCOMPARE(last.operationId,original.operationId);
        QVERIFY(reopened.NeedsReconciliation()); // Completion must never clear the old controller fence.
        QVERIFY(reopened.Observe(PlanTransitionFixture::Operation(original,"failed"),&error)); // Original terminal failure remains unchanged.
        QVERIFY2(reopened.SavePlanRetirementReceipt(original,GetDAQ::DecodeEccControlSystemStatus(context),PlanTransitionFixture::Receipt(original).toStdString(),&path,&error),qPrintable(error));
        auto positive=original;positive.operationId="new-describe";positive.command="describe";positive.successorPlanFile.clear();positive.successorPlanSha256.clear();positive.successorScopeSha256.clear();positive.successorEpoch.clear();positive.successorDataBindingSha256.clear();positive.successorReceiverPlans.clear();
        QVERIFY(!reopened.WriteIntent(GetDAQ::EncodeEccControlRequest(positive),&error));
    }
    void completedPartialClientReopensForReceiptButNeverForNewControl() {
        Fixture f;const auto root=f.directory.filePath("completed-partial");const auto original=PlanTransitionFixture::Request();
        const auto completion=PlanTransitionFixture::Completion(original);QString error,path;
        {
            EccClientStore store(root,f.options);QVERIFY(store.WriteIntent(GetDAQ::EncodeEccControlRequest(original),&error,PlanTransitionFixture::Status().toStdString()));
            QVERIFY(store.Observe(PlanTransitionFixture::Operation(original,"failed"),&error));
            QVERIFY(store.WriteIntent(GetDAQ::EncodeEccControlRequest(completion),&error));QVERIFY(store.Observe(PlanTransitionFixture::Operation(completion,"complete"),&error));
        }
        EccClientStore reopened(root,f.options);bool complete=false;GetDAQ::EccControlRequest saved;std::string context,phase;
        QVERIFY(reopened.PlanRetirement(&saved,&complete));QVERIFY(complete);QVERIFY(!reopened.CanCompletePlanRetirement());QVERIFY(reopened.NeedsReconciliation());
        QVERIFY(reopened.PlanRetirementContext(&context));QVERIFY(reopened.PlanRetirementCompletion(&saved,&phase));QCOMPARE(phase,std::string("complete"));QCOMPARE(saved.operationId,completion.operationId);
        QVERIFY(reopened.Pending().empty());QVERIFY(!reopened.WriteIntent(GetDAQ::EncodeEccControlRequest(PlanTransitionFixture::Completion(original,"new-attempt")),&error));
        QVERIFY(reopened.SavePlanRetirementReceipt(original,GetDAQ::DecodeEccControlSystemStatus(context),PlanTransitionFixture::Receipt(original).toStdString(),&path,&error));
    }
    void partialRetirementCannotInventMissingOrChangedContext() {
        Fixture f;QString error;const auto original=PlanTransitionFixture::Request();
        {
            EccClientStore legacy(f.directory.filePath("legacy"),f.options);QVERIFY(legacy.WriteIntent(GetDAQ::EncodeEccControlRequest(original),&error));
            QVERIFY(legacy.Observe(PlanTransitionFixture::Operation(original,"failed"),&error));QVERIFY(!legacy.CanCompletePlanRetirement());
            QVERIFY(!legacy.WriteIntent(GetDAQ::EncodeEccControlRequest(PlanTransitionFixture::Completion(original)),&error));
        }
        {
            EccClientStore late(f.directory.filePath("late"),f.options);
            QVERIFY(!late.WriteIntent(GetDAQ::EncodeEccControlRequest(original),&error,PlanTransitionFixture::Status(true).toStdString()));
            QVERIFY(late.Pending().empty());
        }
        const auto root=f.directory.filePath("changed");
        {EccClientStore store(root,f.options);QVERIFY(store.WriteIntent(GetDAQ::EncodeEccControlRequest(original),&error,PlanTransitionFixture::Status().toStdString()));}
        QFile changed(root+"/plan-retirement-context.json");QVERIFY(changed.open(QIODevice::Append));changed.write(" ");changed.close();
        QVERIFY_EXCEPTION_THROWN(EccClientStore(root,f.options),std::runtime_error);
    }
    void lateContextJournalRecordCannotRetrofitAnOldRetirement() {
        Fixture f;QString error;const auto root=f.directory.filePath("late-journal");const auto context=PlanTransitionFixture::Status();
        const auto original=PlanTransitionFixture::Request();
        {EccClientStore store(root,f.options);QVERIFY(store.WriteIntent(GetDAQ::EncodeEccControlRequest(original),&error));QVERIFY(store.Observe(PlanTransitionFixture::Operation(original,"failed"),&error));}
        {QFile file(root+"/plan-retirement-context.json");QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::NewOnly));QVERIFY(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner));QCOMPARE(file.write(context),qint64(context.size()));}
        QFile journal(root+"/observations.jsonl");QVERIFY(journal.open(QIODevice::Append));
        QJsonObject row{{"schema",1},{"sequence",2},{"kind","retirement-context"},{"request_key",QString::fromStdString(GetDAQ::EccControlRequestKey(original))},
            {"payload",QString::fromLatin1(QCryptographicHash::hash(context,QCryptographicHash::Sha256).toHex())}};
        const auto bytes=QJsonDocument(row).toJson(QJsonDocument::Compact)+"\n";QCOMPARE(journal.write(bytes),qint64(bytes.size()));journal.close();
        QVERIFY_EXCEPTION_THROWN(EccClientStore(root,f.options),std::runtime_error);
    }
    void typedCompletionIsOnceOnlyAndNeverReopensNormalControl() {
        Fixture f;f.statusOverride=PlanTransitionFixture::Status(true);const auto original=PlanTransitionFixture::Request();
        EccSystemClient client(f.options,f.Writer());client.RetainPlanRetirement(original);
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),operations(&client,&EccSystemClient::OperationSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);QVERIFY(client.CanSubmit("complete-data-handoff"));
        QVERIFY(!client.Submit("complete-data-handoff","untyped",QString(),0,QString::fromStdString(original.operationId)));
        f.dropReply=true;QVERIFY(client.SubmitPlanRetirementCompletion(original,"partial-once"));QTRY_COMPARE(operations.size(),1);
        QCOMPARE(f.posts,1);QVERIFY(f.persistedBeforePost);const auto saved=f.Saved();QCOMPARE(saved.failedOperationId,original.operationId);
        QVERIFY(!client.SubmitPlanRetirementCompletion(original,"another"));QVERIFY(!client.Submit("describe","new-control"));
        f.dropReply=false;QVERIFY(client.Observe(saved));QTRY_COMPARE(operations.size(),2);QCOMPARE(f.posts,1);
        auto other=original;other.instanceId=std::string(64,'9');QVERIFY_EXCEPTION_THROWN(client.RetainPlanRetirement(other),std::invalid_argument);
    }
    void longRetirementIntentSurvivesClientJournalReload() {
        Fixture f;auto request=PlanTransitionFixture::Request();request.successorReceiverPlans.clear();
        for(int i=0;i<12;++i) {GetDAQ::EccReceiverPlanTarget target;target.agentId=QString("node-%1").arg(i,2,10,QLatin1Char('0')).toStdString();target.planSha256=std::string(64,'4');target.planFile="/node/reviewed/"+std::string(260,'a');request.successorReceiverPlans.push_back(target);}
        const auto bytes=GetDAQ::EncodeEccControlRequest(request);QVERIFY(bytes.size()>4096 && bytes.size()<=8192);QString error;const auto root=f.directory.filePath("long-records");
        {EccClientStore store(root,f.options);QVERIFY2(store.WriteIntent(bytes,&error),qPrintable(error));}
        EccClientStore reopened(root,f.options);const auto pending=reopened.Pending();QCOMPARE(pending.size(),std::size_t(1));QCOMPARE(GetDAQ::EncodeEccControlRequest(pending[0]),bytes);
    }
    void sourceRecoveryRequiresOriginalAndForbidsAlarmBinding() {
        Fixture f;f.reconcile=true;f.complete=true;EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),operations(&client,&EccSystemClient::OperationSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);
        QVERIFY(client.Submit("reconcile-acquisition-with-sources","source-recovery",QString(),0,"original-start"));
        QTRY_COMPARE(operations.size(),1);QVERIFY(operations.last()[3].toString().isEmpty());
        auto request=f.Saved();QCOMPARE(QString::fromStdString(request.failedOperationId),QString("original-start"));
        request.failedOperationId.clear();QVERIFY_EXCEPTION_THROWN(GetDAQ::EncodeEccControlRequest(request),std::invalid_argument);
        request.failedOperationId="original-start";request.alarmOperationId="independent-alarm";
        QVERIFY_EXCEPTION_THROWN(GetDAQ::EncodeEccControlRequest(request),std::invalid_argument);
        request.alarmOperationId.clear();request.command="retire-failed-sources";
        QVERIFY_EXCEPTION_THROWN(GetDAQ::EncodeEccControlRequest(request),std::invalid_argument);
        request.failedOperationId.clear();QVERIFY(!GetDAQ::EncodeEccControlRequest(request).empty());QCOMPARE(f.posts,1);
    }
    void stoppedAlarmReleaseProtocol_data() {
        QTest::addColumn<bool>("automatic");
        QTest::newRow("normal-stop") << true;
        QTest::newRow("known-ready") << false;
    }
    void stoppedAlarmReleaseProtocol() {
        QFETCH(bool,automatic);Fixture f;f.reconcile=true;f.complete=true;
        EccSystemClient client(f.options,f.Writer());QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),operations(&client,&EccSystemClient::OperationSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);
        const auto original=automatic ? QString("internal-alarm-original") : QString("original-configure");
        QVERIFY(client.Submit("release-stopped-alarm","new-release",QString(),0,original,automatic ? original : QString()));
        QTRY_COMPARE(operations.size(),1);QVERIFY(operations.last()[3].toString().isEmpty());
        auto request=f.Saved();QCOMPARE(QString::fromStdString(request.failedOperationId),original);
        QCOMPARE(QString::fromStdString(request.alarmOperationId),automatic ? original : QString());
        request.alarmOperationId=request.operationId;QVERIFY_EXCEPTION_THROWN(GetDAQ::EncodeEccControlRequest(request),std::invalid_argument);
        request.alarmOperationId=automatic ? original.toStdString() : std::string();request.failedOperationId=request.operationId;
        QVERIFY_EXCEPTION_THROWN(GetDAQ::EncodeEccControlRequest(request),std::invalid_argument);
        QCOMPARE(f.posts,1);
    }
    void stoppedAlarmReleaseCannotClearIndependentUnknown() {
        Fixture f;QString error;const auto path=f.directory.filePath("stopped-alarm-records");
        GetDAQ::EccControlRequest request;request.instanceId=f.instance.toStdString();request.snapshotSha256=f.options.snapshotSha256.toStdString();
        request.clientId=f.options.clientId.toStdString();request.clientEpoch=f.options.clientEpoch.toStdString();
        {
            EccClientStore store(path,f.options);request.command="stop-production-after-failure";request.operationId="independent-unknown";
            QVERIFY(store.WriteIntent(GetDAQ::EncodeEccControlRequest(request),&error));
            GetDAQ::EccControlOperation op;op.request=request;op.requestKey=GetDAQ::EccControlRequestKey(request);op.phase="failed";
            GetDAQ::EccControlControllerStatus state;state.state=4;state.outcomeUnknown=state.needsReconciliation=true;state.operationId="original-start";
            op.resultJson=GetDAQ::EncodeEccControlControllerStatus(state);QVERIFY(store.Observe(GetDAQ::EncodeEccControlOperation(op),&error));
            request.command="release-stopped-alarm";request.operationId="verified-release";request.failedOperationId=request.alarmOperationId="internal-alarm-original";
            QVERIFY(store.WriteIntent(GetDAQ::EncodeEccControlRequest(request),&error));
            op.request=request;op.requestKey=GetDAQ::EccControlRequestKey(request);op.phase="complete";
            state.state=2;state.outcomeUnknown=state.needsReconciliation=false;state.operationId=request.operationId;
            op.resultJson=GetDAQ::EncodeEccControlControllerStatus(state);QVERIFY(store.Observe(GetDAQ::EncodeEccControlOperation(op),&error));QVERIFY(store.NeedsReconciliation());
        }
        EccClientStore reopened(path,f.options);QVERIFY(reopened.NeedsReconciliation());
    }
    void alarmAcknowledgementProtocolRequiresOriginalBinding_data() {
        QTest::addColumn<QString>("command");
        QTest::newRow("acquisition") << QString("reconcile-acquisition-with-alarm");
        QTest::newRow("preparation") << QString("reconcile-preparation-with-alarm");
    }
    void alarmAcknowledgementProtocolRequiresOriginalBinding() {
        QFETCH(QString,command);
        Fixture f;f.reconcile=true;f.complete=true;EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),operations(&client,&EccSystemClient::OperationSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);
        QVERIFY(client.Submit(command,"acknowledged-recovery",QString(),0,"original-start","internal-alarm-original"));
        QTRY_COMPARE(operations.size(),1);QVERIFY(operations.last()[3].toString().isEmpty());
        auto request=f.Saved();QCOMPARE(QString::fromStdString(request.alarmOperationId),QString("internal-alarm-original"));
        QCOMPARE(QString::fromStdString(request.failedOperationId),QString("original-start"));
        request.alarmOperationId.clear();QVERIFY_EXCEPTION_THROWN(GetDAQ::EncodeEccControlRequest(request),std::invalid_argument);
        request.alarmOperationId=request.operationId;QVERIFY_EXCEPTION_THROWN(GetDAQ::EncodeEccControlRequest(request),std::invalid_argument);
        request.alarmOperationId="internal-alarm-original";request.command="reconcile-acquisition";
        QVERIFY_EXCEPTION_THROWN(GetDAQ::EncodeEccControlRequest(request),std::invalid_argument);
        QCOMPARE(f.posts,1);QCOMPARE(f.commands,1);
    }
    void preparationAlarmRecoveryPreservesUnconfirmedCleanup_data() {
        QTest::addColumn<bool>("independent");
        QTest::newRow("bound") << false;
        QTest::newRow("unconfirmed") << true;
    }
    void preparationAlarmRecoveryPreservesUnconfirmedCleanup() {
        QFETCH(bool,independent);Fixture f;QString error;const auto root=f.directory.filePath("preparation-records");
        GetDAQ::EccControlRequest original;original.operationId="original-prepare";original.command="prepare";
        original.instanceId=f.instance.toStdString();original.snapshotSha256=f.options.snapshotSha256.toStdString();
        original.clientId=f.options.clientId.toStdString();original.clientEpoch=f.options.clientEpoch.toStdString();
        const auto record=[&](EccClientStore &store,GetDAQ::EccControlRequest request,const std::string &phase,bool held) {
            if(!store.WriteIntent(GetDAQ::EncodeEccControlRequest(request),&error))return false;
            GetDAQ::EccControlOperation op;op.request=request;op.requestKey=GetDAQ::EccControlRequestKey(request);op.phase=phase;
            GetDAQ::EccControlControllerStatus status;status.state=2;status.operationId=held ? original.operationId : request.operationId;
            status.outcomeUnknown=status.needsReconciliation=held;op.resultJson=GetDAQ::EncodeEccControlControllerStatus(status);
            return store.Observe(GetDAQ::EncodeEccControlOperation(op),&error);
        };
        {
            EccClientStore store(root,f.options);QVERIFY2(record(store,original,"failed",true),qPrintable(error));
            auto cleanup=original;cleanup.operationId="power-off";cleanup.command="unprepare-after-failure";
            QVERIFY2(record(store,cleanup,"complete",true),qPrintable(error));
            if(independent){cleanup.operationId="unconfirmed-power-off";QVERIFY(record(store,cleanup,"failed",true));}
            auto denied=original;denied.operationId="ordinary-denied";denied.command="reconcile-preparation";denied.failedOperationId=original.operationId;
            QVERIFY2(record(store,denied,"failed",true),qPrintable(error));
            auto recovery=denied;recovery.operationId="explicit-alarm-recovery";recovery.command="reconcile-preparation-with-alarm";recovery.alarmOperationId="internal-alarm-original";
            QVERIFY2(record(store,recovery,"complete",false),qPrintable(error));QCOMPARE(store.NeedsReconciliation(),independent);
        }
        EccClientStore reopened(root,f.options);QCOMPARE(reopened.NeedsReconciliation(),independent);
        auto fresh=original;fresh.operationId="fresh-prepare";
        QCOMPARE(reopened.WriteIntent(GetDAQ::EncodeEccControlRequest(fresh),&error),!independent);
    }
    void preRecoveryMetadataCannotRelatchAfterOwnCompletion_data() {
        QTest::addColumn<QString>("path");
        QTest::newRow("status") << QString("/v1/status");
        QTest::newRow("alarm") << QString("/v1/alarms");
        QTest::newRow("receiver") << QString("/v1/receiver-failure");
    }
    void preRecoveryMetadataCannotRelatchAfterOwnCompletion() {
        QFETCH(QString,path);Fixture f;f.options.timeoutMs=3000;f.options.alarmStatusEnabled=path=="/v1/alarms";
        f.options.receiverFailureStatusEnabled=path=="/v1/receiver-failure";
        f.reconcile=true;f.alarmHeld=true;
        f.receiverFailureOverride=ReceiverFailureFixture::SingleSourceCache(f.instance,f.options.snapshotSha256,QString(64,'3'));
        EccSystemClient client(f.options,f.Writer());QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),operations(&client,&EccSystemClient::OperationSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);
        QVERIFY(client.Submit("reconcile-acquisition-with-alarm","own-recovery",QString(),0,"original-start","internal-alarm-original"));
        QTRY_COMPARE(operations.size(),1);QVERIFY(client.Pending());
        QSignalSpy status(&client,&EccSystemClient::StatusSignal),alarms(&client,&EccSystemClient::AlarmSignal),receivers(&client,&EccSystemClient::ReceiverFailureSignal);
        f.heldMetadataPath=path;
        QVERIFY(path=="/v1/status" ? client.QueryStatus() : path=="/v1/alarms" ? client.QueryAlarms() : client.QueryReceiverFailure());
        QTRY_VERIFY(!f.heldMetadataConnection.isNull());
        f.complete=true;QVERIFY(client.Observe(f.Saved()));QTRY_COMPARE(operations.size(),2);QVERIFY(!client.Pending());
        QVERIFY(operations.last()[3].toString().isEmpty());
        f.reconcile=false;f.alarmHeld=false;f.receiverFailureOverride=ReceiverFailureFixture::Waiting(f.instance,f.options.snapshotSha256);
        f.heldMetadataPath.clear();f.heldMetadataConnection->Reply(200,f.heldMetadataBody);
        QSignalSpy &seen=path=="/v1/status" ? status : path=="/v1/alarms" ? alarms : receivers;
        QTRY_COMPARE(seen.size(),1);QVERIFY(seen.last()[0].toByteArray().isEmpty());QVERIFY(seen.last()[1].toString().startsWith("Pre-recovery "));
        FreshStatus(client);
        if(f.options.alarmStatusEnabled){QVERIFY(client.QueryAlarms());QTRY_COMPARE(alarms.size(),2);}
        QVERIFY(client.CanSubmit("prepare"));QCOMPARE(f.posts,1);QCOMPARE(f.commands,1);
    }
    void acquisitionRecoveryClearsOnlyConfirmedBoundCleanup_data() {
        QTest::addColumn<bool>("independentFailure");QTest::addColumn<bool>("alarmAck");QTest::addColumn<bool>("sourceAck");
        QTest::newRow("bound-cleanup") << false << false << false;
        QTest::newRow("failed-cleanup-remains-held") << true << false << false;
        QTest::newRow("ack-bound-cleanup") << false << true << false;
        QTest::newRow("ack-independent-cleanup-remains-held") << true << true << false;
        QTest::newRow("source-bound-cleanup") << false << false << true;
        QTest::newRow("source-independent-cleanup-held") << true << false << true;
    }
    void acquisitionRecoveryClearsOnlyConfirmedBoundCleanup() {
        QFETCH(bool, independentFailure);QFETCH(bool,alarmAck);QFETCH(bool,sourceAck);
        Fixture f;const auto root=f.directory.filePath("records");QString error;
        GetDAQ::EccControlRequest original;original.operationId="original-start";original.command="start";
        original.instanceId=f.instance.toStdString();original.snapshotSha256=f.options.snapshotSha256.toStdString();
        original.clientId=f.options.clientId.toStdString();original.clientEpoch=f.options.clientEpoch.toStdString();
        const auto observation=[&](const GetDAQ::EccControlRequest &request,const std::string &phase,bool held) {
            GetDAQ::EccControlOperation op;op.request=request;op.requestKey=GetDAQ::EccControlRequestKey(request);op.phase=phase;
            GetDAQ::EccControlControllerStatus status;status.state=request.command.find("reconcile-")==0 ? 2 : 5;
            status.operationId=held ? original.operationId : request.operationId;status.outcomeUnknown=status.needsReconciliation=held;
            op.resultJson=GetDAQ::EncodeEccControlControllerStatus(status);return GetDAQ::EncodeEccControlOperation(op);
        };
        {
            EccClientStore store(root,f.options);
            QVERIFY2(store.WriteIntent(GetDAQ::EncodeEccControlRequest(original),&error),qPrintable(error));
            QVERIFY2(store.Observe(observation(original,"complete",false),&error),qPrintable(error));
            auto cleanup=original;cleanup.operationId="original-cleanup";cleanup.command="unprepare-acquisition-after-failure";
            QVERIFY2(store.WriteIntent(GetDAQ::EncodeEccControlRequest(cleanup),&error),qPrintable(error));
            QVERIFY2(store.Observe(observation(cleanup,"complete",true),&error),qPrintable(error));
            if(sourceAck){auto source=cleanup;source.operationId="original-source-retirement";source.command="retire-failed-sources";
                QVERIFY2(store.WriteIntent(GetDAQ::EncodeEccControlRequest(source),&error),qPrintable(error));QVERIFY(store.Observe(observation(source,"complete",true),&error));}

            if(independentFailure) {
                auto failed=cleanup;failed.operationId="unconfirmed-extra-cleanup";
                QVERIFY2(store.WriteIntent(GetDAQ::EncodeEccControlRequest(failed),&error),qPrintable(error));
                QVERIFY2(store.Observe(observation(failed,"failed",true),&error),qPrintable(error));
            }
            QVERIFY(store.NeedsReconciliation());
            auto recovery=original;recovery.command=sourceAck ? "reconcile-acquisition-with-sources" : alarmAck ? "reconcile-acquisition-with-alarm" : "reconcile-acquisition";recovery.operationId="verified-acquisition";
            if(alarmAck)recovery.alarmOperationId="internal-alarm-original";
            recovery.failedOperationId=original.operationId;
            QVERIFY2(store.WriteIntent(GetDAQ::EncodeEccControlRequest(recovery),&error),qPrintable(error));
            QVERIFY2(store.Observe(observation(recovery,"complete",false),&error),qPrintable(error));
            QCOMPARE(store.NeedsReconciliation(),independentFailure);
            QVERIFY(store.Pending().empty());
        }
        EccClientStore reopened(root,f.options);
        QCOMPARE(reopened.NeedsReconciliation(),independentFailure);
        auto prepare=original;prepare.command="prepare";prepare.operationId="fresh-prepare";
        QCOMPARE(reopened.WriteIntent(GetDAQ::EncodeEccControlRequest(prepare),&error),!independentFailure);
    }
    void productionStoreAllowsReceiverAbortAfterConnectionCleanupHold() {
        Fixture f;const auto root=f.directory.filePath("records");QString error;
        GetDAQ::EccControlRequest connections;connections.operationId="failure-connections";connections.command="breakup-after-failure";
        connections.instanceId=f.instance.toStdString();connections.snapshotSha256=f.options.snapshotSha256.toStdString();
        connections.clientId=f.options.clientId.toStdString();connections.clientEpoch=f.options.clientEpoch.toStdString();
        const auto held=[&](const GetDAQ::EccControlRequest &request) {
            GetDAQ::EccControlOperation op;op.request=request;op.requestKey=GetDAQ::EccControlRequestKey(request);op.phase="complete";
            GetDAQ::EccControlControllerStatus status;status.state=5;status.operationId="original-start";status.outcomeUnknown=status.needsReconciliation=true;
            op.resultJson=GetDAQ::EncodeEccControlControllerStatus(status);return GetDAQ::EncodeEccControlOperation(op);
        };
        {EccClientStore store(root,f.options);QVERIFY(store.WriteIntent(GetDAQ::EncodeEccControlRequest(connections),&error));
            QVERIFY(store.Observe(held(connections),&error));QVERIFY(store.NeedsReconciliation());QVERIFY(store.Pending().empty());
            auto positive=connections;positive.operationId="forbidden-start";positive.command="start";QVERIFY(!store.WriteIntent(GetDAQ::EncodeEccControlRequest(positive),&error));
            auto abort=connections;abort.operationId="failure-receivers";abort.command="abort-failed-receivers";
            QVERIFY2(store.WriteIntent(GetDAQ::EncodeEccControlRequest(abort),&error),qPrintable(error));QVERIFY(store.Observe(held(abort),&error));
            QVERIFY(store.NeedsReconciliation());QVERIFY(store.Pending().empty());
            auto retired=connections;retired.operationId="failure-data-retirement";retired.command="retire-failed-receivers";
            QVERIFY2(store.WriteIntent(GetDAQ::EncodeEccControlRequest(retired),&error),qPrintable(error));QVERIFY(store.Observe(held(retired),&error));
            auto unprepared=connections;unprepared.operationId="failed-acquisition-unprepare";unprepared.command="unprepare-acquisition-after-failure";
            QVERIFY2(store.WriteIntent(GetDAQ::EncodeEccControlRequest(unprepared),&error),qPrintable(error));QVERIFY(store.Observe(held(unprepared),&error));
            QVERIFY(store.NeedsReconciliation());QVERIFY(store.Pending().empty());}
        {EccClientStore reopened(root,f.options);QVERIFY(reopened.NeedsReconciliation());QVERIFY(reopened.Pending().empty());
            auto positive=connections;positive.operationId="forbidden-configure";positive.command="configure";
            QVERIFY(!reopened.WriteIntent(GetDAQ::EncodeEccControlRequest(positive),&error));}
    }
    void productionStoreClearsOnlyBoundOriginalDescribeHold() {
        Fixture f;const auto root=f.directory.filePath("records");QString error;
        GetDAQ::EccControlRequest original;original.operationId="failed-describe";original.command="describe";original.instanceId=f.instance.toStdString();original.snapshotSha256=f.options.snapshotSha256.toStdString();original.clientId=f.options.clientId.toStdString();original.clientEpoch=f.options.clientEpoch.toStdString();
        auto failedOperation=[&](const GetDAQ::EccControlRequest &request,unsigned int state) {GetDAQ::EccControlOperation op;op.request=request;op.requestKey=GetDAQ::EccControlRequestKey(request);op.phase="failed";GetDAQ::EccControlControllerStatus status;status.state=state;status.operationId=request.operationId;status.outcomeUnknown=true;op.resultJson=GetDAQ::EncodeEccControlControllerStatus(status);return op;};
        auto recovery=original;recovery.operationId="recovery";recovery.command="reconcile-description";recovery.failedOperationId=original.operationId;
        GetDAQ::EccControlOperation completed;completed.request=recovery;completed.requestKey=GetDAQ::EccControlRequestKey(recovery);completed.phase="complete";GetDAQ::EccControlControllerStatus status;status.state=2;status.operationId=recovery.operationId;completed.resultJson=GetDAQ::EncodeEccControlControllerStatus(status);
        {EccClientStore store(root,f.options);QVERIFY(store.WriteIntent(GetDAQ::EncodeEccControlRequest(original),&error));QVERIFY(store.Observe(GetDAQ::EncodeEccControlOperation(failedOperation(original,1)),&error));QVERIFY(store.NeedsReconciliation());
         auto attempt=recovery;attempt.operationId="denied-recovery";QVERIFY(store.WriteIntent(GetDAQ::EncodeEccControlRequest(attempt),&error));auto denied=failedOperation(attempt,1);auto deniedStatus=GetDAQ::DecodeEccControlControllerStatus(denied.resultJson);deniedStatus.operationId=original.operationId;denied.resultJson=GetDAQ::EncodeEccControlControllerStatus(deniedStatus);QVERIFY(store.Observe(GetDAQ::EncodeEccControlOperation(denied),&error));QVERIFY(store.NeedsReconciliation());
         QVERIFY(store.WriteIntent(GetDAQ::EncodeEccControlRequest(recovery),&error));QVERIFY2(store.Observe(GetDAQ::EncodeEccControlOperation(completed),&error),qPrintable(error));QVERIFY(!store.NeedsReconciliation());
         auto prepare=original;prepare.operationId="prepare-after-recovery";prepare.command="prepare";QVERIFY(store.WriteIntent(GetDAQ::EncodeEccControlRequest(prepare),&error));QVERIFY(store.Observe(GetDAQ::EncodeEccControlOperation(failedOperation(prepare,2)),&error));QVERIFY(store.NeedsReconciliation());
         QVERIFY(store.Observe(GetDAQ::EncodeEccControlOperation(completed),&error));QVERIFY(store.NeedsReconciliation());}
        {EccClientStore reopened(root,f.options);QVERIFY(reopened.NeedsReconciliation());QVERIFY(reopened.Pending().empty());}
    }

    void historicalDescriptionRecoveryDoesNotClearLaterGate() {
        Fixture f;f.reconcile=true;f.complete=true;EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),operations(&client,&EccSystemClient::OperationSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);QVERIFY(client.CanSubmit("reconcile-description"));
        QVERIFY(client.Submit("reconcile-description","new-recovery",QString(),0,"failed-describe"));QTRY_COMPARE(operations.size(),1);QVERIFY(operations.last()[3].toString().isEmpty());
        const auto request=f.Saved();QVERIFY(!client.Pending());QVERIFY(!client.CanSubmit("prepare"));f.reconcile=false;FreshStatus(client);QVERIFY(client.CanSubmit("prepare"));
        f.reconcile=true;FreshStatus(client);QVERIFY(!client.CanSubmit("prepare"));QVERIFY(client.Observe(request));QTRY_COMPARE(operations.size(),2);
        f.reconcile=false;FreshStatus(client);QVERIFY(!client.CanSubmit("prepare"));QCOMPARE(f.posts,1);
    }
    void wrongDescriptionRecoveryTargetKeepsPendingIntent() {
        Fixture f;f.reconcile=true;f.complete=true;f.recoveryReplyState=5;EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),operations(&client,&EccSystemClient::OperationSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);
        QVERIFY(client.Submit("reconcile-description","wrong-recovery",QString(),0,"failed-describe"));QTRY_COMPARE(operations.size(),1);
        QVERIFY(!operations.last()[3].toString().isEmpty());QVERIFY(client.Pending());QVERIFY(!client.CanSubmit("prepare"));
    }

    void configureReadbacksAreHistoricalAndDoNotClearUnknown() {
        Fixture f;EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),seen(&client,&EccSystemClient::ConfigureReadbacksSignal);
        QVERIFY(!client.QueryConfigureReadbacks());QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);
        QVERIFY(client.QueryConfigureReadbacks());QVERIFY(!client.QueryConfigureReadbacks());QTRY_COMPARE(seen.size(),1);
        QVERIFY(seen.last()[1].toString().isEmpty());QVERIFY(client.CanSubmit("start"));
        QCOMPARE(GetDAQ::DecodeEccControlConfigureReadbacks(seen.last()[0].toByteArray().toStdString()).entries.size(),std::size_t(1));
        f.readbacksOverride=QByteArray::fromStdString(ConfigurationReadbackFixture::Cache(true));
        QVERIFY(client.QueryConfigureReadbacks());QTRY_COMPARE(seen.size(),2);FreshStatus(client);QVERIFY(!client.CanSubmit("start"));
        f.readbacksOverride=QByteArray::fromStdString(ConfigurationReadbackFixture::Cache(false));
        QVERIFY(client.QueryConfigureReadbacks());QTRY_COMPARE(seen.size(),3);FreshStatus(client);QVERIFY(!client.CanSubmit("start"));QCOMPARE(f.posts,0);
    }
    void configureReadbackBadClaimsAndBindingsAreRejected_data(){
        QTest::addColumn<QString>("change");QTest::newRow("identity")<<QString("identity");QTest::newRow("loading")<<QString("loading");QTest::newRow("coverage")<<QString("coverage");
    }
    void configureReadbackBadClaimsAndBindingsAreRejected(){
        QFETCH(QString,change);Fixture f;auto b=f.readbacksOverride;
        if(change=="identity")b.replace(QByteArray(64,'a'),QByteArray(64,'9'));
        else if(change=="loading")b.replace("\"actual_loaded_configuration_verified\":false","\"actual_loaded_configuration_verified\":true");
        else b.replace("\"writes_without_full_readback\":1","\"writes_without_full_readback\":0");
        f.readbacksOverride=b;EccSystemClient client(f.options,f.Writer());QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),seen(&client,&EccSystemClient::ConfigureReadbacksSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);QVERIFY(client.QueryConfigureReadbacks());QTRY_COMPARE(seen.size(),1);
        QVERIFY(!seen.last()[1].toString().isEmpty());QVERIFY(seen.last()[0].toByteArray().isEmpty());FreshStatus(client);QVERIFY(!client.CanSubmit("start"));QCOMPARE(f.posts,0);
    }
    void ownershipObservationIsReadOnlyAndFaultSticks() {
        Fixture f;
        const auto cache=[&](bool fault) {
            const QJsonObject ownership{{"enabled",true},{"scope","single-host-cooperating-ecc"},{"directory","/locks"},
                {"held_at_last_check",!fault},{"fault_latched",fault},{"cached",true},{"board_global_lock_verified",false},
                {"cross_host_lock_verified",false},{"resources",QJsonArray{"127.0.0.1"}}};
            return QJsonDocument(QJsonObject{{"schema",1},{"kind","getdaq.ecc-ownership"},{"instance_id",f.instance},
                {"snapshot_sha256",f.options.snapshotSha256},{"ownership",ownership}}).toJson(QJsonDocument::Compact);
        };
        f.ownershipOverride=cache(false);EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),ownership(&client,&EccSystemClient::OwnershipSignal);
        QVERIFY(!client.QueryOwnership());QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);
        QVERIFY(client.QueryOwnership());QVERIFY(!client.QueryOwnership());QTRY_COMPARE(ownership.size(),1);
        QVERIFY(ownership.last()[1].toString().isEmpty());QVERIFY(client.CanSubmit("start"));
        f.ownershipOverride=cache(true);QVERIFY(client.QueryOwnership());QTRY_COMPARE(ownership.size(),2);
        QVERIFY(ownership.last()[1].toString().isEmpty());FreshStatus(client);QVERIFY(!client.CanSubmit("start"));
        f.ownershipOverride=cache(false);QVERIFY(client.QueryOwnership());QTRY_COMPARE(ownership.size(),3);
        FreshStatus(client);QVERIFY(!client.CanSubmit("start"));QCOMPARE(f.posts,0);QCOMPARE(f.commands,0);
    }
    void ownershipBindingChangesAreRejected_data() {
        QTest::addColumn<QString>("change");
        QTest::newRow("resources")<<QString("resources");
        QTest::newRow("peer")<<QString("peer");
        QTest::newRow("global-claim")<<QString("global");
    }
    void ownershipBindingChangesAreRejected() {
        QFETCH(QString,change);Fixture f;
        QJsonObject ownership{{"enabled",true},{"scope","single-host-cooperating-ecc"},{"directory","/locks"},
            {"held_at_last_check",true},{"fault_latched",false},{"cached",true},{"board_global_lock_verified",false},
            {"cross_host_lock_verified",false},{"resources",QJsonArray{"127.0.0.1"}}};
        QJsonObject body{{"schema",1},{"kind","getdaq.ecc-ownership"},{"instance_id",f.instance},
            {"snapshot_sha256",f.options.snapshotSha256},{"ownership",ownership}};
        f.ownershipOverride=QJsonDocument(body).toJson(QJsonDocument::Compact);EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),observed(&client,&EccSystemClient::OwnershipSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);
        QVERIFY(client.QueryOwnership());QTRY_COMPARE(observed.size(),1);QVERIFY(observed.last()[1].toString().isEmpty());
        if (change=="resources") ownership["resources"]=QJsonArray{"127.0.0.2"};
        else if (change=="global") ownership["board_global_lock_verified"]=true;
        else body["instance_id"]=QString(64,'e');
        body["ownership"]=ownership;f.ownershipOverride=QJsonDocument(body).toJson(QJsonDocument::Compact);
        QVERIFY(client.QueryOwnership());QTRY_COMPARE(observed.size(),2);
        QVERIFY(!observed.last()[1].toString().isEmpty());QVERIFY(observed.last()[0].toByteArray().isEmpty());
        FreshStatus(client);QVERIFY(!client.CanSubmit("start"));QCOMPARE(f.posts,0);
    }
    void receiverObservationBindsIdentityWithoutSendingCommands() {
        Fixture f;f.options.receiverStatusEnabled=true;EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),receivers(&client,&EccSystemClient::ReceiverSignal);
        QVERIFY(!client.QueryReceivers());QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);
        QVERIFY(client.QueryReceivers());QVERIFY(!client.QueryReceivers());QTRY_COMPARE(receivers.size(),1);QVERIFY(receivers.last()[1].toString().isEmpty());
        const auto observation=GetDAQ::DecodeEccControlReceiverObservation(receivers.last()[0].toByteArray().toStdString());QVERIFY(observation.nodes.empty());
        FreshStatus(client);QVERIFY(client.CanSubmit("start"));QCOMPARE(f.posts,0);
        auto wrong=QJsonDocument::fromJson(receivers.last()[0].toByteArray()).object();wrong["data_binding_sha256"]=QString(64,'8');f.receiverOverride=QJsonDocument(wrong).toJson();
        QVERIFY(client.QueryReceivers());QTRY_COMPARE(receivers.size(),2);QVERIFY(!receivers.last()[1].toString().isEmpty());QVERIFY(receivers.last()[0].toByteArray().isEmpty());
        QCOMPARE(f.posts,0);QVERIFY(client.CanSubmit("stop-production-after-failure"));
    }
    void receiverEventQueryBindsIdentityAndPreservesUint64ReadOnly() {
        Fixture f;f.options.receiverStatusEnabled=true;EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal);
        QSignalSpy events(&client,&EccSystemClient::ReceiverEventsSignal);
        QVERIFY(!client.QueryReceiverEvents(QStringLiteral("C0")));
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);QVERIFY(identity.last()[0].toBool());
        QVERIFY(client.QueryReceiverEvents(QStringLiteral("C0")));
        QVERIFY(!client.QueryReceiverEvents(QStringLiteral("C0")));
        QTRY_COMPARE(events.size(),1);
        QVERIFY2(events.last()[2].toString().isEmpty(),qPrintable(events.last()[2].toString()));
        QCOMPARE(events.last()[0].toString(),QStringLiteral("C0"));
        const auto observed=GetDAQ::DecodeEccControlReceiverEvents(
            events.last()[1].toByteArray().toStdString());
        QCOMPARE(QString::fromStdString(observed.sourceId),QStringLiteral("C0"));
        QCOMPARE(observed.acquisitionEvents,quint64(9007199254740993ULL));
        QCOMPARE(observed.pid,4242);QCOMPARE(f.receiverEventsGets,1);
        QCOMPARE(f.receiverEventsPath,QStringLiteral("/v1/receiver-events/4330"));
        QCOMPARE(f.posts,0);

        QJsonObject body=QJsonDocument::fromJson(events.last()[1].toByteArray()).object();
        QJsonObject receiver=body.value("events").toObject();
        receiver["source_id"]=QStringLiteral("C1");body["events"]=receiver;
        f.receiverEventsOverride=QJsonDocument(body).toJson(QJsonDocument::Compact);
        QVERIFY(client.QueryReceiverEvents(QStringLiteral("C0")));QTRY_COMPARE(events.size(),2);
        QVERIFY(!events.last()[2].toString().isEmpty());
        QVERIFY(events.last()[1].toByteArray().isEmpty());QCOMPARE(f.posts,0);
    }
    void sourceObservationIsTypedBoundAndReadOnly() {
        Fixture f;f.options.sourceStatusEnabled=true;EccSystemClient client(f.options,f.Writer());QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),sources(&client,&EccSystemClient::SourceSignal);
        QVERIFY(!client.QuerySources());QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);
        QVERIFY(client.QuerySources());QVERIFY(!client.QuerySources());QTRY_COMPARE(sources.size(),1);QVERIFY(sources.last()[1].toString().isEmpty());
        QCOMPARE(GetDAQ::DecodeEccControlSourceObservation(sources.last()[0].toByteArray().toStdString()).sources[0].locallySentBytes,quint64(9007199254740993ull));QCOMPARE(f.posts,0);
        f.sourceOverride=QByteArray::fromStdString(SourceObservationFixture::Root(SourceObservationFixture::Row("C0",std::string(64,'d')),std::string(64,'0')));
        QVERIFY(client.QuerySources());QTRY_COMPARE(sources.size(),2);QVERIFY(!sources.last()[1].toString().isEmpty());QVERIFY(sources.last()[0].toByteArray().isEmpty());QCOMPARE(f.posts,0);
    }
    void malformedReceiverObservationIsNotDisplayedOrSubmitted() {
        Fixture f;f.options.receiverStatusEnabled=true;EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),receivers(&client,&EccSystemClient::ReceiverSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);f.receiverOverride="{\"schema\":1,\"schema\":1}";
        QVERIFY(client.QueryReceivers());QTRY_COMPARE(receivers.size(),1);QVERIFY(!receivers.last()[1].toString().isEmpty());QVERIFY(receivers.last()[0].toByteArray().isEmpty());QCOMPARE(f.posts,0);
    }
    void configurationCacheRequiresIdentityAndPreservesScope() {
        Fixture f;f.configurationSelected=true;EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),configuration(&client,&EccSystemClient::ConfigurationSignal);
        QVERIFY(!client.QueryConfiguration());QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);
        QVERIFY(client.QueryConfiguration());QTRY_COMPARE(configuration.size(),1);QVERIFY(configuration.last()[1].toString().isEmpty());
        const auto c=GetDAQ::DecodeEccControlConfiguration(configuration.last()[0].toByteArray().toStdString());QVERIFY(c.hasSelection);QCOMPARE(QString::fromStdString(c.selection.registration.repositoryId),f.repositoryId);
        FreshStatus(client);QVERIFY(client.CanSubmit("start"));QCOMPARE(f.posts,0);
        f.repositoryId=QString(64,'8');QVERIFY(client.QueryConfiguration());QTRY_COMPARE(configuration.size(),2);QVERIFY(!configuration.last()[1].toString().isEmpty());
        f.repositoryId=QString(64,'9');QVERIFY(!client.QueryConfiguration());QVERIFY(!client.CanSubmit("start"));QVERIFY(!client.CanSubmit("stop-production-after-failure"));QCOMPARE(f.posts,0);
    }
    void forgedHardwareConfigurationProofIsRejected() {
        Fixture f;EccSystemClient client(f.options,f.Writer());QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),configuration(&client,&EccSystemClient::ConfigurationSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);
        GetDAQ::EccControlConfiguration c;c.instanceId=f.instance.toStdString();c.snapshotSha256=f.options.snapshotSha256.toStdString();c.inputBindingSha256=f.options.inputBindingSha256.toStdString();
        c.configurationSha256=std::string(64,'1');c.artifactSha256=std::string(64,'2');f.configurationOverride=QByteArray::fromStdString(GetDAQ::EncodeEccControlConfiguration(c));
        f.configurationOverride.replace("\"hardware_loaded_verified\":false","\"hardware_loaded_verified\":true");
        QVERIFY(client.QueryConfiguration());QTRY_COMPARE(configuration.size(),1);QVERIFY(!configuration.last()[1].toString().isEmpty());QVERIFY(configuration.last()[0].toByteArray().isEmpty());QCOMPARE(f.posts,0);
    }
    void receiverFailureCacheValidatesBindingAndNeverSubmits() {
        Fixture f;f.options.receiverFailureStatusEnabled=true;EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),cache(&client,&EccSystemClient::ReceiverFailureSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);
        QVERIFY(client.QueryReceiverFailure());QVERIFY(!client.QueryReceiverFailure());QTRY_COMPARE(cache.size(),1);
        QVERIFY(cache.last()[1].toString().isEmpty());QVERIFY(client.CanSubmit("start"));
        f.receiverFailureOverride=ReceiverFailureFixture::SingleSourceCache(f.instance,f.options.snapshotSha256,QString(64,'3'));
        QVERIFY(client.QueryReceiverFailure());QTRY_COMPARE(cache.size(),2);QVERIFY(cache.last()[1].toString().isEmpty());
        QCOMPARE(GetDAQ::DecodeEccControlReceiverFailureStatus(cache.last()[0].toByteArray().toStdString()).failedIds.size(),std::size_t(1));
        QVERIFY(!client.CanSubmit("start"));f.receiverFailureOverride.clear();
        QVERIFY(client.QueryReceiverFailure());QTRY_COMPARE(cache.size(),3);FreshStatus(client);QVERIFY(!client.CanSubmit("start"));QCOMPARE(f.posts,0);
    }
    void receiverFailureCacheRejectsOtherBindingAndWithdrawsOldBody() {
        Fixture f;f.options.receiverFailureStatusEnabled=true;EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),cache(&client,&EccSystemClient::ReceiverFailureSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identity.size(),1);FreshStatus(client);
        f.receiverFailureOverride=ReceiverFailureFixture::SingleSourceCache(f.instance,f.options.snapshotSha256,QString(64,'9'));
        QVERIFY(client.QueryReceiverFailure());QTRY_COMPARE(cache.size(),1);QVERIFY(!cache.last()[1].toString().isEmpty());QVERIFY(cache.last()[0].toByteArray().isEmpty());
        QVERIFY(client.CanSubmit("start"));QCOMPARE(f.posts,0);
        f.receiverFailureOverride="{}";QVERIFY(client.QueryReceiverFailure());QTRY_COMPARE(cache.size(),2);
        QVERIFY(cache.last()[0].toByteArray().isEmpty());QCOMPARE(f.posts,0);
    }
    void alarmCacheRequiresIdentityAndKeepsHold() {
        Fixture f; f.options.alarmStatusEnabled=true; EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),alarms(&client,&EccSystemClient::AlarmSignal);
        QVERIFY(!client.QueryAlarms()); QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identity.size(),1); FreshStatus(client);
        QVERIFY(!client.CanSubmit("start")); QVERIFY(client.QueryAlarms()); QTRY_COMPARE(alarms.size(),1); QVERIFY(alarms.last()[1].toString().isEmpty());
        const auto decoded=GetDAQ::DecodeEccControlAlarmStatus(alarms.last()[0].toByteArray().toStdString()); QCOMPARE(qulonglong(decoded.nodes[0].received),qulonglong(9007199254740993ULL));
        QVERIFY(client.CanSubmit("start")); f.alarmHeld=true; QVERIFY(client.QueryAlarms()); QTRY_COMPARE(alarms.size(),2);
        QVERIFY(!client.CanSubmit("start")); QVERIFY(client.CanSubmit("stop-production-after-failure"));
        f.alarmHeld=false; QVERIFY(client.QueryAlarms()); QTRY_COMPARE(alarms.size(),3); FreshStatus(client); QVERIFY(!client.CanSubmit("start")); QCOMPARE(f.posts,0);
    }
    void changedAlarmInstanceCannotTakeOver() {
        Fixture f; f.options.alarmStatusEnabled=true; EccSystemClient client(f.options,f.Writer());
        QSignalSpy identity(&client,&EccSystemClient::IdentitySignal),alarms(&client,&EccSystemClient::AlarmSignal);
        QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identity.size(),1); FreshStatus(client); f.alarmInstanceMismatch=true;
        QVERIFY(client.QueryAlarms()); QTRY_COMPARE(alarms.size(),1); QVERIFY(!alarms.last()[1].toString().isEmpty());
        QVERIFY(!client.CanSubmit("start")); QVERIFY(!client.CanSubmit("stop-production-after-failure")); QCOMPARE(f.posts,0);
    }
    void persistsBeforePostAndObservesSameId() {
        Fixture f; EccSystemClient client(f.options, f.Writer()); QSignalSpy identities(&client, &EccSystemClient::IdentitySignal), operations(&client, &EccSystemClient::OperationSignal);
        QVERIFY(!client.Submit("start", "before-identity")); QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identities.size(), 1); QVERIFY(identities.last()[0].toBool());
        QVERIFY(!client.Submit("start", "before-status")); FreshStatus(client);
        QVERIFY(client.Submit("start", "start")); QVERIFY(client.Busy()); QVERIFY(!client.Observe(f.Saved()));
        QTRY_COMPARE(operations.size(), 1); QCOMPARE(operations.last()[1].toString(), QString("accepted")); QVERIFY(client.Pending()); QVERIFY(f.persistedBeforePost);
        QVERIFY(!client.Submit("start", "new-id")); QCOMPARE(f.commands, 1);
        f.complete = true; QVERIFY(client.Observe(f.Saved())); QTRY_COMPARE(operations.size(), 2); QCOMPARE(operations.last()[1].toString(), QString("complete")); QVERIFY(!client.Pending()); QCOMPARE(f.commands, 1);
    }
    void lostReplyKeepsIntentAndNeverChangesId() {
        Fixture f; f.dropReply = true; EccSystemClient client(f.options, f.Writer()); QSignalSpy identity(&client, &EccSystemClient::IdentitySignal), operation(&client, &EccSystemClient::OperationSignal);
        QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identity.size(), 1); FreshStatus(client); QVERIFY(client.Submit("start", "lost-start")); QTRY_VERIFY(!operation.isEmpty());
        QCOMPARE(operation.last()[1].toString(), QString("unknown")); QVERIFY(client.Pending()); QCOMPARE(f.commands, 1); QVERIFY(f.persistedBeforePost); QVERIFY(!client.Submit("start", "replacement"));
        QCOMPARE(f.posts, 1); QTest::qWait(100); QCOMPARE(f.posts, 1); // 连接中断也不允许Qt倒回上传并自动重发。
        f.complete = true; QVERIFY(client.Observe(f.Saved())); QTRY_COMPARE(operation.last()[1].toString(), QString("complete")); QVERIFY(!client.Pending()); QCOMPARE(f.commands, 1);
    }
    void scopeMismatchSendsNoCommand() {
        Fixture f; auto options = f.options; options.snapshotSha256 = QString(64, '0'); EccSystemClient client(options, f.Writer()); QSignalSpy identities(&client, &EccSystemClient::IdentitySignal);
        QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identities.size(), 1); QVERIFY(!identities.last()[0].toBool()); QVERIFY(!client.Submit("start", "wrong-scope")); QCOMPARE(f.posts, 0);
    }
    void missingIntentWriterSendsNoCommand() {
        Fixture f; EccSystemClient client(f.options, {}); QSignalSpy identities(&client, &EccSystemClient::IdentitySignal), operations(&client, &EccSystemClient::OperationSignal);
        QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identities.size(), 1); FreshStatus(client); QVERIFY(!client.Submit("start", "no-storage")); QCOMPARE(operations.last()[1].toString(), QString("not-sent")); QCOMPARE(f.posts, 0);
    }
    void restartGateAllowsOnlyHistoricalObservation() {
        Fixture f; EccSystemClient client(f.options, f.Writer()); QSignalSpy identities(&client, &EccSystemClient::IdentitySignal), operations(&client, &EccSystemClient::OperationSignal);
        QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identities.size(), 1); FreshStatus(client); QVERIFY(client.Submit("start", "original")); QTRY_COMPARE(operations.size(), 1); const auto request = f.Saved();
        f.instance = QString(64, '4'); QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identities.size(), 2); QVERIFY(!identities.last()[0].toBool());
        f.complete = true; QVERIFY(client.Observe(request)); QTRY_COMPARE(operations.last()[1].toString(), QString("complete")); QVERIFY(!client.Submit("start", "takeover"));
        f.instance = QString(64, 'a'); QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identities.size(), 3); QVERIFY(!identities.last()[0].toBool()); QCOMPARE(f.commands, 1);
    }
    void recoveredScanObservationClearsOnlyItsOwnTransientControlHold() {
        InProcessScanRecoveryFixture f;EccSystemClient client(f.options,[](const std::string &,QString *){return true;},nullptr,f.Transport());
        QSignalSpy identities(&client,&EccSystemClient::IdentitySignal),operations(&client,&EccSystemClient::OperationSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identities.size(),1);QVERIFY(identities.last()[0].toBool());FreshStatus(client);QVERIFY(client.CanSubmit("start"));
        const QString operationId=QString::fromStdString(f.request.operationId);
        client.BeginScanLifecycleObservation(operationId);
        QVERIFY(client.Observe(f.request));QTRY_COMPARE(operations.size(),1);QCOMPARE(operations.last()[1].toString(),QString("unknown"));
        QVERIFY(!client.CanSubmit("start"));
        QVERIFY(client.Observe(f.request));QTRY_COMPARE(operations.size(),2);QCOMPARE(operations.last()[1].toString(),QString("complete"));
        FreshStatus(client);QVERIFY(client.CompleteScanLifecycleObservation(operationId));QVERIFY(client.CanSubmit("start"));
    }
    void recoveredScanObservationPreservesConcurrentAlarmHold() {
        InProcessScanRecoveryFixture f;f.options.alarmStatusEnabled=true;
        EccSystemClient client(f.options,[](const std::string &,QString *){return true;},nullptr,f.Transport());
        QSignalSpy identities(&client,&EccSystemClient::IdentitySignal),operations(&client,&EccSystemClient::OperationSignal),alarms(&client,&EccSystemClient::AlarmSignal);
        QVERIFY(client.QueryIdentity());QTRY_COMPARE(identities.size(),1);QVERIFY(identities.last()[0].toBool());FreshStatus(client);
        QVERIFY(client.QueryAlarms());QTRY_COMPARE(alarms.size(),1);QVERIFY(alarms.last()[1].toString().isEmpty());QVERIFY(client.CanSubmit("start"));
        const QString operationId=QString::fromStdString(f.request.operationId);
        client.BeginScanLifecycleObservation(operationId);
        QVERIFY(client.Observe(f.request));QTRY_COMPARE(operations.size(),1);QCOMPARE(operations.last()[1].toString(),QString("unknown"));
        QVERIFY(client.Observe(f.request));QTRY_COMPARE(operations.size(),2);QCOMPARE(operations.last()[1].toString(),QString("complete"));
        f.alarmLatched=true;QVERIFY(client.QueryAlarms());QTRY_COMPARE(alarms.size(),2);QVERIFY(alarms.last()[1].toString().isEmpty());
        FreshStatus(client);QVERIFY(!client.CompleteScanLifecycleObservation(operationId));QVERIFY(!client.CanSubmit("start"));
    }
    void eccClientStoreClearsOnlyExactUnknownLifecycleOperation() {
        InProcessScanRecoveryFixture f;const QString root=f.directory.filePath("client-records");QString error;
        GetDAQ::EccControlOperation unknown;unknown.request=f.request;
        unknown.requestKey=GetDAQ::EccControlRequestKey(f.request);unknown.phase="unknown";
        {
            EccClientStore store(root,f.options);
            QVERIFY2(store.WriteIntent(GetDAQ::EncodeEccControlRequest(f.request),&error),qPrintable(error));
            QVERIFY2(store.Observe(GetDAQ::EncodeEccControlOperation(unknown),&error),qPrintable(error));
            QVERIFY(store.NeedsReconciliation());QCOMPARE(store.Pending().size(),std::size_t(1));
            QVERIFY2(store.Observe(f.Operation().toStdString(),&error),qPrintable(error));
            QVERIFY(!store.NeedsReconciliation());QVERIFY(store.Pending().empty());
        }
        {EccClientStore reopened(root,f.options);QVERIFY(!reopened.NeedsReconciliation());QVERIFY(reopened.Pending().empty());}
    }
    void controllerUnknownCannotBeClearedByIdentityQuery() {
        Fixture f; f.complete = f.unknown = true; EccSystemClient client(f.options, f.Writer()); QSignalSpy identities(&client, &EccSystemClient::IdentitySignal), operations(&client, &EccSystemClient::OperationSignal);
        QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identities.size(), 1); FreshStatus(client); QVERIFY(client.Submit("start", "partial")); QTRY_COMPARE(operations.size(), 1); QCOMPARE(operations.last()[1].toString(), QString("complete"));
        QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identities.size(), 2); QVERIFY(!identities.last()[0].toBool()); QVERIFY(!client.Submit("resume", "unsafe-resume")); QCOMPARE(f.commands, 1);
        QVERIFY(client.Submit("stop-production-after-failure", "explicit-production-stop")); QTRY_COMPARE(operations.size(), 2);
        QCOMPARE(QString::fromStdString(f.requests["explicit-production-stop"].command), QString("stop-production-after-failure")); QCOMPARE(f.commands, 2);
        QVERIFY(!client.Submit("start", "after-production-stop"));
        FreshStatus(client);QVERIFY(client.Submit("abort-failed-receivers","original-receiver-cleanup"));QTRY_COMPARE(operations.size(),3);
        QCOMPARE(QString::fromStdString(f.requests["original-receiver-cleanup"].command),QString("abort-failed-receivers"));
        QVERIFY(!client.Submit("start","after-receiver-cleanup"));
    }
    void credentialRestorationCannotClearFault() {
        Fixture f; EccSystemClient client(f.options, f.Writer()); QSignalSpy identities(&client, &EccSystemClient::IdentitySignal);
        QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identities.size(), 1); f.WriteCredential(QByteArray(64, '0')); QVERIFY(!client.QueryIdentity());
        f.WriteCredential(f.token); QVERIFY(!client.QueryIdentity()); QVERIFY(!client.Submit("start", "restored-token")); QVERIFY(!client.Submit("abort-failed-receivers","restored-cleanup")); QCOMPARE(f.posts, 0);
    }
    void freshInstanceDoesNotClearReconciliation() {
        Fixture f; f.reconcile = true; EccSystemClient client(f.options, f.Writer()); QSignalSpy identity(&client, &EccSystemClient::IdentitySignal);
        QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identity.size(), 1); QVERIFY(identity.last()[0].toBool()); FreshStatus(client);
        QVERIFY(!client.Submit("start", "reopened")); QCOMPARE(f.posts, 0);
        f.reconcile = false; FreshStatus(client); QVERIFY(!client.Submit("start", "false-clear")); QCOMPARE(f.posts, 0);
    }
    void statusOwnerMismatchBlocksEvenProductionStop() {
        Fixture f; f.ownerEpoch = QString(64, 'e'); EccSystemClient client(f.options, f.Writer()); QSignalSpy identity(&client, &EccSystemClient::IdentitySignal), status(&client, &EccSystemClient::StatusSignal);
        QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identity.size(), 1); QVERIFY(client.QueryStatus()); QTRY_COMPARE(status.size(), 1); QVERIFY(!status.last()[1].toString().isEmpty());
        QVERIFY(!client.Submit("start", "other-owner")); QVERIFY(!client.Submit("stop-production-after-failure", "other-stop")); QCOMPARE(f.posts, 0);
    }
    void staleStatusAndRemoteBusyNeverQueue() {
        Fixture f; auto options = f.options; options.statusMaxAgeMs = 100; EccSystemClient client(options, f.Writer()); QSignalSpy identity(&client, &EccSystemClient::IdentitySignal);
        QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identity.size(), 1); FreshStatus(client); QTest::qWait(110); QVERIFY(!client.Submit("start", "stale")); QCOMPARE(f.posts, 0);
        f.active = true; FreshStatus(client); QVERIFY(!client.Submit("start", "queued")); QCOMPARE(f.posts, 0);
        f.active = false; FreshStatus(client); QVERIFY(client.Submit("start", "fresh")); QTRY_COMPARE(f.commands, 1);
    }
    void productionStoreReopensPendingWithoutReplay() {
        Fixture f; const auto root=f.directory.filePath("records"); std::string saved; QString error;
        {
            EccClientStore store(root, f.options);
            EccSystemClient client(f.options, [&](const std::string &bytes, QString *message) { saved=bytes; return store.WriteIntent(bytes, message); });
            QSignalSpy identity(&client, &EccSystemClient::IdentitySignal), operation(&client, &EccSystemClient::OperationSignal);
            QVERIFY(client.QueryIdentity()); QTRY_COMPARE(identity.size(), 1); FreshStatus(client); QVERIFY(client.Submit("start", "persisted")); QTRY_COMPARE(operation.size(), 1);
            QVERIFY(store.Observe(operation.last()[2].toByteArray().toStdString(), &error)); QCOMPARE(store.Pending().size(), std::size_t(1));
            auto other=GetDAQ::DecodeEccControlRequest(saved); other.operationId="other";
            QVERIFY(!store.WriteIntent(GetDAQ::EncodeEccControlRequest(other), &error)); QVERIFY(!store.NeedsReconciliation());
            bool locked=false; try { EccClientStore second(root, f.options); } catch (const std::exception &) { locked=true; } QVERIFY(locked);
        }
        {
            EccClientStore store(root, f.options); QCOMPARE(store.Pending().size(), std::size_t(1)); QCOMPARE(f.commands, 1);
            auto pending=store.Pending().front(); f.complete=true;
            EccSystemClient client(f.options, [&](const std::string &bytes, QString *message) { return store.WriteIntent(bytes,message); });
            QSignalSpy observed(&client, &EccSystemClient::OperationSignal); QVERIFY(client.Observe(pending)); QTRY_COMPARE(observed.size(), 1); QCOMPARE(observed.last()[1].toString(), QString("complete"));
            QVERIFY2(store.Observe(observed.last()[2].toByteArray().toStdString(), &error), qPrintable(error)); QVERIFY(store.Pending().empty()); QCOMPARE(f.commands, 1);
            GetDAQ::EccControlOperation late; late.request=pending; late.requestKey=GetDAQ::EccControlRequestKey(pending); late.phase="accepted";
            QVERIFY(store.Observe(GetDAQ::EncodeEccControlOperation(late), &error)); QVERIFY(store.Pending().empty());
        }
        EccClientStore reopened(root, f.options); QVERIFY(reopened.Pending().empty()); QVERIFY(!reopened.NeedsReconciliation());
    }
    void productionStoreRejectsPolicyAndHistoricalCorruption() {
        Fixture f; const auto root=f.directory.filePath("records"); QString error;
        GetDAQ::EccControlRequest request; request.operationId="first"; request.command="describe"; request.instanceId=f.instance.toStdString(); request.snapshotSha256=f.options.snapshotSha256.toStdString();
        request.clientId=f.options.clientId.toStdString(); request.clientEpoch=f.options.clientEpoch.toStdString(); const auto bytes=GetDAQ::EncodeEccControlRequest(request);
        {
            EccClientStore store(root,f.options); QVERIFY(store.WriteIntent(bytes,&error));
            QFile file(root+"/requests/first.json"); QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); QCOMPARE(file.write("corrupt"),qint64(7)); file.close();
            bool rejected=false; try { store.Check(); } catch (const std::exception &) { rejected=true; } QVERIFY(rejected);
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); QCOMPARE(file.write(QByteArray::fromStdString(bytes)),qint64(bytes.size())); file.close();
            rejected=false; try { store.Check(); } catch (const std::exception &) { rejected=true; } QVERIFY(rejected); QVERIFY(store.NeedsReconciliation());
        }
        auto changed=f.options; changed.clientEpoch=QString(64,'9'); bool mismatch=false; try { EccClientStore store(root,changed); } catch (const std::exception &) { mismatch=true; } QVERIFY(mismatch);
    }
};
QTEST_GUILESS_MAIN(TestEccSystemClient)
#include "test_ecc_system_client.moc"
