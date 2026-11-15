#include "simulator.h"
#include <QDebug>
#include <QTimer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace {
QByteArray Soap(int code, const QString &message, int state, bool status, int transition = 0)
{
    QByteArray data; QXmlStreamWriter x(&data);
    x.writeStartDocument(); x.writeStartElement("SOAP-ENV:Envelope");
    x.writeNamespace("http://schemas.xmlsoap.org/soap/envelope/", "SOAP-ENV");
    x.writeStartElement("SOAP-ENV:Body"); x.writeStartElement(status ? "GetStateResponse" : "OperationResponse");
    x.writeTextElement("ErrorCode", QString::number(code)); x.writeTextElement("ErrorMessage", message);
    if (status) { x.writeTextElement("State", QString::number(state)); x.writeTextElement("Transition", QString::number(transition)); }
    else x.writeTextElement("Text", code ? "Failed" : "Accepted");
    x.writeEndElement(); x.writeEndElement(); x.writeEndElement(); x.writeEndDocument(); return data;
}
}
void Simulator::HandleEcc(int id, QString method, QByteArray body, HttpConnection *connection)
{
    if (method != "POST") { connection->Reply(405, "{}"); return; }
    QString operation; int found = 0;
    const QStringList names = {"GetState", "Describe", "Prepare", "Configure", "Start", "Stop", "Undo", "Breakup"};
    QXmlStreamReader xml(body);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && names.contains(xml.name().toString())) { operation = xml.name().toString(); ++found; }
    }
    if (xml.hasError() || found != 1) { connection->Reply(400, "{\"error\":\"invalid SOAP command\"}"); return; }
    if (operation == "GetState") { connection->Reply(200, Soap(0, QString(), fStates[id], true, fTransitions[id]), "text/xml"); return; }
    if (fTransitions[id]) { connection->Reply(200, Soap(1, "Busy", fStates[id], false), "text/xml"); return; }
    const int mutant = fStates.size() - 1;
    int target = -1;
    const int current = fStates[id];
    if (operation == "Describe" && current == 1) target = 2;
    if (operation == "Prepare" && current == 2 && (id == mutant || fStates[mutant] == 3 || fStates[mutant] == 4)) target = 3;
    if (operation == "Configure" && current == 3) {
        bool allowed = true;
        if (id == mutant) for (int i = 0; i < mutant; ++i) if (fStates[i] != 4) allowed = false;
        if (allowed) target = 4;
    }
    if (operation == "Start" && current == 4 && id != fFailStart) {
        bool allowed = true;
        if (id == mutant) for (int i = 0; i < mutant; ++i) if (fStates[i] != 5) allowed = false;
        if (allowed) target = 5;
    }
    if (operation == "Stop" && current == 5 && (id == mutant || fStates[mutant] == 4)) target = 4;
    if (operation == "Breakup" && current == 4) target = 3;
    if (operation == "Undo" && (current == 2 || current == 3)) target = current - 1;
    if (target < 0) { connection->Reply(200, Soap(1, "Invalid transition, ordering, or injected failure", current, false), "text/xml"); return; }
    qInfo().noquote() << QString("Node %1: %2 -> %3").arg(id).arg(operation).arg(target);
    fTransitions[id] = operation == "Describe" ? 1 : operation == "Prepare" ? 2 : operation == "Configure" ? 3 : operation == "Start" ? 4 : operation == "Stop" ? 5 : operation == "Breakup" ? 8 : 9;
    QTimer::singleShot(fDelayMs, this, [this, id, target]() { fStates[id] = target; fTransitions[id] = 0; });
    connection->Reply(200, Soap(0, QString(), current, false), "text/xml");
}
