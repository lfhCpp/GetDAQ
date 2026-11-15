#ifndef GETDAQ_MUTANTCONFIGURATION_H
#define GETDAQ_MUTANTCONFIGURATION_H
#include <array>
#include <cstdint>
#include <string>
namespace GetDAQ {
struct MutantSettings {
    enum RegisterLayout : unsigned int { ModernNamesV15, LegacyNamesV15 };
    enum ClockPolicy : unsigned int { StrictClockNames, LegacyVhdciFallback };
    enum CentrumScalerMode : unsigned int { CsDisabled, Centrum, Scalers };
    RegisterLayout layout = ModernNamesV15;
    ClockPolicy clockPolicy = StrictClockNames;
    CentrumScalerMode centrumScalerMode = CsDisabled;
    unsigned int moduleCount = 1, slaveId = 0;
    bool isSlave = false, slavePortA = true, masterPortAEnabled = true, masterPortBEnabled = true;
    bool level0Enabled = true, level1Enabled = true, level2Enabled = true, hitPatternModified = true;
    bool level1BeforeLevel0 = false, twoProtonMode = false, halfEventReadout = false;
    std::string level1Selection = "LOW";
    double level0GateDelayUs = 0, level0GateDurationUs = 0.01, level1PostTriggerDelayUs = 0;
    double level1bTimeoutUs = 2.56;
    std::array<unsigned int,4> multiplicityThresholds{{1,65535,0,65535}}; // A low/high, B low/high.
    std::array<unsigned int,4> bucketCounts{{511,511,511,511}};
    bool preloadTimestamp = true, preloadEventNumber = true, clearScalers = true, clearCounters = true;
    std::uint64_t timestamp10ns = 0, eventNumber = 0;
    std::array<bool,5> scalerEnabled{{true,true,true,true,true}};
    unsigned int vcxoControl = 0x80, sourceId = 0, frameType = 0, amcTagOnly = 0, userRegister = 0;
    bool readoutEnabled = true, testMode = false, externalClockEnabled = false;
    std::string externalClockSelection = "VHDCI";
    std::array<unsigned int,4> inspection{{0,0,0,0}}; // front1/2, rear1/2.
    // 由已审阅的拓扑/对齐阶段提供，不是计算器证明已完成真实相位对齐。
    unsigned int gmcShelfMask = 0, wscaShelfMask = 0;
    bool twoProtonCounterCommand = true, allowPrototypePowerDf = false;
    bool verifyBoardId = false;
    unsigned int expectedBoardId = 0;
};
struct MutantImage {
    unsigned int globalConfiguration = 0, wscaSource = 0, externalClockSource = 2;
    bool promptWsca = false, externalClockSourceFallback = false;
    unsigned int level0GateDelay = 0, level0GateDuration = 1, level1PostTriggerDelay = 0, level1bTimeout = 1;
    std::array<unsigned int,4> multiplicityThresholds{{0,0,0,0}}, bucketCounts{{0,0,0,0}};
    std::array<unsigned int,3> timestampWords{{0,0,0}};
    std::array<unsigned int,2> eventWords{{0,0}};
    unsigned int scalerMask = 0, shelfMask = 0, clockPathValue = 0, frontInspection = 0, rearInspection = 0;
    const char *shelfRegister = "SHELF_MASK", *tagRegister = "AMC_TAG_ONLY", *userRegister = "USER_REG";
};
unsigned int MutantTenNsCode(double microseconds, bool nonzeroDuration = false);
unsigned int MutantL1bTimeoutCode(double microseconds);
MutantImage CompileMutant(const MutantSettings &settings);
}
#endif
