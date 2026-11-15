#include "systemscanplan.h"
#include "systemscanplan_p.h"

#include "agetconfiguration.h"
#include "autogainscan.h"
#include "coboasadconfiguration.h"
#include "configurationsha256.h"
#include "peakingtimescan.h"
#include "pulseramplitudescan.h"
#include "strictjson.h"
#include "systemexperimentconfiguration.h"
#include "thresholdscan.h"

#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace GetDAQ {
namespace {

std::string String(const JsonValue &value)
{
    if (value.type != JsonValue::String) throw std::invalid_argument("Expected scan-plan string");
    return value.text;
}

bool Boolean(const JsonValue &value, bool expected)
{
    return value.type == JsonValue::Boolean && value.text == (expected ? "true" : "false");
}

int Integer(const JsonValue &value, int minimum, int maximum)
{
    if (value.type != JsonValue::Number || value.text.empty()) throw std::invalid_argument("Expected scan-plan integer");
    std::size_t offset = 0;
    long parsed = 0;
    try { parsed = std::stol(value.text, &offset, 10); }
    catch (...) { throw std::invalid_argument("Invalid scan-plan integer"); }
    if (offset != value.text.size() || parsed < minimum || parsed > maximum)
        throw std::invalid_argument("Scan-plan integer is fractional or out of range");
    return static_cast<int>(parsed);
}

bool Sha256(const std::string &value)
{
    return value.size() == 64 && value.find_first_not_of("0123456789abcdef") == std::string::npos;
}

void RequireKeys(const JsonValue &value, const std::set<std::string> &expected)
{
    if (value.type != JsonValue::Object) throw std::invalid_argument("Expected scan-plan object");
    std::set<std::string> actual;
    for (const auto &member : value.members) actual.insert(member.first);
    if (actual != expected) throw std::invalid_argument("Scan-plan object has missing or unsupported fields");
}

bool SafeLeaf(const std::string &name)
{
    if (name.empty() || name.size() > 255 || name == "." || name == "..") return false;
    for (unsigned char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return false;
    return true;
}

} // namespace

SystemScanPlanSummary ValidateSystemScanPlan(
    const std::string &manifestBytes,
    const std::function<std::string(const std::string &)> &loadArtifact)
{
    if (!loadArtifact) throw std::invalid_argument("Scan plan requires an artifact loader");
    JsonLimits manifestLimits;
    manifestLimits.stringBytes = 1024 * 1024;
    manifestLimits.arrayItems = 4096;
    const JsonValue plan = ParseStrictJson(manifestBytes, manifestLimits);
    if (plan.type != JsonValue::Object || Integer(plan.At("schema_version"), 1, 1) != 1)
        throw std::invalid_argument("Unsupported scan-plan manifest schema");
    SystemScanPlanSummary summary;
    summary.kind = String(plan.At("kind"));
    summary.targetCoBo = String(plan.At("target_cobo"));
    if (summary.kind == "getdaq.peaking-time-scan") {
        RequireKeys(plan, {"schema_version", "kind", "target_cobo", "hardware_loaded_verified",
            "requires_full_configure_per_point", "acquisition_event_count_unspecified", "points"});
    } else if (summary.kind == "getdaq.pulser-amplitude-scan") {
        RequireKeys(plan, {"schema_version", "kind", "target_cobo", "start_mv", "stop_mv", "step_mv",
            "hardware_loaded_verified", "requires_full_configure_per_point",
            "acquisition_event_count_unspecified", "points"});
    } else if (summary.kind == "getdaq.aget-threshold-scan") {
        RequireKeys(plan, {"schema_version", "kind", "target_cobo", "target_asad", "target_aget",
            "start_code", "stop_code", "step_code", "hardware_loaded_verified",
            "requires_full_configure_per_point", "acquisition_event_count_unspecified", "points"});
    } else if (summary.kind == "getdaq.auto-gain-scan") {
        RequireKeys(plan, {"schema_version", "kind", "target_cobo", "target_asad", "target_aget",
            "first_channel", "last_channel", "start_mv", "stop_mv", "step_mv", "channel_order",
            "legacy_channel_mask_mapping_verified_from_source", "hardware_loaded_verified",
            "requires_full_configure_per_point", "acquisition_event_count_unspecified",
            "gain_result_interpretation_verified", "points"});
    } else {
        throw std::invalid_argument("Unsupported scan-plan kind");
    }
    if (summary.targetCoBo.empty() || !Boolean(plan.At("hardware_loaded_verified"), false) ||
        !Boolean(plan.At("requires_full_configure_per_point"), true) ||
        !Boolean(plan.At("acquisition_event_count_unspecified"), true))
        throw std::invalid_argument("Scan plan has invalid target or unsafe hardware/configure claims");
    if (summary.kind == "getdaq.auto-gain-scan" &&
        !Boolean(plan.At("gain_result_interpretation_verified"), false))
        throw std::invalid_argument("AutoGain result interpretation must remain unverified");

    const JsonValue &points = plan.At("points");
    if (points.type != JsonValue::Array || points.items.empty() || points.items.size() > 256)
        throw std::invalid_argument("Scan plan must contain 1..256 points");
    unsigned int expectedCount = 0;
    if (summary.kind == "getdaq.peaking-time-scan") expectedCount = 16;
    else if (summary.kind == "getdaq.aget-threshold-scan") {
        const int start = Integer(plan.At("start_code"), 0, 127);
        const int stop = Integer(plan.At("stop_code"), 0, 127);
        const int step = Integer(plan.At("step_code"), 1, 127);
        if (start > stop || (stop - start) % step) throw std::invalid_argument("Invalid threshold scan range");
        expectedCount = unsigned((stop - start) / step + 1);
        (void)Integer(plan.At("target_asad"), 0, 3);
        (void)Integer(plan.At("target_aget"), 0, 3);
    } else {
        const int start = Integer(plan.At("start_mv"), -1000, 1000);
        const int stop = Integer(plan.At("stop_mv"), -1000, 1000);
        const int step = Integer(plan.At("step_mv"), 1, 2000);
        if (start > stop || (stop - start) % step) throw std::invalid_argument("Invalid scan voltage range");
        expectedCount = unsigned((stop - start) / step + 1);
        if (summary.kind == "getdaq.auto-gain-scan") {
            const int first = Integer(plan.At("first_channel"), 0, 67);
            const int last = Integer(plan.At("last_channel"), 0, 67);
            (void)Integer(plan.At("target_asad"), 0, 3);
            (void)Integer(plan.At("target_aget"), 0, 3);
            if (first > last) throw std::invalid_argument("Invalid AutoGain channel range");
            expectedCount *= unsigned(last - first + 1);
        }
    }
    const unsigned int maximum = summary.kind == "getdaq.aget-threshold-scan" ? 128U : 256U;
    if (!expectedCount || expectedCount > maximum || points.items.size() != expectedCount)
        throw std::invalid_argument("Point count does not match the complete declared scan grid");

    std::set<std::string> names;
    for (unsigned int i = 0; i < expectedCount; ++i) {
        const JsonValue &point = points.items[i];
        if (summary.kind == "getdaq.peaking-time-scan")
            RequireKeys(point, {"index", "artifact", "configuration_sha256", "artifact_sha256"});
        else if (summary.kind == "getdaq.pulser-amplitude-scan")
            RequireKeys(point, {"index", "voltage_mv", "artifact", "configuration_sha256", "artifact_sha256"});
        else if (summary.kind == "getdaq.aget-threshold-scan")
            RequireKeys(point, {"index", "threshold_code", "artifact", "configuration_sha256", "artifact_sha256"});
        else
            RequireKeys(point, {"index", "channel_id", "voltage_mv", "artifact", "configuration_sha256", "artifact_sha256"});
        if (point.type != JsonValue::Object || Integer(point.At("index"), 0, 255) != int(i))
            throw std::invalid_argument("Scan point index is missing, invalid, or out of order");
        const std::string name = String(point.At("artifact"));
        if (!SafeLeaf(name) || !names.insert(name).second)
            throw std::invalid_argument("Scan point has an unsafe or duplicate artifact filename");
        const std::string expectedArtifact = String(point.At("artifact_sha256"));
        const std::string expectedConfiguration = String(point.At("configuration_sha256"));
        if (!Sha256(expectedArtifact) || !Sha256(expectedConfiguration))
            throw std::invalid_argument("Scan point contains an invalid SHA-256 digest");
        const std::string bytes = loadArtifact(name);
        if (bytes.empty() || bytes.size() > 64U * 1024U * 1024U || ConfigurationSha256(bytes) != expectedArtifact)
            throw std::invalid_argument("Scan point artifact is empty, oversized, or has a mismatched digest");
        const SystemExperimentArtifact artifact = DecodeSystemExperimentArtifact(bytes);
        if (artifact.configurationSha256 != expectedConfiguration || artifact.artifactSha256 != expectedArtifact)
            throw std::invalid_argument("Scan point artifact/configuration digest differs from its manifest");
        SystemScanPlanInternal::ValidatePoint(plan, point, artifact, i, expectedCount);
    }
    summary.pointCount = expectedCount;
    return summary;
}

} // namespace GetDAQ
