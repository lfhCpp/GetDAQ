#include "systemscanplan_p.h"
#include "agetconfiguration.h"
#include "configurationsha256.h"
#include "peakingtimescan.h"
#include "strictjson.h"
#include "systemexperimentconfiguration.h"
#include <limits>
#include <stdexcept>
#include <vector>
namespace GetDAQ { namespace SystemScanPlanInternal {
std::string String(const JsonValue &v) { if (v.type != JsonValue::String) throw std::invalid_argument("Expected scan-plan string"); return v.text; }
int Integer(const JsonValue &v, int min, int max) { if (v.type != JsonValue::Number || v.text.empty()) throw std::invalid_argument("Expected scan-plan integer"); std::size_t off=0; long n=0; try { n=std::stol(v.text,&off,10); } catch (...) { throw std::invalid_argument("Invalid scan-plan integer"); } if(off!=v.text.size()||n<min||n>max) throw std::invalid_argument("Scan-plan integer is fractional or out of range"); return int(n); }
bool Boolean(const JsonValue &v, bool expected) { return v.type==JsonValue::Boolean && v.text==(expected?"true":"false"); }
unsigned int Decimal(const std::string &s) { if(s.empty()||s.size()>10||(s.size()>1&&s[0]=='0')) throw std::invalid_argument("Expected canonical decimal AGET setting"); unsigned int n=0; for(char c:s){if(c<'0'||c>'9'||n>(std::numeric_limits<unsigned int>::max()-unsigned(c-'0'))/10U) throw std::invalid_argument("Invalid decimal AGET setting"); n=n*10U+unsigned(c-'0');} return n; }
const SystemExperimentCoBo &FindCoBo(const SystemExperimentArtifact &a,const std::string &id){for(const auto &c:a.settings.cobos)if(c.id==id&&c.enabled)return c;throw std::invalid_argument("Scan target CoBo is absent or disabled in a point artifact");}
const CoBoAgetChipSettings &FindChip(const SystemExperimentCoBo &c,unsigned int asad,unsigned int aget){for(const auto &x:c.configure.readout.aget.chips)if(x.asadId==asad&&x.agetId==aget)return x;throw std::invalid_argument("Scan target AGET is not active in a point artifact");}
int GridValue(const JsonValue &plan,const JsonValue &point,int index,const std::string &kind){const int start=Integer(plan.At("start_mv"),-1000,1000),stop=Integer(plan.At("stop_mv"),-1000,1000),step=Integer(plan.At("step_mv"),1,2000);if(start>stop||(stop-start)%step)throw std::invalid_argument("Invalid scan voltage grid");const int count=(stop-start)/step+1;const int voltageIndex=kind=="getdaq.pulser-amplitude-scan"?index:index%count;const int expected=start+voltageIndex*step;if(Integer(point.At("voltage_mv"),-1000,1000)!=expected)throw std::invalid_argument("Scan point voltage does not match declared grid");return expected;}
void ValidatePoint(const JsonValue &plan, const JsonValue &point,
                   const SystemExperimentArtifact &artifact, unsigned int index,
                   unsigned int pointCount)
{
    const std::string kind = String(plan.At("kind"));
    const SystemExperimentCoBo &cobo = FindCoBo(artifact, String(plan.At("target_cobo")));
    if (kind == "getdaq.peaking-time-scan") {
        if (point.members.size() != 4 || index >= 16) throw std::invalid_argument("PeakingTime requires exactly 16 canonical points");
        const auto &chips = cobo.configure.readout.aget.chips;
        if (chips.empty()) throw std::invalid_argument("PeakingTime target has no active AGET");
        for (const auto &chip : chips) {
            const auto value = chip.settings.globals.find("Reg1/peackingTime");
            if (value == chip.settings.globals.end() ||
                Decimal(value->second) != PeakingTimeForScanIndex(chip.settings.chipVersion, index))
                throw std::invalid_argument("PeakingTime point differs from the chip-version scan table");
        }
        return;
    }
    if (kind == "getdaq.pulser-amplitude-scan" || kind == "getdaq.auto-gain-scan") {
        const bool autoGain = kind == "getdaq.auto-gain-scan";
        const int voltage = GridValue(plan, point, static_cast<int>(index), kind);
        const auto voltageSetting = cobo.configure.asad.generator.find("amplitudeStart");
        if (voltageSetting == cobo.configure.asad.generator.end() ||
            ParseAsAdVoltage(voltageSetting->second, "Generator/amplitudeStart") != voltage)
            throw std::invalid_argument("Point Pulser setting differs from its declared voltage");
        if (!autoGain) return;

        const unsigned int first = unsigned(Integer(plan.At("first_channel"), 0, 67));
        const unsigned int last = unsigned(Integer(plan.At("last_channel"), 0, 67));
        const unsigned int asad = unsigned(Integer(plan.At("target_asad"), 0, 3));
        const unsigned int aget = unsigned(Integer(plan.At("target_aget"), 0, 3));
        if (first > last || String(plan.At("channel_order")) != "aget-reg3-bit-ascending-then-reg4-bit-ascending" ||
            !Boolean(plan.At("legacy_channel_mask_mapping_verified_from_source"), true))
            throw std::invalid_argument("Unsupported AutoGain channel metadata");
        std::vector<unsigned int> order;
        const int lastReg3 = last < 33 ? int(last) : 33;
        for (int channel = lastReg3; channel >= int(first); --channel) order.push_back(unsigned(channel));
        const unsigned int firstReg4 = first > 34 ? first : 34;
        for (unsigned int channel = firstReg4; channel <= last; ++channel) order.push_back(channel);
        const int voltageCount = (Integer(plan.At("stop_mv"), -1000, 1000) -
                                  Integer(plan.At("start_mv"), -1000, 1000)) /
                                 Integer(plan.At("step_mv"), 1, 2000) + 1;
        if (order.empty() || std::size_t(voltageCount) * order.size() != pointCount)
            throw std::invalid_argument("AutoGain point count differs from channel by voltage grid");
        const unsigned int expectedChannel = order[index / unsigned(voltageCount)];
        if (unsigned(Integer(point.At("channel_id"), 0, 67)) != expectedChannel)
            throw std::invalid_argument("AutoGain channel does not match legacy register order");
        const auto &chip = FindChip(cobo, asad, aget);
        bool active = false;
        for (const auto &channel : chip.settings.channels) {
            if (channel.id == expectedChannel && channel.active) active = true;
            if (channel.selectedForTest != (channel.id == expectedChannel))
                throw std::invalid_argument("AutoGain point must select exactly its declared channel");
        }
        if (!active) throw std::invalid_argument("AutoGain point channel is inactive");
        return;
    }
    if (kind == "getdaq.aget-threshold-scan") {
        const int start = Integer(plan.At("start_code"), 0, 127);
        const int stop = Integer(plan.At("stop_code"), 0, 127);
        const int step = Integer(plan.At("step_code"), 1, 127);
        if (start > stop || (stop - start) % step) throw std::invalid_argument("Invalid threshold grid");
        const int code = start + int(index) * step;
        if (Integer(point.At("threshold_code"), 0, 127) != code)
            throw std::invalid_argument("Threshold point does not match declared grid");
        const auto &chip = FindChip(cobo, unsigned(Integer(plan.At("target_asad"), 0, 3)),
                                    unsigned(Integer(plan.At("target_aget"), 0, 3)));
        const auto global = chip.settings.globals.find("Reg1/GlobalThresholdValue");
        if (global == chip.settings.globals.end() || Decimal(global->second) != unsigned(code / 16))
            throw std::invalid_argument("Threshold coarse value differs from point code");
        for (const auto &channel : chip.settings.channels)
            if (channel.threshold != unsigned(code % 16))
                throw std::invalid_argument("Threshold fine value differs from point code");
        return;
    }
    throw std::invalid_argument("Unsupported scan-plan kind");
}
} }
