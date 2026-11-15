// Adapted from CEA GET MutantConfigParser/MutantNode (received 2021 release).
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "mutantconfiguration.h"
#include <cmath>
#include <stdexcept>
namespace GetDAQ {
namespace {
void Limit(unsigned int value, unsigned int maximum, const char *name)
{
    if (value > maximum) throw std::invalid_argument(std::string("MuTaNT ")+name+" exceeds hardware width");
}
}
unsigned int MutantTenNsCode(double us, bool nonzeroDuration)
{
    if (!std::isfinite(us) || us < (nonzeroDuration ? 0.01 : 0.0) || us > 655.35)
        throw std::invalid_argument("MuTaNT 10ns timing must be finite and within hardware range");
    // 保留旧解析器float中间量；拒绝越界后再转换，不默默钳制用户输入。
    const auto raw = std::floor(100.0f*static_cast<float>(us));
    if (raw > 65535 || raw < (nonzeroDuration ? 1 : 0)) throw std::invalid_argument("MuTaNT timing quantization exceeds hardware range");
    return static_cast<unsigned int>(raw);
}
unsigned int MutantL1bTimeoutCode(double us)
{
    // 旧实现先赋值uint16再钳制；不能以窄化后的值检测溢出。
    if (!std::isfinite(us) || us < 0 || us > 65535*2.56)
        throw std::invalid_argument("MuTaNT L1B timeout must be finite and within 0..167769.6 us");
    const auto raw = std::floor(static_cast<float>(us)/2.56+0.5);
    if (raw < 0 || raw > 65535) throw std::invalid_argument("MuTaNT L1B timeout quantization exceeds hardware range");
    return static_cast<unsigned int>(raw);
}
MutantImage CompileMutant(const MutantSettings &s)
{
    if (s.layout > MutantSettings::LegacyNamesV15 || s.clockPolicy > MutantSettings::LegacyVhdciFallback || s.centrumScalerMode > MutantSettings::Scalers)
        throw std::invalid_argument("Invalid MuTaNT layout/clock/interface policy");
    if (!s.moduleCount || s.moduleCount > 4096) throw std::invalid_argument("MuTaNT module count must be 1..4096");
    if (s.isSlave && (s.level0Enabled || s.level1Enabled || s.level2Enabled)) throw std::invalid_argument("MuTaNT slave cannot enable trigger levels");
    if (s.isSlave && s.centrumScalerMode == MutantSettings::Centrum) throw std::invalid_argument("CENTRUM coupling is allowed only on MuTaNT master");
    if (s.isSlave) Limit(s.slaveId,1,"slave ID");
    Limit(s.gmcShelfMask,4095,"GMC shelf mask"); Limit(s.wscaShelfMask,4095,"WSCA shelf mask");
    Limit(s.amcTagOnly,4095,"AMC tag mask"); Limit(s.userRegister,65535,"user register");
    Limit(s.sourceId,255,"source ID"); Limit(s.frameType,3,"frame type"); Limit(s.vcxoControl,255,"VCXO control");
    if (s.frameType == 3) throw std::invalid_argument("MuTaNT frame type 3 is reserved");
    Limit(s.expectedBoardId,0xffffff,"board ID");
    if (s.timestamp10ns > 0xffffffffffffULL || s.eventNumber > 0xffffffffULL) throw std::invalid_argument("MuTaNT preload exceeds 48/32-bit counter width");
    MutantImage image;
    const char *selections[] = {"LOW","HIGH","OR","AND"}; unsigned int level1Selection = 4;
    for (unsigned int i = 0; i < 4; ++i) if (s.level1Selection == selections[i]) level1Selection = i;
    if (level1Selection == 4) throw std::invalid_argument("MuTaNT L1 selection must be LOW, HIGH, OR or AND (v1.5 mapping)");
    const char *clocks[] = {"FireWire1","FireWire2","VHDCI","Optical"};
    const char *aliases[] = {"FireWireA","FireWireB","VHDCI","Optical"}; bool found = false;
    for (unsigned int i = 0; i < 4; ++i) if (s.externalClockSelection == clocks[i] || s.externalClockSelection == aliases[i]) {
        image.externalClockSource = i; found = true;
    }
    if (!found) {
        if (s.clockPolicy == MutantSettings::StrictClockNames) throw std::invalid_argument("Unknown MuTaNT external clock source");
        if (s.externalClockSelection.empty() || s.externalClockSelection.size() > 128) throw std::invalid_argument("Invalid MuTaNT clock source text");
        image.externalClockSourceFallback = true;
    }
    const bool portA = s.isSlave ? s.slavePortA : s.masterPortAEnabled;
    const bool portB = s.isSlave ? !s.slavePortA : s.masterPortBEnabled;
    image.globalConfiguration = (!s.isSlave ? 1U : 0U) | ((s.isSlave ? s.slaveId : 0U)<<1) | (unsigned(portA)<<2) | (unsigned(portB)<<3) |
        (unsigned(s.level0Enabled)<<4) | (unsigned(s.level1Enabled)<<5) | (unsigned(s.level2Enabled)<<6) |
        (unsigned(s.level2Enabled && s.hitPatternModified)<<7) | (unsigned(s.level1BeforeLevel0)<<8) | (level1Selection<<9) |
        (unsigned(s.twoProtonMode)<<11) | (unsigned(s.twoProtonMode && s.halfEventReadout)<<12);
    if (s.centrumScalerMode != MutantSettings::CsDisabled) image.globalConfiguration |= 1U<<14;
    if (s.centrumScalerMode == MutantSettings::Centrum) image.globalConfiguration |= 1U<<15;
    image.wscaSource = s.isSlave ? (s.slavePortA ? 1U : 2U) : 0U;
    image.promptWsca = !s.isSlave && s.moduleCount >= 2;
    image.clockPathValue = (s.isSlave || s.externalClockEnabled) ? 5U : 0U; // 只更新clk_path_sel的位0、2。
    image.level0GateDelay = MutantTenNsCode(s.level0GateDelayUs);
    image.level0GateDuration = MutantTenNsCode(s.level0GateDurationUs,true);
    image.level1PostTriggerDelay = MutantTenNsCode(s.level1PostTriggerDelayUs);
    image.level1bTimeout = s.twoProtonMode ? MutantL1bTimeoutCode(s.level1bTimeoutUs) : 1;
    image.multiplicityThresholds = s.multiplicityThresholds; image.bucketCounts = s.bucketCounts;
    if (!s.twoProtonMode) { image.multiplicityThresholds[2] = 0; image.multiplicityThresholds[3] = 65535; image.bucketCounts[2] = image.bucketCounts[3] = 511; }
    for (unsigned int i = 0; i < 4; ++i) {
        Limit(image.multiplicityThresholds[i],65535,"multiplicity threshold");
        if (!image.bucketCounts[i] || image.bucketCounts[i] > 511) throw std::invalid_argument("MuTaNT bucket count must be 1..511");
        Limit(s.inspection[i],255,"inspection code");
    }
    for (unsigned int i = 0; i < 3; ++i) image.timestampWords[i] = unsigned((s.timestamp10ns>>(16*i)) & 65535);
    for (unsigned int i = 0; i < 2; ++i) image.eventWords[i] = unsigned((s.eventNumber>>(16*i)) & 65535);
    for (unsigned int i = 0; i < 5; ++i) if (s.scalerEnabled[i]) image.scalerMask |= 1U<<i;
    image.shelfMask = s.gmcShelfMask & s.wscaShelfMask;
    image.frontInspection = s.inspection[0] | (s.inspection[1]<<8); image.rearInspection = s.inspection[2] | (s.inspection[3]<<8);
    if (s.layout == MutantSettings::LegacyNamesV15) { image.shelfRegister = "MASTER_MASK"; image.tagRegister = "USER_REG1"; image.userRegister = "USER_REG2"; }
    return image;
}
}
