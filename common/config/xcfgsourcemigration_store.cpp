#include "xcfgsourcemigration.h"
#include "xcfgsourcemigration_p.h"
#include "xcfgtransaction.h"
#include "xcfgsavejournal.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QUuid>
#include <memory>
#include <stdexcept>
#include <vector>
#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <io.h>
#include <qt_windows.h>
#endif

using namespace XcfgSourceMigrationInternal;

namespace {
QByteArray Read(const QString &path,qint64 maximum=8*1024*1024) {
    const QFileInfo info(path);
    Require(info.isFile() && !info.isSymLink() && info.size()>=0 && info.size()<=maximum,"Invalid migration file: "+path);
    QFile file(path);Require(file.open(QIODevice::ReadOnly),"Cannot read migration file: "+path);
    const auto bytes=file.read(maximum+1);
    Require(file.error()==QFile::NoError && bytes.size()<=maximum,"Migration read failed or exceeds budget: "+path);
    return bytes;
}
void SyncDirectory(const QString &path) {
#ifdef Q_OS_UNIX
    const int fd=::open(QFile::encodeName(path).constData(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    Require(fd>=0,"Cannot open migration directory for synchronization: "+path);
    const int result=::fsync(fd);::close(fd);Require(result==0,"Cannot synchronize migration directory: "+path);
#else
    Q_UNUSED(path);
#endif
}
void SyncFile(QFile &file) {
    Require(file.flush(),"Cannot flush migration file");
#ifdef Q_OS_UNIX
    Require(::fsync(file.handle())==0,"Cannot synchronize migration file");
#elif defined(Q_OS_WIN)
    Require(::_commit(file.handle())==0,"Cannot synchronize migration file");
#endif
}
void GuardDirectory(const QString &path,bool create=false) {
    if(create) {
#ifdef Q_OS_UNIX
        Require(::mkdir(QFile::encodeName(path).constData(),0700)==0,"Cannot exclusively create migration record directory");
#else
        Require(QDir().mkdir(path),"Cannot exclusively create migration record directory");
        Require(QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner),"Cannot protect migration directory");
#endif
    }
    const QFileInfo info(path);
    Require(info.isDir() && !info.isSymLink() && info.canonicalFilePath()==info.absoluteFilePath(),"Migration directory identity differs");
#ifdef Q_OS_UNIX
    struct stat value;
    Require(::lstat(QFile::encodeName(path).constData(),&value)==0 && value.st_uid==::geteuid() && (value.st_mode&0777)==0700,
            "Migration record directory owner/permissions differ");
#endif
    if(create)SyncDirectory(info.absolutePath());
}
void WriteNew(const QString &path,const QByteArray &bytes) {
    QFile file(path);Require(file.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Migration evidence already exists or cannot be created: "+path);
    Require(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner),"Cannot protect migration evidence");
    Require(file.write(bytes)==bytes.size(),"Migration evidence write incomplete");
    SyncFile(file);file.close();SyncDirectory(QFileInfo(path).absolutePath());
}
QMap<QString,QByteArray> Observe(const Record &record) {
    QMap<QString,QByteArray> observed;
    for(const auto &path:Paths(record.before,record.after)) {
        Require(Slot(path)==path,"Migration path changed");
        if(!QFileInfo::exists(path)) {
            Require(!record.before.contains(path) || !record.after.contains(path),"An unchanged-path source disappeared during migration");continue;
        }
        const auto bytes=Read(path);
        Require((record.before.contains(path) && bytes==record.before.value(path)) ||
                (record.after.contains(path) && bytes==record.after.value(path)),"Migration source is neither recorded original nor candidate: "+path);
        observed[path]=bytes;
    }
    return observed;
}
void Own(QLockFile &lock) {
    lock.setStaleLockTime(0);Require(lock.tryLock(0),"Original migration/save writer is still active");
}
Record Begin(const XcfgSourceMigration::Preview &preview,QString *directory,std::unique_ptr<QLockFile> *lock) {
    const auto base=QFileInfo(preview.originalRoots.first()).absolutePath()+"/.getdaq-workspace-recovery";
    GuardDirectory(base,!QFileInfo::exists(base));
    const auto records=QDir(base).entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot);
    Require(records.size()<256,"Recovery record limit reached; archive completed records first");
    qint64 retained=0;
    for(const auto &item:records) {
        Require(!item.isSymLink(),"Unexpected recovery record symlink");
        for(const auto &part:{QString("original"),QString("candidate")})
            for(const auto &file:QDir(item.filePath()+'/'+part).entryInfoList(QDir::Files|QDir::System)) {
                Require(file.isFile() && !file.isSymLink(),"Unexpected recovery payload type");retained+=file.size();
            }
    }
    for(const auto &bytes:preview.original)retained+=bytes.size();
    for(const auto &bytes:preview.candidate)retained+=bytes.size();
    Require(retained<=2LL*1024*1024*1024,"Recovery storage limit would be exceeded; no source changed");
    *directory=base+"/move-"+QUuid::createUuid().toString(QUuid::WithoutBraces);GuardDirectory(*directory,true);
    GuardDirectory(*directory+"/original",true);GuardDirectory(*directory+"/candidate",true);
    lock->reset(new QLockFile(*directory+"/writer.lock"));Own(**lock);
    QJsonArray files,moves;int index=0;
    for(const auto &path:Paths(preview.original,preview.candidate)) {
        const bool before=preview.original.contains(path),after=preview.candidate.contains(path);
        if(before)WriteNew(*directory+"/original/"+QString::number(index)+".xcfg",preview.original.value(path));
        if(after)WriteNew(*directory+"/candidate/"+QString::number(index)+".xcfg",preview.candidate.value(path));
        files.append(QJsonObject{{"path",path},{"before_exists",before},{"after_exists",after},
            {"before_sha256",before ? Hash(preview.original.value(path)) : QString()},
            {"after_sha256",after ? Hash(preview.candidate.value(path)) : QString()}});++index;
    }
    for(auto it=preview.request.destinations.constBegin();it!=preview.request.destinations.constEnd();++it)moves.append(QJsonObject{{"from",it.key()},{"to",it.value()}});
    const auto bytes=QJsonDocument(QJsonObject{{"schema",1},{"kind","getdaq.source-path-migration"},
        {"roots_before",QJsonArray::fromStringList(preview.originalRoots)},
        {"roots_after",QJsonArray::fromStringList(preview.migratedRoots)},{"moves",moves},{"files",files}}).toJson(QJsonDocument::Compact);
    Require(bytes.size()<=2*1024*1024,"Migration manifest exceeds 2 MiB");WriteNew(*directory+"/manifest.json",bytes);
    auto record=LoadRecord(*directory);Require(Observe(record)==preview.original,"Sources changed before migration markers");
    InstallRecordMarkers(record,true);CheckRecordState(record,preview.original);return record;
}
struct NewSource {
    QString path;
    ~NewSource(){if(!path.isEmpty())QFile::remove(path);}
};
void PublishNew(const NewSource &file,const QString &path) {
    Require(!QFileInfo::exists(path) && !QFileInfo(path).isSymLink(),"Migration destination appeared; refusing replacement");
    const auto temporary=file.path;
#ifdef Q_OS_UNIX
    Require(::link(QFile::encodeName(temporary).constData(),QFile::encodeName(path).constData())==0,"Cannot atomically publish a new migration source without replacement");
#elif defined(Q_OS_WIN)
    const auto success=::MoveFileExW(reinterpret_cast<LPCWSTR>(temporary.utf16()),reinterpret_cast<LPCWSTR>(path.utf16()),MOVEFILE_WRITE_THROUGH);
    const auto code=success ? 0 : ::GetLastError();
    Require(success!=0,QString("Cannot atomically publish a new Windows migration source without replacement; Windows error %1").arg(qulonglong(code)));
#else
    Require(false,"Atomic new migration source publication is unsupported on this platform");
#endif
    SyncDirectory(QFileInfo(path).absolutePath());
}
void Execute(const Record &record,bool candidate,QMap<QString,QByteArray> observed,const XcfgSourceMigration::Progress &progress) {
    const auto &target=candidate ? record.after : record.before;
    struct Prepared {
        QString path;std::unique_ptr<QSaveFile> replace;std::unique_ptr<NewSource> create;
    };
    std::vector<Prepared> prepared;
    // Every changed/new source is completely prepared before any source changes.
    for(auto it=target.constBegin();it!=target.constEnd();++it) {
        if(observed.contains(it.key()) && observed.value(it.key())==it.value())continue;
        Prepared item;item.path=it.key();
        if(observed.contains(it.key())) {
            item.replace.reset(new QSaveFile(it.key()));item.replace->setDirectWriteFallback(false);
            Require(item.replace->open(QIODevice::WriteOnly) && item.replace->write(it.value())==it->size(),"Cannot prepare migration source replacement");
        }else {
            QTemporaryFile temporary(QFileInfo(it.key()).absolutePath()+"/.getdaq-migration-XXXXXX");
            Require(temporary.open() && temporary.setPermissions(QFile::ReadOwner|QFile::WriteOwner) && temporary.write(it.value())==it->size(),"Cannot prepare new migration source");SyncFile(temporary);
            item.create.reset(new NewSource());item.create->path=temporary.fileName();temporary.setAutoRemove(false);
            // QTemporaryFile::close retains its Windows handle. Destroy the
            // allocator before MoveFileExW; retain only our cleanup path.
        }
        prepared.push_back(std::move(item));
    }
    CheckRecordState(record,observed);
    // Publish all needed files before rewriting existing references; remove old
    // paths only after all target files and references have been synchronized.
    for(bool creating:{true,false})for(auto &item:prepared) {
        if(bool(item.create)!=creating)continue;
        CheckRecordState(record,observed);
        if(creating)PublishNew(*item.create,item.path);
        else {Require(item.replace->commit(),"Migration source replacement failed; inspect original intent");QString error;Require(XcfgSaveJournal::SyncSource(item.path,&error),error);}
        observed[item.path]=target.value(item.path);CheckRecordState(record,observed);
        if(progress)progress(creating ? "created" : "replaced",item.path);
    }
    for(const auto &path:observed.keys())if(!target.contains(path)) {
        CheckRecordState(record,observed);Require(QFile::remove(path),"Cannot remove old migration source; inspect original intent");
        SyncDirectory(QFileInfo(path).absolutePath());observed.remove(path);CheckRecordState(record,observed);
        if(progress)progress("removed",path);
    }
    CheckRecordState(record,target);FinishRecord(record,candidate);
}
}

namespace XcfgSourceMigrationInternal {
QByteArray ReadRecordFile(const QString &path,qint64 maximum) { return ::Read(path,maximum); }
void GuardRecordDirectory(const QString &path,bool create) { ::GuardDirectory(path,create); }
void SyncRecordDirectory(const QString &path) { ::SyncDirectory(path); }
void WriteNewRecordFile(const QString &path,const QByteArray &bytes) { ::WriteNew(path,bytes); }
QMap<QString,QByteArray> ObserveRecord(const Record &record) { return ::Observe(record); }
void OwnRecordLock(QLockFile &lock) { ::Own(lock); }
void ExecuteRecord(const Record &record,bool candidate,QMap<QString,QByteArray> observed,
                   const XcfgSourceMigration::Progress &progress) { ::Execute(record,candidate,std::move(observed),progress); }
bool IsMigrationRecordImpl(const QString &directory) {
    try{return ParseRecordObject(Read(directory+"/manifest.json",2*1024*1024))["kind"].toString()=="getdaq.source-path-migration";}
    catch(const std::exception &){return false;}
}
}

namespace XcfgSourceMigrationInternal {
void ApplyReviewedMigration(const XcfgSourceMigration::Preview &preview,QString *recoveryDirectory,
                            const XcfgSourceMigration::Progress &progress) {
    std::unique_ptr<QLockFile> lock;
    const auto record=Begin(preview,recoveryDirectory,&lock);
    Execute(record,true,preview.original,progress);
}
}
