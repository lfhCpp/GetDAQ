#include "scanexecutionpolicy.h"

#include "configurationsha256.h"
#include "strictjson.h"
#include "systemexperimentconfiguration.h"

#include <set>
#include <stdexcept>

namespace GetDAQ {
namespace {

void Require(bool condition, const char *message)
{
    if (!condition) throw std::invalid_argument(message);
}

std::string Text(const JsonValue &value)
{
    Require(value.type == JsonValue::String, "Expected execution-policy string");
    return value.text;
}

unsigned int Number(const JsonValue &value, unsigned int maximum)
{
    Require(value.type == JsonValue::Number && !value.text.empty(),
            "Expected execution-policy integer");
    unsigned int result = 0;
    for (char c : value.text) {
        Require(c >= '0' && c <= '9', "Invalid execution-policy integer");
        const unsigned int digit = unsigned(c - '0');
        Require(result <= maximum / 10U &&
                (result < maximum / 10U || digit <= maximum % 10U),
                "Execution-policy integer is out of range");
        result = result * 10U + digit;
    }
    return result;
}

void ExactKeys(const JsonValue &value, const std::set<std::string> &keys)
{
    Require(value.type == JsonValue::Object, "Expected execution-policy object");
    std::set<std::string> actual;
    for (const auto &member : value.members) actual.insert(member.first);
    Require(actual == keys, "Execution policy has missing or unsupported fields");
}

void ValidateIdentity(const std::string &planBytes,
                      const SystemScanPlanSummary &plan)
{
    Require(planBytes.size() <= 1024U * 1024U,
            "Scan-plan manifest exceeds the execution-policy limit");
    const JsonValue manifest = ParseStrictJson(planBytes);
    Require(manifest.type == JsonValue::Object &&
            manifest.At("schema_version").type == JsonValue::Number &&
            manifest.At("schema_version").text == "1",
            "Unsupported source scan-plan manifest");
    Require(Text(manifest.At("kind")) == plan.kind &&
            Text(manifest.At("target_cobo")) == plan.targetCoBo &&
            manifest.At("points").type == JsonValue::Array &&
            manifest.At("points").items.size() == plan.pointCount,
            "Validated scan-plan identity does not match its manifest");
    Require(plan.pointCount > 0 && plan.pointCount <= 256,
            "Scan plan point count is out of range");
}

bool IsThreshold(const std::string &kind)
{
    return kind == "getdaq.aget-threshold-scan";
}

}

ScanExecutionPolicy::ScanExecutionPolicy(
    const std::string &planBytes, const SystemScanPlanSummary &plan,
    unsigned int eventsPerPoint, unsigned int maximumDurationMsPerPoint)
    : fSourcePlanSha256(ConfigurationSha256(planBytes)), fKind(plan.kind),
      fTargetCoBo(plan.targetCoBo), fPointCount(plan.pointCount),
      fEventsPerPoint(eventsPerPoint),
      fMaximumDurationMsPerPoint(maximumDurationMsPerPoint)
{
    ValidateIdentity(planBytes, plan);
    Require(!fTargetCoBo.empty(), "Scan execution policy requires a target CoBo");
    Require(!IsThreshold(fKind),
            "Threshold plans are manual-only; automatic Run is disabled");
    Require(fEventsPerPoint > 0 && fEventsPerPoint <= MaximumEventsPerPoint,
            "Event count per point must be within 1..1000000000");
    Require(fMaximumDurationMsPerPoint > 0 &&
            fMaximumDurationMsPerPoint <= MaximumEventRunDurationMs,
            "Maximum event-run duration per point must be within 1..86400000 milliseconds");
}

ScanExecutionPolicy ScanExecutionPolicy::ForEventCount(
    const std::string &planBytes, const SystemScanPlanSummary &plan,
    unsigned int eventsPerPoint, unsigned int maximumDurationMsPerPoint)
{
    return ScanExecutionPolicy(planBytes, plan, eventsPerPoint,
                               maximumDurationMsPerPoint);
}

std::string ScanExecutionPolicy::Encode() const
{
    std::string result = "{\"schema_version\":1,\"kind\":\"getdaq.scan-execution-policy\",\"source_plan_sha256\":" +
        EncodeJsonString(fSourcePlanSha256) + ",\"scan_kind\":" + EncodeJsonString(fKind) +
        ",\"target_cobo\":" + EncodeJsonString(fTargetCoBo) +
        ",\"point_count\":" + std::to_string(fPointCount) + ",\"stopping_strategy\":";
    if (fStrategy == ScanStopStrategy::EventCount) {
        result += "\"event_count_per_point\",\"events_per_point\":" +
                  std::to_string(fEventsPerPoint) + ",\"maximum_duration_ms_per_point\":" +
                  std::to_string(fMaximumDurationMsPerPoint);
    }
    return result + "}\n";
}

ScanExecutionPolicy ScanExecutionPolicy::Decode(
    const std::string &policyBytes, const std::string &planBytes,
    const SystemScanPlanSummary &validatedPlan)
{
    Require(policyBytes.size() <= 4096, "Scan execution policy exceeds 4096 bytes");
    ValidateIdentity(planBytes, validatedPlan);
    const JsonValue policy = ParseStrictJson(policyBytes);
    Require(policy.type == JsonValue::Object &&
            policy.At("schema_version").type == JsonValue::Number &&
            policy.At("schema_version").text == "1" &&
            Text(policy.At("kind")) == "getdaq.scan-execution-policy",
            "Unsupported scan execution policy");
    const std::string strategy = Text(policy.At("stopping_strategy"));
    ScanExecutionPolicy decoded = [&]() {
        if (strategy == "event_count_per_point") {
            ExactKeys(policy, {"schema_version", "kind", "source_plan_sha256", "scan_kind",
                               "target_cobo", "point_count", "stopping_strategy", "events_per_point",
                               "maximum_duration_ms_per_point"});
            return ForEventCount(planBytes, validatedPlan,
                Number(policy.At("events_per_point"), MaximumEventsPerPoint),
                Number(policy.At("maximum_duration_ms_per_point"), MaximumEventRunDurationMs));
        }
        throw std::invalid_argument("Unknown scan execution stopping strategy");
    }();
    Require(Text(policy.At("source_plan_sha256")) == decoded.fSourcePlanSha256 &&
            Text(policy.At("scan_kind")) == decoded.fKind &&
            Text(policy.At("target_cobo")) == decoded.fTargetCoBo &&
            Number(policy.At("point_count"), 256) == decoded.fPointCount,
            "Scan execution policy does not match the validated scan plan");
    return decoded;
}

}
