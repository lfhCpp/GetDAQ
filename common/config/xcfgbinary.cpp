#include "xcfgtransaction.h"
#include "xcfgbinary.h"
#include "xcfgbinary_codec_p.h"
#include "effectiveconfigurationsnapshot.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <stdexcept>

namespace {
const int MaxBytes=8*1024*1024;
void Require(bool condition,const char *message) {if(!condition)throw std::runtime_error(message);}
bool Fail(QString *error,const QString &message) {if(error)*error=message;return false;}
bool Inspect(const QByteArray &xml,int *objects,int *scalars,QString *error,QJsonObject *captured=nullptr) {
    QTemporaryDir temporary;if(!temporary.isValid())return Fail(error,"Cannot create binary conversion validation directory");QFile file(temporary.filePath("candidate.xcfg"));
    if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly) || file.write(xml)!=xml.size() || !file.flush())return Fail(error,"Cannot validate converted XML");
    file.close();
    XcfgDocument document;QJsonObject snapshot;if(!document.Load(file.fileName(),error) || !EffectiveConfigurationSnapshot::Capture(document,&snapshot,error))return false;
    if(captured)*captured=snapshot;
    *objects=0;*scalars=0;
    for(const auto &n:document.Nodes())n.scalar ? ++*scalars : ++*objects;
    return true;
}
bool Destination(const QString &path,QString *destination,QString *error) {
    QFileInfo f(path);const auto parent=QFileInfo(f.absolutePath()).canonicalFilePath();
    if(path.isEmpty() || path.contains(QChar(0)) || path.contains("://") || parent.isEmpty() || f.fileName().isEmpty())return Fail(error,"Choose a new local conversion file in an existing directory");
    *destination=QDir(parent).filePath(f.fileName());if(QFileInfo::exists(*destination) || QFileInfo(*destination).isSymLink())return Fail(error,"Conversion output exists; choose a new file");return true;
}
bool Read(const QString &path,QByteArray *bytes,QString *error) {
    QFile file(path);if(!file.open(QIODevice::ReadOnly) || file.size()>MaxBytes)return Fail(error,"Cannot read binary configuration within 8MiB limit");*bytes=file.read(MaxBytes+1);
    return bytes->size()<=MaxBytes && file.error()==QFile::NoError ? true : Fail(error,"Binary input read failed or grew beyond limit");
}
}
bool XcfgBinary::Decode(const QByteArray &binary,QByteArray *xml,QString *error) {
    try {Require(xml,"Missing XML output");*xml=XcfgBinaryCodec::Decode(binary);int objects,scalars;return Inspect(*xml,&objects,&scalars,error);}catch(const std::exception &e){return Fail(error,QString::fromUtf8(e.what()));}
}
bool XcfgBinary::Encode(const QByteArray &xml,QByteArray *binary,QString *error) {
    try {Require(binary,"Missing binary output");int objects,scalars;QJsonObject before,after;if(!Inspect(xml,&objects,&scalars,error,&before))return false;*binary=XcfgBinaryCodec::Encode(xml);QByteArray decoded;
        if(!Decode(*binary,&decoded,error) || !Inspect(decoded,&objects,&scalars,error,&after))return false;
        Require(before.value("effective_sha256")==after.value("effective_sha256"),"Binary round trip changed effective configuration semantics");return true;}catch(const std::exception &e){return Fail(error,QString::fromUtf8(e.what()));}
}
bool XcfgBinary::PreviewImport(const QString &source,const QString &destination,Preview *preview,QString *error) {
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    if(!preview)return Fail(error,"Missing conversion preview");
    Preview p;p.importing=true;p.source=QFileInfo(source).canonicalFilePath();QByteArray input;
    if(p.source.isEmpty())return Fail(error,"Binary input file does not exist");
    if(!Destination(destination,&p.destination,error) || !Read(p.source,&input,error) || !Decode(input,&p.bytes,error) || !Inspect(p.bytes,&p.objects,&p.scalars,error))return false;
    p.sourceBytes[p.source]=input;*preview=p;return true;
}
bool XcfgBinary::PreviewExport(const XcfgDocument &document,const XcfgExporter::Request &request,Preview *preview,QString *error) {
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    if(!preview)return Fail(error,"Missing conversion preview");
    if(document.RootFile().isEmpty())return Fail(error,"Open an XML configuration before binary export");
    XcfgExporter::Preview exported;
    if(!XcfgExporter::CreatePreview(document,request,&exported,error))return false;
    Preview p;p.source=document.RootFile();p.destination=exported.request.destination;p.objectPath=exported.request.path;p.mode=exported.request.mode;p.sourceBytes=exported.sourceBytes;
    if(!Encode(exported.xml,&p.bytes,error))return false;
    p.objects=exported.objectCount;p.scalars=exported.scalarCount;*preview=p;return true;
}
bool XcfgBinary::Save(const Preview &preview,QString *error) {
    XcfgIoTransaction ioTransaction(error,XcfgIoTransaction::Write);if(!ioTransaction.IsLocked())return false;

    Preview current;
    if(preview.importing) {if(!PreviewImport(preview.source,preview.destination,&current,error))return false;}
    else {XcfgDocument document;if(!document.Load(preview.source,error))return false;XcfgExporter::Request request;request.path=preview.objectPath;request.destination=preview.destination;request.mode=preview.mode;if(!PreviewExport(document,request,&current,error))return false;}
    if(current.sourceBytes!=preview.sourceBytes || current.bytes!=preview.bytes || current.destination!=preview.destination)return Fail(error,"Conversion review is stale or altered; review again");
    QFile file(current.destination);if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly))return Fail(error,"Cannot exclusively create conversion output: "+file.errorString());
    if(file.write(current.bytes)!=current.bytes.size() || !file.flush()) {const auto message=file.errorString();file.close();return Fail(error,"Conversion output incomplete; inspect retained file: "+message);}file.close();return true;
}
