#include "native_status_tests_p.h"

#include <QtTest>
#include <QSet>

#include <stdexcept>

#include "nativestatusprojection.h"
#include "nativestatusobservations.h"

namespace GetDAQTestInternal {

void RunNativeStatusProjectionRequiresMatchingLiveReceiverEvidence()
{
    RunConfig config;
    config.nativeClient.receiverStatusEnabled=true;
    config.nodes.clear();
    NodeConfig nodeConfig;nodeConfig.id=0;nodeConfig.nativeAgentId="A0";nodeConfig.nativeSenderId="C0";
    config.nodes.push_back(nodeConfig);

    NodeStatus row;row.id=0;row.nativePid=4242;row.nativeSessionId="session-1";
    QVector<NodeStatus> inputRows;inputRows.push_back(row);
    GetDAQ::EccControlSystemStatus system;
    GetDAQ::EccControlDataNodeStatus systemNode;
    systemNode.agentId="A0";systemNode.instanceId="receiver-1";systemNode.planSha256=std::string(64,'a');
    system.nodes.push_back(systemNode);

    GetDAQ::EccControlReceiverObservation receivers;
    GetDAQ::EccControlReceiverNodeObservation receiverNode;
    receiverNode.agentId="A0";receiverNode.instanceId="receiver-1";
    receiverNode.planSha256=std::string(64,'a');receiverNode.available=true;
    receiverNode.lifecycle.sessionId="session-1";
    GetDAQ::EccControlReceiverProcess process;process.id="C0";process.state="running";process.pid=4242;
    receiverNode.receivers.push_back(process);receivers.nodes.push_back(receiverNode);

    GetDAQ::EccControlSourceObservation sources;
    GetDAQ::EccControlReceiverFailureStatus receiverFailure;
    const auto projected=ProjectNativeStatus(config,inputRows,system,true,50,receivers,true,QString(),
        sources,false,QString(),receiverFailure,false,QString());
    QCOMPARE(projected.size(),1);
    QVERIFY(projected[0].nativeReceiverObserved);
    QVERIFY(!projected[0].nativeReceiverFault);
    QVERIFY(projected[0].process.contains(QStringLiteral("Running")));

    auto mismatchedRows=inputRows;mismatchedRows[0].nativePid=4343;
    const auto mismatched=ProjectNativeStatus(config,mismatchedRows,system,true,50,receivers,true,QString(),
        sources,false,QString(),receiverFailure,false,QString());
    QVERIFY(!mismatched[0].nativeReceiverObserved);
    QVERIFY(mismatched[0].nativeReceiverError.contains(QStringLiteral("matching receiver")));

    const auto stale=ProjectNativeStatus(config,inputRows,system,true,50,receivers,false,QString(),
        sources,false,QString(),receiverFailure,false,QString());
    QVERIFY(!stale[0].nativeReceiverObserved);
    QVERIFY(stale[0].nativeReceiverError.contains(QStringLiteral("stale")));
}

void RunNativeSystemStatusRowsPreserveReceiverOwnershipAndStateGates()
{
    RunConfig config;config.nodes.clear();
    NodeConfig cobo;cobo.id=2;cobo.nativeSenderId="C2";cobo.nativeAgentId="A2";
    config.nodes.push_back(cobo);
    NodeConfig mutant;mutant.id=0;mutant.mutant=true;config.nodes.push_back(mutant);

    GetDAQ::EccControlSystemStatus system;system.controller.state=4;
    system.message="reconciliation required";system.controller.outcomeUnknown=true;
    GetDAQ::EccControlDataNodeStatus dataNode;dataNode.agentId="A2";dataNode.state="ready";
    GetDAQ::EccControlReceiverStatus receiver;receiver.id="C2";receiver.sessionId="session-2";
    receiver.outputDirectory="/data/run";receiver.pid=4242;receiver.receivedBytes=99;
    receiver.frames=12;receiver.files=3;receiver.reaped=false;receiver.sealVerified=false;
    receiver.sourceUnusedConfirmed=true;receiver.sourceEndConfirmed=true;
    dataNode.receivers.push_back(receiver);system.nodes.push_back(dataNode);

    const auto rows=BuildNativeSystemStatusRows(config,system,75,false);
    QCOMPARE(rows.size(),2);
    QCOMPARE(rows[0].id,2);QVERIFY(rows[0].eccOnline);QVERIFY(rows[0].sentryOnline);
    QCOMPARE(rows[0].state,4);QCOMPARE(rows[0].eccTime,qint64(75));
    QCOMPARE(rows[0].sentryTime,qint64(75));QCOMPARE(rows[0].process,QStringLiteral("ready (cached)"));
    QCOMPARE(rows[0].nativeSessionId,QStringLiteral("session-2"));QCOMPARE(rows[0].nativePid,4242);
    QCOMPARE(rows[0].nativeReceivedBytes,QStringLiteral("99"));
    QCOMPARE(rows[0].nativeFrames,QStringLiteral("12"));QCOMPARE(rows[0].nativeFiles,QStringLiteral("3"));
    QVERIFY(rows[0].nativeSourceUnusedConfirmed);QVERIFY(rows[0].nativeSourceEndConfirmed);
    QVERIFY(rows[0].nativeHardwareOutcomeUnknown);
    QCOMPARE(rows[0].error,QStringLiteral("reconciliation required"));
    QVERIFY(!rows[1].sentryOnline);QCOMPARE(rows[1].id,0);

    system.nodes[0].agentId="different-agent";
    bool rejectedWrongOwner=false;
    try {BuildNativeSystemStatusRows(config,system,75,false);}
    catch(const std::runtime_error &) {rejectedWrongOwner=true;}
    QVERIFY(rejectedWrongOwner);

    system.nodes[0].agentId="A2";system.nodes[0].receivers.clear();
    bool rejectedMissingRunningReceiver=false;
    try {BuildNativeSystemStatusRows(config,system,75,false);}
    catch(const std::runtime_error &) {rejectedMissingRunningReceiver=true;}
    QVERIFY(rejectedMissingRunningReceiver);
    system.operationActive=true;
    const auto operationRows=BuildNativeSystemStatusRows(config,system,75,false);
    QCOMPARE(operationRows[0].process,QStringLiteral("No receiver observation yet"));
}

void RunNativeStatusObservationCacheOwnsDecodeFreshnessAndReset()
{
    const std::string hash(64,'a');
    const QByteArray receiver=QByteArray::fromStdString(
        "{\"schema\":1,\"kind\":\"getdaq.ecc-receivers\",\"instance_id\":\""+hash+
        "\",\"snapshot_sha256\":\""+hash+"\",\"data_binding_sha256\":\""+hash+
        "\",\"nodes\":[{\"agent_id\":\"A\",\"plan_sha256\":\""+hash+
        "\",\"instance_id\":\""+hash+"\",\"available\":true,\"message\":\"\",\"observation\":{\"schema\":1,\"busy\":false,\"observed_at_unix_ms\":\"1\",\"lifecycle\":{\"schema\":1,\"state\":\"ready\",\"needs_reconciliation\":false,\"plan_sha256\":\""+hash+
        "\",\"session_id\":\""+hash+"\",\"message\":\"\",\"receivers\":[{\"id\":\"C0\",\"session_id\":\""+hash+
        "\",\"output_directory\":\"/data/session\",\"pid\":42,\"exit_code\":-1,\"reaped\":false,\"seal_verified\":false,\"received_bytes\":\"1\",\"frames\":\"1\",\"files\":\"1\"}]},\"receivers\":[{\"id\":\"C0\",\"pid\":42,\"state\":\"running\",\"exit_code\":-1,\"signal\":0,\"message\":\"\"}]}}]}");
    const QByteArray sources=QByteArray::fromStdString(
        "{\"schema\":1,\"kind\":\"getdaq.ecc-sources\",\"instance_id\":\""+hash+
        "\",\"snapshot_sha256\":\""+hash+"\",\"configuration_sha256\":\""+hash+
        "\",\"artifact_sha256\":\""+hash+"\",\"controller_operation_id\":\"\",\"busy\":false,\"sources\":[]}");
    const QByteArray failure=QByteArray::fromStdString(
        "{\"schema\":1,\"kind\":\"getdaq.ecc-receiver-failure\",\"instance_id\":\""+hash+
        "\",\"snapshot_sha256\":\""+hash+"\",\"enabled\":false,\"latched\":false,\"operation_id\":\"\",\"phase\":\"disabled\",\"evidence\":null}");
    const QByteArray alarm=QByteArray::fromStdString(
        "{\"schema\":1,\"kind\":\"getdaq.ecc-alarms\",\"instance_id\":\""+hash+
        "\",\"snapshot_sha256\":\""+hash+"\",\"enabled\":true,\"latched\":true,\"operation_id\":\"internal-alarm-cache-test\",\"phase\":\"complete\",\"observation\":null}");

    NativeStatusObservations cache;
    QSet<QString> expected;expected.insert(QStringLiteral("A"));
    auto update=cache.ObserveReceivers(receiver,QString(),expected);
    QVERIFY(update.error.isEmpty());QVERIFY(!update.errorChanged);
    QVERIFY(cache.ReceiversFresh(60000));QCOMPARE(cache.ReceiverBody(),receiver);
    QCOMPARE(cache.Receivers().nodes.size(),std::size_t(1));
    update=cache.ObserveReceivers(receiver,QStringLiteral("offline"),expected);
    QCOMPARE(update.error,QStringLiteral("offline"));QVERIFY(update.errorChanged);
    QVERIFY(!cache.ReceiversFresh(60000));QVERIFY(cache.ReceiverBody().isEmpty());
    update=cache.ObserveReceivers(receiver,QStringLiteral("offline"),expected);
    QVERIFY(!update.errorChanged);
    update=cache.ObserveReceivers(receiver,QString(),QSet<QString>());
    QVERIFY(update.error.contains(QStringLiteral("agents differ")));QVERIFY(update.errorChanged);
    QVERIFY(!cache.ReceiversFresh(60000));

    update=cache.ObserveSources(sources,QString());
    QVERIFY(update.error.isEmpty());QVERIFY(cache.SourcesFresh(60000));
    update=cache.ObserveSources(sources,QStringLiteral("source offline"));
    QCOMPARE(update.error,QStringLiteral("source offline"));QVERIFY(!cache.SourcesFresh(60000));

    update=cache.ObserveReceiverFailure(failure,QString());
    QVERIFY(update.error.isEmpty());QVERIFY(update.bodyChanged);
    QVERIFY(cache.ReceiverFailureFresh(60000));QCOMPARE(cache.ReceiverFailureBody(),failure);
    update=cache.ObserveReceiverFailure(failure,QString());
    QVERIFY(!update.bodyChanged);
    update=cache.ObserveAlarms(alarm,QString());
    QVERIFY(update.error.isEmpty());QVERIFY(update.bodyChanged);
    QVERIFY(cache.AlarmsFresh(60000));QCOMPARE(cache.AlarmBody(),alarm);
    QCOMPARE(QString::fromStdString(cache.Alarms().operationId),QStringLiteral("internal-alarm-cache-test"));
    update=cache.ObserveAlarms(alarm,QString());QVERIFY(!update.bodyChanged);
    update=cache.ObserveAlarms(QByteArray("{}"),QString());
    QVERIFY(!update.error.isEmpty());QVERIFY(cache.AlarmsFresh(60000));QCOMPARE(cache.AlarmBody(),alarm);
    update=cache.ObserveAlarms(alarm,QStringLiteral("alarm offline"));
    QCOMPARE(update.error,QStringLiteral("alarm offline"));QVERIFY(!cache.AlarmsFresh(60000));
    QCOMPARE(cache.AlarmBody(),alarm);
    update=cache.ObserveAlarms(alarm,QString());QVERIFY(cache.AlarmsFresh(60000));
    cache.Reset();
    QVERIFY(!cache.AlarmsFresh(60000));QVERIFY(cache.AlarmBody().isEmpty());
    QVERIFY(!cache.ReceiversFresh(60000));QVERIFY(!cache.SourcesFresh(60000));
    QVERIFY(!cache.ReceiverFailureFresh(60000));QVERIFY(cache.ReceiverBody().isEmpty());
    QVERIFY(cache.ReceiverError().isEmpty());QVERIFY(cache.SourceError().isEmpty());
    QVERIFY(cache.ReceiverFailureBody().isEmpty());
}

}
