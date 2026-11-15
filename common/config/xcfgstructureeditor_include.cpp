#include "xcfgstructureeditor.h"
#include "xcfgstructureeditor_p.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>
#include <functional>
#include <stdexcept>

bool XcfgStructureEditor::CreateIncludeReplacement(const XcfgDocument &document,const Request &request,
                                                   const XcfgDocument::Node &node,const XcfgDocument::Source &source,
                                                   QString *replacement,QMap<QString,QByteArray> *dependencies,
                                                   QString *error,bool verifyDisk)
{
    if(node.scalar)return XcfgStructureEditorInternal::Fail(error,"Select an object to include or import configuration");
    const bool importing=request.operation==ImportConfiguration;
    if(request.includePath.isEmpty() && request.operation!=ChangeInclude)
        return XcfgStructureEditorInternal::Fail(error,"Choose a local configuration file");
    if(request.includePath.contains(QChar(0)))return XcfgStructureEditorInternal::Fail(error,"Invalid local include path");

    XcfgDocument imported;
    if(!request.includePath.isEmpty()) {
        QString path=request.includePath;
        if(path.startsWith("file:")) {
            const QUrl url(path);
            if(!url.isLocalFile() || (!url.host().isEmpty() && url.host()!="localhost"))
                return XcfgStructureEditorInternal::Fail(error,"Only local file includes/imports are supported");
            path=url.toLocalFile();
        } else if(path.contains("://")) {
            return XcfgStructureEditorInternal::Fail(error,"Only local file includes/imports are supported");
        }
        if(path.isEmpty())return XcfgStructureEditorInternal::Fail(error,"Empty local include/import path");
        const auto importFile=QDir(QFileInfo(node.file).absolutePath()).absoluteFilePath(path);
        if(verifyDisk) {
            if(!imported.Load(importFile,error))return false;
        } else {
            imported.fRootFile=QFileInfo(importFile).canonicalFilePath();
            imported.fReadBytes=document.fReadBytes;
            if(imported.fRootFile.isEmpty())return XcfgStructureEditorInternal::Fail(error,"Import source does not exist");
            if(imported.Read(imported.fRootFile,{},error)<0)return false;
        }
        const int root=imported.Sources().value(imported.RootFile()).root;
        if(imported.Nodes()[root].scalar)
            return XcfgStructureEditorInternal::Fail(error,"Include/import requires an object document root");
        for(auto it=imported.Sources().constBegin();it!=imported.Sources().constEnd();++it)
            (*dependencies)[it.key()]=it->bytes;
    }

    QString opening=source.text.mid(node.tagStart,node.contentStart-node.tagStart);
    opening=RewriteOpeningAttribute(opening,"include",request.includePath,importing || request.includePath.isEmpty());
    if(request.operation==ChangeInclude) {
        *replacement=opening+source.text.mid(node.contentStart,node.elementEnd-node.contentStart);
        return true;
    }

    QString children;
    if(importing) {
        int visits=0;
        std::function<QString(int,int)> content;
        std::function<QString(int,int)> declaration;
        content=[&](int index,int depth) -> QString {
            if(depth>128 || ++visits>200000)throw std::runtime_error("Import expansion depth/node budget exceeded");
            const auto object=imported.fNodes[index];const auto bytes=imported.fSources.value(object.file);
            QMap<QString,int> unique;
            if(!imported.ChildIndex(index,&unique,error))throw std::runtime_error(error->toStdString());
            QString text=object.selfClosing ? QString() : bytes.text.mid(object.contentStart,object.contentEnd-object.contentStart);
            for(int i=object.children.size()-1;i>=0;--i) {
                const auto child=imported.fNodes[object.children[i]];
                text.replace(child.tagStart-object.contentStart,child.elementEnd-child.tagStart,declaration(object.children[i],depth+1));
                if(text.toUtf8().size()>8*1024*1024)throw std::runtime_error("Import exceeds document byte limit");
            }
            if(object.includedRoot>=0)text+='\n'+content(object.includedRoot,depth);
            if(text.toUtf8().size()>8*1024*1024)throw std::runtime_error("Import exceeds document byte limit");
            return text;
        };
        declaration=[&](int index,int depth) -> QString {
            if(depth>128 || ++visits>200000)throw std::runtime_error("Import expansion depth/node budget exceeded");
            const auto value=imported.fNodes[index];const auto bytes=imported.fSources.value(value.file);
            if(value.scalar)return bytes.text.mid(value.tagStart,value.elementEnd-value.tagStart);
            QString start=bytes.text.mid(value.tagStart,value.contentStart-value.tagStart);
            start=RewriteOpeningAttribute(start,"include",QString(),true);
            const auto inner=content(index,depth);
            if(value.selfClosing){const int slash=start.lastIndexOf("/>");start.replace(slash,2,">");}
            return start+inner+"</"+value.name+">";
        };
        children=content(imported.Sources().value(imported.RootFile()).root,0);
    }
    if(node.selfClosing){const int slash=opening.lastIndexOf("/>");opening.replace(slash,2,">");}
    *replacement=opening+children+"</"+node.name+">";
    return true;
}
