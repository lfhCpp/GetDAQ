#ifndef GETDAQ_XCFGSOURCEMIGRATION_H
#define GETDAQ_XCFGSOURCEMIGRATION_H
#include "xcfgworkspace.h"
#include <functional>

// 配置文件实际迁移及全部已知include改写，不是声明移动或另存副本。
class XcfgSourceMigration {
public:
    struct Request {QMap<QString,QString> destinations;};
    struct Preview {
        quint64 revision=0;
        Request request;
        QStringList originalRoots,migratedRoots;
        QMap<QString,QByteArray> original,candidate;
        QJsonObject report;
    };
    struct Result {
        bool complete=false;
        QString recoveryDirectory;
        QStringList roots;
    };
    struct Recovery {
        QString directory,manifestSha256;
        QStringList roots,missingObserved,missingTarget;
        QMap<QString,QByteArray> observed,target;
        QJsonObject report;
        bool candidate=false;
    };
    using Progress=std::function<void(const QString &,const QString &)>;
    // Read-only inventory of XML include referrers beneath an explicitly
    // selected project directory. A partial scan is never reported as complete.
    static bool DiscoverReferrers(const QString &directory,const QString &source,QJsonObject *report,QString *error);
    static bool CreatePreview(const XcfgWorkspace &workspace,const Request &request,Preview *preview,QString *error);
    static bool Apply(XcfgWorkspace *workspace,const Preview &preview,Result *result,QString *error,const Progress &progress={});
    static bool IsMigrationRecord(const QString &directory);
    static bool PreviewRecovery(const QString &directory,bool candidate,Recovery *preview,QString *error);
    static bool ApplyRecovery(const Recovery &preview,QString *error,const Progress &progress={});
private:
    static bool BuildGraph(const QStringList &roots,const QMap<QString,QByteArray> &bytes,
                           QMap<QString,XcfgDocument> *documents,QString *error);
    static bool CheckSemantics(const QStringList &beforeRoots,const QStringList &afterRoots,
                               const QMap<QString,QByteArray> &before,const QMap<QString,QByteArray> &after,
                               QJsonObject *reports,QString *error);
};
#endif
