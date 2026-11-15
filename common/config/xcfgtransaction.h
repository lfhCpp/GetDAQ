#ifndef GETDAQ_XCFGTRANSACTION_H
#define GETDAQ_XCFGTRANSACTION_H
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QStandardPaths>
#include <QUuid>
#include <memory>
#include <cerrno>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#include <unistd.h>
#endif

// 同一用户/操作环境/缓存中的新版工具共用：读者各持保留，写者独占登记门。
// 只在登记时短时等候，不排队重放来源操作；DAQ和不可变工件不使用此锁。
class XcfgIoTransaction {
public:
    enum Mode {Read,Write};
    explicit XcfgIoTransaction(QString *error,Mode mode=Read) {
        auto &state=Current();
        if(state.depth>0) {
            if(mode==Write && !state.writer) {Fail(error,"Cannot upgrade an active configuration reader to a writer");return;}
            if(!Owned(state,error))return;
            ++state.depth;fLocked=true;return;
        }
        const auto cache=QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
        if(cache.isEmpty() || !QDir().mkpath(cache)) {Fail(error,"Cannot prepare shared configuration cache");return;}
        const auto canonical=QFileInfo(cache).canonicalFilePath();
        const auto directory=QDir(canonical).filePath("getdaq-configuration-transactions-v1");
        if(canonical.isEmpty()) {Fail(error,"Configuration cache is not canonical");return;}
        if(!QFileInfo::exists(directory)) {
#ifdef Q_OS_UNIX
            // Avoid a temporary 0755 directory while concurrent first readers start.
            if(::mkdir(QFile::encodeName(directory).constData(),0700) && errno!=EEXIST) {Fail(error,"Cannot create configuration transaction directory");return;}
#else
            if(QDir().mkdir(directory)) {
                if(!QFile::setPermissions(directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner)) {Fail(error,"Cannot protect configuration transaction directory");return;}
            }else if(!QFileInfo::exists(directory)) {Fail(error,"Cannot create configuration transaction directory");return;}
#endif
        }
        const QFileInfo info(directory);
        if(!info.isDir() || info.isSymLink() || info.canonicalFilePath()!=info.absoluteFilePath()) {Fail(error,"Invalid configuration transaction cache directory");return;}
#ifdef Q_OS_UNIX
        struct stat status;
        if(::lstat(QFile::encodeName(directory).constData(),&status) || status.st_uid!=::geteuid() || (status.st_mode&0777)!=0700) {Fail(error,"Configuration transaction cache owner/permissions differ");return;}
#endif
        const auto gatePath=QDir(directory).filePath("source-access.lock");
        std::unique_ptr<QLockFile> gate(new QLockFile(gatePath));gate->setStaleLockTime(0);
        if(!gate->tryLock(250)) {Fail(error,"Configuration source transaction busy; another writer or registration is active. No source was changed by this call.");return;}
        const auto reservations=QDir(directory).entryInfoList({"read-*.lock"},QDir::Files|QDir::System);
        if(reservations.size()>256) {Fail(error,"Configuration reader reservation limit exceeded; inspect cache after closing configuration tools");return;}
        int readers=0;
        // Holding the registration gate prevents new readers. A stale slot is
        // removed only after QLockFile proves its previous owner is not active.
        if(mode==Write || reservations.size()>=256) {
            for(const auto &slot:reservations) {
                if(!slot.isFile() || slot.isSymLink()) {
                    if(!QFileInfo::exists(slot.absoluteFilePath()))continue; // reader finished after enumeration
                    Fail(error,"Invalid configuration reader reservation");return;
                }
                QLockFile previous(slot.absoluteFilePath());previous.setStaleLockTime(0);
                if(!previous.tryLock(0))++readers;
            }
        }
        if(mode==Write) {
            if(readers) {Fail(error,"Configuration source transaction busy; active readers must finish before writing");return;}
            state.path=gatePath;state.lock=std::move(gate);state.writer=true;
        }else {
            if(reservations.size()>=256 && readers>=256) {Fail(error,"Configuration reader reservation limit reached");return;}
            state.path=QDir(directory).filePath("read-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".lock");
            state.lock.reset(new QLockFile(state.path));state.lock->setStaleLockTime(0);
            if(!state.lock->tryLock(0)) {state.lock.reset();Fail(error,"Cannot reserve configuration reader");return;}
            state.writer=false;
        }
        if(!Owned(state,error)) {state.lock.reset();return;}
        state.depth=1;fLocked=true;
    }
    ~XcfgIoTransaction() {if(fLocked){auto &state=Current();if(--state.depth==0)state.lock.reset();}}
    XcfgIoTransaction(const XcfgIoTransaction &)=delete;
    XcfgIoTransaction &operator=(const XcfgIoTransaction &)=delete;
    bool IsLocked() const {return fLocked;}
private:
    struct State {int depth=0;bool writer=false;QString path;std::unique_ptr<QLockFile> lock;};
    static State &Current() {static thread_local State value;return value;}
    static void Fail(QString *error,const char *message) {if(error)*error=QString::fromLatin1(message);}
    static bool Owned(const State &state,QString *error) {
        qint64 pid=0;QString host,application;const QFileInfo file(state.path);
        if(!state.lock || !state.lock->isLocked() || !file.isFile() || file.isSymLink() ||
            !state.lock->getLockInfo(&pid,&host,&application) || pid!=QCoreApplication::applicationPid()) {
            Fail(error,"Configuration transaction lock changed; preserve pending save evidence");return false;
        }
        return true;
    }
    bool fLocked=false;
};
#endif
