#ifndef GETDAQ_EFFECTIVECONFIGURATIONSNAPSHOT_H
#define GETDAQ_EFFECTIVECONFIGURATIONSNAPSHOT_H
#include "xcfgdocument.h"
#include <QJsonObject>

// 本地有效配置和来源审阅，不是硬件发布或实际加载凭据。
class EffectiveConfigurationSnapshot {
public:
    static bool Capture(const XcfgDocument &document,QJsonObject *snapshot,QString *error);
    static bool Validate(const QJsonObject &snapshot,QString *error);
    static bool Load(const QString &path,QJsonObject *snapshot,QString *error);
    static bool Save(const QJsonObject &snapshot,const QString &path,QString *error);
    static bool Compare(const QJsonObject &before,const QJsonObject &after,QJsonObject *report,QString *error);
    static bool SaveReport(const QJsonObject &report,const QString &path,QString *error);
private:
    friend class XcfgStructureEditor;
    friend class XcfgEditHistory;
    friend class XcfgWorkspace;
    friend class XcfgSourceMigration;
    // 只用于已核对原来源的内存候选，不把候选字节当磁盘实况。
    static bool CaptureLoaded(const XcfgDocument &document,bool verifySources,QJsonObject *snapshot,QString *error);
};
#endif
