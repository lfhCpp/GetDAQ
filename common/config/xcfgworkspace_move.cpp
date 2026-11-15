#include "xcfgtransaction.h"
#include "xcfgworkspace.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>

namespace {
bool Fail(QString *error, const QString &message) { *error = message; return false; }
}

bool XcfgWorkspace::PreviewMove(const MoveRequest &request,MovePreview *preview,QString *error) const {
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    if(!preview)return Fail(error,"No cross-file move preview destination");
    XcfgDocument from,to;
    if(!Document(request.sourceRoot,&from,error) || !Document(request.targetRoot,&to,error))return false;
    if(request.sourceNode<0 || request.sourceNode>=from.fNodes.size() || request.targetParent<0 || request.targetParent>=to.fNodes.size())return Fail(error,"Select a source declaration and target object");
    const auto node=from.fNodes[request.sourceNode],parent=to.fNodes[request.targetParent];
    if(node.file==parent.file)return Fail(error,"Cross-file move requires different physical source paths");
    const auto source=from.fSources.value(node.file),target=to.fSources.value(parent.file);
    if(request.sourceNode==source.root)return Fail(error,"Cannot move a document/include root");
    if(parent.scalar)return Fail(error,"Move target must be an object");
    if(node.tagStart<0 || node.elementEnd<=node.tagStart || parent.tagStart<0 || parent.elementEnd<=parent.tagStart)return Fail(error,"Declaration offsets unavailable");
    QMap<QString,int> children;if(!to.ChildIndex(request.targetParent,&children,error))return false;
    if(children.contains(node.name+'['+node.attributes.value("id")+']'))return Fail(error,"Move destination already contains this declaration/include key");
    MovePreview result;result.revision=fRevision;result.request=request;result.request.sourceRoot=from.RootFile();result.request.targetRoot=to.RootFile();result.sourceFile=node.file;result.targetFile=parent.file;result.sourcePath=node.path;result.targetPath=parent.path;
    QString fragment=source.text.mid(node.tagStart,node.elementEnd-node.tagStart);
    // Rewrite only literal include attributes inside the moved source fragment.
    // Included files keep their own origins and are never copied or modified.
    QVector<int> includes;
    for(int i=0;i<from.fNodes.size();++i){const auto &n=from.fNodes[i];if(n.file==node.file && n.tagStart>=node.tagStart && n.elementEnd<=node.elementEnd && !n.attributes.value("include").isEmpty())includes.append(i);}
    std::sort(includes.begin(),includes.end(),[&](int a,int b){return from.fNodes[a].tagStart>from.fNodes[b].tagStart;});
    QJsonArray rewrites;
    for(int index:includes) {
        const auto &n=from.fNodes[index];if(n.includedRoot<0 || n.includedRoot>=from.fNodes.size())return Fail(error,"Moved include has no bound local source");
        const auto dependency=from.fNodes[n.includedRoot].file;
        const auto relative=QDir(QFileInfo(parent.file).absolutePath()).relativeFilePath(dependency);
        const auto opening=source.text.mid(n.tagStart,n.contentStart-n.tagStart);
        fragment.replace(n.tagStart-node.tagStart,opening.size(),XcfgStructureEditor::RewriteOpeningAttribute(opening,"include",relative));
        QJsonObject row;row["declaration"]=n.path;row["before"]=n.attributes.value("include");row["after"]=relative;row["same_dependency_file"]=dependency;rewrites.append(row);
    }
    QString removed=source.text;removed.remove(node.tagStart,node.elementEnd-node.tagStart);
    QString inserted=target.text;
    if(parent.selfClosing) {
        auto opening=target.text.mid(parent.tagStart,parent.contentStart-parent.tagStart);const auto slash=opening.lastIndexOf("/>");if(slash<0)return Fail(error,"Invalid target object opening");opening.replace(slash,2,">");
        inserted.replace(parent.tagStart,parent.elementEnd-parent.tagStart,opening+'\n'+fragment+'\n'+"</"+parent.name+'>');
    } else inserted.insert(parent.contentEnd,'\n'+fragment+'\n');
    result.before[node.file]=source.bytes;result.before[parent.file]=target.bytes;result.after[node.file]=removed.toUtf8();result.after[parent.file]=inserted.toUtf8();
    XcfgWorkspace candidate=*this;
    if(!candidate.StageSourcesImpl(result.after,false,error) || !candidate.CreatePreview(&result.workspace,error))return false;
    result.report["kind"]="getdaq.workspace-cross-file-move";result.report["source_file"]=node.file;result.report["target_file"]=parent.file;result.report["source_declaration"]=node.path;result.report["target_parent"]=parent.path;result.report["include_rewrites"]=rewrites;result.report["disk_written"]=false;result.report["hardware_loaded_verified"]=false;
    if(!CheckSources(error))return false;
    *preview=result;return true;
}

bool XcfgWorkspace::StageMove(const MovePreview &preview,QString *error) {
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    if(preview.revision!=fRevision)return Fail(error,"Workspace changed; review cross-file move again");
    MovePreview current;if(!PreviewMove(preview.request,&current,error))return false;
    if(current.sourceFile!=preview.sourceFile || current.targetFile!=preview.targetFile || current.sourcePath!=preview.sourcePath || current.targetPath!=preview.targetPath || current.before!=preview.before || current.after!=preview.after || current.report!=preview.report || current.workspace.writes!=preview.workspace.writes || current.workspace.reports!=preview.workspace.reports || current.workspace.revision!=preview.workspace.revision)return Fail(error,"Cross-file move review differs; inspect a new preview");
    return StageSources(current.after,error);
}
