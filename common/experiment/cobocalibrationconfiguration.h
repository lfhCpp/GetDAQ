#ifndef GETDAQ_COBOCALIBRATIONCONFIGURATION_H
#define GETDAQ_COBOCALIBRATIONCONFIGURATION_H
#include <array>
#include <cstdint>
#include <map>
#include <vector>
namespace GetDAQ {
struct AsAdCalibrationSettings {
    unsigned int asadId = 0, calibrationAgetId = 0; // 旧 0 表示自动选择第一片活动芯片；1..3 显式选择。
    bool useAutomaticReadDelay = false;
};
struct CoBoCalibrationSettings {
    enum DelayLayout : unsigned int { GlobalDelayField, PerAsAdDelayFields };
    DelayLayout delayLayout = GlobalDelayField;
    unsigned int asadMask = 0;
    bool calibrationAgetRegister = true, computeAutomaticReadDelay = true;
    std::map<unsigned int,std::uint16_t> chipVersions; // key=4*AsAd+AGET，恰好覆盖活动芯片，调用者指定预期。
    std::vector<AsAdCalibrationSettings> boards; // 必须完整覆盖活动 AsAd；没有活动芯片的板不冒称已校准。
};
struct CoBoCalibrationImage {
    unsigned int asadMask = 0, agetMask = 0, calibrationAsAdMask = 0, automaticReadDelayMask = 0;
    unsigned int firstCalibrationChip = 0;
    bool globalAutomaticReadDelay = false;
    std::array<unsigned int,4> calibrationAgetIds{{0,0,0,0}};
};
// 纯检查/选择；实际串行与读取延迟校准由原生计划执行，不证明电气延迟已正确。
CoBoCalibrationImage CompileCoBoCalibration(const CoBoCalibrationSettings &settings);
}
#endif
