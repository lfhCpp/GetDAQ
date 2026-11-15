#include "xcfgsourcemigration.h"
#include "xcfgsourcemigration_p.h"
#include "xcfgtransaction.h"
#include "effectiveconfigurationsnapshot.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <stdexcept>

using namespace XcfgSourceMigrationInternal;

bool XcfgSourceMigration::BuildGraph(const QStringList &roots,const QMap<QString,QByteArray> &bytes,
                                    QMap<QString,XcfgDocument> *documents,QString *error) {
    QMap<QString,QByteArray> sources;
    if(!XcfgWorkspace::Build(roots,bytes,documents,&sources,error,true))return false;
    for(const auto &path:bytes.keys())if(!sources.contains(path)) {
        QMap<QString,XcfgDocument> extra;QMap<QString,QByteArray> dependencies;
        if(!XcfgWorkspace::Build({path},bytes,&extra,&dependencies,error,true))return false;
        for(auto it=extra.constBegin();it!=extra.constEnd();++it)(*documents)[it.key()]=it.value();
        for(auto it=dependencies.constBegin();it!=dependencies.constEnd();++it)sources[it.key()]=it.value();
    }
    Require(sources==bytes,"Migration source graph differs from its closed reviewed set");return true;
}
bool XcfgSourceMigration::CheckSemantics(const QStringList &beforeRoots,const QStringList &afterRoots,
                                        const QMap<QString,QByteArray> &before,const QMap<QString,QByteArray> &after,
                                        QJsonObject *reports,QString *error) {
    QMap<QString,XcfgDocument> original,candidate;
    if(!BuildGraph(beforeRoots,before,&original,error) || !BuildGraph(afterRoots,after,&candidate,error))return false;
    Require(beforeRoots.size()==afterRoots.size(),"Migration root count differs");*reports=QJsonObject();
    for(int i=0;i<beforeRoots.size();++i) {
        QJsonObject a,b,report;
        if(!EffectiveConfigurationSnapshot::CaptureLoaded(original.value(beforeRoots[i]),false,&a,error) ||
           !EffectiveConfigurationSnapshot::CaptureLoaded(candidate.value(afterRoots[i]),false,&b,error) ||
           !EffectiveConfigurationSnapshot::Compare(a,b,&report,error))return false;
        Require(a["effective_sha256"]==b["effective_sha256"],"Path migration changes effective objects, values, types, units or ranges");
        (*reports)[beforeRoots[i]]=report;
    }
    return true;
}
bool XcfgSourceMigration::CreatePreview(const XcfgWorkspace &workspace,const Request &request,Preview *preview,QString *error) {
    XcfgIoTransaction transaction(error);if(!transaction.IsLocked())return false;
    try {
        Require(preview && !workspace.fRoots.isEmpty() && !request.destinations.isEmpty(),"Open all affected roots and select source paths to migrate");
        Require(workspace.fStaged.isEmpty(),"Save or undo pending workspace changes before path migration");
        if(!workspace.CheckSources(error))return false;
        Preview result;result.revision=workspace.fRevision;result.original=workspace.fBaseline;result.originalRoots=workspace.fRoots;
        QStringList destinations;
        for(auto it=request.destinations.constBegin();it!=request.destinations.constEnd();++it) {
            const auto from=Slot(it.key()),to=Slot(it.value());
            Require(result.original.contains(from) && !QFileInfo(it.key()).isSymLink(),"Select a known regular workspace source");
            Require(!QFileInfo::exists(to) && !destinations.contains(to),"Migration destination already exists or is duplicated");
            Require(!result.request.destinations.contains(from),"Duplicate migration source");
            result.request.destinations[from]=to;destinations.append(to);
        }
        QMap<QString,XcfgDocument> documents;if(!BuildGraph(result.originalRoots,result.original,&documents,error))return false;
        struct Edge {int at,length;QString path,include,target;};
        QMap<QString,QMap<int,Edge>> edges;
        for(const auto &document:documents)for(const auto &node:document.Nodes())if(node.includedRoot>=0) {
            const auto target=document.Nodes()[node.includedRoot].file;
            if(edges[node.file].contains(node.tagStart))Require(edges[node.file][node.tagStart].target==target,"Shared include binding differs across roots");
            edges[node.file][node.tagStart]={node.tagStart,node.contentStart-node.tagStart,node.path,node.attributes.value("include"),target};
        }
        QJsonArray rewrites;
        for(auto source=result.original.constBegin();source!=result.original.constEnd();++source) {
            const auto destination=result.request.destinations.value(source.key(),source.key());
            QString text=QString::fromUtf8(source.value());const auto fileEdges=edges.value(source.key());
            for(auto it=fileEdges.constEnd();it!=fileEdges.constBegin();) {
                --it;const auto edge=it.value();const auto target=result.request.destinations.value(edge.target,edge.target);
                auto literal=edge.include;if(literal.startsWith("file:"))literal=QUrl(literal).toLocalFile();
                const auto resolves=QDir::cleanPath(QDir(QFileInfo(destination).absolutePath()).absoluteFilePath(literal));
                if(resolves==target)continue;
                const auto relative=QDir(QFileInfo(destination).absolutePath()).relativeFilePath(target);
                const auto opening=text.mid(edge.at,edge.length);
                text.replace(edge.at,edge.length,XcfgStructureEditor::RewriteOpeningAttribute(opening,"include",relative));
                rewrites.append(QJsonObject{{"file_before",source.key()},{"file_after",destination},{"declaration",edge.path},
                    {"include_before",edge.include},{"include_after",relative},{"dependency_before",edge.target},{"dependency_after",target}});
            }
            result.candidate[destination]=text.toUtf8();
        }
        Budget(result.original);Budget(result.candidate);
        for(const auto &root:result.originalRoots)result.migratedRoots.append(result.request.destinations.value(root,root));
        QJsonObject roots;if(!CheckSemantics(result.originalRoots,result.migratedRoots,result.original,result.candidate,&roots,error))return false;
        QJsonArray files;
        for(const auto &path:Paths(result.original,result.candidate))files.append(QJsonObject{{"path",path},
            {"before_exists",result.original.contains(path)},{"after_exists",result.candidate.contains(path)},
            {"before_sha256",result.original.contains(path) ? Hash(result.original.value(path)) : QString()},
            {"after_sha256",result.candidate.contains(path) ? Hash(result.candidate.value(path)) : QString()}});
        result.report=QJsonObject{{"kind","getdaq.source-path-migration-review"},{"files",files},{"include_rewrites",rewrites},
            {"root_reviews",roots},{"roots_after",QJsonArray::fromStringList(result.migratedRoots)},
            {"disk_written",false},{"hardware_loaded_verified",false},{"unopened_referrers_checked",false}};
        if(!workspace.CheckSources(error))return false;
        for(const auto &path:destinations)Require(!QFileInfo::exists(path) && !QFileInfo(path).isSymLink(),"Migration destination appeared during preview");
        *preview=result;error->clear();return true;
    }catch(const std::exception &e){*error=QString::fromUtf8(e.what());return false;}
}
