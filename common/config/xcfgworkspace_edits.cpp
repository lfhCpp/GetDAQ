#include "xcfgtransaction.h"
#include "xcfgworkspace.h"

namespace {
bool Fail(QString *error, const QString &message) { *error = message; return false; }
}

bool XcfgWorkspace::StageSource(const QString &file,const QByteArray &bytes,QString *error) {
    QMap<QString,QByteArray> changes;changes[file]=bytes;return StageSources(changes,error);
}

bool XcfgWorkspace::StageSources(const QMap<QString,QByteArray> &changes,QString *error) {
    return StageSourcesImpl(changes,true,error);
}

bool XcfgWorkspace::StageSourcesImpl(const QMap<QString,QByteArray> &changes,bool recordHistory,QString *error) {
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    if(!CheckSources(error)) return false;
    auto staged=fStaged;
    HistoryEntry entry;
    for(auto it=changes.constBegin();it!=changes.constEnd();++it) {
        if(!fBaseline.contains(it.key())) return Fail(error,"Open the source in this workspace first");
        if(it->size()>8*1024*1024 || QString::fromUtf8(it.value()).toUtf8()!=it.value())
            return Fail(error,"Source must be UTF-8 and at most 8 MiB");
        const auto before=Bytes(it.key());
        if(before!=it.value()) {entry.before[it.key()]=before;entry.after[it.key()]=it.value();entry.bytes+=qint64(before.size())+it->size();}
        if(it.value()==fBaseline.value(it.key())) staged.remove(it.key());else staged[it.key()]=it.value();
    }
    auto overlay=fBaseline;for(auto it=staged.constBegin();it!=staged.constEnd();++it) overlay[it.key()]=it.value();
    QMap<QString,XcfgDocument> documents;QMap<QString,QByteArray> sources;
    if(!Budget(overlay,error) || !Build(fRoots,overlay,&documents,&sources,error)) return false;
    // Validate edited sources detached by an include change, without retaining
    // a duplicate full tree for every already-reachable shared source.
    for(auto it=staged.constBegin();it!=staged.constEnd();++it) if(!sources.contains(it.key())) {
        QMap<QString,XcfgDocument> detached;QMap<QString,QByteArray> dependencies;
        if(!Build(QStringList{it.key()},overlay,&detached,&dependencies,error)) return false;
        for(auto dependency=dependencies.constBegin();dependency!=dependencies.constEnd();++dependency) {
            if(sources.contains(dependency.key()) && sources.value(dependency.key())!=dependency.value())
                return Fail(error,"Detached source dependency changed during staging: "+dependency.key());
            sources[dependency.key()]=dependency.value();
        }
    }
    auto baseline=fBaseline;
    for(auto it=sources.constBegin();it!=sources.constEnd();++it) if(!baseline.contains(it.key())) baseline[it.key()]=it.value();
    if(!Budget(baseline,error) || !CheckSources(error)) return false;
    XcfgWorkspace checked=*this;checked.fBaseline=baseline;
    if(!checked.CheckSources(error)) return false;
    if(recordHistory && !entry.after.isEmpty()) {
        if(entry.bytes>128LL*1024*1024) return Fail(error,"Workspace history entry exceeds 128 MiB");
        while(fHistory.size()>fHistoryPosition) {fHistoryBytes-=fHistory.last().bytes;fHistory.removeLast();}
        fHistory.append(entry);fHistoryBytes+=entry.bytes;
        while(fHistory.size()>32 || fHistoryBytes>128LL*1024*1024) {fHistoryBytes-=fHistory.first().bytes;fHistory.removeFirst();}
        fHistoryPosition=fHistory.size();
    }
    fBaseline=baseline;fStaged=staged;
    if(!entry.after.isEmpty())++fRevision;
    error->clear();return true;
}

bool XcfgWorkspace::ApplyHistory(bool redo,QString *error) {
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    if(!CheckSources(error))return false;
    if(redo ? !CanRedo() : !CanUndo())return Fail(error,redo ? "No workspace edit to redo" : "No workspace edit to undo");
    const auto entry=fHistory.at(redo ? fHistoryPosition : fHistoryPosition-1);
    const auto &expected=redo ? entry.before : entry.after;
    for(auto it=expected.constBegin();it!=expected.constEnd();++it)
        if(!fBaseline.contains(it.key()) || Bytes(it.key())!=it.value())return Fail(error,"Workspace history no longer matches current source bytes");
    // Revalidate the combined state against every currently open root, including
    // newly added roots. A failed history step changes no files or history cursor.
    if(!StageSourcesImpl(redo ? entry.after : entry.before,false,error))return false;
    fHistoryPosition+=redo ? 1 : -1;return true;
}
bool XcfgWorkspace::Undo(QString *error) {return ApplyHistory(false,error);}
bool XcfgWorkspace::Redo(QString *error) {return ApplyHistory(true,error);}

