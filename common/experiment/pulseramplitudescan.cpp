#include "pulseramplitudescan.h"
#include "coboasadconfiguration.h"
#include <cstdint>
#include <stdexcept>

namespace GetDAQ {

std::vector<PulserAmplitudeScanPoint> BuildPulserAmplitudeScan(
    const SystemExperimentArtifact &baseline, const std::string &coboId,
    int startMv, int stopMv, int stepMv)
{
    if (coboId.empty()) throw std::invalid_argument("Pulser scan requires an explicit CoBo ID");
    if (startMv < -1000 || startMv > 1000 || stopMv < -1000 || stopMv > 1000)
        throw std::invalid_argument("Pulser scan voltages must be within -1000..1000 mV");
    if (stepMv <= 0 || startMv > stopMv)
        throw std::invalid_argument("Pulser scan requires an ascending interval and positive step");

    const SystemExperimentArtifact normalized = DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(baseline));
    const SystemExperimentCoBo *selected = nullptr;
    for (const auto &cobo : normalized.settings.cobos) {
        if (cobo.id != coboId) continue;
        if (!cobo.enabled) throw std::invalid_argument("Selected CoBo is disabled");
        const auto current = cobo.configure.asad.generator.find("amplitudeStart");
        if (current == cobo.configure.asad.generator.end() ||
            ParseAsAdVoltage(current->second, "Generator/amplitudeStart") != startMv)
            throw std::invalid_argument("Pulser scan start must match the selected CoBo configuration");
        CompileCoBoConfigure(cobo.configure);
        selected = &cobo;
        break;
    }
    if (!selected) throw std::invalid_argument("Selected CoBo ID is not present in the experiment artifact");

    const std::int64_t span = static_cast<std::int64_t>(stopMv) - startMv;
    if (span % stepMv != 0) throw std::invalid_argument("Pulser scan step must land exactly on stop voltage");
    const std::int64_t count = span / stepMv + 1;
    if (count > 256) throw std::invalid_argument("Pulser scan exceeds the 256-point planning limit");

    std::vector<PulserAmplitudeScanPoint> points;
    points.reserve(static_cast<std::size_t>(count));
    for (std::int64_t index = 0; index < count; ++index) {
        const int voltage = static_cast<int>(static_cast<std::int64_t>(startMv) + index * stepMv);
        PulserAmplitudeScanPoint point;
        point.index = static_cast<unsigned int>(index);
        point.voltageMv = voltage;
        point.artifact = normalized;
        for (auto &cobo : point.artifact.settings.cobos) {
            if (cobo.id != coboId) continue;
            cobo.configure.asad.generator["amplitudeStart"] = std::to_string(voltage);
            CompileCoBoConfigure(cobo.configure);
        }
        ValidateSystemExperimentSettings(point.artifact.settings);
        point.artifact.configurationSha256.clear();
        point.artifact.artifactSha256.clear();
        point.artifact = DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(point.artifact));
        points.push_back(std::move(point));
    }
    return points;
}

} // namespace GetDAQ
