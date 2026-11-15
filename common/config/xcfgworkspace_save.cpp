#include "xcfgtransaction.h"
#include "xcfgworkspace.h"
#include "xcfgsavejournal.h"
#include <QSaveFile>
#include <memory>
#include <vector>

namespace {
bool Fail(QString *error, const QString &message) { *error = message; return false; }
}

bool XcfgWorkspace::SaveAll(const Preview &preview,SaveResult *result,QString *error,const std::function<void(const QString &,const QString &)> &progress) {
    if(!result) return Fail(error,"No save result destination");
    *result=SaveResult();result->remainingFiles=fStaged.keys();
    XcfgIoTransaction ioTransaction(error,XcfgIoTransaction::Write);if(!ioTransaction.IsLocked())return false;
    Preview current;
    if(!CreatePreview(&current,error)) return false;
    if(preview.revision!=current.revision || preview.writes!=current.writes || preview.reports!=current.reports)
        return Fail(error,"Workspace review changed; create and inspect a new review");
    std::vector<std::unique_ptr<QSaveFile>> prepared;
    for(auto it=current.writes.constBegin();it!=current.writes.constEnd();++it) {
        std::unique_ptr<QSaveFile> file(new QSaveFile(it.key()));file->setDirectWriteFallback(false);
        if(!file->open(QIODevice::WriteOnly) || file->write(it.value())!=it->size()) return Fail(error,file->errorString());
        prepared.push_back(std::move(file));
    }
    // All temporary files are ready before replacing the first source.
    if(!CheckSources(error)) return false;
    if(current.writes.isEmpty()){result->complete=true;error->clear();return true;}
    XcfgSaveJournal journal;
    if(!journal.Begin(fRoots,fBaseline,current.writes,error)) {
        result->recoveryDirectory=journal.Directory();
        if(!result->recoveryDirectory.isEmpty())fRequiresReopen=true;
        return false;
    }
    fActiveSave=journal.Directory();result->recoveryDirectory=fActiveSave;
    std::size_t index=0;
    for(auto it=current.writes.constBegin();it!=current.writes.constEnd();++it,++index) {
        if(!CheckSources(error) || !journal.Check(error) || !prepared[index]->commit()) {
            if(error->isEmpty()) *error=prepared[index]->errorString();
            fRequiresReopen=true;
            *error="Save All did not complete. Saved: "+result->savedFiles.join(", ")+"; remaining: "+result->remainingFiles.join(", ")+". "+*error;
            return false;
        }
        fBaseline[it.key()]=it.value();fStaged.remove(it.key());
        result->savedFiles.append(it.key());result->remainingFiles.removeAll(it.key());++fRevision;
        if(!XcfgSaveJournal::SyncSource(it.key(),error)){fRequiresReopen=true;return false;}
        if(progress)try{progress(it.key(),fActiveSave);}catch(const std::exception &e){fRequiresReopen=true;return Fail(error,QString::fromUtf8(e.what()));}catch(...){fRequiresReopen=true;return Fail(error,"Save progress observer failed; recovery intent retained");}
    }
    if(!CheckSources(error)) {fRequiresReopen=true;*error="Files saved, but final source verification failed; reopen and inspect. "+*error;return false;}
    if(!journal.Complete("candidate",error)){fRequiresReopen=true;return false;}
    fActiveSave.clear();
    result->complete=true;error->clear();return true;
}
