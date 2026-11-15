#include "peakingtimescan.h"
#include "coboasadconfiguration.h"
#include <stdexcept>

namespace GetDAQ {
namespace {
const unsigned int AgetPeakingTimes[16] = {70, 117, 232, 280, 334, 383, 502, 541,
                                           568, 632, 721, 760, 831, 870, 976, 1014};
const unsigned int AstrePeakingTimes[16] = {68, 116, 180, 225, 343, 442, 541, 641,
                                            739, 836, 943, 1039, 2034, 3985, 5770, 7700};
}

unsigned int PeakingTimeForScanIndex(unsigned int chipVersion,unsigned int index)
{
    if (index>=16) throw std::invalid_argument("Peaking-time scan index must be in 0..15");
    if (chipVersion==0x203) return AstrePeakingTimes[index];
    if (chipVersion==0x201 || chipVersion==0x202) return AgetPeakingTimes[index];
    throw std::invalid_argument("Unsupported AGET/ASTRE version in peaking-time scan");
}

std::vector<PeakingTimeScanPoint> BuildPeakingTimeScan(const SystemExperimentArtifact &baseline,
                                                       const std::string &coboId)
{
    if (coboId.empty()) throw std::invalid_argument("Peaking-time scan requires an explicit CoBo ID");
    // Validate the original artifact first, so invalid input cannot be made to
    // look valid merely because the scan overwrites one of its parameters.
    const SystemExperimentArtifact normalizedBaseline = DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(baseline));
    const SystemExperimentCoBo *selected = nullptr;
    for (const auto &cobo : normalizedBaseline.settings.cobos) {
        if (cobo.id != coboId) continue;
        if (!cobo.enabled) throw std::invalid_argument("Selected CoBo is disabled");
        const auto &chips = cobo.configure.readout.aget.chips;
        if (chips.empty()) throw std::invalid_argument("Enabled CoBo has no active AGET chips");
        selected = &cobo;
        for (const auto &chip : chips) {
            if (chip.settings.chipVersion != 0x201 && chip.settings.chipVersion != 0x202 && chip.settings.chipVersion != 0x203)
                throw std::invalid_argument("Unsupported AGET/ASTRE version in peaking-time scan");
        }
    }
    if (!selected) throw std::invalid_argument("Selected CoBo ID is not present in the experiment artifact");

    std::vector<PeakingTimeScanPoint> points;
    points.reserve(16);
    for (unsigned int index = 0; index < 16; ++index) {
        PeakingTimeScanPoint point;
        point.index = index;
        point.artifact = normalizedBaseline;
        for (auto &cobo : point.artifact.settings.cobos) {
            if (cobo.id != coboId) continue;
            for (auto &chip : cobo.configure.readout.aget.chips) {
                const unsigned int value = PeakingTimeForScanIndex(chip.settings.chipVersion,index);
                chip.settings.globals["Reg1/peackingTime"] = std::to_string(value);
                // Compile each modified chip immediately to catch table, version,
                // and parameter validation errors before returning any plan.
                CompileAget(chip.settings);
            }
        }
        ValidateSystemExperimentSettings(point.artifact.settings);
        // Recompute both configuration and full artifact digests for this point.
        point.artifact.configurationSha256.clear();
        point.artifact.artifactSha256.clear();
        point.artifact = DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(point.artifact));
        points.push_back(std::move(point));
    }
    return points;
}

} // namespace GetDAQ
