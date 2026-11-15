#ifndef GETDAQ_SYSTEMEXPERIMENTCOMPILER_H
#define GETDAQ_SYSTEMEXPERIMENTCOMPILER_H
#include "xcfgdocument.h"
#include "../experiment/systemexperimentconfiguration.h"
class SystemExperimentCompiler {
public:
    // 必须显式给每个实际实例硬件兼容策略；不连接硬件或猜测芯片型号。
    static bool Compile(const XcfgDocument &document,const QByteArray &profile,
                        GetDAQ::SystemExperimentArtifact *artifact,QString *error);
    static bool Save(const XcfgDocument &document,const QString &profilePath,const QString &outputPath,QString *error);
};
#endif
