#include "sentrycatalog_p.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <limits>

namespace SentryCatalogInternal {

QString NativeReceiverSealError(const QString &dataDirectory)
{
    const QDir root(dataDirectory);
    const QString sealPath=root.filePath(".getdaq-receiver-seal.json");
    QFile seal(sealPath);
    if(QFileInfo(sealPath).isSymLink() || !seal.open(QIODevice::ReadOnly) || seal.size()>16*1024*1024)
        return "No valid receiver seal; acquisition may be incomplete. Inspect receiver logs.";

    QJsonParseError parse;
    const QJsonDocument document=QJsonDocument::fromJson(seal.readAll(),&parse);
    const QJsonObject receipt=document.object();
    if(parse.error!=QJsonParseError::NoError || !document.isObject() || receipt.value("schema").toInt()!=1 ||
       !receipt.value("complete").toBool() || !receipt.value("durable").toBool() || !receipt.value("files").isArray())
        return "Receiver seal is invalid or data was not durably synced.";

    QMap<QString,qint64> expected;
    quint64 total=0;
    for(const QJsonValue &value:receipt.value("files").toArray()) {
        const QJsonObject item=value.toObject();
        const QString name=item.value("name").toString();
        bool sizeOkay=false;
        const qint64 size=item.value("bytes").toString().toLongLong(&sizeOkay);
        if(name.isEmpty() || QFileInfo(name).fileName()!=name || !name.endsWith(".graw") ||
           expected.contains(name) || !sizeOkay || size<0)
            return "Malformed receiver file entry.";
        const quint64 fileBytes=quint64(size);
        if(fileBytes>std::numeric_limits<quint64>::max()-total)
            return "Receiver seal total overflows.";
        expected.insert(name,size);
        total+=fileBytes;
    }

    bool totalOkay=false;
    if(receipt.value("received_bytes").toString().toULongLong(&totalOkay)!=total || !totalOkay)
        return "Receiver seal total does not match file entries.";

    const QFileInfoList raw=root.entryInfoList({"*.graw"},QDir::Files|QDir::System|QDir::Hidden);
    if(raw.size()!=expected.size())return "Incoming file set changed after receiver seal.";
    for(const QFileInfo &file:raw)
        if(file.isSymLink() || !file.isFile() || !expected.contains(file.fileName()) || expected.value(file.fileName())!=file.size())
            return "Incoming file size/name changed after receiver seal.";
    return QString();
}

}
