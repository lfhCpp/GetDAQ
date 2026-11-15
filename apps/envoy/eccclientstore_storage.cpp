#include "eccclientstore_storage_p.h"
#include <QDir>
#include <QFileInfo>
#include <stdexcept>
#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <io.h>
#endif

namespace EccClientStoreStorage {
namespace {
void Require(bool value,const char *message) {
    if(!value)throw std::runtime_error(message);
}
}

void SyncFile(QFile &file) {
    Require(file.flush(),"Cannot flush ECC client record");
#ifdef Q_OS_UNIX
    Require(!::fsync(file.handle()),"Cannot sync ECC client record");
#elif defined(Q_OS_WIN)
    Require(!::_commit(file.handle()),"Cannot sync ECC client record");
#endif
}
void SyncDirectory(const QString &path) {
#ifdef Q_OS_UNIX
    const int descriptor=::open(path.toUtf8().constData(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    Require(descriptor>=0,"Cannot open ECC client directory for sync");
    const int result=::fsync(descriptor);::close(descriptor);Require(!result,"Cannot sync ECC client directory");
#else
    Q_UNUSED(path); // Windows文件提交采用_commit；目录断电持久性仍需目标环境验收。
#endif
}
void Directory(const QString &path) {
    Require(QDir().mkdir(path),"ECC client directory exists or cannot be created");
    Require(QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner),"Cannot protect ECC client directory");
    SyncDirectory(QFileInfo(path).absolutePath());
}
void WriteNew(const QString &path,const QByteArray &bytes) {
    QFile file(path);Require(file.open(QIODevice::WriteOnly|QIODevice::NewOnly),"ECC client file exists or cannot be created");
    Require(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner),"Cannot protect ECC client file");
    Require(file.write(bytes)==bytes.size(),"Cannot write complete ECC client file");
    SyncFile(file);file.close();SyncDirectory(QFileInfo(path).absolutePath());
}
QByteArray Read(const QString &path,qint64 maximum) {
    const QFileInfo info(path);Require(info.isFile()&&!info.isSymLink()&&info.size()<=maximum,"Invalid/bounded ECC client file");
    QFile file(path);Require(file.open(QIODevice::ReadOnly),"Cannot read ECC client file");
    const auto bytes=file.read(maximum+1);
    Require(bytes.size()<=maximum&&file.error()==QFile::NoError,"ECC client file read failed");
    return bytes;
}

class Guard::Impl {
public:
    QString path;
    bool directory;
#ifdef Q_OS_UNIX
    struct stat identity;
#else
    QDateTime birth;
#endif
    Impl(const QString &name,bool dir):path(name),directory(dir) {
#ifdef Q_OS_UNIX
        Require(!::lstat(path.toUtf8().constData(),&identity),"Cannot bind ECC client file identity");
#else
        birth=QFileInfo(path).birthTime();
#endif
        Check();
    }
    void Check() const {
        const QFileInfo info(path);
        Require(!info.isSymLink()&&(directory?info.isDir():info.isFile()),"ECC client file/directory type changed");
#ifdef Q_OS_UNIX
        struct stat current;
        Require(!::lstat(path.toUtf8().constData(),&current)&&current.st_dev==identity.st_dev&&current.st_ino==identity.st_ino&&
            current.st_uid==::geteuid()&&(current.st_mode&0777)==(directory?0700:0600),"ECC client identity/owner/permissions changed");
#else
        Require(info.birthTime()==birth,"ECC client file identity changed");
#endif
    }
    void CheckHandle(int descriptor) const {
        Check();
#ifdef Q_OS_UNIX
        struct stat held;
        Require(!::fstat(descriptor,&held)&&held.st_dev==identity.st_dev&&held.st_ino==identity.st_ino,"ECC client held journal identity differs");
#else
        Q_UNUSED(descriptor);
#endif
    }
};
Guard::Guard(const QString &path,bool directory):fImpl(new Impl(path,directory)) {}
Guard::~Guard()=default;
void Guard::Check() const {fImpl->Check();}
void Guard::CheckHandle(int descriptor) const {fImpl->CheckHandle(descriptor);}
}
