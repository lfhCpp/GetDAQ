// Register mapping adapted from CEA GET 2.3.0 CoBoConfigParser.cpp/CoBoNode.cpp.
// Original work © CEA, contributors Patrick Sizun et al., governed by CeCILL.
// Retained license: third_party/licenses/getbench/COPYING; source remains in source/get/.
#include "agetconfiguration.h"
#include <algorithm>
#include <initializer_list>
#include <set>
#include <stdexcept>

namespace GetDAQ {
namespace {
typedef std::map<std::string, std::string> Parameters;
void Bad(const std::string &name, const std::string &reason)
{
    throw std::invalid_argument(name + ": " + reason);
}
std::string Value(const Parameters &values, const std::string &name)
{
    const auto found = values.find(name);
    if (found == values.end()) Bad(name, "missing parameter");
    if (found->second.empty() || found->second.size() > 128) Bad(name, "empty or oversized value");
    return found->second;
}
unsigned int Number(const Parameters &values, const std::string &name, unsigned int maximum)
{
    const std::string text = Value(values, name); unsigned int result = 0;
    for (char c : text) {
        if (c < '0' || c > '9') Bad(name, "expected unsigned decimal integer");
        const unsigned int digit = static_cast<unsigned int>(c - '0');
        if (digit > maximum || result > (maximum - digit) / 10) Bad(name, "value exceeds " + std::to_string(maximum));
        result = result * 10 + digit;
    }
    return result;
}
bool Boolean(const Parameters &values, const std::string &name)
{
    const std::string text = Value(values, name);
    if (text == "true") return true;
    if (text == "false") return false;
    Bad(name, "expected true or false"); return false;
}
unsigned int Choice(const Parameters &values, const std::string &name, std::initializer_list<const char *> choices)
{
    const std::string text = Value(values, name); unsigned int index = 0;
    for (const char *choice : choices) { if (text == choice) return index; ++index; }
    Bad(name, "unsupported value '" + text + "'"); return 0;
}
unsigned int PowerIndex(const Parameters &values, const std::string &name)
{
    const unsigned int number = Number(values, name, 8);
    if (number == 1) return 0;
    if (number == 2) return 1;
    if (number == 4) return 2;
    if (number == 8) return 3;
    Bad(name, "expected 1, 2, 4 or 8"); return 0;
}
unsigned int PeakingIndex(const Parameters &values, std::uint16_t version)
{
    const unsigned int aget[16] = {70, 117, 232, 280, 334, 383, 502, 541, 568, 632, 721, 760, 831, 870, 976, 1014};
    const unsigned int astre[16] = {68, 116, 180, 225, 343, 442, 541, 641, 739, 836, 943, 1039, 2034, 3985, 5770, 7700};
    const unsigned int value = Number(values, "Reg1/peackingTime", 7700);
    const unsigned int *times = version == 0x203 ? astre : aget;
    const unsigned int *found = std::find(times, times + 16, value);
    if (found == times + 16) Bad("Reg1/peackingTime", "not in this chip version's peaking-time table");
    return static_cast<unsigned int>(found - times);
}
unsigned int TestCapacitor(const Parameters &values)
{
    const std::string value = Value(values, "Reg1/TestModeRange");
    const char *units[4][4] = {{"120fF", "120 fF", "120fC", "120 fC"}, {"240fF", "240 fF", "240fC", "240 fC"},
                             {"1pF", "1 pF", "1pC", "1 pC"}, {"10pF", "10 pF", "10pC", "10 pC"}};
    for (unsigned int i = 0; i < 4; ++i) for (unsigned int j = 0; j < 4; ++j) if (value == units[i][j]) return i;
    Bad("Reg1/TestModeRange", "expected 120fF, 240fF, 1pF, 10pF or documented legacy fC/pC alias"); return 0;
}
void Set(std::uint64_t &value, unsigned int offset, unsigned int width, std::uint64_t field)
{
    if (!width || width > 64 || offset > 64 - width) Bad("bit field", "invalid layout");
    const std::uint64_t mask = width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1;
    if (field > mask) Bad("bit field", "overflow");
    value |= field << offset;
}
}
const std::vector<AgetParameter> &AgetParameters()
{
    static const std::vector<AgetParameter> parameters = {
        {"ReadoutPointerOffset"}, {"Reg1/isIcsax2"}, {"Reg1/TestModeRange"}, {"Reg1/peackingTime"},
        {"Reg1/TestModeSelection"}, {"Reg1/IntegrationMode"}, {"Reg1/SCA_Pointer"}, {"Reg1/SCA_Splitting"},
        {"Reg1/is32channels"}, {"Reg1/isAllChannelRead"}, {"Reg1/isFPNRead"}, {"Reg1/isPositivePolarity"},
        {"Reg1/Vicm"}, {"Reg1/GlobalThresholdValue"}, {"Reg1/isThresholdSignedPositive"},
        {"Reg1/TriggerVetoOption"}, {"Reg1/isckwriteSynchro"}, {"Reg1/isTOTActive"},
        {"Reg1/isTriggerWidth200ns"}, {"Reg1/TriggerWidthRange"}, {"Reg1/ExternalLink"},
        {"Reg2/DebugSelection"}, {"Reg2/isReadFromColumn0"}, {"Reg2/isDigitalOutputTest"},
        {"Reg2/isResetLevelUndefined"}, {"Reg2/isMarkerEnable"}, {"Reg2/isResetLevelVdd"},
        {"Reg2/isBoostPower"}, {"Reg2/isOutputscCkSynchro"}, {"Reg2/isSynchroEdgeFalling"},
        {"Reg2/isSCoutputBufferInTriState"}, {"Reg2/currentBias"}, {"Reg2/bufferCurrent"},
        {"Reg2/isPWDonWrite"}, {"Reg2/isPWDonRead"}, {"Reg2/isAlternatePower"},
        {"Reg2/is1bitShortResetReadSeq"}, {"Reg2/isTriggerOutputDisable"},
        {"Reg2/isRisingedgeWriteAutoresetBank"}, {"Reg2/islvdsTriggerOutput"},
        {"Reg2/inputDynamicRange_5pct", "false"}, {"Reg2/triggerLvdsThreshold", "1"}, {"Reg2/multiplicityLevel", "1"}
    };
    return parameters;
}
int AgetPhysicalChannel(unsigned int id)
{
    if (id > 67) Bad("channel", "ID must be 0..67");
    if (id == 11 || id == 22 || id == 45 || id == 56) return -1;
    return static_cast<int>(id) - (id > 11) - (id > 22) - (id > 45) - (id > 56);
}
AgetChannel ParseAgetChannel(unsigned int id, const Parameters &parameters)
{
    if (id > 67) Bad("channel", "ID must be 0..67");
    const std::set<std::string> names = {"isActive", "isSelectedforTestMode", "Gain", "LSBThresholdValue", "TriggerInhibition", "Reading"};
    for (const auto &p : parameters) if (!names.count(p.first)) Bad(p.first, "unknown channel parameter");
    AgetChannel channel; channel.id = id; channel.active = Boolean(parameters, "isActive");
    if (!channel.active) return channel;
    channel.selectedForTest = Boolean(parameters, "isSelectedforTestMode");
    channel.gain = Choice(parameters, "Gain", {"120fC", "240fC", "1pC", "10pC"});
    channel.threshold = Number(parameters, "LSBThresholdValue", 15);
    const std::string mode = Value(parameters, "TriggerInhibition");
    if (mode == "none") channel.inhibition = 0;
    else if (mode == "inhibit_channel" || mode == "inhibit_trigger_function") channel.inhibition = 1;
    else if (mode == "inhibit_trigger" || mode == "inhibit_trigger_data") channel.inhibition = 2;
    else Bad("TriggerInhibition", "unsupported value '" + mode + "'");
    channel.reading = static_cast<AgetChannel::Reading>(Choice(parameters, "Reading", {"never", "always", "only_if_hit"}));
    return channel;
}
AgetRegisterImage CompileAget(const AgetSettings &settings)
{
    if (settings.chipVersion != 0x201 && settings.chipVersion != 0x202 && settings.chipVersion != 0x203)
        Bad("chipVersion", "expected explicit 0x201, 0x202 or 0x203");
    Parameters values = settings.globals; std::set<std::string> names;
    for (const auto &p : AgetParameters()) {
        names.insert(p.name);
        if (!values.count(p.name) && !p.defaultValue.empty()) values[p.name] = p.defaultValue;
    }
    for (const auto &p : values) if (!names.count(p.first)) Bad(p.first, "unknown global parameter");
    if (settings.channels.size() != 68) Bad("channels", "must declare all 68 channels, including FPN");
    std::set<unsigned int> channelIds;
    for (const auto &c : settings.channels) {
        if (c.id > 67 || !channelIds.insert(c.id).second) Bad("channels", "duplicate or out-of-range channel ID");
        if (c.gain > 3 || c.threshold > 15 || c.inhibition > 2 || c.reading < AgetChannel::Never || c.reading > AgetChannel::OnlyIfHit)
            Bad("channel[" + std::to_string(c.id) + "]", "invalid gain, threshold, inhibition or reading");
    }
    if (Boolean(values, "Reg1/SCA_Splitting") != settings.twoPulseMode)
        Bad("Reg1/SCA_Splitting", "must agree with Module/enableMem2pMode");
    const unsigned int capacity = settings.twoPulseMode ? 256 : 512;
    const unsigned int offset = Number(values, "ReadoutPointerOffset", capacity - 1);
    const int depth = settings.readoutDepth == -1 ? static_cast<int>(capacity) : settings.readoutDepth;
    if (depth < 1 || depth > static_cast<int>(capacity - offset))
        Bad("Module/readoutDepth", "must be positive and fit within SCA capacity after pointer offset");
    std::uint64_t reg1 = 0, reg2 = 0;
    Set(reg1, 0, 1, Boolean(values, "Reg1/isIcsax2"));
    Set(reg1, 1, 2, TestCapacitor(values));
    Set(reg1, 3, 4, PeakingIndex(values, settings.chipVersion));
    Set(reg1, 7, 2, Choice(values, "Reg1/TestModeSelection", {"nothing", "calibration", "test", "functionality"}));
    Set(reg1, 9, 1, Choice(values, "Reg1/IntegrationMode", {"variable", "fixed_500ns"}));
    Set(reg1, 10, 2, Choice(values, "Reg1/SCA_Pointer", {"column_0", "col_0or256", "col_0or128or256or384", "anySCAPointer"}));
    const char *reg1Booleans[] = {"SCA_Splitting", "is32channels", "isAllChannelRead", "isFPNRead", "isPositivePolarity"};
    for (unsigned int i = 0; i < 5; ++i) Set(reg1, 12 + i, 1, Boolean(values, std::string("Reg1/") + reg1Booleans[i]));
    Set(reg1, 17, 2, Choice(values, "Reg1/Vicm", {"1.25V", "1.35V", "1.55V", "1.65V"}));
    Set(reg1, 19, 3, Number(values, "Reg1/GlobalThresholdValue", 7));
    Set(reg1, 22, 1, Boolean(values, "Reg1/isThresholdSignedPositive"));
    // 原配置模板的 Undefined 大写与旧解析器 lowercase 不一致；保留 lowercase 的精确含义。
    Set(reg1, 23, 2, Choice(values, "Reg1/TriggerVetoOption", {"none", "4us", "HitRegisterWidth", "undefined"}));
    Set(reg1, 25, 1, Boolean(values, "Reg1/isckwriteSynchro"));
    Set(reg1, 26, 1, Boolean(values, "Reg1/isTOTActive"));
    Set(reg1, 27, 1, Boolean(values, "Reg1/isTriggerWidth200ns"));
    Set(reg1, 28, 2, Choice(values, "Reg1/TriggerWidthRange", {"width0", "width1", "width2", "width3"}));
    Set(reg1, 30, 2, Choice(values, "Reg1/ExternalLink", {"none", "SKFilterInput", "Gain-2Input", "CSAStandby"}));
    Set(reg2, 0, 3, Choice(values, "Reg2/DebugSelection", {"standby", "CSA", "CR", "Gain-2", "PositiveInputDiscri", "NegativeInputDiscri", "trigger", "none"}));
    const char *reg2LowBooleans[] = {"isReadFromColumn0", "isDigitalOutputTest", "isResetLevelUndefined", "isMarkerEnable", "isResetLevelVdd", "isBoostPower", "isOutputscCkSynchro", "isSynchroEdgeFalling", "isSCoutputBufferInTriState"};
    for (unsigned int i = 0; i < 9; ++i) Set(reg2, 3 + i, 1, Boolean(values, std::string("Reg2/") + reg2LowBooleans[i]));
    Set(reg2, 12, 2, Choice(values, "Reg2/currentBias", {"current0", "current1", "current2", "current3"}));
    Set(reg2, 14, 2, Choice(values, "Reg2/bufferCurrent", {"1.503mA", "1.914mA", "2.7mA", "4.870mA"}));
    const char *reg2HighBooleans[] = {"isPWDonWrite", "isPWDonRead", "isAlternatePower", "is1bitShortResetReadSeq", "isTriggerOutputDisable", "isRisingedgeWriteAutoresetBank", "islvdsTriggerOutput"};
    for (unsigned int i = 0; i < 7; ++i) Set(reg2, 16 + i, 1, Boolean(values, std::string("Reg2/") + reg2HighBooleans[i]));
    const bool dynamicRange = Boolean(values, "Reg2/inputDynamicRange_5pct");
    if (settings.chipVersion == 0x201 && dynamicRange) Bad("Reg2/inputDynamicRange_5pct", "not available on AGET 2.1");
    Set(reg2, 24, 1, dynamicRange);
    const unsigned int lvds = PowerIndex(values, "Reg2/triggerLvdsThreshold"), mult = PowerIndex(values, "Reg2/multiplicityLevel");
    if (settings.chipVersion != 0x203 && (lvds || mult)) Bad("Reg2", "non-default multiplicity levels require ASTRE 0x203");
    Set(reg2, 25, 2, lvds); Set(reg2, 27, 2, mult);
    AgetRegisterImage result; result.chipVersion = settings.chipVersion; result.twoPulseMode = settings.twoPulseMode;
    result.registerStageDepth = capacity - offset; result.moduleReadoutDepth = static_cast<unsigned int>(depth);
    const char *regNames[13] = {"reg1", "reg2", "reg3", "reg4", "reg6", "reg7", "reg8", "reg8msb", "reg9", "reg9msb", "reg10", "reg11", "reg12"};
    const unsigned int regWidths[13] = {32, 32, 34, 34, 64, 64, 64, 64, 64, 64, 64, 64, 9};
    std::uint64_t registers[13] = {reg1, reg2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, offset};
    for (const auto &c : settings.channels) {
        if (!c.active) continue;
        if (c.selectedForTest) Set(registers[c.id <= 33 ? 2 : 3], c.id <= 33 ? 33 - c.id : c.id - 34, 1, 1);
        const int physical = AgetPhysicalChannel(c.id);
        if (physical >= 0) {
            const unsigned int p = static_cast<unsigned int>(physical);
            Set(registers[p < 32 ? 4 : 5], p < 32 ? 62 - 2 * p : 2 * (p - 32), 2, c.gain);
            Set(registers[p < 32 ? 10 : 11], p < 32 ? 62 - 2 * p : 2 * (p - 32), 2, c.inhibition);
            const unsigned int thresholdRegister = p < 16 ? 7 : p < 32 ? 6 : p < 48 ? 8 : 9;
            const unsigned int shift = p < 32 ? 60 - 4 * (p % 16) : 4 * (p % 16);
            Set(registers[thresholdRegister], shift, 4, c.threshold);
        }
        const unsigned int group = c.id < 4 ? 0 : c.id < 36 ? 1 : 2;
        const unsigned int shift = (group == 0 ? 3 : group == 1 ? 35 : 67) - c.id;
        if (c.reading == AgetChannel::Always) result.readAlways[group] |= UINT32_C(1) << shift;
        if (c.reading == AgetChannel::OnlyIfHit) result.readIfHit[group] |= UINT32_C(1) << shift;
    }
    for (unsigned int i = 0; i < 13; ++i) result.registers.push_back(AgetRegisterValue(regNames[i], regWidths[i], registers[i]));
    return result;
}
}
