#include "autogainscan.h"
#include "coboasadconfiguration.h"
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace GetDAQ {

std::vector<AutoGainScanPoint> BuildAutoGainScan(
    const SystemExperimentArtifact &baseline, const std::string &coboId,
    unsigned int asadId, unsigned int agetId,
    unsigned int firstChannel, unsigned int lastChannel,
    int startMv, int stopMv, int stepMv)
{
    if (coboId.empty()) throw std::invalid_argument("AutoGain scan requires an explicit CoBo ID");
    if (asadId > 3 || agetId > 3)
        throw std::invalid_argument("AutoGain scan AsAd and AGET IDs must be in 0..3");
    if (firstChannel > 67 || lastChannel > 67 || firstChannel > lastChannel)
        throw std::invalid_argument("AutoGain channel interval must be ascending within 0..67");
    if (startMv < -1000 || startMv > 1000 || stopMv < -1000 || stopMv > 1000)
        throw std::invalid_argument("AutoGain Pulser voltages must be within -1000..1000 mV");
    if (stepMv <= 0 || startMv > stopMv)
        throw std::invalid_argument("AutoGain voltage interval must be ascending with a positive step");
    const std::int64_t span = static_cast<std::int64_t>(stopMv) - startMv;
    if (span % stepMv)
        throw std::invalid_argument("AutoGain voltage step must land exactly on stop voltage");
    const std::uint64_t voltageCount = static_cast<std::uint64_t>(span / stepMv) + 1U;
    const std::uint64_t channelCount = static_cast<std::uint64_t>(lastChannel - firstChannel) + 1U;
    const std::uint64_t pointCount = voltageCount * channelCount;
    if (pointCount > 256U)
        throw std::invalid_argument("AutoGain plan exceeds the 256-point planning limit; narrow the channel or voltage range");

    const SystemExperimentArtifact normalized =
        DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(baseline));
    const SystemExperimentCoBo *selectedCoBo = nullptr;
    const CoBoAgetChipSettings *selectedChip = nullptr;
    for (const auto &cobo : normalized.settings.cobos) {
        if (cobo.id != coboId) continue;
        if (!cobo.enabled) throw std::invalid_argument("Selected CoBo is disabled");
        selectedCoBo = &cobo;
        for (const auto &chip : cobo.configure.readout.aget.chips) {
            if (chip.asadId == asadId && chip.agetId == agetId) {
                selectedChip = &chip;
                break;
            }
        }
        break;
    }
    if (!selectedCoBo) throw std::invalid_argument("Selected CoBo ID is not present in the experiment artifact");
    if (!selectedChip) throw std::invalid_argument("Selected AGET is not active in the selected CoBo");

    const auto baseVoltage = selectedCoBo->configure.asad.generator.find("amplitudeStart");
    if (baseVoltage == selectedCoBo->configure.asad.generator.end() ||
        ParseAsAdVoltage(baseVoltage->second, "Generator/amplitudeStart") != startMv)
        throw std::invalid_argument("AutoGain start voltage must match the selected CoBo configuration");

    for (unsigned int channel = firstChannel; channel <= lastChannel; ++channel) {
        bool active = false;
        for (const auto &entry : selectedChip->settings.channels)
            if (entry.id == channel && entry.active) active = true;
        if (!active)
            throw std::invalid_argument("Every selected AutoGain channel must be active in the baseline AGET");
    }
    CompileCoBoConfigure(selectedCoBo->configure);

    std::vector<AutoGainScanPoint> points;
    points.reserve(static_cast<std::size_t>(pointCount));
    std::vector<unsigned int> channelOrder;
    channelOrder.reserve(static_cast<std::size_t>(channelCount));
    // Legacy TestController walks a one-hot mask upward. Reg3 maps bit 0..33
    // to channel 33..0; Reg4 then maps bit 0..33 to channel 34..67.
    const int lastReg3Channel = lastChannel < 33 ? static_cast<int>(lastChannel) : 33;
    for (int channel = lastReg3Channel; channel >= static_cast<int>(firstChannel); --channel)
        channelOrder.push_back(static_cast<unsigned int>(channel));
    const unsigned int firstReg4Channel = firstChannel > 34 ? firstChannel : 34;
    for (unsigned int channel = firstReg4Channel; channel <= lastChannel; ++channel)
        channelOrder.push_back(channel);
    unsigned int index = 0;
    // Match legacy AutoGain order: finish the amplitude sweep for one channel,
    // then advance in the old Reg3/Reg4 one-hot mask order.
    for (const unsigned int channel : channelOrder) {
        for (std::uint64_t voltageIndex = 0; voltageIndex < voltageCount; ++voltageIndex) {
            const int voltage = static_cast<int>(static_cast<std::int64_t>(startMv) +
                                                  static_cast<std::int64_t>(voltageIndex) * stepMv);
            AutoGainScanPoint point;
            point.index = index++;
            point.channelId = channel;
            point.voltageMv = voltage;
            point.artifact = normalized;
            bool changed = false;
            for (auto &cobo : point.artifact.settings.cobos) {
                if (cobo.id != coboId) continue;
                cobo.configure.asad.generator["amplitudeStart"] = std::to_string(voltage);
                for (auto &chip : cobo.configure.readout.aget.chips) {
                    if (chip.asadId != asadId || chip.agetId != agetId) continue;
                    for (auto &entry : chip.settings.channels) entry.selectedForTest = entry.id == channel;
                    CompileAget(chip.settings);
                    changed = true;
                }
                CompileCoBoConfigure(cobo.configure);
            }
            if (!changed) throw std::logic_error("Selected AGET disappeared while building AutoGain scan");
            ValidateSystemExperimentSettings(point.artifact.settings);
            point.artifact.configurationSha256.clear();
            point.artifact.artifactSha256.clear();
            point.artifact = DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(point.artifact));
            points.push_back(std::move(point));
        }
    }
    return points;
}

} // namespace GetDAQ
