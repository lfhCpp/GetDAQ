#include "protocol.h"
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <cmath>
#include <limits>

QByteArray Protocol::EccRequest(const NodeConfig &n, const QString &experiment, const QString &operation)
{
    QByteArray bytes;
    QXmlStreamWriter x(&bytes);
    x.writeStartDocument();
    x.writeStartElement("SOAP-ENV:Envelope");
    x.writeNamespace("http://schemas.xmlsoap.org/soap/envelope/", "SOAP-ENV");
    x.writeNamespace("http://schemas.xmlsoap.org/soap/encoding/", "SOAP-ENC");
    x.writeNamespace("http://www.w3.org/2001/XMLSchema-instance", "xsi");
    x.writeNamespace("http://www.w3.org/2001/XMLSchema", "xsd");
    // GetBench overrides the generated `ecc` mapping in both its SOAP client
    // and server namespace tables with this legacy service URI.
    x.writeNamespace(n.soapNamespace, "ecc");
    x.writeStartElement("SOAP-ENV:Body");
    x.writeStartElement("ecc:" + operation);
    if (operation != "GetState") {
        QString configXml, linksXml;
        QXmlStreamWriter config(&configXml), links(&linksXml);
        config.writeStartElement("ConfigId");
        for (const QString &kind : {QString("describe"), QString("prepare"), QString("configure")}) {
            const QString id = n.ConfigId(kind, experiment);
            config.writeStartElement("SubConfigId");
            config.writeAttribute("type", kind);
            config.writeCharacters(id);
            config.writeEndElement();
        }
        config.writeEndElement();
        links.writeStartElement("DataLinkSet");
        for (int i = 0; i < 2; ++i) {
            const int port = i == 0 ? n.routerPort : n.exporterPort;
            if (port == 0) continue;
            const QString host = i == 0 ? n.routerHost : n.exporterHost;
            links.writeStartElement("DataLink");
            links.writeEmptyElement("DataSender");
            links.writeAttribute("id", n.mutant ? "Mutant[master]" : QString("CoBo[%1]").arg(n.boardIndex>=0?n.boardIndex:n.id));
            links.writeEmptyElement("DataRouter");
            links.writeAttribute("ipAddress", host.isEmpty() ? n.host : host);
            links.writeAttribute("name", QString(i == 0 ? "data%1" : "exporter%1").arg(n.id));
            links.writeAttribute("port", QString::number(port));
            links.writeAttribute("type", "TCP");
            links.writeEndElement();
        }
        links.writeEndElement();
        auto parameter = [&x, &n](const QString &name, const QString &value) {
            x.writeStartElement(name);
            if (n.soapInlineXml) {
                QXmlStreamReader r(value);
                while (!r.atEnd()) { r.readNext(); if (!r.isStartDocument() && !r.isEndDocument()) x.writeCurrentToken(r); }
            } else x.writeCharacters(value);
            x.writeEndElement();
        };
        parameter("configID", configXml);
        parameter("table", linksXml);
    }
    x.writeEndElement();
    x.writeEndElement();
    x.writeEndElement();
    x.writeEndDocument();
    return bytes;
}

bool Protocol::ParseEcc(const QByteArray &xml, bool status, int *state, int *transition, QString *error)
{
    QXmlStreamReader x(xml);
    const QString soapNamespace = "http://schemas.xmlsoap.org/soap/envelope/";
    QMap<QString, QString> values;
    bool envelope = false;
    bool body = false;
    bool response = false;
    bool fault = false;
    int bodyPayloadCount = 0;
    int responseDepth = -1;
    int depth = 0;
    while (!x.atEnd()) {
        x.readNext();
        if (x.tokenType() == QXmlStreamReader::DTD) { *error = "SOAP DTD is not supported."; return false; }
        if (x.isStartElement()) {
            const QString name = x.name().toString();
            const QString localName = name.toLower();
            const QString namespaceUri = x.namespaceUri().toString();
            if (depth == 0) {
                if (localName != "envelope" || namespaceUri != soapNamespace) {
                    *error = "Missing SOAP 1.1 Envelope.";
                    return false;
                }
                envelope = true;
            } else if (depth == 1 && envelope && localName == "body" && namespaceUri == soapNamespace) {
                if (body) { *error = "Duplicate SOAP Body."; return false; }
                body = true;
            } else if (depth == 2 && body) {
                if (++bodyPayloadCount != 1) {
                    *error = "SOAP Body must contain exactly one operation response.";
                    return false;
                }
                if (localName == "fault" && namespaceUri == soapNamespace) {
                    fault = true;
                    responseDepth = depth + 1;
                } else if (localName.endsWith("response", Qt::CaseInsensitive) ||
                           localName == "responsetext" || localName == "responsestate") {
                    response = true;
                    responseDepth = depth + 1;
                } else {
                    *error = "Unexpected SOAP Body operation response.";
                    return false;
                }
            }
            const bool inResponse = responseDepth >= 0 && depth >= responseDepth;
            if (inResponse && (localName == "errorcode" || localName == "errormessage" ||
                               localName == "state" || localName == "transition" || localName == "faultstring")) {
                if (values.contains(localName)) { *error = "Duplicate SOAP field: " + localName; return false; }
                values.insert(localName, x.readElementText());
                continue;
            }
            ++depth;
        } else if (x.isEndElement()) {
            if (responseDepth == depth) responseDepth = -1;
            if (depth > 0) --depth;
        }
    }
    if (x.hasError()) { *error = x.errorString(); return false; }
    if (!envelope || !body || (!response && !fault)) { *error = "Missing SOAP 1.1 Body operation response."; return false; }
    if (fault) { *error = "SOAP fault: " + values.value("faultstring"); return false; }
    bool ok = false;
    const int code = values.value("errorcode").toInt(&ok);
    if (!ok) { *error = "Missing/invalid ErrorCode."; return false; }
    if (code != 0) { *error = QString("ECC error %1: %2").arg(code).arg(values.value("errormessage")); return false; }
    if (status) {
        *state = values.value("state").toInt(&ok);
        if (!ok || *state < 0 || *state > 6) { *error = "Invalid ECC State."; return false; }
        *transition = values.value("transition").toInt(&ok);
        if (!ok || *transition < 0 || *transition > 9) { *error = "Missing/invalid Transition."; return false; }
    }
    return true;
}

bool Protocol::ParseSentry(const QByteArray &json, NodeStatus *s, QString *error)
{
    QJsonParseError parse;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) { *error = "Invalid sentry JSON: " + parse.errorString(); return false; }
    const QJsonObject o = doc.object();
    for (const QString &key : {QString("disk"), QString("process"), QString("data_path")})
        if (!o.value(key).isString()) { *error = "Missing/invalid sentry field: " + key; return false; }
    for (const QString &key : {QString("data_written_gb"), QString("data_path_files"), QString("disk_avail_gb"), QString("disk_total_gb")}) {
        const auto v = o.value(key);
        if (!v.isDouble() || !std::isfinite(v.toDouble()) || v.toDouble() < 0) { *error = "Missing/invalid sentry field: " + key; return false; }
    }
    const double files = o.value("data_path_files").toDouble();
    if (files != std::floor(files) || files > std::numeric_limits<int>::max()) { *error = "Invalid file count."; return false; }
    s->disk = o.value("disk").toString();
    s->process = o.value("process").toString();
    s->dataPath = o.value("data_path").toString();
    s->files = int(files);
    s->writtenGB = o.value("data_written_gb").toDouble();
    if(o.contains("data_written_bytes")) {
        const auto value=o.value("data_written_bytes");bool okay=false;
        const auto bytes=value.toString().toULongLong(&okay);
        if(!value.isString() || !okay || QString::number(bytes)!=value.toString()) {*error="Invalid raw-file byte counter.";return false;}
        s->rawFileBytes=bytes;s->rawFileBytesKnown=true;
    } else {
        // Old Sentry reports the same actual directory sizes in decimal GB.
        const long double bytes=static_cast<long double>(s->writtenGB)*1e9L;
        if(bytes>=0 && bytes<static_cast<long double>(std::numeric_limits<quint64>::max())) {
            s->rawFileBytes=static_cast<quint64>(bytes+0.5L);s->rawFileBytesKnown=true;
        }
    }
    s->availableGB = o.value("disk_avail_gb").toDouble();
    s->totalGB = o.value("disk_total_gb").toDouble();
    if (s->availableGB > s->totalGB) { *error = "Available disk space exceeds total."; return false; }
    return true;
}
