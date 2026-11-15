#ifndef GETDAQ_COBOMONITORINGCONFIGURATION_H
#define GETDAQ_COBOMONITORINGCONFIGURATION_H
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace GetDAQ {
struct AsAdMonitoringBoardSettings {
    unsigned int asadId = 0;
    bool checkPowerSupply = true, alarmsEnabled = true;
    bool averagingDisabled = false, fastConversionEnabled = false, singleChannelModeEnabled = false;
    std::string singleChannelItem = "VDD1";
    double vddLower = 3.0, vddUpper = 3.5, iddLower = 0, iddUpper = 2.0;
    double vddSettleLower = 3.0; // 现代配置等于 vddLower；旧平铺字段保留旧等待函数的 3.2V 默认值。
    int externalTemperatureLower = 17, externalTemperatureUpper = 50;
    // Text high/low/out-of-order, VDD, IDD；true 为关闭对应自动保护。
    std::array<bool,5> disableAlerts{{false,false,false,false,false}};
};
struct CoBoMonitoringSettings {
    enum ModeMapping : unsigned int { DeclaredParameters, LegacyCallArguments };
    enum LedPolicy : unsigned int { NoLedManager, RequiredLedManager };
    ModeMapping modeMapping = DeclaredParameters; // 明确修正旧调用顺序；兼容行为须显式选择。
    LedPolicy ledPolicy = NoLedManager; // 旧板缺方法必须明确关闭，不从写失败降级。
    unsigned int ledHardwareVersion = 2; // 收到的LED实现只支持板型2或5，不能推定任意>=2。
    bool checkAsAdAlarms = true, subscribeAlarms = true; // CoBo Alarms 策略，与每板保护开关不同。
    unsigned int asadMask = 0;
    unsigned int powerDelayMs = 10, settleAttempts = 10, settleDelayMs = 100, zeroReadRetries = 10;
    std::vector<AsAdMonitoringBoardSettings> boards;
};
struct AsAdMonitoringBoardImage {
    unsigned int asadId = 0;
    bool checkPowerSupply = true;
    std::array<unsigned int,6> limits{{0,0,0,0,0,0}}; // Text low/high, VDD low/high, IDD low/high。
    unsigned int configuration2 = 0, configuration3 = 0, protectionMask = 0xff, protectionConfiguration1 = 0x2d;
    unsigned int vddSettleMinimum = 0; // ceil(请求最低电压/读取 LSB)，非阈值寄存器的 floor。
};
struct CoBoMonitoringImage {
    unsigned int asadMask = 0;
    CoBoMonitoringSettings::ModeMapping modeMapping = CoBoMonitoringSettings::DeclaredParameters;
    std::vector<AsAdMonitoringBoardImage> boards;
};
struct AsAdMeasurements {
    double vddVolts = 0, vdd1Volts = 0, iddAmps = 0;
    int internalTemperatureC = 0, externalTemperatureC = 0;
};
unsigned int CompileAsAdMonitoringItem(const std::string &name);
unsigned int CompileAsAdVddLimit(double volts);
unsigned int CompileAsAdIddLimit(double amps);
CoBoMonitoringImage CompileCoBoMonitoring(const CoBoMonitoringSettings &settings);
AsAdMeasurements DecodeAsAdMeasurements(const std::array<std::uint64_t,5> &raw);
double ParseMonitoringReal(const std::string &text, const std::string &name);
}
#endif
