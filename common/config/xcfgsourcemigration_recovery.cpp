#include "xcfgsourcemigration.h"
#include "xcfgsourcemigration_p.h"
#include "xcfgtransaction.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <stdexcept>

using namespace XcfgSourceMigrationInternal;

bool XcfgSourceMigration::IsMigrationRecord(const QString &directory) {
    return IsMigrationRecordImpl(directory);
}

bool XcfgSourceMigration::PreviewRecovery(const QString &directory,bool candidate,Recovery *preview,QString *error) {
    XcfgIoTransaction transaction(error);if(!transaction.IsLocked())return false;
    try {
        Require(preview,"Missing migration recovery preview");const auto record=LoadRecord(directory);
        Require(!RecordCompleted(record),"Migration already completed; recovery is not replayed");
        QLockFile lock(directory+"/writer.lock");OwnRecordLock(lock);
        QJsonObject rootReports;
        if(!CheckSemantics(record.beforeRoots,record.afterRoots,record.before,record.after,&rootReports,error))return false;
        Recovery result;result.directory=directory;result.manifestSha256=record.key;result.candidate=candidate;
        result.observed=ObserveRecord(record);result.target=candidate ? record.after : record.before;
        result.roots=candidate ? record.afterRoots : record.beforeRoots;
        QJsonArray files;
        for(const auto &path:Paths(record.before,record.after)) {
            CheckRecordMarker(record,path,false);
            if(!result.observed.contains(path))result.missingObserved.append(path);
            if(!result.target.contains(path))result.missingTarget.append(path);
            files.append(QJsonObject{{"path",path},{"current_exists",result.observed.contains(path)},
                {"target_exists",result.target.contains(path)},
                {"current_sha256",result.observed.contains(path) ? Hash(result.observed.value(path)) : QString()},
                {"target_sha256",result.target.contains(path) ? Hash(result.target.value(path)) : QString()}});
        }
        result.report=QJsonObject{{"kind","getdaq.source-path-migration-recovery"},
            {"direction",candidate ? "candidate" : "original"},{"files",files},{"root_reviews",rootReports},
            {"roots_after_recovery",QJsonArray::fromStringList(result.roots)},
            {"disk_written",false},{"hardware_loaded_verified",false}};
        *preview=result;error->clear();return true;
    }catch(const std::exception &e){*error=QString::fromUtf8(e.what());return false;}
}

bool XcfgSourceMigration::ApplyRecovery(const Recovery &preview,QString *error,const Progress &progress) {
    XcfgIoTransaction transaction(error,XcfgIoTransaction::Write);if(!transaction.IsLocked())return false;
    try {
        Recovery current;
        if(!PreviewRecovery(preview.directory,preview.candidate,&current,error))return false;
        Require(current.manifestSha256==preview.manifestSha256 && current.roots==preview.roots &&
                current.observed==preview.observed && current.target==preview.target &&
                current.missingObserved==preview.missingObserved && current.missingTarget==preview.missingTarget &&
                current.report==preview.report,"Migration recovery review changed; inspect a new preview");
        QLockFile lock(current.directory+"/writer.lock");OwnRecordLock(lock);
        const auto record=LoadRecord(current.directory);
        Require(record.key==current.manifestSha256 && !RecordCompleted(record),
                "Migration record changed/completed after review");
        InstallRecordMarkers(record,false);CheckRecordState(record,current.observed);
        ExecuteRecord(record,current.candidate,current.observed,progress);
        error->clear();return true;
    }catch(const std::exception &e){*error=QString::fromUtf8(e.what());return false;}
}
