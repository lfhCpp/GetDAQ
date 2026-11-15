#ifndef GETDAQ_CLOCKCOMPILER_H
#define GETDAQ_CLOCKCOMPILER_H
#include "xcfgdocument.h"
#include "../experiment/coboclockconfiguration.h"
#include <QJsonObject>
// Qt 仅解析配置与生成工件；时钟计算和硬件执行保持独立。
class ClockCompiler {
public:
    static bool Compile(const XcfgDocument &document, const QString &coboId, quint32 hardwareVersion,
                        QJsonObject *output, QString *error, GetDAQ::CoBoClockSettings *settings = nullptr);
    static bool Save(const XcfgDocument &document, const QString &coboId, quint32 hardwareVersion,
                     const QString &path, QString *error);
};
#endif
