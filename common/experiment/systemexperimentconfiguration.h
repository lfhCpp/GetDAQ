#ifndef GETDAQ_SYSTEMEXPERIMENTCONFIGURATION_H
#define GETDAQ_SYSTEMEXPERIMENTCONFIGURATION_H
#include "coboprepareconfiguration.h"
#include "coboasadconfiguration.h"
#include "mutantconfiguration.h"
namespace GetDAQ {
// 完整实验参数，不包含网络代理、硬件描述或控制权声明。
// Qt配置工具与无Qt的原生后端共用；不能把它当已加载/已配置的硬件状态。
struct SystemExperimentCoBo {
    std::string id;
    bool enabled = true;
    CoBoPrepareSettings prepare;
    CoBoConfigureSettings configure;
};
struct SystemExperimentMutant {
    std::string id;
    MutantSettings settings;
};
struct SystemExperimentSettings {
    std::vector<SystemExperimentCoBo> cobos;
    std::vector<SystemExperimentMutant> mutants;
};
struct ExperimentConfigurationSource { std::string path, sha256; };
struct SystemExperimentArtifact {
    SystemExperimentSettings settings;
    std::vector<ExperimentConfigurationSource> sources;
    std::string profileSha256, configurationSha256, artifactSha256;
};
// 纯检查全部活动/型号/布局及主从约束；模块数须与本次实体列表一致，掩码由后续对齐提供。
void ValidateSystemExperimentSettings(const SystemExperimentSettings &settings);
// 确定性JSON参数编码；整数/实数用十进制字符串避免JSON的53位精度限制。
// 不包含来源路径或运行状态，不需要Qt、Ice或解析库。
std::string EncodeSystemExperimentSettings(const SystemExperimentSettings &settings);
std::string EncodeJsonString(const std::string &text);
// Explicit bounded metadata envelopes; default scalar budget stays 4096 bytes.
std::string EncodeJsonString(const std::string &text,std::size_t maximumBytes);
// 版本摘要覆盖schema域和实际执行参数，来源摘要另存；工件摘要覆盖收到的全部文件字节。
std::string SystemExperimentDigest(const SystemExperimentSettings &settings);
std::string EncodeSystemExperimentArtifact(const SystemExperimentArtifact &artifact);
SystemExperimentArtifact DecodeSystemExperimentArtifact(const std::string &bytes);
SystemExperimentArtifact LoadSystemExperimentArtifact(const std::string &path);
}
#endif
