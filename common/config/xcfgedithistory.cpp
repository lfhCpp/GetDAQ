#include "xcfgtransaction.h"
#include "xcfgedithistory.h"
#include "effectiveconfigurationsnapshot.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <stdexcept>

namespace {
bool Fail(QString *error,const QString &message) { *error=message;return false; }
QString Hash(const QByteArray &bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()); }
bool Read(const QString &path,QByteArray *bytes,QString *error) {
    if(QFileInfo(path).canonicalFilePath()!=path) return Fail(error,"History source identity changed: "+path);
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly) || file.size()>8*1024*1024) return Fail(error,"Cannot read bounded history source: "+path);
    *bytes=file.read(8*1024*1024+1);
    if(file.error()!=QFile::NoError || bytes->size()>8*1024*1024) return Fail(error,"History source unreadable or too large: "+path);
    return true;
}
}
XcfgEditHistory::XcfgEditHistory(int maximumEntries,qint64 byteBudget) : fMaximum(maximumEntries),fBudget(byteBudget) {
    if(maximumEntries<1 || maximumEntries>256 || byteBudget<32*1024*1024 || byteBudget>1024ll*1024*1024)
        throw std::invalid_argument("History requires 1..256 entries and 32 MiB..1 GiB retained-byte budget");
}
void XcfgEditHistory::Clear() { fEntries.clear();fCursor=0;fBytes=0;++fRevision; }
bool XcfgEditHistory::SetValue(XcfgDocument *document,int node,const QString &value,QString *error) {
    return Execute(document,"Change scalar value",[=] { return document->SetValue(node,value,error); },error);
}
bool XcfgEditHistory::ApplyOverride(XcfgDocument *document,const XcfgDocument::OverridePreview &preview,QString *error) {
    return Execute(document,"Apply concrete instance override",[=] { return document->ApplyOverride(preview,error); },error);
}
bool XcfgEditHistory::ApplyStructure(XcfgDocument *document,const XcfgStructureEditor::Preview &preview,QString *error) {
    return Execute(document,"Edit declaration structure",[=] { return XcfgStructureEditor::Apply(document,preview,error); },error,preview.dependencyBytes);
}
bool XcfgEditHistory::Execute(XcfgDocument *document,const QString &label,const std::function<bool()> &operation,QString *error,const QMap<QString,QByteArray> &dependencies) {
    XcfgIoTransaction ioTransaction(error,XcfgIoTransaction::Write);if(!ioTransaction.IsLocked())return false;

    if(!document) return Fail(error,"No configuration document");
    QJsonObject before;
    if(!EffectiveConfigurationSnapshot::Capture(*document,&before,error)) return false;
    const XcfgDocument original=*document;
    if(!operation()) {
        // 校验失败不消耗历史；已写入但重载失败/来源漂移则不得保留可重放历史。
        QString check;
        bool unchanged=document->RootFile()==original.RootFile() && document->Sources().size()==original.Sources().size();
        for(auto it=original.Sources().constBegin();unchanged && it!=original.Sources().constEnd();++it)
            unchanged=document->Sources().contains(it.key()) && document->Sources().value(it.key()).bytes==it->bytes;
        if(!unchanged || !original.SourcesUnchanged(&check)) Clear();
        return false;
    }
    QJsonObject after;
    if(!EffectiveConfigurationSnapshot::Capture(*document,&after,error)) {
        Clear();document->fNodes.clear();document->fSources.clear();
        *error="Edit saved, but history verification failed; reopen and inspect: "+*error;return false;
    }
    Entry entry;entry.root=original.RootFile();entry.label=label;
    entry.beforeDigest=before.value("snapshot_sha256").toString();entry.afterDigest=after.value("snapshot_sha256").toString();
    for(auto it=original.Sources().constBegin();it!=original.Sources().constEnd();++it) {
        if(document->Sources().contains(it.key()) && document->Sources().value(it.key()).bytes!=it->bytes) {
            if(!entry.destination.isEmpty()) { Clear();return Fail(error,"Edit saved, but more than one source changed; reopen and inspect"); }
            entry.destination=it.key();entry.beforeBytes=it->bytes;entry.afterBytes=document->Sources().value(it.key()).bytes;
        }
        entry.guards[it.key()]=Hash(it->bytes);
    }
    for(auto it=document->Sources().constBegin();it!=document->Sources().constEnd();++it)
        if(!entry.guards.contains(it.key())) entry.guards[it.key()]=Hash(it->bytes);
    for(auto it=dependencies.constBegin();it!=dependencies.constEnd();++it) {
        const auto digest=Hash(it.value());
        if(entry.guards.contains(it.key()) && entry.guards.value(it.key())!=digest) {
            Clear();return Fail(error,"Edit saved, but reviewed import dependency differs; reopen and inspect");
        }
        entry.guards[it.key()]=digest;
    }
    if(entry.destination.isEmpty()) { error->clear();return true; }
    entry.guards.remove(entry.destination);
    // 已不在有效include树中的原来源也须核对；之后撤销可能重新引用它。
    for(auto it=entry.guards.constBegin();it!=entry.guards.constEnd();++it) {
        QByteArray bytes;
        if(!Read(it.key(),&bytes,error) || Hash(bytes)!=it.value()) {
            Clear();document->fNodes.clear();document->fSources.clear();
            return Fail(error,"Edit saved, but a history dependency changed; reopen and inspect: "+it.key());
        }
    }
    if(fCursor>0 && (fEntries[fCursor-1].root!=entry.root || fEntries[fCursor-1].afterDigest!=entry.beforeDigest)) Clear();
    if(fCursor==0 && !fEntries.isEmpty() && (fEntries[0].root!=entry.root || fEntries[0].beforeDigest!=entry.beforeDigest)) Clear();
    while(fEntries.size()>fCursor) {fBytes-=fEntries.last().cost;fEntries.removeLast();}
    entry.cost=entry.beforeBytes.size()+qint64(entry.afterBytes.size());
    for(auto it=entry.guards.constBegin();it!=entry.guards.constEnd();++it) entry.cost+=2ll*(it.key().size()+it.value().size());
    entry.cost+=2ll*(entry.root.size()+entry.label.size()+entry.destination.size()+entry.beforeDigest.size()+entry.afterDigest.size());
    if(entry.cost>fBudget) { Clear();return Fail(error,"Edit saved, but exceeds history budget; reopen and inspect"); }
    while(!fEntries.isEmpty() && (fEntries.size()>=fMaximum || fBytes>fBudget-entry.cost)) {
        fBytes-=fEntries.first().cost;fEntries.removeFirst();--fCursor;
    }
    fEntries.append(entry);++fCursor;fBytes+=entry.cost;++fRevision;error->clear();return true;
}
bool XcfgEditHistory::Candidate(const XcfgDocument &document,const Entry &entry,const QByteArray &bytes,XcfgDocument *candidate,QString *error) {
    if(!document.SourcesUnchanged(error)) return false;
    candidate->fRootFile=document.RootFile();
    for(auto it=document.Sources().constBegin();it!=document.Sources().constEnd();++it) candidate->fReadBytes[it.key()]=it->bytes;
    for(auto it=entry.guards.constBegin();it!=entry.guards.constEnd();++it) {
        QByteArray actual;
        if(!Read(it.key(),&actual,error)) return false;
        if(Hash(actual)!=it.value()) return Fail(error,"History dependency changed; reopen instead of overwriting: "+it.key());
        candidate->fReadBytes[it.key()]=actual;
    }
    candidate->fReadBytes[entry.destination]=bytes;
    if(candidate->Read(candidate->fRootFile,{},error)<0) return false;
    for(int i=0;i<candidate->fNodes.size();++i) {
        const auto node=candidate->fNodes[i];
        if(!node.scalar) {QMap<QString,int> children;if(!candidate->ChildIndex(i,&children,error)) return false;}
        else {
            QVector<XcfgDocument::ScalarProperties> properties;
            if(!candidate->DeclarationProperties(i,&properties,error)) return false;
            for(const auto &info:properties) {
                auto value=node;value.attributes.remove("range");
                if(info.rangeSource>=0) value.attributes["range"]=info.range;
                if(!XcfgDocument::CheckValue(value,value.value,error)) return false;
            }
        }
    }
    return true;
}
bool XcfgEditHistory::CreatePreview(const XcfgDocument &document,bool redo,Preview *preview,QString *error) const {
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    if(!preview) return Fail(error,"No history preview destination");
    if(redo ? RedoSteps()==0 : UndoSteps()==0) return Fail(error,redo ? "No edit to redo" : "No edit to undo");
    const auto &entry=fEntries[redo ? fCursor : fCursor-1];
    if(document.RootFile()!=entry.root) return Fail(error,"History belongs to another configuration");
    Preview result;result.redo=redo;result.revision=fRevision;result.label=entry.label;result.destinationFile=entry.destination;
    result.originalBytes=redo ? entry.beforeBytes : entry.afterBytes;
    result.updatedBytes=redo ? entry.afterBytes : entry.beforeBytes;
    if(!document.Sources().contains(entry.destination) || document.Sources().value(entry.destination).bytes!=result.originalBytes)
        return Fail(error,"Current declaration differs from history; reopen and inspect");
    if(!EffectiveConfigurationSnapshot::Capture(document,&result.beforeSnapshot,error)) return false;
    if(result.beforeSnapshot.value("snapshot_sha256").toString()!=(redo ? entry.beforeDigest : entry.afterDigest))
        return Fail(error,"Current configuration differs from history; reopen and inspect");
    XcfgDocument candidate;
    if(!Candidate(document,entry,result.updatedBytes,&candidate,error) ||
       !EffectiveConfigurationSnapshot::CaptureLoaded(candidate,false,&result.afterSnapshot,error)) return false;
    if(result.afterSnapshot.value("snapshot_sha256").toString()!=(redo ? entry.afterDigest : entry.beforeDigest))
        return Fail(error,"History candidate differs from the original configuration");
    if(!EffectiveConfigurationSnapshot::Compare(result.beforeSnapshot,result.afterSnapshot,&result.report,error)) return false;
    *preview=result;error->clear();return true;
}
bool XcfgEditHistory::Apply(XcfgDocument *document,const Preview &preview,QString *error) {
    XcfgIoTransaction ioTransaction(error,XcfgIoTransaction::Write);if(!ioTransaction.IsLocked())return false;

    if(!document) return Fail(error,"No configuration document");
    if(preview.revision!=fRevision) return Fail(error,"History changed; create a new review");
    Preview current;
    if(!CreatePreview(*document,preview.redo,&current,error)) return false;
    if(current.label!=preview.label || current.destinationFile!=preview.destinationFile || current.originalBytes!=preview.originalBytes ||
       current.updatedBytes!=preview.updatedBytes || current.beforeSnapshot!=preview.beforeSnapshot ||
       current.afterSnapshot!=preview.afterSnapshot || current.report!=preview.report) return Fail(error,"History preview differs; create a new review");
    if(!document->SourcesUnchanged(error)) return false;
    const auto &entry=fEntries[current.redo ? fCursor : fCursor-1];
    for(auto it=entry.guards.constBegin();it!=entry.guards.constEnd();++it) {
        QByteArray bytes;if(!Read(it.key(),&bytes,error)) return false;
        if(Hash(bytes)!=it.value()) return Fail(error,"History dependency changed before save: "+it.key());
    }
    QSaveFile file(current.destinationFile);
    if(!file.open(QIODevice::WriteOnly) || file.write(current.updatedBytes)!=current.updatedBytes.size() || !file.commit()) {
        QString check;if(!document->SourcesUnchanged(&check)) Clear();return Fail(error,file.errorString());
    }
    const auto root=document->RootFile();QJsonObject actual;
    if(!document->Load(root,error) || !EffectiveConfigurationSnapshot::Capture(*document,&actual,error) || actual!=current.afterSnapshot) {
        Clear();document->fNodes.clear();document->fSources.clear();
        *error="History edit saved, but reloaded configuration differs; reopen and inspect: "+*error;return false;
    }
    fCursor+=current.redo ? 1 : -1;++fRevision;error->clear();return true;
}
