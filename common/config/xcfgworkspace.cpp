#include "xcfgtransaction.h"
#include "xcfgworkspace.h"
#include "xcfgsavejournal.h"
#include "effectiveconfigurationsnapshot.h"
#include <QFile>
#include <QFileInfo>

namespace {
bool Fail(QString *error, const QString &message) { *error = message; return false; }
}

bool XcfgWorkspace::CheckSources(QString *error) const {
    if (fRequiresReopen) return Fail(error, "A partial or uncertain save requires inspection and reopening; automatic retry is disabled");
    for (auto it=fBaseline.constBegin();it!=fBaseline.constEnd();++it) {
        QString pending;
        if(!XcfgSaveJournal::Pending(it.key(),&pending,error))return false;
        if(!pending.isEmpty() && pending!=fActiveSave)return Fail(error,"Source has an unfinished Save All; inspect recovery: "+pending);
        QFile file(it.key());
        if (QFileInfo(it.key()).canonicalFilePath()!=it.key() || !file.open(QIODevice::ReadOnly) ||
            file.size()!=it->size() || file.read(it->size()+1)!=it.value() || file.error()!=QFile::NoError)
            return Fail(error, "Workspace source changed; do not overwrite: "+it.key());
    }
    error->clear();return true;
}

bool XcfgWorkspace::Open(const QString &path,QString *error) {
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    if(!CheckSources(error)) return false;
    const auto root=QFileInfo(path).canonicalFilePath();
    if(root.isEmpty()) return Fail(error,"Root does not exist: "+path);
    QString pending;if(!XcfgSaveJournal::Pending(root,&pending,error))return false;
    if(!pending.isEmpty())return Fail(error,"Root has an unfinished Save All; inspect recovery: "+pending);
    if(fRoots.contains(root)) {error->clear();return true;}
    if(fRoots.size()>=16) return Fail(error,"Workspace supports at most 16 root documents");
    auto roots=fRoots;roots.append(root);
    auto overlay=fBaseline;for(auto it=fStaged.constBegin();it!=fStaged.constEnd();++it) overlay[it.key()]=it.value();
    QMap<QString,XcfgDocument> documents;QMap<QString,QByteArray> sources;
    if(!Build(roots,overlay,&documents,&sources,error)) return false;
    auto baseline=fBaseline;
    for(auto it=sources.constBegin();it!=sources.constEnd();++it) if(!baseline.contains(it.key())) baseline[it.key()]=it.value();
    if(!Budget(baseline,error) || !CheckSources(error)) return false;
    XcfgWorkspace checked=*this;checked.fBaseline=baseline;
    if(!checked.CheckSources(error)) return false;
    fRoots=roots;fBaseline=baseline;++fRevision;error->clear();return true;
}

bool XcfgWorkspace::Document(const QString &path,XcfgDocument *document,QString *error) const {
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    if(!document || !CheckSources(error))return false;
    const auto root=QFileInfo(path).canonicalFilePath();
    if(!fRoots.contains(root))return Fail(error,"Select an opened workspace root");
    auto overlay=fBaseline;for(auto it=fStaged.constBegin();it!=fStaged.constEnd();++it)overlay[it.key()]=it.value();
    QMap<QString,XcfgDocument> documents;QMap<QString,QByteArray> sources;
    if(!Build(fRoots,overlay,&documents,&sources,error) || !CheckSources(error))return false;
    *document=documents.value(root);return true;
}

bool XcfgWorkspace::CreatePreview(Preview *preview,QString *error) const {
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    if(!preview) return Fail(error,"No save review destination");
    if(!CheckSources(error)) return false;
    auto overlay=fBaseline;for(auto it=fStaged.constBegin();it!=fStaged.constEnd();++it) overlay[it.key()]=it.value();
    QMap<QString,XcfgDocument> before,after;QMap<QString,QByteArray> sources;
    if(!Build(fRoots,fBaseline,&before,&sources,error) || !Build(fRoots,overlay,&after,&sources,error)) return false;
    Preview result;result.revision=fRevision;result.writes=fStaged;
    for(const auto &root:fRoots) {
        QJsonObject first,second,report;
        if(!EffectiveConfigurationSnapshot::CaptureLoaded(before.value(root),false,&first,error) ||
           !EffectiveConfigurationSnapshot::CaptureLoaded(after.value(root),false,&second,error) ||
           !EffectiveConfigurationSnapshot::Compare(first,second,&report,error)) return false;
        result.reports[root]=report;
    }
    if(!CheckSources(error)) return false;
    *preview=result;error->clear();return true;
}

