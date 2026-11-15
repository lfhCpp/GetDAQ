#ifndef GETDAQ_AGETCOMPILER_H
#define GETDAQ_AGETCOMPILER_H
#include "xcfgdocument.h"
#include <QJsonObject>
#include <QMap>
namespace GetDAQ { struct CoBoAgetSettings; struct CoBoReadoutSettings; struct CoBoConfigureSettings; }

// 离线编译一个明确选择的活动 AGET；不连接硬件，不输出“完整 Configure 已完成”。
class AgetCompiler {
public:
    static bool Compile(const XcfgDocument &document, const QString &coboId, unsigned int asadId,
                        unsigned int agetId, quint16 chipVersion, QJsonObject *output, QString *error);
    static bool Save(const XcfgDocument &document, const QString &coboId, unsigned int asadId,
                     unsigned int agetId, quint16 chipVersion, const QString &path, QString *error);
    // 型号表必须恰好覆盖活动芯片，key 为 4*AsAd+AGET。
    static bool CompileCoBo(const XcfgDocument &document, const QString &coboId, const QMap<int, quint16> &versions,
                            QJsonObject *output, QString *error, GetDAQ::CoBoAgetSettings *typedSettings = nullptr);
    static bool SaveCoBo(const XcfgDocument &document, const QString &coboId, const QMap<int, quint16> &versions,
                         const QString &path, QString *error);
    // AGET 加上 CoBo 的 Module/零抑制/循环缓冲区；仍不是完整 Configure。
    static bool CompileReadout(const XcfgDocument &document, const QString &coboId, const QMap<int, quint16> &versions,
                               QJsonObject *output, QString *error, GetDAQ::CoBoReadoutSettings *typedSettings = nullptr);
    static bool SaveReadout(const XcfgDocument &document, const QString &coboId, const QMap<int, quint16> &versions,
                            const QString &path, QString *error);
    // Configure 参数再加入 AsAd TCM/inspection/pulser；仍需完整 Prepare 和生产 ECC 接入。
    static bool CompileConfigure(const XcfgDocument &document, const QString &coboId, const QMap<int, quint16> &versions,
                                 QJsonObject *output, QString *error, GetDAQ::CoBoConfigureSettings *typedSettings = nullptr);
    static bool SaveConfigure(const XcfgDocument &document, const QString &coboId, const QMap<int, quint16> &versions,
                              const QString &path, QString *error);
};
#endif
