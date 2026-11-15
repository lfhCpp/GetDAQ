#ifndef GETDAQ_TEST_NATIVE_SCAN_RUN_FIXTURE_H
#define GETDAQ_TEST_NATIVE_SCAN_RUN_FIXTURE_H

#include "../common/control/eccprotocol.h"
#include "../common/control/scanruntimeidentity.h"
#include "../common/http/httpserver.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTcpSocket>

// Small metadata-only ECC HTTP service. It changes only its reported control
// state and event counter; it never starts a receiver or writes DAQ payload.
class NativeScanRunFixture : public QObject
{
public:
    QTemporaryDir directory;
    HttpServer server;
    QByteArray credential = QByteArray(64, 'f');
    QString instance = QString(64, 'a');
    QString snapshot = QString(64, 'b');
    QString inputBinding = QString(64, 'c');
    QString clientEpoch = QString(64, 'd');
    QString configuration = QString(64, '1');
    QString artifact = QString(64, '2');
    QString dataBinding = QString(64, '3');
    QString dataScope = QString(64, '4');
    QString dataEpoch = QString(64, '5');
    QString receiverInstance = QString(64, '6');
    QString receiverPlan = QString(64, '7');
    QString session = QString(64, '8');
    QString controllerOperationId;
    unsigned int controllerState = 1;
    quint64 events = 100;
    bool receiverConfigured = false;
    bool receiverFinished = false;
    bool catalogued = false;
    GetDAQ::EccControlCatalogReceipt catalog;
    int runtimeIdentityRequests = 0;
    int statusObservationRequests = 0;
    int receiverObservationRequests = 0;
    int runStarts = 0;
    int runStops = 0;
    int eventRequests = 0;
    QStringList commands;

    NativeScanRunFixture()
    {
        if (!directory.isValid() || !server.listen(QHostAddress::LocalHost, 0))
            throw std::runtime_error("Cannot create local Native scan fixture");
        QFile key(CredentialPath());
        if (!key.open(QIODevice::WriteOnly) || key.write(credential + '\n') != 65)
            throw std::runtime_error("Cannot write local Native scan fixture credential");
        key.close();
        if (!key.setPermissions(QFile::ReadOwner | QFile::WriteOwner))
            throw std::runtime_error("Cannot protect local Native scan fixture credential");

        connect(&server, &HttpServer::RequestSignal, this,
            [this](const QString &method, const QString &path, const QByteArray &body,
                   HttpConnection *connection) {
                if (connection->RequestHeader("authorization") != "Bearer " + credential) {
                    connection->Reply(401, "{}");
                    return;
                }
                if (method == "GET" && path == "/v1/identity") {
                    connection->Reply(200, IdentityBytes());
                    return;
                }
                if (method == "GET" && path == "/v1/status") {
                    ++statusObservationRequests;
                    connection->Reply(200, StatusBytes());
                    return;
                }
                if (method == "GET" && path == "/v1/receivers") {
                    ++receiverObservationRequests;
                    connection->Reply(200, ReceiverBytes());
                    return;
                }
                if (method == "GET" && path == "/v1/receiver-events/4330") {
                    ++eventRequests;
                    QJsonObject event{{"agent_id", "A0"}, {"plan_sha256", receiverPlan},
                        {"instance_id", receiverInstance}, {"source_id", "C0"},
                        {"session_id", session}, {"pid", 4242}, {"available", true},
                        {"acquisition_events", QString::number(events)},
                        {"observed_at_unix_ms", QString::number(1700000000000ULL + quint64(eventRequests))}};
                    QJsonObject root{{"schema", 1}, {"kind", "getdaq.ecc-receiver-events"},
                        {"instance_id", instance}, {"snapshot_sha256", snapshot},
                        {"data_binding_sha256", dataBinding}, {"events", event}};
                    connection->Reply(200, QJsonDocument(root).toJson(QJsonDocument::Compact));
                    return;
                }
                if (method == "POST" && path == "/v1/scan-runtime-identity") {
                    ++runtimeIdentityRequests;
                    const auto request = GetDAQ::DecodeScanRuntimePointIdentityRequest(body.toStdString());
                    GetDAQ::ScanRuntimeIdentityExpectation expected;
                    expected.executionSnapshotSha256 = request.executionSnapshotSha256;
                    expected.pointIndex = request.pointIndex;
                    expected.serviceId = "scan-ecc";
                    expected.clientId = "scan-console";
                    expected.instanceId = instance.toStdString();
                    expected.snapshotSha256 = snapshot.toStdString();
                    expected.inputBindingSha256 = inputBinding.toStdString();
                    expected.configurationSha256 = request.configurationSha256;
                    expected.artifactSha256 = request.artifactSha256;
                    expected.dataBindingSha256 = dataBinding.toStdString();
                    expected.dataScopeSha256 = dataScope.toStdString();
                    expected.dataEpoch = dataEpoch.toStdString();
                    expected.dataNodes.push_back({"A0", receiverInstance.toStdString(), receiverPlan.toStdString()});
                    GetDAQ::ScanRuntimePointIdentityResponse response;
                    response.mode = request.mode;
                    response.observedControllerState = request.expectedControllerState;
                    response.expectation = expected;
                    response.identityJson = IdentityBytes().toStdString();
                    response.statusJson = StatusBytes().toStdString();
                    response.receiversJson = ReceiverBytes().toStdString();
                    connection->Reply(200, QByteArray::fromStdString(
                        GetDAQ::EncodeScanRuntimePointIdentityResponse(response)));
                    return;
                }
                if (method == "POST" && path == "/v1/operations") {
                    const auto request = GetDAQ::DecodeEccControlRequest(body.toStdString());
                    commands.append(QString::fromStdString(request.command));
                    controllerOperationId = QString::fromStdString(request.operationId);
                    if (request.command == "describe" && controllerState == 1) {
                        controllerState = 2;
                    } else if (request.command == "prepare" && controllerState == 2) {
                        controllerState = 3;
                    } else if (request.command == "configure" && controllerState == 3) {
                        controllerState = 4;
                        receiverConfigured = true;
                    } else if (request.command == "start" && controllerState == 4) {
                        ++runStarts;
                        controllerState = 5;
                        events = 102;
                    } else if (request.command == "stop" && controllerState == 5) {
                        ++runStops;
                        controllerState = 4;
                    } else if (request.command == "breakup" && controllerState == 4) {
                        controllerState = 3;
                        receiverFinished = true;
                    } else if (request.command == "catalog" && controllerState == 3 && receiverFinished) {
                        catalogued = true;
                        catalog.requestKey = GetDAQ::EccControlRequestKey(request);
                        catalog.manifestSha256 = receiverPlan.toStdString();
                        catalog.operationId = request.operationId;
                        catalog.directory = "/metadata-fixture/catalog";
                        catalog.linkedFiles = 1;
                    } else {
                        connection->Reply(400, "{}");
                        return;
                    }
                    GetDAQ::EccControlControllerStatus result;
                    result.state = controllerState;
                    result.operationId = request.operationId;
                    GetDAQ::EccControlOperation operation;
                    operation.request = request;
                    operation.requestKey = GetDAQ::EccControlRequestKey(request);
                    operation.phase = "complete";
                    operation.resultJson = GetDAQ::EncodeEccControlControllerStatus(result);
                    connection->Reply(202, QByteArray::fromStdString(GetDAQ::EncodeEccControlOperation(operation)));
                    return;
                }
                connection->Reply(404, "{}");
            });
    }

    QString CredentialPath() const { return directory.filePath("ecc-credential"); }

    QByteArray IdentityBytes() const
    {
        QJsonObject identity{{"schema", 1}, {"service", "getdaq-ecc-server"},
            {"service_id", "scan-ecc"}, {"client_id", "scan-console"},
            {"instance_id", instance}, {"snapshot_sha256", snapshot},
            {"input_binding_sha256", inputBinding}, {"configuration_sha256", configuration},
            {"artifact_sha256", artifact}, {"data_binding_sha256", dataBinding}};
        return QJsonDocument(identity).toJson(QJsonDocument::Compact);
    }

    QJsonObject Lifecycle() const
    {
        QJsonArray processes;
        if (receiverConfigured) {
            const QJsonObject process{{"id", "C0"}, {"session_id", session},
                {"output_directory", "/metadata-fixture/session"}, {"pid", 4242},
                {"exit_code", receiverFinished ? 0 : -1}, {"reaped", receiverFinished},
                {"seal_verified", receiverFinished}, {"received_bytes", receiverFinished ? "4096" : "0"},
                {"frames", receiverFinished ? "1" : "0"}, {"files", receiverFinished ? "1" : "0"}};
            processes.append(process);
        }
        const QString lifecycleState = !receiverConfigured ? QStringLiteral("fresh")
            : receiverFinished ? QStringLiteral("stopped") : QStringLiteral("ready");
        return QJsonObject{{"schema", 1}, {"state", lifecycleState},
            {"needs_reconciliation", false}, {"plan_sha256", receiverPlan},
            {"session_id", receiverConfigured ? session : QString()}, {"message", ""},
            {"receivers", processes}};
    }

    QByteArray StatusBytes() const
    {
        GetDAQ::EccControlControllerStatus controller;
        controller.state = controllerState;
        controller.operationId = controllerOperationId.toStdString();
        QJsonObject node{{"agent_id", "A0"}, {"instance_id", receiverInstance},
            {"plan_sha256", receiverPlan}, {"lifecycle", Lifecycle()}};
        if (catalogued) node.insert("catalog", QJsonDocument::fromJson(
            QByteArray::fromStdString(GetDAQ::EncodeEccControlCatalogReceipt(catalog))).object());
        QJsonObject data{{"needs_reconciliation", false}, {"controller_scope_sha256", dataScope},
            {"controller_epoch", dataEpoch}, {"message", ""}, {"nodes", QJsonArray{node}}};
        QJsonObject root{{"schema", 1}, {"service", "getdaq-ecc-server"},
            {"instance_id", instance}, {"snapshot_sha256", snapshot},
            {"operation_active", false}, {"operation_id", ""}, {"client_epoch", clientEpoch},
            {"control_needs_reconciliation", false}, {"message", ""},
            {"controller", QJsonDocument::fromJson(QByteArray::fromStdString(
                GetDAQ::EncodeEccControlControllerStatus(controller))).object()}, {"data", data}};
        return QJsonDocument(root).toJson(QJsonDocument::Compact);
    }

    QByteArray ReceiverBytes() const
    {
        QJsonArray processes;
        if (receiverConfigured) {
            const QJsonObject process{{"id", "C0"}, {"pid", 4242},
                {"state", receiverFinished ? "reaped" : "running"},
                {"exit_code", receiverFinished ? 0 : -1}, {"signal", 0}, {"message", ""}};
            processes.append(process);
        }
        QJsonObject observation{{"schema", 1}, {"busy", false},
            {"observed_at_unix_ms", "1700000000000"}, {"lifecycle", Lifecycle()},
            {"receivers", processes}};
        QJsonObject node{{"agent_id", "A0"}, {"plan_sha256", receiverPlan},
            {"instance_id", receiverInstance}, {"available", true}, {"message", ""},
            {"observation", observation}};
        QJsonObject root{{"schema", 1}, {"kind", "getdaq.ecc-receivers"},
            {"instance_id", instance}, {"snapshot_sha256", snapshot},
            {"data_binding_sha256", dataBinding}, {"nodes", QJsonArray{node}}};
        return QJsonDocument(root).toJson(QJsonDocument::Compact);
    }
};

#endif
