#include "xcfgtransaction.h"
#include "xcfgdocument.h"
#include "xcfgpending.h"
#include "xcfgdocumentinternal_p.h"
#include "configurationrange.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <functional>
#include <exception>

using namespace XcfgDocumentInternal;

QStringList XcfgDocument::EffectiveChildren(const QString &path,QString *error,bool includeDefaults) const
{
    if (Lookup(path,error)<0) return {};
    const auto parts=PathParts(path,error); if (!error->isEmpty()) return {};
    const QRegularExpression segment("^([^/\\[\\]]+)(?:\\[([^\\[\\]]*)\\])?$");
    QVector<QPair<QString,QString>> query;
    for (int i=1;i<parts.size();++i) { const auto match=segment.match(parts[i]); query.append(qMakePair(match.captured(1),match.captured(2))); }
    QSet<QString> candidates;QStringList orderedCandidates; int reached=-1,visits=0; QString problem;
    std::function<void(int,int)> visit;
    visit=[&](int parent,int level) {
        if (++visits>200000) { problem="Effective child branch limit exceeded"; return; }
        QMap<QString,int> children;
        if (!ChildIndex(parent,&children,&problem)) return;
        if (level==query.size()) {
            if(includeDefaults) {
                std::function<void(int)> append;
                append=[&](int content) {
                    if(fNodes[content].includedRoot>=0)append(fNodes[content].includedRoot);
                    for(int child:fNodes[content].children) {
                        const auto &node=fNodes[child];const auto id=node.attributes.value("id");
                        const auto key=path+'/'+node.name+(id.isEmpty() ? QString() : '['+id+']');
                        if(!candidates.contains(key)){candidates.insert(key);orderedCandidates.append(key);}
                    }
                };
                append(parent);return;
            }
            for (int child:children) {
                const auto &node=fNodes[child]; const QString id=node.attributes.value("id");
                if (includeDefaults || id!="*") candidates.insert(path+'/'+node.name+(id.isEmpty() ? QString() : '['+id+']'));
            }
            return;
        }
        const auto name=query[level].first,id=query[level].second;
        for (int pass=0;pass<(id=="*" ? 1 : 2);++pass) {
            if (pass==1 && reached<level) continue;
            const QString key=name+'['+(pass ? "*" : id)+']';
            if (!children.contains(key)) continue;
            reached=qMax(reached,level); visit(children[key],level+1); if (!problem.isEmpty()) return;
        }
    };
    visit(fSources.value(fRootFile).root,0);
    if (!problem.isEmpty()) { *error=problem; return {}; }
    QStringList result=includeDefaults ? orderedCandidates : candidates.values();
    if(!includeDefaults)result.sort();
    error->clear();return result;
}

bool XcfgDocument::EffectiveNodes(QMap<QString,Node> *values,QString *error) const
{
    if (!fSources.contains(fRootFile)) return Fail(error,"No configuration loaded");
    QMap<QString,Node> result; int visits=0; qint64 bytes=0;
    std::function<bool(const QString &)> walk;
    walk=[&](const QString &path) -> bool {
        if (++visits>200000 || (bytes+=path.toUtf8().size())>64*1024*1024) return Fail(error,"Effective configuration budget exceeded");
        const int source=Lookup(path,error); if (source<0) return false;
        if (fNodes[source].scalar) {
            Node node; ScalarProperties info; if (!ResolveScalar(path,&node,&info,error)) return false;
            result[path]=node; return true;
        }
        result[path]=fNodes[source];
        const auto children=EffectiveChildren(path,error); if (!error->isEmpty()) return false;
        for (const auto &child:children) if (!walk(child)) return false;
        return true;
    };
    if (!walk(fNodes[fSources.value(fRootFile).root].path)) return false;
    *values=result; error->clear(); return true;
}

bool XcfgDocument::EffectiveScalars(QMap<QString,Node> *values,QString *error) const
{
    QMap<QString,Node> nodes,result;
    if (!EffectiveNodes(&nodes,error)) return false;
    for (auto it=nodes.constBegin();it!=nodes.constEnd();++it) if (it->scalar) result[it.key()]=it.value();
    *values=result; return true;
}

bool XcfgDocument::CaptureEffectiveTree(QMap<QString,Node> *values,QString *error) const
{
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    if (!CheckSources(error)) return false;
    QMap<QString,Node> result;
    if (!EffectiveNodes(&result,error) || !CheckSources(error)) return false;
    *values=result; return true;
}

bool XcfgDocument::PreviewOverride(const QString &path,const QString &value,OverridePreview *preview,QString *error) const
{
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    Node original; ScalarProperties info;
    if (!ResolveScalar(path,&original,&info,error) || !CheckValue(original,value,error)) return false;
    if (!CheckSources(error)) return false;
    auto parts=PathParts(path,error); if (!error->isEmpty()) return false;
    const int root=fSources.value(fRootFile).root; parts[0]=fNodes[root].path;
    const QRegularExpression segment("^([^/\\[\\]]+)(?:\\[([^\\[\\]]*)\\])?$");
    QVector<QPair<QString,QString>> query;
    for (int i=1;i<parts.size();++i) {
        const auto match=segment.match(parts[i]);
        if (!match.hasMatch() || match.captured(2)=="*") return Fail(error,"Override requires a concrete instance path without '*'");
        query.append(qMakePair(match.captured(1),match.captured(2)));
    }
    if (query.isEmpty()) return Fail(error,"Cannot create an override for the document root");
    int parent=root,missing=0;
    for (;missing<query.size();++missing) {
        int found=-1;
        for (int child:fNodes[parent].children) if (fNodes[child].name==query[missing].first && fNodes[child].attributes.value("id")==query[missing].second) {
            if (found>=0) return Fail(error,"Duplicate direct override declaration");
            found=child;
        }
        if (found<0) break;
        if (missing==query.size()-1) return Fail(error,"A direct value already exists; edit its declaration");
        if (fNodes[found].scalar) return Fail(error,"Override parent is a scalar");
        parent=found;
    }
    const auto &destination=fNodes[parent];
    const auto escape=[](QString text,bool attribute) -> QString {
        text.replace('&',"&amp;"); text.replace('<',"&lt;"); text.replace('>',"&gt;");
        if (attribute) { text.replace('"',"&quot;"); text.replace('\'',"&apos;"); } return text;
    };
    QString element;
    try {
        const auto declared=original.attributes.value("type").toStdString();
        const auto type=declared.empty() ? GetDAQ::ConfigurationValueType(GetDAQ::ParseConfigurationValue(original.value.toStdString())) : declared;
        for (int i=query.size()-1;i>=missing;--i) {
            const QString name=query[i].first,id=query[i].second;
            const QString tag=name+(id.isEmpty() ? QString() : " id=\""+escape(id,true)+"\"");
            if (i==query.size()-1) element='<'+tag+" type=\""+QString::fromStdString(type)+"\">"+EncodeScalarText(value)+"</"+name+'>';
            else element='<'+tag+">\n"+element+"\n</"+name+'>';
        }
    } catch (const std::exception &e) { return Fail(error,QString::fromUtf8(e.what())); }
    const auto source=fSources.value(destination.file); QString updated=source.text;
    if (destination.selfClosing) {
        QString opening=source.text.mid(destination.tagStart,destination.contentStart-destination.tagStart);
        const int slash=opening.lastIndexOf("/>"); if (slash<0) return Fail(error,"Self-closing parent offsets differ");
        opening.replace(slash,2,">");
        updated.replace(destination.tagStart,destination.elementEnd-destination.tagStart,opening+'\n'+element+"\n</"+destination.name+'>');
    } else {
        if (destination.contentEnd<destination.contentStart) return Fail(error,"Parent insertion offsets differ");
        updated.insert(destination.contentEnd,'\n'+element+'\n');
    }
    const auto bytes=updated.toUtf8();
    if (bytes.size()>8*1024*1024 || fTotalBytes-source.bytes.size()+bytes.size()>64*1024*1024) return Fail(error,"Override exceeds document byte limits");
    XcfgDocument candidate; candidate.fRootFile=fRootFile;
    for (auto it=fSources.constBegin();it!=fSources.constEnd();++it) candidate.fReadBytes[it.key()]=it->bytes;
    candidate.fReadBytes[destination.file]=bytes;
    if (candidate.Read(fRootFile,{},error)<0) return false;
    QMap<QString,Node> before,after;
    if (!EffectiveScalars(&before,error) || !candidate.EffectiveScalars(&after,error)) return false;
    const QString canonical=parts.join('/');
    if (before.keys()!=after.keys()) return Fail(error,"Override changes effective scalar set; inspect configuration structure manually");
    if (!after.contains(canonical) || after[canonical].file!=destination.file || after[canonical].value!=value) return Fail(error,"Override did not resolve to the intended declaration/value");
    for (auto it=before.constBegin();it!=before.constEnd();++it) {
        const auto &a=it.value(),&b=after[it.key()];
        if (!a.scalar || !b.scalar) return Fail(error,"Override changes scalar/object role");
        if (a.attributes.value("unit")!=b.attributes.value("unit") || a.attributes.value("range")!=b.attributes.value("range")) return Fail(error,"Override changes shared class properties");
        if (it.key()!=canonical && (a.value!=b.value || a.attributes.value("type")!=b.attributes.value("type"))) return Fail(error,"Override changes another effective value: "+it.key());
    }
    OverridePreview result; result.path=canonical; result.destinationFile=destination.file; result.previousValue=original.value; result.value=value;
    result.originalBytes=source.bytes; result.updatedBytes=bytes; *preview=result; error->clear(); return true;
}

bool XcfgDocument::ApplyOverride(const OverridePreview &preview,QString *error)
{
    XcfgIoTransaction ioTransaction(error,XcfgIoTransaction::Write);if(!ioTransaction.IsLocked())return false;

    OverridePreview current;
    if (!PreviewOverride(preview.path,preview.value,&current,error)) return false;
    if (current.destinationFile!=preview.destinationFile || current.previousValue!=preview.previousValue || current.originalBytes!=preview.originalBytes || current.updatedBytes!=preview.updatedBytes)
        return Fail(error,"Override preview differs; create and inspect a new preview");
    if (!CheckSources(error)) return false;
    const auto before=fSources;
    QSaveFile file(current.destinationFile);
    if (!file.open(QIODevice::WriteOnly) || file.write(current.updatedBytes)!=current.updatedBytes.size() || !file.commit()) return Fail(error,file.errorString());
    return ReloadEdited(current.destinationFile,current.updatedBytes,before,"Override",error);
}
