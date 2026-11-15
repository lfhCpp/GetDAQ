#ifndef GETDAQ_ECCCLIENTSTORE_STORAGE_P_H
#define GETDAQ_ECCCLIENTSTORE_STORAGE_P_H
#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QString>
#include <memory>

namespace EccClientStoreStorage {
void SyncFile(QFile &file);
void SyncDirectory(const QString &path);
void Directory(const QString &path);
void WriteNew(const QString &path,const QByteArray &bytes);
QByteArray Read(const QString &path,qint64 maximum);

class Guard {
public:
    Guard(const QString &path,bool directory);
    ~Guard();
    void Check() const;
    void CheckHandle(int descriptor) const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
