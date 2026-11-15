#ifndef GETDAQ_XCFGSAVEJOURNAL_H
#define GETDAQ_XCFGSAVEJOURNAL_H
#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <memory>
class QLockFile;
class XcfgSaveJournal {
public:
    struct Recovery {
        QString directory,manifestSha256;
        QStringList roots;
        QMap<QString,QByteArray> observed,target;
        QJsonObject report;
        bool candidate=false;
    };
    XcfgSaveJournal();
    ~XcfgSaveJournal();
    bool Begin(const QStringList &roots,const QMap<QString,QByteArray> &original,
               const QMap<QString,QByteArray> &writes,QString *error);
    bool Check(QString *error) const;
    bool Complete(const QString &outcome,QString *error);
    QString Directory() const {return fDirectory;}
    QString Key() const {return fKey;}
    static bool Pending(const QString &source,QString *directory,QString *error);
    static bool PreviewRecovery(const QString &directory,bool candidate,Recovery *preview,QString *error);
    static bool ApplyRecovery(const Recovery &preview,QString *error);
    static bool SyncSource(const QString &path,QString *error);
private:
    QString fDirectory,fKey;
    QByteArray fManifest;
    QMap<QString,QByteArray> fMarkers;
    std::unique_ptr<QLockFile> fLock;
};
#endif
