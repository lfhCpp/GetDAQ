#ifndef GETDAQ_SYSTEMEXPERIMENTBINDING_H
#define GETDAQ_SYSTEMEXPERIMENTBINDING_H
#include "systempreparation.h"
#include "systemexperimentconfiguration.h"
namespace GetDAQ {
// 只复制/检查绑定；保持调用者控制权、数据停止及回调端点。不会连接或写硬件。
SystemPreparationOptions BindSystemExperiment(const SystemExperimentArtifact &artifact,const SystemPreparationOptions &endpoints);
// 从现有拓扑建立参数绑定；控制权/数据停止默认为false、回调端点留空，由生产后端填写。
SystemPreparationOptions BindSystemExperiment(const SystemExperimentArtifact &artifact,const SystemAlignmentOptions &topology);
}
#endif
