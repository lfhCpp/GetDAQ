#ifndef GETDAQ_COBOCLOCKCONFIGURATION_H
#define GETDAQ_COBOCLOCKCONFIGURATION_H
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace GetDAQ {
// LCM 的 80 位线上格式由原 AsAdBus 补入固定 0x0001；这里只计算 64 位表示。
struct AsAdClockImage {
    std::uint64_t value = 0;
    double nominalOutputMHz = 0, nominalAdcMHz = 0;
    bool nominalOutputKnown = true; // 专家选择未知的 LCM1 输出源时不猜测频率。
    bool nominalMatchesRequested = true, nominalWithinAgetRange = true;
    int quantizedAdcDelayPs = 0;
};
struct CoBoPllImage {
    unsigned int divider = 0; // 按 CKW 请求计算的默认分频；专家覆盖可改变实际寄存器。
    double nominalDefaultWritingMHz = 0; // 覆盖专家寄存器之前、假定 VCO=2000MHz 的值；不是测量。
    bool nonStandardFrequency = false, expertRegisters = false;
    std::vector<std::uint32_t> packets; // 包含第一包 RESET，以及所需的第二次 R11。
};
struct CoBoPllParameter {
    std::string name, defaultValue, legacyName;
    unsigned int reg, offset, width;
    bool boolean;
};
const std::vector<CoBoPllParameter> &CoBoPllParameters(); // 旧 ClockInit 读取 R10..13，不含闲置的 R14 设置。
struct AsAdClockParameter { std::string name; unsigned int width; bool boolean; };
const std::vector<AsAdClockParameter> &AsAdSamplingClockParameters();
const std::vector<AsAdClockParameter> &AsAdReadingClockParameters();
struct AsAdBoardClockSettings {
    unsigned int asadId = 0;
    float samplingDivider = 1;
    int phaseDegrees = 270, delayPs = 5335;
    std::map<std::string, unsigned int> samplingExpert, readingExpert;
};
struct CoBoClockSettings {
    std::uint32_t hardwareVersion = 2;
    unsigned int asadMask = 0;
    bool verifyHardwareVersion = true, skipPllConfig = false;
    bool allowNominalDifference = false, allowUnverifiedUpstream = false;
    float writingMHz = 100, readingMHz = 25;
    std::map<std::string, std::string> pll;
    std::vector<AsAdBoardClockSettings> boards;
};
struct AsAdBoardClockImage {
    unsigned int asadId = 0;
    AsAdClockImage sampling, reading;
    double effectiveNominalSamplingMHz = 0, effectiveNominalReadingMHz = 0, effectiveNominalAdcMHz = 0;
};
struct CoBoClockImage {
    unsigned int asadMask = 0, writingDivider = 0, readingDivider = 2;
    bool programPll = false, programWritingDivider = false, programReadingDivider = false;
    // 仅表明旧模型可计算，绝不是硬件测量；未知模型时不要使用预测值作部署判据。
    bool nominalUpstreamKnown = true, nominalTargetsMatched = true, nominalOutputsWithinRange = true;
    bool nominalKnownDifference = false; // 已知模型中的偏差；未知输出单独表示。
    double nominalCoBoWritingMHz = 0, nominalCoBoReadingMHz = 25;
    CoBoPllImage pll;
    std::vector<AsAdBoardClockImage> boards;
};
// 专家模式必须提供全部已定义字段；不允许以缺项/截断默默改变 PLL 参数。
typedef std::map<std::string, unsigned int> AsAdClockExpertFields;
AsAdClockImage CompileAsAdSamplingClock(float ckwMHz, float samplingDivider = 1,
                                       const AsAdClockExpertFields &expert = AsAdClockExpertFields());
AsAdClockImage CompileAsAdReadingClock(float ckrMHz, int phaseDegrees, int delayPs,
                                      const AsAdClockExpertFields &expert = AsAdClockExpertFields());
CoBoPllImage CompileCoBoPll(float writingMHz, const std::map<unsigned int, std::uint32_t> &overrides = std::map<unsigned int, std::uint32_t>());
unsigned int CompileReducedCoBoWritingDivider(float writingMHz);
float ParseClockReal(const std::string &text, const std::string &name, double minimum, double maximum);
CoBoClockImage CompileCoBoClock(const CoBoClockSettings &settings);
}
#endif
