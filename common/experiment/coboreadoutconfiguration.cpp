// Adapted from CEA GET 2.3.0 CoBoNode Configure/readout/trigger/buffer routines.
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "coboreadoutconfiguration.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace GetDAQ {
const std::vector<CoBoParameter> &CoBoModuleParameters()
{
    static const std::vector<CoBoParameter> parameters = {
        {"enableWriteHittedregister", "", "", false}, {"isAcqTestMode", "", "", false},
        {"enableMem2pMode", "", "", false}, {"multiplicityThreshold", "", "", false},
        {"multWindowSize", "", "", false}, {"multiplicityThreshold_2p", "", "", false},
        {"multSubtractAverage", "false", "", true}, {"multSubtractDevs", "0", "", true},
        {"dataSource", "0", "", true}, {"coboId", "0", "", true},
        {"triggerPeriod_10ns", "", "10ns", true}, {"triggerPeriod", "", "ms", true},
        {"triggerDelay", "", "10ns", false}, {"triggerDeadTime_2p", "", "10ns", false},
        {"triggerDelay_2p", "", "10ns", false}, {"triggerTime_2p", "", "10ns", false},
        {"triggerTimeOut_2p", "", "10ns", false}, {"triggerMode", "", "", false},
        {"secondaryTriggerMode", "", "", true}, {"externalTriggerMode", "", "", true},
        {"scwMultDelay", "200", "cycles", true}, {"scwScrDelay", "", "10ns", true},
        {"writeReadDelay", "0", "10ns", true}, {"enableZeroSuppression", "false", "", true},
        {"zeroSuppressionInverted", "false", "", true}, {"useExternalTimeReset", "false", "", true}
    };
    return parameters;
}
const std::vector<CoBoParameter> &CoBoBufferParameters()
{
    static const std::vector<CoBoParameter> parameters = {
        {"startOfMemory", "", "", false}, {"endOfMemory", "", "", false},
        {"interruptRate", "1", "", true}, {"levelAlmostFullMemory", "", "B", false},
        {"levelAlmostEmptyMemory", "", "B", false}
    };
    return parameters;
}
namespace {
std::string Value(const std::map<std::string, std::string> &values, const std::string &name,
                  const std::string &fallback = std::string())
{
    const auto found = values.find(name);
    if (found != values.end()) return found->second;
    if (!fallback.empty()) return fallback;
    throw std::invalid_argument("Missing CoBo parameter: " + name);
}
std::uint32_t Unsigned(const std::string &text, const std::string &name, std::uint32_t maximum = UINT32_MAX)
{
    std::size_t index = 0; unsigned int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) { base = 16; index = 2; }
    if (text.empty()) throw std::invalid_argument(name + ": expected unsigned decimal or hexadecimal integer");
    std::uint32_t result = 0;
    for (; index < text.size(); ++index) {
        const char c = text[index];
        const unsigned int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 16;
        if (digit >= base || digit > maximum || result > (maximum - digit) / base)
            throw std::invalid_argument(name + ": invalid or out-of-range unsigned integer");
        result = result * base + digit;
    }
    return result;
}
bool Boolean(const std::string &text, const std::string &name)
{
    if (text == "true") return true;
    if (text == "false") return false;
    throw std::invalid_argument(name + ": expected true or false");
}
class Parameters {
public:
    explicit Parameters(const std::map<std::string, std::string> &values) : fValues(values) {}
    std::string Get(const std::string &name, const std::string &fallback = std::string()) const { return Value(fValues, name, fallback); }
    std::uint32_t Number(const std::string &name, const std::string &fallback = std::string(), std::uint32_t maximum = UINT32_MAX) const {
        return Unsigned(Get(name, fallback), name, maximum);
    }
    bool Bool(const std::string &name, const std::string &fallback = std::string()) const { return Boolean(Get(name, fallback), name); }
private:
    const std::map<std::string, std::string> &fValues;
};
int Trigger(const std::string &name, bool secondary, bool twoPulseMode)
{
    if (name == "noTrigger") return -1;
    const char *modes[] = {"external", "onRequest", "periodically", "onMultiplicity", "onSlowControl"};
    for (int id = 0; id < 5; ++id) {
        if (name == modes[id]) return id;
        if (!secondary && name == std::string(modes[id]) + "_2p") {
            if (!twoPulseMode) throw std::invalid_argument("2p triggerMode requires enableMem2pMode=true");
            return id + 8;
        }
    }
    throw std::invalid_argument("Invalid " + std::string(secondary ? "secondaryTriggerMode" : "triggerMode") + ": " + name);
}
void Known(const std::map<std::string, std::string> &values, const std::vector<CoBoParameter> &parameters)
{
    for (const auto &entry : values)
        if (std::none_of(parameters.begin(), parameters.end(), [&entry](const CoBoParameter &p) { return p.name == entry.first; }))
            throw std::invalid_argument("Unknown CoBo parameter: " + entry.first);
}
}
std::uint32_t ParseCoBoUnsigned(const std::string &text, const std::string &name, std::uint32_t maximum)
{
    return Unsigned(text, name, maximum);
}
CoBoReadoutImage CompileCoBoReadout(const CoBoReadoutSettings &settings)
{
    Known(settings.module, CoBoModuleParameters()); Known(settings.circularBuffer, CoBoBufferParameters());
    CoBoReadoutImage result; result.aget = CompileCoBoAget(settings.aget);
    const Parameters module(settings.module), circular(settings.circularBuffer);
    if (module.Bool("enableMem2pMode") != result.aget.twoPulseMode) throw std::invalid_argument("Module enableMem2pMode differs from AGET settings");
    result.fullReadout = (result.aget.chips.front().image.registers.front().value & UINT64_C(0x4000)) != 0;
    for (const auto &chip : result.aget.chips)
        if (((chip.image.registers.front().value & UINT64_C(0x4000)) != 0) != result.fullReadout)
            throw std::invalid_argument("All active AGET chips must share isAllChannelRead (full/partial readout)");
    if (settings.chips.size() != result.aget.chips.size()) throw std::invalid_argument("Readout settings must cover exactly all active chips");
    unsigned int chipMask = 0; int position[4] = {-1,-1,-1,-1};
    auto chips = settings.chips;
    std::sort(chips.begin(), chips.end(), [](const CoBoChipReadout &a, const CoBoChipReadout &b) { return 4*a.asadId+a.agetId < 4*b.asadId+b.agetId; });
    const auto write = [&result](const std::string &device, const std::string &reg, const std::string &field, std::uint64_t v) {
        CoBoRegisterValue entry; entry.device = device; entry.reg = reg; entry.field = field; entry.value = v; result.writes.push_back(entry);
    };
    const auto ctrl = [&write](const std::string &reg, const std::string &field, std::uint64_t v) { write("ctrl", reg, field, v); };
    for (const auto &chip : chips) {
        if (chip.asadId > 3 || chip.agetId > 3) throw std::invalid_argument("Invalid CoBo readout chip ID");
        const unsigned int bit = 1U << (4*chip.asadId+chip.agetId);
        if (!(result.aget.agetMask & bit) || (chipMask & bit)) throw std::invalid_argument("Duplicate or inactive CoBo readout chip");
        chipMask |= bit;
        if (position[chip.agetId] >= 0 && position[chip.agetId] != int(chip.suppressMultiplicity))
            throw std::invalid_argument("suppressMultiplicity must agree across active AsAd chips at the same AGET position");
        position[chip.agetId] = chip.suppressMultiplicity;
        const std::string device = chip.asadId == 0 ? "zeroSuppress" : "zeroSuppress_AsAd" + std::to_string(chip.asadId);
        for (unsigned int id = 0; id < 68; ++id) {
            if (chip.zeroSuppressionThresholds[id] > 4095) throw std::invalid_argument("zeroSuppressionThreshold must be 0..4095");
            write(device, "aget" + std::to_string(chip.agetId) + "ch" + std::to_string(id), "threshold", chip.zeroSuppressionThresholds[id]);
        }
    }
    if (chipMask != result.aget.agetMask) throw std::invalid_argument("Readout settings must cover exactly all active chips");
    for (unsigned int id = 0; id < 4; ++id) if (position[id] != 0) result.suppressMultiplicity |= 1U << id;
    ctrl("pipeCtrl", "isFPNRead", result.aget.fpnRead);
    ctrl("pipeCtrl", "writeHitRegisterMaskEnable", module.Bool("enableWriteHittedregister"));
    ctrl("pipeCtrl", "testMode", module.Bool("isAcqTestMode"));
    ctrl("multThreshold", "", module.Number("multiplicityThreshold")); ctrl("multWindSize", "", module.Number("multWindowSize"));
    ctrl("multThreshold_2p", "", module.Number("multiplicityThreshold_2p"));
    ctrl("pipeCtrl", "MultSubtractAverage", module.Bool("multSubtractAverage", "false"));
    ctrl("pipeCtrl", "MultSubtractDevs", module.Number("multSubtractDevs", "0", 7));
    ctrl("pipeCtrl", "suppressMultiplicity", result.suppressMultiplicity);
    ctrl("pipeCtrl", "modeEnable_2p", result.aget.twoPulseMode);
    ctrl("dataSource", "value", module.Number("dataSource", "0", 255)); ctrl("COBO_index", "d", module.Number("coboId", "0", 255));
    const auto explicitPeriod = settings.module.find("triggerPeriod_10ns");
    const auto period = explicitPeriod != settings.module.end() ? Unsigned(explicitPeriod->second, "triggerPeriod_10ns")
        : module.Number("triggerPeriod", "", UINT32_MAX / 100000) * 100000U;
    ctrl("triggerPeriod_10ns", "", period);
    ctrl("triggerDelay", "", module.Number("triggerDelay")); ctrl("triggerWindowStart_2p", "", module.Number("triggerDeadTime_2p"));
    ctrl("triggerDelay_2p", "", module.Number("triggerDelay_2p")); ctrl("triggerTime_10ns_2p", "", module.Number("triggerTime_2p"));
    ctrl("triggerWindowEnd_2p", "", module.Number("triggerTimeOut_2p"));
    const std::uint32_t start = circular.Number("startOfMemory"), end = circular.Number("endOfMemory");
    if (start >= end) throw std::invalid_argument("CircularBuffer startOfMemory must be less than endOfMemory");
    unsigned int defined = 0;
    for (const auto &range : settings.buffers) {
        if (range.start.empty() != range.end.empty()) throw std::invalid_argument("CircularBuffer explicit range needs both start and end");
        if (!range.start.empty()) ++defined;
    }
    if (defined && defined != 4) throw std::invalid_argument("Provide all four explicit AsAd buffer ranges or none");
    std::uint32_t cursor = start;
    unsigned int count = 0; for (unsigned int id = 0; id < 4; ++id) if (result.aget.asadMask & (1U << id)) ++count;
    const std::uint32_t size = (end - start) / (count == 3 ? 4 : count);
    std::uint32_t minimum = UINT32_MAX;
    for (unsigned int id = 0; id < 4; ++id) {
        const bool active = (result.aget.asadMask & (1U << id)) != 0;
        result.bufferStart[id] = defined ? Unsigned(settings.buffers[id].start, "AsAd buffer start") : cursor;
        result.bufferEnd[id] = defined ? Unsigned(settings.buffers[id].end, "AsAd buffer end") : cursor + (active ? size : 0);
        cursor = result.bufferEnd[id];
        const auto begin = result.bufferStart[id], finish = result.bufferEnd[id];
        if (begin < start || finish > end || begin > finish || (active && begin == finish)) throw std::invalid_argument("Invalid or empty active AsAd buffer range");
        if (active) minimum = std::min(minimum, finish - begin);
        if (active) for (unsigned int other = 0; other < id; ++other)
            if ((result.aget.asadMask & (1U << other)) && begin < result.bufferEnd[other] && result.bufferStart[other] < finish)
                throw std::invalid_argument("Active AsAd circular buffers overlap");
    }
    if (settings.bufferLayout == CoBoReadoutSettings::PerAsAdBuffers) {
        for (unsigned int id = 0; id < 4; ++id) {
            ctrl("memStart_asad" + std::to_string(id), "", result.bufferStart[id]);
            ctrl("memEnd_asad" + std::to_string(id), "", result.bufferEnd[id]);
            ctrl("readPtr_asad" + std::to_string(id), "", result.bufferStart[id]);
        }
    } else if (settings.bufferLayout == CoBoReadoutSettings::SharedBuffer) {
        ctrl("memStart", "", start); ctrl("memEnd", "", end); ctrl("readPtr", "", start); minimum = end - start;
    } else throw std::invalid_argument("Unknown CoBo buffer layout");
    const auto full = circular.Number("levelAlmostFullMemory"), empty = circular.Number("levelAlmostEmptyMemory");
    if (full >= minimum || empty < full || empty > minimum) throw std::invalid_argument("CircularBuffer levels must satisfy full < capacity and full <= empty <= capacity");
    const auto rate = circular.Number("interruptRate", "1"); if (!rate) throw std::invalid_argument("CircularBuffer interruptRate must be positive");
    ctrl("interruptRate", "", rate); ctrl("aFullLevel", "", full); ctrl("aEmptyLevel", "", empty);
    const int primary = Trigger(module.Get("triggerMode"), false, result.aget.twoPulseMode);
    const std::string secondary = settings.module.count("secondaryTriggerMode") ? module.Get("secondaryTriggerMode")
        : settings.module.count("externalTriggerMode") ? module.Get("externalTriggerMode") : module.Get("triggerMode");
    // 旧缺省可继承 primary 的 _2p 字符串，但 secondary 没有对应位：沿用不启用行为。
    const int output = secondary == module.Get("triggerMode") && primary >= 8 && !settings.module.count("secondaryTriggerMode")
        && !settings.module.count("externalTriggerMode") ? -1 : Trigger(secondary, true, result.aget.twoPulseMode);
    const char *fields[] = {"external", "manual", "timing", "multiplicity", "slowControl"};
    for (unsigned int group = 0; group < 3; ++group)
        for (unsigned int id = 0; id < 5; ++id) ctrl("triggerMask", std::string(fields[id]) + (group == 1 ? "_2p" : group == 2 ? "_out" : ""), 0);
    if (primary >= 0) {
        const unsigned int id = primary >= 8 ? primary - 8 : primary;
        ctrl("triggerMask", fields[id], 1); result.triggerMask |= 1U << id;
        if (primary >= 8) { ctrl("triggerMask", std::string(fields[id]) + "_2p", 1); result.triggerMask |= 1U << (id + 8); }
    }
    if (output >= 0) { ctrl("triggerMask", std::string(fields[output]) + "_out", 1); result.triggerMask |= 1U << (output + 16); }
    ctrl("ScwMultDelay", "", module.Number("scwMultDelay", "200"));
    ctrl("ScwScrDelay", "", settings.module.count("scwScrDelay") ? module.Number("scwScrDelay") : module.Number("writeReadDelay", "0"));
    ctrl("pipeCtrl", "partialReadOut", !result.fullReadout);
    ctrl("pipeCtrl", "zeroSuppressionEnable", module.Bool("enableZeroSuppression", "false"));
    ctrl("pipeCtrl", "zeroSuppressInvert", module.Bool("zeroSuppressionInverted", "false"));
    ctrl("readDepth", "", result.aget.readoutDepth); ctrl("readOffset", "value", result.aget.readoutOffset);
    result.useExternalTimeReset = module.Bool("useExternalTimeReset", "false");
    return result;
}
}
