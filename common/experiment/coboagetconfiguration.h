#ifndef GETDAQ_COBOAGETCONFIGURATION_H
#define GETDAQ_COBOAGETCONFIGURATION_H
#include "agetconfiguration.h"
namespace GetDAQ {
struct CoBoAgetChipSettings {
    unsigned int asadId = 0, agetId = 0;
    AgetSettings settings;
};
struct CoBoAgetSettings {
    unsigned int asadMask = 0; // 所有活动 AsAd；可包含没有活动芯片的板。
    bool allowReg2BankResetDifference = false; // 旧 bit21 差异策略必须显式启用。
    std::vector<CoBoAgetChipSettings> chips; // 恰好列出所有活动芯片，不生成缺省实例。
};
struct CoBoAgetChipImage {
    unsigned int asadId = 0, agetId = 0;
    AgetRegisterImage image;
};
struct CoBoAgetImage {
    unsigned int asadMask = 0, agetMask = 0, readoutOffset = 0, readoutDepth = 0;
    bool twoPulseMode = false, fpnRead = false;
    std::vector<CoBoAgetChipImage> chips; // 按 4*asadId+agetId 排序。
};
// 检查所有活动芯片的共享 CoBo 参数，不执行硬件操作。
CoBoAgetImage CompileCoBoAget(const CoBoAgetSettings &settings);
}
#endif
