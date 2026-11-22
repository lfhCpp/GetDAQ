#include "mocknode_fixture_p.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QXmlStreamReader>

namespace GetDAQTestFixtures {

MockNode::MockNode(QString r, QStringList *t) : role(r), trace(t)
{
    listen(QHostAddress::LocalHost);
    connect(this, &QTcpServer::newConnection, this, [this]() {
        while (hasPendingConnections()) {
            QTcpSocket *socket = nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                QByteArray buffer = socket->property("buffer").toByteArray() + socket->readAll();
                socket->setProperty("buffer", buffer);
                const int split = buffer.indexOf("\r\n\r\n");
                if (split < 0 || socket->property("handled").toBool()) return;
                const QByteArray header = buffer.left(split);
                int length = 0;
                for (QByteArray line : header.split('\n')) {
                    const QByteArray normalized = line.trimmed();
                    if (normalized.toLower().startsWith("content-length:")) length = normalized.mid(15).trimmed().toInt();
                    if (!socket->property("headers-captured").toBool() && normalized.toLower().startsWith("soapaction:"))
                        soapActionHeaders.append(QString::fromLatin1(normalized.mid(11).trimmed()));
                }
                socket->setProperty("headers-captured", true);
                if (buffer.size() < split + 4 + length) return;
                socket->setProperty("handled", true);
                const QByteArray body = buffer.mid(split + 4, length);
                QByteArray response;
                if (header.startsWith("POST /catalog/finalize")) {
                    ++replacementFinalizeCount; trace->append(role + ":FinalizeReplacement");
                    const auto params = QJsonDocument::fromJson(body).object();
                    const bool okay = !property("legacy-no-cleanup").toBool() && params.value("operation_id").toString() == replacementOperationId &&
                        params.value("experiment").isString() && params.value("run_number").isDouble();
                    response = okay ? R"({"replacement_cleanup_complete":true})" : R"({"error":"replacement operation mismatch"})";
                } else if (header.startsWith("GET /status") || header.startsWith("POST /catalog")) {
                    if (header.startsWith("POST /catalog")) {
                        ++catalogCount; trace->append(role + ":Catalog");
                        if (hangCatalog) return;
                        const auto params = QJsonDocument::fromJson(body).object();
                        if (!params.contains("run_number") || !params.contains("experiment")) return;
                        if (params.value("replace_completed_run").toBool()) {
                            ++replacementCatalogCount;
                            const QString operationId = params.value("operation_id").toString();
                            if (replacementOperationId.isEmpty()) replacementOperationId = operationId;
                            if (replacementOperationId != operationId) return;
                        }
                    }
                    response = R"({"run_file_policies":["overwrite","append"],"disk":"test","process":"dataRouter","data_path":"/incoming","data_written_gb":0,"data_path_files":0,"disk_avail_gb":90,"disk_total_gb":100})";
                    if(header.startsWith("POST /catalog")){const auto params=QJsonDocument::fromJson(body).object();auto receipt=QJsonDocument::fromJson(response).object();if(params["overwrite_existing_run"].toBool())receipt["replacement_policy"]="overwrite";if(params["append_existing_run"].toBool())receipt["replacement_policy"]="append";response=QJsonDocument(receipt).toJson(QJsonDocument::Compact);}
                    if(property("legacy-file-policy").toBool()){auto receipt=QJsonDocument::fromJson(response).object();receipt.remove("run_file_policies");response=QJsonDocument(receipt).toJson(QJsonDocument::Compact);}
                    if(generateData) {
                        if(header.startsWith("POST /catalog"))generatedBytes=0;
                        else if(state==5)generatedBytes+=1000000;
                        auto observation=QJsonDocument::fromJson(response).object();
                        observation["data_written_bytes"]=QString::number(generatedBytes);
                        observation["data_written_gb"]=double(generatedBytes)/1e9;
                        observation["data_path_files"]=generatedBytes?1:0;
                        response=QJsonDocument(observation).toJson(QJsonDocument::Compact);
                    }
                } else {
                    QString operation;
                    QXmlStreamReader xml(body);
                    const QStringList operations = {"GetState", "Describe", "Prepare", "Configure", "Start", "Stop", "Pause", "Resume", "Undo", "Breakup"};
                    while (!xml.atEnd()) { xml.readNext(); if (xml.isStartElement() && operations.contains(xml.name().toString())) operation = xml.name().toString(); }
                    int error = 0;
                    if (operation != "GetState") trace->append(role + ":" + operation);
                    if (operation == "GetState" && hangState) return;
                    if (operation == "Start" && failStart) error = 9;
                    else if (operation == "Describe") state = 2;
                    else if (operation == "Prepare") state = 3;
                    else if (operation == "Configure") state = 4;
                    else if (operation == "Start") { if (!deferState) state = 5; }
                    else if (operation == "Stop") { if (busyStop) error = 5; else if (!deferStop) state = 4; }
                    else if (operation == "Pause" && state == 5) state = 6;
                    else if (operation == "Resume" && state == 6) state = 5;
                    else if (operation == "Breakup") state = 3;
                    else if (operation == "Undo") --state;
                    response = QString("<s:Envelope xmlns:s='http://schemas.xmlsoap.org/soap/envelope/' xmlns:ecc='urn:http://localhost:8083/'><s:Body><ecc:%4Response><ErrorCode>%1</ErrorCode><ErrorMessage>test</ErrorMessage><State>%2</State><Transition>%3</Transition></ecc:%4Response></s:Body></s:Envelope>").arg(error).arg(state).arg(transition).arg(operation.isEmpty() ? QString("Unknown") : operation).toUtf8();
                }
                if(busyStop)response.replace("<ErrorMessage>test</ErrorMessage>","<ErrorMessage>The ECC state machine is busy!</ErrorMessage>");
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(response.size()) + "\r\nConnection: close\r\n\r\n" + response);
                socket->disconnectFromHost();
            });
        }
    });
}

} // namespace GetDAQTestFixtures
