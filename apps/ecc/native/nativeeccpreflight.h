#ifndef GETDAQ_NATIVEECCPREFLIGHT_H
#define GETDAQ_NATIVEECCPREFLIGHT_H
#include "nativeecccoordinator.h"
namespace GetDAQ {
// 内部只读预检查。实际子流程仍重新核对；不能代替现场控制权或实际加载版本。
void ValidateNativeEccHardware(const SystemPreparationOptions &options,const std::vector<DataSenderOptions> &senders,
                               EccCommand command,EccState from,SystemPreparationClient *prepared,
                               const std::string &maintenanceNode=std::string(),unsigned int asadId=0,
                               unsigned int ledType=0,unsigned int ledState=0,int pulserVoltageMv=0,unsigned int ledFlipId=0,
                               unsigned int ledPulseId=0,unsigned int ledPulsePeriodMs=0);
}
#endif
