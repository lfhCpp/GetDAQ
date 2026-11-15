#include "thresholdscan.h"
#include "agetconfiguration.h"
#include <stdexcept>
#include <utility>

namespace GetDAQ {
namespace {
unsigned int GlobalThreshold(const AgetSettings &settings)
{
    const auto found = settings.globals.find("Reg1/GlobalThresholdValue");
    if (found == settings.globals.end() || found->second.empty())
        throw std::invalid_argument("Selected AGET has no Reg1/GlobalThresholdValue");
    unsigned int value = 0;
    for (char c : found->second) {
        if (c < '0' || c > '9')
            throw std::invalid_argument("Reg1/GlobalThresholdValue must be decimal");
        const unsigned int digit = static_cast<unsigned int>(c - '0');
        if (digit > 7U || value > (7U - digit) / 10U)
            throw std::invalid_argument("Reg1/GlobalThresholdValue exceeds 7");
        value = value * 10U + digit;
    }
    return value;
}
}

std::vector<ThresholdScanPoint> BuildThresholdScan(
    const SystemExperimentArtifact &baseline, const std::string &coboId,
    unsigned int asadId, unsigned int agetId, unsigned int startCode,
    unsigned int stopCode, unsigned int stepCode)
{
    if (coboId.empty()) throw std::invalid_argument("Threshold scan requires an explicit CoBo ID");
    if (asadId > 3 || agetId > 3)
        throw std::invalid_argument("Threshold scan AsAd and AGET IDs must be in 0..3");
    if (startCode > 127 || stopCode > 127)
        throw std::invalid_argument("Threshold codes must be within 0..127");
    if (!stepCode || startCode > stopCode)
        throw std::invalid_argument("Threshold scan requires an ascending interval and positive step");
    const unsigned int span = stopCode - startCode;
    if (span % stepCode)
        throw std::invalid_argument("Threshold scan step must land exactly on stop code");
    const unsigned int count = span / stepCode + 1;
    if (count > 128) throw std::invalid_argument("Threshold scan exceeds the 128-point planning limit");

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
    if (GlobalThreshold(selectedChip->settings) != startCode / 16U)
        throw std::invalid_argument("Threshold scan start coarse code must match the baseline global threshold");

    std::vector<ThresholdScanPoint> points;
    points.reserve(count);
    for (unsigned int index = 0; index < count; ++index) {
        const unsigned int code = startCode + index * stepCode;
        ThresholdScanPoint point;
        point.index = index;
        point.thresholdCode = code;
        point.artifact = normalized;
        bool changed = false;
        for (auto &cobo : point.artifact.settings.cobos) {
            if (cobo.id != coboId) continue;
            for (auto &chip : cobo.configure.readout.aget.chips) {
                if (chip.asadId != asadId || chip.agetId != agetId) continue;
                chip.settings.globals["Reg1/GlobalThresholdValue"] = std::to_string(code / 16U);
                for (auto &channel : chip.settings.channels) channel.threshold = code % 16U;
                CompileAget(chip.settings);
                changed = true;
                break;
            }
        }
        if (!changed) throw std::logic_error("Selected AGET disappeared while building threshold scan");
        ValidateSystemExperimentSettings(point.artifact.settings);
        point.artifact.configurationSha256.clear();
        point.artifact.artifactSha256.clear();
        point.artifact = DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(point.artifact));
        points.push_back(std::move(point));
    }
    return points;
}

} // namespace GetDAQ
