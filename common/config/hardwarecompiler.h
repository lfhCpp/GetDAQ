#ifndef GETDAQ_HARDWARECOMPILER_H
#define GETDAQ_HARDWARECOMPILER_H
#include "xcfgdocument.h"
#include <QJsonObject>
// 离线编译：仅将 GET 硬件描述转换为可检查的通信结构，不连接任何服务。
class HardwareCompiler {
public:
    static bool Compile(const XcfgDocument &document, QJsonObject *output, QString *error);
    static bool Save(const XcfgDocument &document, const QString &path, QString *error);
};
#endif
