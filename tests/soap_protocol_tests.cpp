#include "soap_protocol_tests_p.h"

#include <QtTest>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QStringList>
#include <QSignalSpy>

#include "runconfig.h"
#include "protocol.h"
#include "eccclient.h"
#include "getdaq_test_support_p.h"

namespace GetDAQTestInternal {

void RunSoapStringParametersAndTopology()
{
    QTemporaryDir dir; RunConfig c = GetDAQTestSupport::Config(dir.path()); c.nodes.removeLast();
    NodeConfig &n = c.nodes[0]; QCOMPARE(n.exporterPort, 0);
    n.exporterPort = 46007;
    QVERIFY(c.Save(dir.filePath("legacy-target.yml"), nullptr));
    RunConfig legacy; QString legacyError;
    QVERIFY2(legacy.Load(dir.filePath("legacy-target.yml"), &legacyError), qPrintable(legacyError));
    QCOMPARE(legacy.nodes[0].exporterPort, 46007); // 显式旧第二目标仍可使用。
    n.routerHost = "10.0.0.2"; n.sentryHost = "10.0.0.3"; n.exporterPort = 0;
    n.describeId = "custom<&>"; n.prepareId = "prep"; n.configureId = "config";
    QString error; QVERIFY(c.Validate(&error));
    QVERIFY(c.Save(dir.filePath("single.yml"), &error));
    RunConfig loaded; QVERIFY(loaded.Load(dir.filePath("single.yml"), &error));
    QCOMPARE(loaded.nodes[0].routerHost, n.routerHost); QCOMPARE(loaded.nodes[0].describeId, n.describeId);
    QCOMPARE(loaded.nodes[0].sentryHost, n.sentryHost);
    QXmlStreamReader reader(Protocol::EccRequest(n, "Exp", "Configure"));
    QString config, links;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == "configID") config = reader.readElementText();
        if (reader.isStartElement() && reader.name() == "table") links = reader.readElementText();
    }
    QVERIFY(!reader.hasError()); QVERIFY(config.contains("custom&lt;&amp;&gt;"));
    QVERIFY(links.contains("10.0.0.2")); QVERIFY(!links.contains("exporter"));
    QCOMPARE(StateName(6), QString("Paused"));
}

#ifdef GETDAQ_GSOAP_INTEROP
void RunLegacyGsoapProcessInterop()
{
    bool portOk = false;
    const int port = QString::fromLocal8Bit(qgetenv("GETDAQ_GSOAP_FIXTURE_PORT")).toInt(&portOk);
    QVERIFY(portOk && port > 0 && port <= 65535);
    RunConfig config = RunConfig::Default();
    QVERIFY(!config.nodes.isEmpty());
    config.nodes = QVector<NodeConfig>{config.nodes.first()};
    config.simulation = false;
    config.timeoutMs = 5000;
    config.experiment = "GSoapInterop";
    NodeConfig node = config.nodes.first();
    node.id = 0;
    node.name = "CoBo";
    node.host = "127.0.0.1";
    node.eccPort = port;
    node.describeId = "fixture-describe-id";
    node.routerHost = "127.0.0.1";
    node.routerPort = 46005;

    EccClient client(node, config);
    QSignalSpy replies(&client, &EccClient::ReplySignal);
    QVERIFY(client.Send("GetState"));
    QTRY_COMPARE(replies.size(), 1);
    QCOMPARE(replies.last().at(1).toString(), QString("GetState"));
    QVERIFY2(replies.last().at(4).toString().isEmpty(), qPrintable(replies.last().at(4).toString()));
    QCOMPARE(replies.last().at(2).toInt(), 4);
    QCOMPARE(replies.last().at(3).toInt(), 0);

    for (const QString &operation : {QString("Describe"), QString("Prepare"), QString("Configure"),
                                     QString("Start"), QString("Stop"), QString("Pause"),
                                     QString("Resume"), QString("Breakup"), QString("Undo")}) {
        const int expectedReplies = replies.size() + 1;
        QVERIFY(client.Send(operation));
        QTRY_COMPARE(replies.size(), expectedReplies);
        QCOMPARE(replies.last().at(1).toString(), operation);
        QVERIFY2(replies.last().at(4).toString().isEmpty(), qPrintable(replies.last().at(4).toString()));
    }
}
#endif

void RunProtocolValidation()
{
    const auto c = RunConfig::Default();
    const QByteArray request = Protocol::EccRequest(c.nodes.first(), "A&B", "Configure");
    QXmlStreamReader envelope(request); QString config;
    while (!envelope.atEnd()) {
        envelope.readNext();
        if (envelope.isStartElement() && envelope.name() == "configID") config = envelope.readElementText();
    }
    QVERIFY(!envelope.hasError());
    QXmlStreamReader inner(config); QStringList ids;
    while (!inner.atEnd()) {
        inner.readNext();
        if (inner.isStartElement() && inner.name() == "SubConfigId") ids.append(inner.readElementText());
    }
    QVERIFY(!inner.hasError()); QVERIFY(ids.contains("A&B"));
    QVERIFY(request.contains("CoBo[0]")); QVERIFY(request.contains("46005"));
    QVERIFY(!request.contains("46007")); QVERIFY(!request.contains("exporter0"));
    QXmlStreamReader requestNamespaceReader(request);
    QString operationNamespace;
    bool parametersUnqualified = true;
    while (!requestNamespaceReader.atEnd()) {
        requestNamespaceReader.readNext();
        if (!requestNamespaceReader.isStartElement()) continue;
        if (requestNamespaceReader.name() == "Configure") operationNamespace = requestNamespaceReader.namespaceUri().toString();
        if (requestNamespaceReader.name() == "configID" || requestNamespaceReader.name() == "table")
            parametersUnqualified = parametersUnqualified && requestNamespaceReader.namespaceUri().isEmpty();
    }
    QVERIFY(!requestNamespaceReader.hasError());
    QCOMPARE(operationNamespace, QString("urn:http://localhost:8083/"));
    QVERIFY(parametersUnqualified);
    NodeConfig legacy = c.nodes.first(); legacy.exporterPort = 46007;
    const QByteArray legacyRequest = Protocol::EccRequest(legacy, "A&B", "Configure");
    QVERIFY(legacyRequest.contains("46007")); QVERIFY(legacyRequest.contains("exporter0"));
    int state = -1, transition = -1; QString error;
    const QByteArray legacySoapReply = "<s:Envelope xmlns:s='http://schemas.xmlsoap.org/soap/envelope/'><s:Body><GetStateResponse>"
                                      "<ErrorCode>0</ErrorCode><ErrorMessage/><State>2</State><Transition>0</Transition>"
                                      "</GetStateResponse></s:Body></s:Envelope>";
    QVERIFY(Protocol::ParseEcc(legacySoapReply, true, &state, &transition, &error));
    QCOMPARE(state, 2); QCOMPARE(StateName(state), QString("Described"));
    QVERIFY(!Protocol::ParseEcc("<Reply><ErrorCode>0</ErrorCode><State>5</State><Transition>0</Transition></Reply>", true, &state, &transition, &error));
    QVERIFY(!Protocol::ParseEcc("<s:Envelope xmlns:s='http://schemas.xmlsoap.org/soap/envelope/'><s:Body><DescribeResponse><State>5</State></DescribeResponse></s:Body></s:Envelope>", true, &state, &transition, &error));
    QVERIFY(!Protocol::ParseEcc("<s:Envelope xmlns:s='http://schemas.xmlsoap.org/soap/envelope/'><s:Body><DescribeResponse><ErrorCode>3</ErrorCode><ErrorMessage>failed</ErrorMessage></DescribeResponse></s:Body></s:Envelope>", false, &state, &transition, &error));
    QVERIFY(!Protocol::ParseEcc("<s:Envelope xmlns:s='http://schemas.xmlsoap.org/soap/envelope/'><s:Body><s:Fault><faultstring>failed</faultstring></s:Fault></s:Body></s:Envelope>", false, &state, &transition, &error));
    NodeStatus s;
    QVERIFY(!Protocol::ParseSentry("{}", &s, &error));
    QVERIFY(!Protocol::ParseSentry(R"({"disk":"x","process":"x","data_path":"x","data_written_gb":0,"data_path_files":-1,"disk_avail_gb":1,"disk_total_gb":2})", &s, &error));
}

void RunSoapStringAndInlineXmlParameterForms()
{
    NodeConfig node = RunConfig::Default().nodes.first();
    node.routerHost = "10.0.0.2"; node.routerPort = 46005; node.exporterPort = 0;
    node.describeId = "custom<&>"; node.prepareId = "pedestals"; node.configureId = "physics";
    for (bool inlineXml : {false, true}) {
        node.soapInlineXml = inlineXml;
        const QByteArray request = Protocol::EccRequest(node, "Experiment & beam", "Configure");
        QStringList ids, routers, payloadNamespaces;
        auto noteNamespace = [&payloadNamespaces](const QString &name, const QString &uri) {
            if (name == "ConfigId" || name == "SubConfigId" || name == "DataLinkSet" || name == "DataLink" ||
                name == "DataSender" || name == "DataRouter") payloadNamespaces.append(uri);
        };
        auto parsePayload = [&ids, &routers, &noteNamespace](const QString &payload) {
            QXmlStreamReader inner(payload);
            while (!inner.atEnd()) {
                inner.readNext();
                if (!inner.isStartElement()) continue;
                noteNamespace(inner.name().toString(), inner.namespaceUri().toString());
                if (inner.name() == "SubConfigId") {
                    const QString type = inner.attributes().value("type").toString();
                    const QString value = inner.readElementText();
                    ids.append(type + "=" + value);
                } else if (inner.name() == "DataRouter") {
                    const auto a = inner.attributes();
                    routers.append(a.value("name").toString() + "|" + a.value("ipAddress").toString() + "|" +
                                   a.value("port").toString() + "|" + a.value("type").toString());
                }
            }
            return !inner.hasError();
        };
        QXmlStreamReader outer(request);
        while (!outer.atEnd()) {
            outer.readNext();
            if (!outer.isStartElement()) continue;
            if (inlineXml) noteNamespace(outer.name().toString(), outer.namespaceUri().toString());
            if (outer.name() == "configID" && !inlineXml) {
                QVERIFY(parsePayload(outer.readElementText()));
            } else if (outer.name() == "table" && !inlineXml) {
                QVERIFY(parsePayload(outer.readElementText()));
            } else if (inlineXml && outer.name() == "SubConfigId") {
                const QString type = outer.attributes().value("type").toString();
                const QString value = outer.readElementText();
                ids.append(type + "=" + value);
            } else if (inlineXml && outer.name() == "DataRouter") {
                const auto a = outer.attributes();
                routers.append(a.value("name").toString() + "|" + a.value("ipAddress").toString() + "|" +
                               a.value("port").toString() + "|" + a.value("type").toString());
            }
        }
        QVERIFY2(!outer.hasError(), qPrintable(outer.errorString()));
        QCOMPARE(ids, QStringList({"describe=custom<&>", "prepare=pedestals", "configure=physics"}));
        QCOMPARE(routers, QStringList({"data0|10.0.0.2|46005|TCP"}));
        QCOMPARE(payloadNamespaces.size(), 8);
        const QString expectedNamespace;
        for (const QString &uri : payloadNamespaces) QCOMPARE(uri, expectedNamespace);
        QVERIFY(request.contains("urn:http://localhost:8083/"));
    }
}

void RunSoapNamespacedResponseEnvelopes()
{
    int state = -1, transition = -1; QString error;
    const QByteArray getState =
        "<?xml version=\"1.0\"?>"
        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:ecc=\"urn:ecc\">"
        "<SOAP-ENV:Body><ecc:GetStateResponse><ErrorCode>0</ErrorCode><ErrorMessage/>"
        "<State>5</State><Transition>7</Transition></ecc:GetStateResponse></SOAP-ENV:Body></SOAP-ENV:Envelope>";
    QVERIFY2(Protocol::ParseEcc(getState, true, &state, &transition, &error), qPrintable(error));
    QCOMPARE(state, 5); QCOMPARE(transition, 7);

    // The archived GetBench gSOAP server publishes its response DTO types
    // in urn:http://localhost:8083/ (getEccSoapServer.cpp). Keep the
    // generated ResponseState shape as well as its namespace here.
    const QByteArray archivedGetBenchReply =
        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "xmlns:ecc=\"urn:http://localhost:8083/\"><SOAP-ENV:Body>"
        "<ecc:ResponseState><ErrorCode>0</ErrorCode><ErrorMessage/>"
        "<State>4</State><Transition>0</Transition></ecc:ResponseState>"
        "</SOAP-ENV:Body></SOAP-ENV:Envelope>";
    QVERIFY2(Protocol::ParseEcc(archivedGetBenchReply, true, &state, &transition, &error), qPrintable(error));
    QCOMPARE(state, 4); QCOMPARE(transition, 0);

    const QByteArray describe =
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:e=\"urn:ecc\"><s:Body>"
        "<e:DescribeResponse><ErrorCode>0</ErrorCode><ErrorMessage/><Text>described</Text></e:DescribeResponse>"
        "</s:Body></s:Envelope>";
    QVERIFY2(Protocol::ParseEcc(describe, false, &state, &transition, &error), qPrintable(error));
    const QByteArray archivedDescribe =
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "xmlns:e=\"urn:http://localhost:8083/\"><s:Body>"
        "<e:ResponseText><ErrorCode>0</ErrorCode><ErrorMessage/><Text>described</Text></e:ResponseText>"
        "</s:Body></s:Envelope>";
    QVERIFY2(Protocol::ParseEcc(archivedDescribe, false, &state, &transition, &error), qPrintable(error));

    QVERIFY(!Protocol::ParseEcc(
        "<s:Envelope xmlns:s='http://schemas.xmlsoap.org/soap/envelope/'><s:Body>"
        "<ErrorCode>0</ErrorCode></s:Body></s:Envelope>", false, &state, &transition, &error));
    QVERIFY(!Protocol::ParseEcc(
        "<s:Envelope xmlns:s='urn:not-soap'><s:Body><GetStateResponse>"
        "<ErrorCode>0</ErrorCode><State>2</State><Transition>0</Transition>"
        "</GetStateResponse></s:Body></s:Envelope>", true, &state, &transition, &error));
    QVERIFY(!Protocol::ParseEcc(
        "<s:Envelope xmlns:s='http://schemas.xmlsoap.org/soap/envelope/'><s:Body>"
        "<GetState><ErrorCode>0</ErrorCode><State>2</State><Transition>0</Transition>"
        "</GetState></s:Body></s:Envelope>", true, &state, &transition, &error));
    QVERIFY(!Protocol::ParseEcc(
        "<s:Envelope xmlns:s='http://schemas.xmlsoap.org/soap/envelope/'><s:Body>"
        "<GetStateResponse><ErrorCode>0</ErrorCode></GetStateResponse>"
        "<UnrelatedResponse><State>2</State><Transition>0</Transition></UnrelatedResponse>"
        "</s:Body></s:Envelope>", true, &state, &transition, &error));
    QVERIFY(error.contains("exactly one"));
    QVERIFY(!Protocol::ParseEcc(
        "<s:Envelope xmlns:s='http://schemas.xmlsoap.org/soap/envelope/'><s:Body>"
        "<Unexpected><ErrorCode>0</ErrorCode><State>2</State><Transition>0</Transition>"
        "</Unexpected></s:Body></s:Envelope>", true, &state, &transition, &error));

    const QByteArray fault =
        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\"><SOAP-ENV:Body>"
        "<SOAP-ENV:Fault><faultcode>SOAP-ENV:Server</faultcode><faultstring>backend unavailable</faultstring>"
        "</SOAP-ENV:Fault></SOAP-ENV:Body></SOAP-ENV:Envelope>";
    QVERIFY(!Protocol::ParseEcc(fault, false, &state, &transition, &error));
    QVERIFY(error.contains("backend unavailable"));
}

}
