#ifndef GETDAQ_MUTANTCOMPILER_H
#define GETDAQ_MUTANTCOMPILER_H
#include "xcfgdocument.h"
#include "../experiment/mutantconfiguration.h"
#include <QJsonObject>
class MutantCompiler {
public:
    // options只提供布局、模块数、对齐掩码及硬件兼容策略；实验参数来自document。
    static bool Compile(const XcfgDocument &document, const QString &instance,
                        const GetDAQ::MutantSettings &options, QJsonObject *output, QString *error,
                        GetDAQ::MutantSettings *settings = nullptr);
    static bool Save(const XcfgDocument &document, const QString &instance,
                     const GetDAQ::MutantSettings &options, const QString &path, QString *error);
};
#endif
