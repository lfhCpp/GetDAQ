#include "scanexecutionsnapshot.h"

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
    Require(value.type == JsonValue::String, "Scan execution snapshot field must be a string");
    return value.text;
}

unsigned int Number(const JsonValue &value, unsigned int maximum)
{
    Require(value.type == JsonValue::Number && !value.text.empty(),
            "Scan execution snapshot field must be an integer");
    unsigned int result = 0;
    for (char c : value.text) {
        Require(c >= '0' && c <= '9', "Scan execution snapshot integer is not canonical decimal");
        const unsigned int digit = static_cast<unsigned int>(c - '0');
        Require(digit <= maximum, "Scan execution snapshot integer is out of range");
        Require(result <= (maximum - digit) / 10U,
                "Scan execution snapshot integer is out of range");
        result = result * 10U + digit;
    }
    return result;
}

void ExactKeys(const JsonValue &value, const std::set<std::string> &expected)
{
    Require(value.type == JsonValue::Object, "Scan execution snapshot must be an object");
    std::set<std::string> actual;
    for (const auto &member : value.members) actual.insert(member.first);
    Require(actual == expected, "Scan execution snapshot has missing or unsupported fields");
}

void ValidateExecutionId(const std::string &id)
{
    Require(!id.empty() && id.size() <= 128 && id != "." && id != "..",
            "Scan execution snapshot requires a bounded execution ID");
    for (unsigned char c : id)
        Require((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_',
                "Scan execution ID contains unsafe characters");
}

std::string PointArray(const std::vector<ScanExecutionSnapshotPoint> &points)
{
    std::string result = "[";
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (i) result += ',';
        const auto &point = points[i];
        result += "{\"index\":" + std::to_string(point.index) +
            ",\"artifact\":" + EncodeJsonString(point.artifact) +
            ",\"configuration_sha256\":" + EncodeJsonString(point.configurationSha256) +
            ",\"artifact_sha256\":" + EncodeJsonString(point.artifactSha256) + "}";
    }
    return result + "]";
}

}

ScanExecutionSnapshot ScanExecutionSnapshot::Create(
    const std::string &executionId, const std::string &planBytes,
    const std::string &policyBytes,
    const std::function<std::string(const std::string &)> &loadArtifact)
{
    ValidateExecutionId(executionId);
    Require(static_cast<bool>(loadArtifact), "Scan execution snapshot requires an artifact loader");
    Require(planBytes.size() <= 1024U * 1024U, "Scan plan exceeds the snapshot validation limit");
    Require(policyBytes.size() <= 4096U, "Scan policy exceeds the snapshot validation limit");

    const SystemScanPlanSummary plan = ValidateSystemScanPlan(planBytes, loadArtifact);
    (void)ScanExecutionPolicy::Decode(policyBytes, planBytes, plan);
    JsonLimits planLimits;
    planLimits.stringBytes = 1024U * 1024U;
    planLimits.arrayItems = 4096U;
    const JsonValue manifest = ParseStrictJson(planBytes, planLimits);
    const JsonValue &points = manifest.At("points");
    Require(points.type == JsonValue::Array && points.items.size() == plan.pointCount,
            "Validated scan plan point list changed during snapshot creation");

    ScanExecutionSnapshot snapshot;
    snapshot.fExecutionId = executionId;
    snapshot.fSourcePlanSha256 = ConfigurationSha256(planBytes);
    snapshot.fSourcePolicySha256 = ConfigurationSha256(policyBytes);
    snapshot.fScanKind = plan.kind;
    snapshot.fTargetCoBo = plan.targetCoBo;
    snapshot.fPoints.reserve(plan.pointCount);
    for (unsigned int i = 0; i < plan.pointCount; ++i) {
        const JsonValue &row = points.items[i];
        ScanExecutionSnapshotPoint point;
        point.index = Number(row.At("index"), 255U);
        point.artifact = Text(row.At("artifact"));
        point.configurationSha256 = Text(row.At("configuration_sha256"));
        point.artifactSha256 = Text(row.At("artifact_sha256"));
        Require(point.index == i, "Scan execution snapshot points are not in order");
        snapshot.fPoints.push_back(point);
    }
    return snapshot;
}

std::string ScanExecutionSnapshot::Encode() const
{
    ValidateExecutionId(fExecutionId);
    Require(fPoints.size() > 0 && fPoints.size() <= 256,
            "Scan execution snapshot point count is out of range");
    return "{\"schema_version\":1,\"kind\":\"getdaq.scan-execution-snapshot\",\"execution_id\":" +
        EncodeJsonString(fExecutionId) + ",\"source_plan_sha256\":" +
        EncodeJsonString(fSourcePlanSha256) + ",\"source_policy_sha256\":" +
        EncodeJsonString(fSourcePolicySha256) + ",\"scan_kind\":" +
        EncodeJsonString(fScanKind) + ",\"target_cobo\":" + EncodeJsonString(fTargetCoBo) +
        ",\"point_count\":" + std::to_string(fPoints.size()) + ",\"points\":" +
        PointArray(fPoints) + "}\n";
}

ScanExecutionSnapshot ScanExecutionSnapshot::Decode(
    const std::string &snapshotBytes, const std::string &planBytes,
    const std::string &policyBytes,
    const std::function<std::string(const std::string &)> &loadArtifact)
{
    Require(snapshotBytes.size() <= 1024U * 1024U,
            "Scan execution snapshot exceeds 1 MiB");
    JsonLimits snapshotLimits;
    snapshotLimits.stringBytes = 1024U * 1024U;
    snapshotLimits.arrayItems = 4096U;
    const JsonValue root = ParseStrictJson(snapshotBytes, snapshotLimits);
    ExactKeys(root, {"schema_version", "kind", "execution_id", "source_plan_sha256",
                     "source_policy_sha256", "scan_kind", "target_cobo", "point_count", "points"});
    Require(Number(root.At("schema_version"), 1U) == 1U &&
            Text(root.At("kind")) == "getdaq.scan-execution-snapshot",
            "Unsupported scan execution snapshot schema");
    const std::string executionId = Text(root.At("execution_id"));
    const ScanExecutionSnapshot expected = Create(executionId, planBytes, policyBytes, loadArtifact);
    Require(Number(root.At("point_count"), 256U) == expected.PointCount() &&
            Text(root.At("source_plan_sha256")) == expected.SourcePlanSha256() &&
            Text(root.At("source_policy_sha256")) == expected.SourcePolicySha256() &&
            Text(root.At("scan_kind")) == expected.ScanKind() &&
            Text(root.At("target_cobo")) == expected.TargetCoBo(),
            "Scan execution snapshot no longer matches its plan or policy");
    const JsonValue &rows = root.At("points");
    Require(rows.type == JsonValue::Array && rows.items.size() == expected.PointCount(),
            "Scan execution snapshot point list has the wrong size");
    for (unsigned int i = 0; i < expected.PointCount(); ++i) {
        const JsonValue &row = rows.items[i];
        ExactKeys(row, {"index", "artifact", "configuration_sha256", "artifact_sha256"});
        const ScanExecutionSnapshotPoint &point = expected.Points()[i];
        Require(Number(row.At("index"), 255U) == point.index &&
                Text(row.At("artifact")) == point.artifact &&
                Text(row.At("configuration_sha256")) == point.configurationSha256 &&
                Text(row.At("artifact_sha256")) == point.artifactSha256,
                "Scan execution snapshot point identity changed");
    }
    return expected;
}

}
