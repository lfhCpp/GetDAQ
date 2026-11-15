#ifndef GETDAQ_COBOPREPARECONFIGURATION_H
#define GETDAQ_COBOPREPARECONFIGURATION_H
#include "coboreadoutconfiguration.h"
#include "coboclockconfiguration.h"
#include "cobomonitoringconfiguration.h"
#include "cobocalibrationconfiguration.h"
namespace GetDAQ {
// Prepare 前段的确定性参数；不包含上电、时钟、监控或校准。
struct CoBoPrepareFoundationSettings {
    std::uint32_t hardwareVersion = 2; // 1=R-CoBo；>=2=µTCA，不能以失败推断型号。
    bool verifyHardwareVersion = true; // 旧无版本寄存器的 R-CoBo 必须显式关闭。
    bool firmwareInfoRegisters = true, mutantStatusRegister = true, interruptRateRegister = true;
    CoBoReadoutSettings::BufferLayout bufferLayout = CoBoReadoutSettings::PerAsAdBuffers;
    std::array<std::string, 4> lemo{{"none", "none", "none", "none"}};
};
struct AsAdInputPosition {
    bool active = false, enabled = true, connectGroundToZAP = false, antiSparklingEnabled = true;
};
struct AsAdInputBoardSettings {
    unsigned int asadId = 0;
    std::array<AsAdInputPosition, 4> positions;
};
struct AsAdInputSettings {
    unsigned int asadMask = 0;
    std::vector<AsAdInputBoardSettings> boards; // 必须完整覆盖活动 AsAd；含四个物理位置。
};
struct AsAdInputBoardImage {
    unsigned int asadId = 0, adc0 = 0, im0 = 0, im1 = 0;
};
struct AsAdInputImage {
    unsigned int asadMask = 0, agetMask = 0;
    std::vector<AsAdInputBoardImage> boards;
};
// 单板Prepare所有参数的不可变输入；跨板对齐/系统状态由上层协调器接续。
struct CoBoPrepareSettings {
    CoBoPrepareFoundationSettings foundation;
    AsAdInputSettings inputs;
    CoBoClockSettings clock;
    CoBoMonitoringSettings monitoring;
    CoBoCalibrationSettings calibration;
    unsigned int manualReadDataDelay = 14;
    bool adcDoubleDataRate = true;
};
// 仅计算、拒绝未知功能和非法布局，不连接硬件。
unsigned int CompileCoBoLemo(const std::string &function);
std::vector<CoBoRegisterValue> CompileCoBoPrepareFoundation(const CoBoPrepareFoundationSettings &settings);
AsAdInputImage CompileAsAdInputs(const AsAdInputSettings &settings);
// 检查各片段的AsAd/AGET活动集合与板型策略一致；不访问硬件。
void ValidateCoBoPrepareSettings(const CoBoPrepareSettings &settings);
}
#endif
