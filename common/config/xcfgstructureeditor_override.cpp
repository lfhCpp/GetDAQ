#include "xcfgtransaction.h"
#include "xcfgstructureeditor.h"
#include "xcfgstructureeditor_p.h"
#include "effectiveconfigurationsnapshot.h"
#include <QRegularExpression>
#include <QJsonArray>
#include <functional>
#include <stdexcept>

using namespace XcfgStructureEditorInternal;

bool XcfgStructureEditor::CreateObjectOverride(const XcfgDocument &document,const Request &request,Preview *preview,QString *error,bool verifyDisk)
{
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    try {
        if(!preview)return Fail(error,"No object override preview destination");
        QJsonObject before;
        if(!EffectiveConfigurationSnapshot::CaptureLoaded(document,verifyDisk,&before,error))return false;
        bool inherited=false;const int target=document.LookupImpl(request.path,&inherited,error);
        if(target<0)return false;
        if(document.fNodes[target].scalar)return Fail(error,"Object override requires an object; use scalar override for a parameter");
        if(!inherited)return Fail(error,"Object is already a direct declaration (including direct include content); there is no inherited object to override");
        auto parts=document.SplitPath(request.path,error);if(!error->isEmpty())return false;
        parts[0]=document.fNodes[document.fSources.value(document.fRootFile).root].path;
        const auto canonical=parts.join('/');int first=-1,parent=-1;
        for(int i=1;i<parts.size();++i) {
            bool fromDefault=false;const int index=document.LookupImpl(parts.mid(0,i+1).join('/'),&fromDefault,error);
            if(index<0)return false;
            if(document.fNodes[index].scalar)return Fail(error,"Object override ancestor is a scalar");
            if(fromDefault){first=i;break;}parent=index;
        }
        if(first<1)return Fail(error,"Cannot override the document root");
        if(parent<0)parent=document.fSources.value(document.fRootFile).root;
        const auto owner=document.fNodes[parent];const auto source=document.fSources.value(owner.file);
        const QRegularExpression segment("^([^/\\[\\]]+)(?:\\[([^\\[\\]]*)\\])?$");
        QStringList created;int visits=0;
        QVector<int> physicalParents(document.fNodes.size(),-1),preStarts(document.fNodes.size(),-1);
        for(int container=0;container<document.fNodes.size();++container) {
            const auto &node=document.fNodes[container];int begin=node.contentStart;
            for(int child:node.children){physicalParents[child]=container;preStarts[child]=begin;begin=document.fNodes[child].elementEnd;}
        }
        const auto comments=[](const QString &text) {
            QString output;int begin=0;
            while((begin=text.indexOf("<!--",begin))>=0) {
                const int end=text.indexOf("-->",begin+4);if(end<0)break;
                output+=text.mid(begin,end+3-begin)+'\n';begin=end+3;
            }
            return output;
        };
        std::function<QString(int,const QString &)> plan;
        plan=[&](int level,const QString &parentPath) -> QString {
            if(level>=parts.size())return QString();
            const auto selected=segment.match(parts[level]);if(!selected.hasMatch())throw std::runtime_error("Invalid object override segment");
            const auto siblings=document.EffectiveChildren(parentPath,error,true);
            if(!error->isEmpty())throw std::runtime_error(error->toStdString());
            QString output;
            for(const auto &path:siblings) {
                if(++visits>200000)throw std::runtime_error("Object override visit budget exceeded");
                const auto part=path.mid(parentPath.size()+1);const auto match=segment.match(part);
                if(match.captured(1)!=selected.captured(1) || (selected.captured(2)!="*" && match.captured(2)!=selected.captured(2)))continue;
                bool fromDefault=false;const int index=document.LookupImpl(path,&fromDefault,error);
                if(index<0)throw std::runtime_error(error->toStdString());
                if(!fromDefault)continue;
                const auto node=document.fNodes[index];if(node.scalar)throw std::runtime_error("Object/scalar sibling conflict");
                if(node.attributes.value("id")!=match.captured(2))throw std::runtime_error("Object source ID differs from the visible declaration");
                auto attributes=node.attributes;attributes.remove("include");
                const auto original=document.fSources.value(node.file);
                QString pre,post;
                if(physicalParents[index]>=0)pre=comments(original.text.mid(preStarts[index],node.tagStart-preStarts[index]));
                if(!node.selfClosing) {
                    const int begin=node.children.isEmpty() ? node.contentStart : document.fNodes[node.children.last()].elementEnd;
                    post=comments(original.text.mid(begin,node.contentEnd-begin));
                }
                const auto nested=part==parts[level] ? plan(level+1,path) : QString();
                const auto text=pre+Element(node.name,attributes,QString(),false,nested+post);
                if(text.toUtf8().size()+output.toUtf8().size()>8*1024*1024)throw std::runtime_error("Object override exceeds document budget");
                // Original insertInside prepends each clone in visible source order.
                output=text+'\n'+output;created.append(path);
            }
            return output;
        };
        const auto inserted=plan(first,parts.mid(0,first).join('/'));
        if(inserted.isEmpty())return Fail(error,"No inherited objects were created");
        QString updated=source.text;
        if(owner.selfClosing) {
            QString opening=source.text.mid(owner.tagStart,owner.contentStart-owner.tagStart);
            const int slash=opening.lastIndexOf("/>");if(slash<0)return Fail(error,"Invalid object insertion offsets");opening.replace(slash,2,">");
            updated.replace(owner.tagStart,owner.elementEnd-owner.tagStart,opening+'\n'+inserted+"</"+owner.name+'>');
        } else updated.insert(owner.contentStart,'\n'+inserted);
        const auto bytes=updated.toUtf8();
        if(bytes.size()>8*1024*1024 || document.fTotalBytes-source.bytes.size()+bytes.size()>64*1024*1024)return Fail(error,"Object override exceeds source byte budget");
        XcfgDocument candidate;candidate.fRootFile=document.fRootFile;
        for(auto it=document.fSources.constBegin();it!=document.fSources.constEnd();++it)candidate.fReadBytes[it.key()]=it->bytes;
        candidate.fReadBytes[owner.file]=bytes;if(candidate.Read(candidate.fRootFile,{},error)<0)return false;
        bool remainsInherited=false;const int actual=candidate.LookupImpl(canonical,&remainsInherited,error);
        if(actual<0)return false;
        if(remainsInherited || candidate.fNodes[actual].scalar || candidate.fNodes[actual].file!=owner.file)return Fail(error,"Object override did not resolve to the intended local object declaration");
        Preview result;result.request=request;result.request.path=canonical;result.destinationFile=owner.file;result.sourcePath=canonical;
        result.originalBytes=source.bytes;result.updatedBytes=bytes;result.beforeSnapshot=before;result.createdPaths=created;
        if(!EffectiveConfigurationSnapshot::CaptureLoaded(candidate,false,&result.afterSnapshot,error))return false;
        if(before.value("effective_sha256")!=result.afterSnapshot.value("effective_sha256"))return Fail(error,"Object override changes effective values or objects; shallow override must preserve them");
        for(const auto &object:before.value("objects").toArray()) {
            const auto path=object.toString();bool wasInherited=false,isInherited=false;
            if(document.LookupImpl(path,&wasInherited,error)<0 || candidate.LookupImpl(path,&isInherited,error)<0)return false;
            if(wasInherited && !isInherited)result.localizedPaths.append(path);
        }
        if(!EffectiveConfigurationSnapshot::Compare(before,result.afterSnapshot,&result.report,error))return false;
        for(auto it=document.fSources.constBegin();it!=document.fSources.constEnd();++it)if(it.key()!=owner.file)result.dependencyBytes[it.key()]=it->bytes;
        if(verifyDisk && !document.SourcesUnchanged(error))return false;
        *preview=result;error->clear();return true;
    }catch(const std::exception &failure){return Fail(error,QString::fromUtf8(failure.what()));}
}
