#ifndef GETDAQ_XCFG_SOURCE_MIGRATION_P_H
#define GETDAQ_XCFG_SOURCE_MIGRATION_P_H

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QLockFile>
#include <QJsonObject>
#include <QStringList>
#include <stdexcept>

#include "xcfgsourcemigration.h"

namespace XcfgSourceMigrationInternal {
struct Record {
    QString directory,key;QByteArray manifest;
    QStringList beforeRoots,afterRoots;
    QMap<QString,QString> moves;
    QMap<QString,QByteArray> before,after;
};
inline void Require(bool okay,const QString &message) {
    if(!okay)throw std::runtime_error(message.toStdString());
}
inline QString Hash(const QByteArray &bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
}
inline QString Slot(const QString &path) {
    Require(!path.isEmpty() && !path.contains(QChar(0)) && !path.contains("://"),"Choose a local migration path");
    const QFileInfo info(QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
    const auto parent=QFileInfo(info.absolutePath()).canonicalFilePath();
    Require(!parent.isEmpty() && !info.fileName().isEmpty(),"Migration parent directory must exist");
    const auto result=QDir(parent).filePath(info.fileName());
    const QFileInfo file(result);
    Require(!file.isSymLink() && (!file.exists() || (file.isFile() && file.canonicalFilePath()==result)),"Migration slot is not a regular canonical file: "+result);
    return result;
}
inline QStringList Paths(const QMap<QString,QByteArray> &before,const QMap<QString,QByteArray> &after) {
    auto paths=before.keys();for(const auto &path:after.keys())if(!before.contains(path))paths.append(path);paths.sort();return paths;
}
inline void Budget(const QMap<QString,QByteArray> &files) {
    Require(!files.isEmpty() && files.size()<=256,"Migration source budget differs");qint64 total=0;
    for(const auto &bytes:files){Require(bytes.size()<=8*1024*1024,"Migration source exceeds 8 MiB");total+=bytes.size();}
    Require(total<=64LL*1024*1024,"Migration source set exceeds 64 MiB");
}
Record LoadRecord(const QString &directory);
QByteArray ReadRecordFile(const QString &path,qint64 maximum = 8*1024*1024);
void GuardRecordDirectory(const QString &path,bool create = false);
void SyncRecordDirectory(const QString &path);
void WriteNewRecordFile(const QString &path,const QByteArray &bytes);
QJsonObject ParseRecordObject(const QByteArray &bytes);
void RequireRecordFields(const QJsonObject &value,const QStringList &fields);
QString RecordText(const QJsonObject &value,const QString &key);
bool RecordCompleted(const Record &record);
QMap<QString,QByteArray> ObserveRecord(const Record &record);
void OwnRecordLock(QLockFile &lock);
void CheckRecordMarker(const Record &record,const QString &path,bool required);
void InstallRecordMarkers(const Record &record,bool initial);
void CheckRecordState(const Record &record,const QMap<QString,QByteArray> &observed);
void FinishRecord(const Record &record,bool candidate);
void ExecuteRecord(const Record &record,bool candidate,QMap<QString,QByteArray> observed,
                   const XcfgSourceMigration::Progress &progress);
bool IsMigrationRecordImpl(const QString &directory);
void ApplyReviewedMigration(const XcfgSourceMigration::Preview &preview,QString *recoveryDirectory,
                            const XcfgSourceMigration::Progress &progress);
} // namespace XcfgSourceMigrationInternal

#endif
