#ifndef GETDAQ_COBOASADCONFIGURATION_H
#define GETDAQ_COBOASADCONFIGURATION_H
#include "coboreadoutconfiguration.h"
namespace GetDAQ {
struct CoBoAsAdBoardSettings {
    unsigned int asadId = 0, triggerDelay = 0; // WCK periods, 0..31。
    bool calibrationMode = false;
    std::string capacitor = "1pF";
    std::string inspectionDevice = "ADC", isp1 = "SPI_CS", isp2 = "CAS";
};
struct CoBoAsAdSettings {
    unsigned int asadMask = 0;
    std::map<std::string, std::string> generator;
    std::vector<CoBoAsAdBoardSettings> boards; // 恰好覆盖活动 AsAd，包括无活动 AGET 的板。
};
struct CoBoAsAdBoardImage {
    unsigned int asadId = 0, triggerDelay = 0;
    bool calibrationMode = false, capacitor11pF = false;
    unsigned int inspection = 0;
};
struct CoBoPulserImage {
    // 与旧 Ice long 接口一致：有限 mV 实数先向零截断为整 mV，再计算 DAC。
    int defaultVoltageMv = 0, voltageMv = 0, secondVoltageMv = 0;
    unsigned int defaultDac = 0, dac = 0, secondDac = 0, periodMs = 1000;
    bool external = false, periodic = false, doublePulse = false, poisson = false;
};
struct CoBoAsAdImage {
    unsigned int asadMask = 0;
    CoBoPulserImage pulser;
    std::vector<CoBoAsAdBoardImage> boards; // 按 AsAd ID 排序。
};
struct CoBoConfigureSettings {
    CoBoReadoutSettings readout;
    CoBoAsAdSettings asad;
};
struct CoBoConfigureImage {
    CoBoReadoutImage readout;
    CoBoAsAdImage asad;
};
unsigned int CompileAsAdInspection(const std::string &device, const std::string &isp1, const std::string &isp2);
int ParseAsAdVoltage(const std::string &text, const std::string &name);
unsigned int AsAdDacCode(int voltageMv);
CoBoAsAdImage CompileCoBoAsAd(const CoBoAsAdSettings &settings);
// Configure 参数编译；不包含 Prepare、状态转换、所有权或实机验证。
CoBoConfigureImage CompileCoBoConfigure(const CoBoConfigureSettings &settings);
}
#endif
