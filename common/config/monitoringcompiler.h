#ifndef GETDAQ_MONITORINGCOMPILER_H
#define GETDAQ_MONITORINGCOMPILER_H
#include "xcfgdocument.h"
#include "../experiment/cobomonitoringconfiguration.h"
#include <QJsonObject>
// 映射策略必须由调用者明确选择；Qt 只处理配置工件，不传输采集原始数据。
class MonitoringCompiler {
public:
    static bool Compile(const XcfgDocument &document, const QString &coboId,
                        GetDAQ::CoBoMonitoringSettings::ModeMapping mapping,
                        QJsonObject *output, QString *error, GetDAQ::CoBoMonitoringSettings *settings = nullptr);
    static bool Save(const XcfgDocument &document, const QString &coboId,
                     GetDAQ::CoBoMonitoringSettings::ModeMapping mapping, const QString &path, QString *error);
};
#endif
