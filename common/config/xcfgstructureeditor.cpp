#include "xcfgtransaction.h"
#include "xcfgstructureeditor.h"
#include "xcfgstructureeditor_p.h"
#include "effectiveconfigurationsnapshot.h"
#include "configurationrange.h"
#include <QRegularExpression>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <stdexcept>

namespace {
using namespace XcfgStructureEditorInternal;

QString Attribute(QString value) {
    value.replace('&',"&amp;");value.replace('<',"&lt;");value.replace('>',"&gt;");
    value.replace('"',"&quot;");value.replace('\n',"&#10;");value.replace('\r',"&#13;");value.replace('\t',"&#9;");
    return value;
}
// 逐属性扫描，不能用正则误匹配另一个带引号属性值里的id/include文本。
QString EditAttribute(QString opening,const QString &name,const QString &value,bool remove) {
    int at=1;
    while (at<opening.size() && !opening[at].isSpace() && opening[at]!='>' && opening[at]!='/') ++at;
    while (at<opening.size()) {
        const int begin=at;
        while (at<opening.size() && opening[at].isSpace()) ++at;
        if (at>=opening.size() || opening[at]=='/' || opening[at]=='>') break;
        const int keyStart=at;
        while (at<opening.size() && !opening[at].isSpace() && opening[at]!='=') ++at;
        const QString key=opening.mid(keyStart,at-keyStart);
        while (at<opening.size() && opening[at].isSpace()) ++at;
        if (at>=opening.size() || opening[at++]!='=') throw std::runtime_error("Invalid attribute offsets");
        while (at<opening.size() && opening[at].isSpace()) ++at;
        if (at>=opening.size() || (opening[at]!='\'' && opening[at]!='"')) throw std::runtime_error("Invalid attribute quote");
        const auto quote=opening[at++];
        while (at<opening.size() && opening[at]!=quote) ++at;
        if (at>=opening.size()) throw std::runtime_error("Unterminated attribute");
        ++at;
        if (key==name) {
            opening.replace(begin,at-begin,remove ? QString() : " "+name+"=\""+Attribute(value)+"\"");
            return opening;
        }
    }
    if (!remove) {
        int end=opening.lastIndexOf('>');
        if (end<0) throw std::runtime_error("Invalid opening tag");
        if (end>0 && opening[end-1]=='/') --end;
        opening.insert(end," "+name+"=\""+Attribute(value)+"\"");
    }
    return opening;
}
bool ValidId(const QString &id) {
    return id.size()<=1024 && id==id.trimmed() && !id.contains('[') && !id.contains(']') &&
        std::none_of(id.begin(),id.end(),[](QChar c) {return c.unicode()<32;});
}
QString RenameElement(const XcfgDocument::Node &node,const XcfgDocument::Source &source,const QString &name) {
    QString text=source.text.mid(node.tagStart,node.elementEnd-node.tagStart);
    if (!node.selfClosing) text.replace(node.contentEnd-node.tagStart+2,node.name.size(),name);
    text.replace(1,node.name.size(),name);
    return text;
}
}

QString XcfgStructureEditor::RewriteOpeningAttribute(const QString &opening,const QString &name,const QString &value,bool remove)
{
    return EditAttribute(opening,name,value,remove);
}

bool XcfgStructureEditor::CreatePreview(const XcfgDocument &document,const Request &request,Preview *preview,QString *error)
{
    return CreatePreviewImpl(document,request,preview,error,true);
}
bool XcfgStructureEditor::CreatePreviewImpl(const XcfgDocument &document,const Request &request,Preview *preview,QString *error,bool verifyDisk)
{
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    if(request.operation==OverrideInheritedObject)return CreateObjectOverride(document,request,preview,error,verifyDisk);
    try {
        if (!preview || request.node<0 || request.node>=document.fNodes.size()) return Fail(error,"Select a source declaration");
        if (request.operation<AddObject || request.operation>ImportConfiguration) return Fail(error,"Unknown structural operation");
        if (verifyDisk && !document.CheckSources(error)) return false;
        const auto node=document.fNodes[request.node];const auto source=document.fSources.value(node.file);
        if (node.tagStart<0 || node.elementEnd<=node.tagStart) return Fail(error,"Declaration offsets unavailable");
        QString replacement;QString updated=source.text;bool wholeSource=false;
        QMap<QString,QByteArray> extraDependencies;
        const bool insert=request.operation==AddObject || request.operation==AddScalar;
        if (insert) {
            if (node.scalar) return Fail(error,"Cannot insert inside a scalar");
            if (!QRegularExpression("^[A-Za-z_][A-Za-z0-9_.-]*$").match(request.name).hasMatch()) return Fail(error,"Use a simple XML element name");
            if (!ValidId(request.id)) return Fail(error,"Invalid object ID");
            QMap<QString,QString> attributes;
            if (!request.id.isEmpty()) attributes["id"]=request.id;
            const bool scalar=request.operation==AddScalar;
            if (scalar) {
                if (!QStringList({"int","hex","real","bool","string"}).contains(request.type)) return Fail(error,"Choose an explicit scalar type");
                attributes["type"]=request.type;
                if (!request.unit.isEmpty()) attributes["unit"]=request.unit;
                if (!request.range.isEmpty()) attributes["range"]=request.range;
                XcfgDocument::Node value;value.scalar=true;value.value=request.value;value.attributes=attributes;
                if (!XcfgDocument::CheckValue(value,request.value,error)) return false;
            }
            QMap<QString,int> siblings;
            if (!document.ChildIndex(request.node,&siblings,error)) return false;
            if (siblings.contains(request.name+'['+request.id+']')) return Fail(error,"Duplicate declaration/include collision");
            replacement=AppendChildren(node,source,Element(request.name,attributes,request.value,scalar));
        } else if (request.operation==RemoveDeclaration || request.operation==ChangeId || request.operation==ChangeName) {
            if (request.node==source.root) return Fail(error,"Cannot remove or rename a document root");
            if (request.operation==ChangeId) {
                if (!ValidId(request.id)) return Fail(error,"Invalid object ID");
                if (request.id==node.attributes.value("id")) return Fail(error,"ID is unchanged");
                const auto opening=source.text.mid(node.tagStart,node.contentStart-node.tagStart);
                replacement=EditAttribute(opening,"id",request.id,request.id.isEmpty())+
                    source.text.mid(node.contentStart,node.elementEnd-node.contentStart);
            } else if (request.operation==ChangeName) {
                if (!QRegularExpression("^[A-Za-z_][A-Za-z0-9_.-]*$").match(request.name).hasMatch()) return Fail(error,"Use a simple XML element name");
                if (request.name==node.name) return Fail(error,"Element name is unchanged");
                replacement=RenameElement(node,source,request.name);
            }
        } else if (request.operation==ChangeScalarProperties) {
            if (!node.scalar) return Fail(error,"Select a scalar to edit type/unit/range");
            if (!request.type.isEmpty() && !QStringList({"int","hex","real","bool","string"}).contains(request.type)) return Fail(error,"Unknown scalar type");
            if (request.type.isEmpty() && node.value.isEmpty()) return Fail(error,"Empty scalar requires an explicit type");
            QString opening=source.text.mid(node.tagStart,node.contentStart-node.tagStart);
            opening=EditAttribute(opening,"type",request.type,request.type.isEmpty());
            opening=EditAttribute(opening,"unit",request.unit,request.unit.isEmpty());
            opening=EditAttribute(opening,"range",request.range,request.range.isEmpty());
            replacement=opening+source.text.mid(node.contentStart,node.elementEnd-node.contentStart);
        } else if (request.operation==ReplaceWithInclude || request.operation==ChangeInclude || request.operation==ImportConfiguration) {
            if(!CreateIncludeReplacement(document,request,node,source,&replacement,&extraDependencies,error,verifyDisk))return false;
        } else if (request.operation==CloneDeclaration || request.operation==MoveBefore || request.operation==MoveAfter) {
            int parent=-1;
            for (int i=0;i<document.fNodes.size();++i) if (document.fNodes[i].children.contains(request.node)) {parent=i;break;}
            if (parent<0) return Fail(error,"Cannot clone or move a document root");
            const auto owner=document.fNodes[parent];
            if (request.operation==CloneDeclaration) {
                const auto name=request.name.isEmpty() ? node.name : request.name;
                if (!QRegularExpression("^[A-Za-z_][A-Za-z0-9_.-]*$").match(name).hasMatch() || !ValidId(request.id)) return Fail(error,"Invalid clone name/ID");
                QString copy=RenameElement(node,source,name);
                const int openingLength=node.contentStart-node.tagStart+name.size()-node.name.size();
                copy.replace(0,openingLength,EditAttribute(copy.left(openingLength),"id",request.id,request.id.isEmpty()));
                replacement=AppendChildren(owner,source,copy);
                updated.replace(owner.tagStart,owner.elementEnd-owner.tagStart,replacement);wholeSource=true;
            } else {
                if (request.sibling==request.node || !owner.children.contains(request.sibling)) return Fail(error,"Choose a different sibling in the same source object");
                const auto sibling=document.fNodes[request.sibling];
                if (sibling.file!=node.file) return Fail(error,"Cross-file move is not supported");
                const QString text=source.text.mid(node.tagStart,node.elementEnd-node.tagStart);
                int at=request.operation==MoveBefore ? sibling.tagStart : sibling.elementEnd;
                updated.remove(node.tagStart,node.elementEnd-node.tagStart);
                if (at>node.tagStart) at-=node.elementEnd-node.tagStart;
                updated.insert(at,text);wholeSource=true;
            }
        } else if (request.operation==MaterializeObject) {
            if(!CreateMaterializedReplacement(document,request,node,source,&replacement,error))return false;
        } else {
            return Fail(error,"Unknown structural operation");
        }
        if (!wholeSource) updated.replace(node.tagStart,node.elementEnd-node.tagStart,replacement);
        const auto bytes=updated.toUtf8();
        if (bytes==source.bytes) return Fail(error,"No structural change to apply");
        if (bytes.size()>8*1024*1024 || document.fTotalBytes-source.bytes.size()+bytes.size()>64*1024*1024)
            return Fail(error,"Structural edit exceeds document byte limits");
        XcfgDocument candidate;candidate.fRootFile=document.fRootFile;
        if(!verifyDisk)candidate.fReadBytes=document.fReadBytes;
        for (auto it=document.fSources.constBegin();it!=document.fSources.constEnd();++it) candidate.fReadBytes[it.key()]=it->bytes;
        candidate.fReadBytes[node.file]=bytes;
        if (candidate.Read(candidate.fRootFile,{},error)<0) return false;
        // 包含尚无具体实例的默认声明也必须通过类型/共享范围检查。
        for (int i=0;i<candidate.fNodes.size();++i) {
            if (!candidate.fNodes[i].scalar) {
                QMap<QString,int> children;
                if (!candidate.ChildIndex(i,&children,error)) return false;
                continue;
            }
            QVector<XcfgDocument::ScalarProperties> properties;
            if (!candidate.DeclarationProperties(i,&properties,error)) return false;
            for (const auto &info:properties) {
                auto value=candidate.fNodes[i];value.attributes.remove("range");
                if (info.rangeSource>=0) value.attributes["range"]=info.range;
                if (!XcfgDocument::CheckValue(value,value.value,error)) return false;
            }
        }
        Preview result;result.request=request;result.destinationFile=node.file;result.sourcePath=node.path;
        result.originalBytes=source.bytes;result.updatedBytes=bytes;
        result.dependencyBytes=extraDependencies;
        for(auto it=candidate.fSources.constBegin();it!=candidate.fSources.constEnd();++it)
            if(it.key()!=node.file)result.dependencyBytes[it.key()]=it->bytes;
        for(auto it=result.dependencyBytes.constBegin();it!=result.dependencyBytes.constEnd();++it) {
            if(it.key()==node.file)return Fail(error,"Cannot import the destination source into itself");
            QFile dependency(it.key());
            if(!verifyDisk && document.fReadBytes.contains(it.key())) {
                if(document.fReadBytes.value(it.key())!=it.value())return Fail(error,"Workspace include/import bytes differ");
                continue;
            }
            if(QFileInfo(it.key()).canonicalFilePath()!=it.key() || !dependency.open(QIODevice::ReadOnly) || dependency.read(8*1024*1024+1)!=it.value())
                return Fail(error,"Include/import source changed during preview: "+it.key());
        }
        if (!EffectiveConfigurationSnapshot::CaptureLoaded(document,verifyDisk,&result.beforeSnapshot,error) ||
            !EffectiveConfigurationSnapshot::CaptureLoaded(candidate,false,&result.afterSnapshot,error) ||
            !EffectiveConfigurationSnapshot::Compare(result.beforeSnapshot,result.afterSnapshot,&result.report,error)) return false;
        if (request.operation==MaterializeObject && result.report.value("effective_changed").toBool())
            return Fail(error,"Materialization changes effective parameters/objects; use explicit structure edits and review");
        if (verifyDisk && !document.CheckSources(error)) return false;
        *preview=result;error->clear();return true;
    } catch (const std::exception &e) {return Fail(error,QString::fromUtf8(e.what()));}
}
