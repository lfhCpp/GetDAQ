#include "xcfgtransaction.h"
#include "xcfgexporter.h"
#include "configurationrange.h"
#include "effectiveconfigurationsnapshot.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QXmlStreamWriter>
#include <functional>
#include <stdexcept>
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace {
const int MaxBytes=8*1024*1024;
bool Fail(QString *error,const QString &message) { *error=message; return false; }
void Append(QString *destination,const QString &piece)
{
    if (piece.size()>MaxBytes || destination->size()>MaxBytes-piece.size())
        throw std::runtime_error("XML export character budget exceeded");
    *destination+=piece;
}
QString Element(const QString &name,const QMap<QString,QString> &attributes,const QString &content)
{
    QString output;QXmlStreamWriter xml(&output);xml.writeStartElement(name);
    for(auto it=attributes.constBegin();it!=attributes.constEnd();++it)xml.writeAttribute(it.key(),it.value());
    xml.writeCharacters(QString());xml.writeEndElement();
    const int close=output.lastIndexOf("</");
    if(close<0)throw std::runtime_error("Cannot form exported element");
    if(output.size()>MaxBytes-content.size())throw std::runtime_error("XML export character budget exceeded");
    output.insert(close,content);return output;
}
QString ClassPath(QString path)
{
    path.remove(QRegularExpression("\\[[^\\]]*\\]"));return path;
}
QString CanonicalQuery(const XcfgDocument &document,const QString &path)
{
    bool bracket=false;int slash=-1;
    for(int i=0;i<path.size();++i) {
        if(path[i]=='[')bracket=true;else if(path[i]==']')bracket=false;
        else if(path[i]=='/' && !bracket){slash=i;break;}
    }
    const auto root=document.Nodes()[document.Sources().value(document.RootFile()).root].path;
    return root+(slash<0 ? QString() : path.mid(slash));
}
QString LastPart(const QString &path)
{
    bool bracket=false;
    for(int i=path.size()-1;i>=0;--i) {
        if(path[i]==']')bracket=true;else if(path[i]=='[')bracket=false;
        else if(path[i]=='/' && !bracket)return path.mid(i+1);
    }
    return path;
}
QMap<QString,QList<QByteArray>> SemanticSubtree(const QJsonObject &snapshot,const QString &path,const QString &newRoot)
{
    QMap<QString,QList<QByteArray>> result;
    const auto rebase=[&](const QString &key) {return newRoot+key.mid(path.size());};
    const auto selected=[&](const QString &key) {return key==path || key.startsWith(path+'/');};
    for(const auto &object:snapshot.value("objects").toArray()) {
        const auto key=object.toString();if(selected(key))result[rebase(key)]={QByteArray("object")};
    }
    for(const auto &value:snapshot.value("scalars").toArray()) {
        const auto scalar=value.toObject();const auto key=scalar.value("path").toString();if(!selected(key))continue;
        result[rebase(key)]={QByteArray("scalar"),scalar.value("type").toString().toUtf8(),
            scalar.value("normalized_value").toString().toUtf8(),scalar.value("unit").toString().toUtf8(),
            QByteArray::fromStdString(GetDAQ::EncodeConfigurationRange(scalar.value("range").toString().toStdString()))};
    }
    return result;
}
bool Inspect(const QByteArray &bytes,QJsonObject *snapshot,QString *error)
{
    QTemporaryDir temporary;
    if(!temporary.isValid())return Fail(error,"Cannot create private XML export validation directory");
    const auto path=temporary.filePath("candidate.xcfg");QFile file(path);
    if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly) || file.write(bytes)!=bytes.size() || !file.flush())
        return Fail(error,"Cannot write private XML export candidate: "+file.errorString());
    file.close();XcfgDocument candidate;
    return candidate.Load(path,error) && EffectiveConfigurationSnapshot::Capture(candidate,snapshot,error);
}
}

bool XcfgExporter::CreatePreview(const XcfgDocument &document,const Request &request,Preview *preview,QString *error)
{
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    try {
        if(!preview || (request.mode!=FlattenDeclarations && request.mode!=EffectiveConcreteObject))
            return Fail(error,"Unknown XML export mode");
        if(request.destination.isEmpty() || request.destination.contains(QChar(0)) || request.destination.contains("://"))
            return Fail(error,"Choose a new local XML output file");
        const QFileInfo target(request.destination);const auto parent=QFileInfo(target.absolutePath()).canonicalFilePath();
        if(parent.isEmpty() || !QFileInfo(parent).isDir() || target.fileName().isEmpty())
            return Fail(error,"XML output parent directory must exist");
        const auto destination=QDir(parent).filePath(target.fileName());
        if(QFileInfo::exists(destination) || QFileInfo(destination).isSymLink())return Fail(error,"XML output already exists; choose a new file");
        QJsonObject original;
        if(!EffectiveConfigurationSnapshot::Capture(document,&original,error) || !document.BuildProperties(error))return false;
        const int selected=document.Lookup(request.path,error);
        if(selected<0)return false;
        if(document.fNodes[selected].scalar)return Fail(error,"XML export requires an object; select its parent to export parameters");
        const auto query=CanonicalQuery(document,request.path);int visits=0;
        const auto visit=[&](int depth) {
            if(depth>128 || ++visits>200000)throw std::runtime_error("XML export depth/node budget exceeded");
        };
        const auto scalarAttributes=[&](const XcfgDocument::Node &node,const QString &classPath) {
            auto attributes=node.attributes;attributes.remove("include");attributes.remove("unit");attributes.remove("range");
            if(!document.fProperties.contains(classPath))throw std::runtime_error("Exported scalar class not projected: "+classPath.toStdString());
            const auto info=document.fProperties.value(classPath);
            if(info.unitSource>=0)attributes["unit"]=info.unit;
            if(info.rangeSource>=0)attributes["range"]=info.range;
            attributes["type"]=QString::fromStdString(GetDAQ::ConfigurationValueType(GetDAQ::ParseConfigurationValue(node.value.toStdString(),node.attributes.value("type").toStdString())));
            return attributes;
        };
        QString output;
        if(request.mode==FlattenDeclarations) {
            std::function<QString(int,const QString &,int)> declaration,content;
            content=[&](int index,const QString &classPath,int depth) -> QString {
                visit(depth);const auto &node=document.fNodes[index];const auto &source=document.fSources[node.file];QString text;
                if(node.includedRoot>=0)Append(&text,content(node.includedRoot,classPath,depth+1));
                int begin=node.contentStart;
                for(int child:node.children) {
                    const auto &value=document.fNodes[child];
                    Append(&text,source.text.mid(begin,value.tagStart-begin));
                    Append(&text,declaration(child,classPath+'/'+value.name,depth+1));begin=value.elementEnd;
                }
                if(!node.selfClosing && node.contentEnd>=begin)Append(&text,source.text.mid(begin,node.contentEnd-begin));
                return text;
            };
            declaration=[&](int index,const QString &classPath,int depth) -> QString {
                visit(depth);const auto &node=document.fNodes[index];auto attributes=node.attributes;attributes.remove("include");
                // A standalone document root is a container, not a wildcard child
                // declaration. Nested '*' templates retain their original IDs.
                if(index==selected && depth==0 && attributes.value("id")=="*")attributes.remove("id");
                if(node.scalar) {
                    const auto &source=document.fSources[node.file];
                    const auto inner=node.selfClosing ? QString() : source.text.mid(node.contentStart,node.contentEnd-node.contentStart);
                    return Element(node.name,scalarAttributes(node,classPath),inner);
                }
                return Element(node.name,attributes,content(index,classPath,depth));
            };
            output=declaration(selected,ClassPath(query),0);
        } else {
            if(query.contains("[*]"))return Fail(error,"Effective XML export requires a concrete object path, without '*' templates");
            std::function<QString(const QString &,int)> effective;
            effective=[&](const QString &path,int depth) -> QString {
                visit(depth);const int index=document.Lookup(path,error);
                if(index<0)throw std::runtime_error(error->toStdString());
                const auto &node=document.fNodes[index];auto attributes=node.attributes;attributes.remove("include");
                const auto part=LastPart(path);const int bracket=part.indexOf('[');
                if(bracket<0)attributes.remove("id");else attributes["id"]=part.mid(bracket+1,part.size()-bracket-2);
                if(node.scalar) {
                    auto scalar=scalarAttributes(node,ClassPath(path));
                    if(attributes.contains("id"))scalar["id"]=attributes.value("id");else scalar.remove("id");
                    return Element(node.name,scalar,XcfgDocument::EncodeScalarText(node.value));
                }
                QString children;
                const auto paths=document.EffectiveChildren(path,error);
                if(!error->isEmpty())throw std::runtime_error(error->toStdString());
                for(const auto &child:paths)Append(&children,effective(child,depth+1));
                return Element(node.name,attributes,children);
            };
            output=effective(query,0);
        }
        const auto bytes=output.toUtf8();if(bytes.size()>MaxBytes)return Fail(error,"XML export exceeds 8 MiB");
        Preview result;result.request=request;result.request.path=query;result.request.destination=destination;
        result.rootFile=document.RootFile();result.xml=bytes;
        QJsonObject exported;if(!Inspect(bytes,&exported,error))return false;
        result.effectiveSha256=exported.value("effective_sha256").toString();
        result.objectCount=exported.value("objects").toArray().size();result.scalarCount=exported.value("scalars").toArray().size();
        if(request.mode==EffectiveConcreteObject && SemanticSubtree(original,query,LastPart(query))!=
            SemanticSubtree(exported,exported.value("root_path").toString(),LastPart(query)))
            return Fail(error,"Exported concrete subtree does not preserve every effective object/value/type/unit/range");
        if(!document.SourcesUnchanged(error))return false;
        for(auto it=document.Sources().constBegin();it!=document.Sources().constEnd();++it)result.sourceBytes[it.key()]=it->bytes;
        *preview=result;error->clear();return true;
    } catch(const std::exception &failure) {return Fail(error,QString::fromUtf8(failure.what()));}
}

bool XcfgExporter::Save(const XcfgDocument &document,const Preview &preview,QString *error)
{
    XcfgIoTransaction ioTransaction(error,XcfgIoTransaction::Write);if(!ioTransaction.IsLocked())return false;

    Preview current;
    if(document.RootFile()!=preview.rootFile)return Fail(error,"Loaded configuration changed; create a new XML export preview");
    if(!CreatePreview(document,preview.request,&current,error))return false;
    if(current.xml!=preview.xml || current.sourceBytes!=preview.sourceBytes || current.effectiveSha256!=preview.effectiveSha256 ||
       current.objectCount!=preview.objectCount || current.scalarCount!=preview.scalarCount)
        return Fail(error,"XML export preview changed; review a new preview");
    if(!document.SourcesUnchanged(error))return false;
    QFile file(current.request.destination);
    if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly))return Fail(error,"Cannot create new XML output: "+file.errorString());
    if(file.write(current.xml)!=current.xml.size() || !file.flush())
        return Fail(error,"XML output created but writing failed; inspect "+current.request.destination+": "+file.errorString());
#ifdef Q_OS_UNIX
    if(::fsync(file.handle())!=0)return Fail(error,"XML output created but file synchronization failed: "+current.request.destination);
#endif
    file.close();
    XcfgDocument saved;QJsonObject snapshot;
    if(!saved.Load(current.request.destination,error) || !EffectiveConfigurationSnapshot::Capture(saved,&snapshot,error) ||
       snapshot.value("effective_sha256").toString()!=current.effectiveSha256 ||
       saved.Sources().value(saved.RootFile()).bytes!=current.xml || !document.SourcesUnchanged(error)) {
        const auto cause=error->isEmpty() ? QString("Saved bytes or effective digest differ from the reviewed output") : *error;
        return Fail(error,"XML output was saved but subsequent verification failed; inspect "+current.request.destination+": "+cause);
    }
    error->clear();return true;
}
