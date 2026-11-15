#ifndef GETDAQ_ACQUISITIONPLANS_H
#define GETDAQ_ACQUISITIONPLANS_H
#include "hardwareclient.h"
#include "coboasadconfiguration.h"
#include "mutantconfiguration.h"
namespace GetDAQ {
// 在正确的sender边界执行；不是能单独运行的完整Start/ECC状态转换。
struct CoBoAcquisitionPlan {
    std::vector<HardwareStep> stopped, runInit, beforeSenderStart, afterSenderStart;
    std::vector<HardwareStep> stopPulser, stopFirmware, stopInterrupts, temperatures;
};
// 停生产不依赖Configure成功或内存布局；Prepare失败时仍可使用相同停止步骤。
struct CoBoProductionStopPlan {
    std::vector<HardwareStep> pulser, firmware, interrupts;
};
CoBoProductionStopPlan BuildCoBoProductionStopPlan();
CoBoAcquisitionPlan BuildCoBoAcquisitionPlan(const CoBoConfigureSettings &settings);
std::vector<HardwareStep> MutantAcquisitionRun(bool enabled);
}
#endif
