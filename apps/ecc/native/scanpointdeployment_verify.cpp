#include "scanpointdeployment.h"
#include "scanpointdeployment_p.h"

#include "configurationsha256.h"
#include "eccprotocol.h"
#include "fixedeccconfiguration.h"
#include "planio.h"
#include "supervisorio.h"
#include "../datasupervisor/planio.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
namespace {
using namespace SupervisorIO;
using namespace ScanPointDeploymentInternal;

bool IsPointScopedRoot(const std::string &path, const std::string &executionId,
                       unsigned int point)
{
    const std::string suffix = ".scan-" + executionId + "/point-" + Suffix(point);
    return path.size() > suffix.size() &&
        path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string CanonicalBundleDirectory(const std::string &path)
{
    Path(path);
    char *resolved = ::realpath(path.c_str(), nullptr);
    const bool canonical = resolved && path == resolved;
    const std::string result = resolved ? resolved : "";
    ::free(resolved);
    struct stat info;
    Require(canonical && ::lstat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode) &&
            !S_ISLNK(info.st_mode) && info.st_uid == ::geteuid() &&
            !(info.st_mode & 0022),
            "Scan deployment bundle directory must be canonical, owned and not group/world writable");
    return result;
}

std::string ReadSealedBundleFile(const std::string &path, std::size_t maximum)
{
    Path(path);
    Fd file(::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC));
    struct stat before;
    Require(file.value >= 0 && ::fstat(file.value, &before) == 0 &&
            S_ISREG(before.st_mode) && before.st_uid == ::geteuid() &&
            (before.st_mode & 0777) == 0400 && before.st_size > 0 &&
            static_cast<std::uint64_t>(before.st_size) <= maximum,
            "Scan deployment bundle file must be an owned, sealed, bounded regular file");
    std::string bytes(static_cast<std::size_t>(before.st_size), '\0');
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const ssize_t count = ::read(file.value, &bytes[offset], bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        Require(count > 0, "Scan deployment bundle file changed or was truncated while reading");
        offset += static_cast<std::size_t>(count);
    }
    char extra;
    ssize_t trailing;
    do { trailing = ::read(file.value, &extra, 1); } while (trailing < 0 && errno == EINTR);
    struct stat after;
    Require(trailing == 0 && ::fstat(file.value, &after) == 0 && Same(before, after),
            "Scan deployment bundle file changed while reading");
    return bytes;
}

void ExactFields(const JsonValue &value, const std::set<std::string> &fields)
{
    ExactSupervisorFields(value, fields);
}

}

ScanDeploymentBundle VerifyScanPointDeploymentBundle(
    const ScanExecutionSnapshot &snapshot,
    const std::string &bundleDirectory)
{
    Require(snapshot.PointCount() > 0 && snapshot.PointCount() <= 256,
            "Scan deployment snapshot point count is out of range");
    const std::string root = CanonicalBundleDirectory(bundleDirectory);
    Directory directory(root);
    const std::string manifestBytes = ReadSealedBundleFile(root + "/manifest.json", 1024U * 1024U);
    const JsonValue manifest = ParseStrictJson(manifestBytes);
    ExactFields(manifest, {"schema", "kind", "execution_id", "snapshot_sha256", "points"});
    Require(Unsigned(manifest.At("schema")) == 1 &&
            String(manifest.At("kind")) == "getdaq.scan-deployment-bundle" &&
            String(manifest.At("execution_id")) == snapshot.ExecutionId() &&
            String(manifest.At("snapshot_sha256")) == ConfigurationSha256(snapshot.Encode()),
            "Scan deployment bundle identity differs from the frozen execution snapshot");
    Require(manifest.At("points").type == JsonValue::Array &&
            manifest.At("points").items.size() == snapshot.PointCount(),
            "Scan deployment bundle does not contain every frozen point exactly once");

    ScanDeploymentBundle result;
    result.executionId = snapshot.ExecutionId();
    result.manifestFile = root + "/manifest.json";
    result.points.reserve(snapshot.PointCount());
    for (std::size_t i = 0; i < snapshot.PointCount(); ++i) {
        const JsonValue &entry = manifest.At("points").items[i];
        ExactFields(entry, {"index", "configuration_sha256", "artifact_sha256", "artifact_file",
                            "service_file", "controller_audit_root",
                            "required_controller_audit_parent_directory", "receiver_plans"});
        const ScanExecutionSnapshotPoint &expected = snapshot.Points()[i];
        Require(Unsigned(entry.At("index")) == expected.index &&
                String(entry.At("configuration_sha256")) == expected.configurationSha256 &&
                String(entry.At("artifact_sha256")) == expected.artifactSha256,
                "Scan deployment point index/configuration/artifact differs from the frozen snapshot");

        ScanPointDeployment point;
        point.index = expected.index;
        point.artifactFile = root + "/point-" + Suffix(point.index) + "-experiment.json";
        point.serviceFile = root + "/point-" + Suffix(point.index) + "-service.json";
        point.configurationSha256 = expected.configurationSha256;
        point.artifactSha256 = expected.artifactSha256;
        point.controllerAuditRoot = String(entry.At("controller_audit_root"));
        Path(point.controllerAuditRoot);
        point.requiredControllerAuditParentDirectory = Parent(point.controllerAuditRoot);
        Require(String(entry.At("artifact_file")) == point.artifactFile &&
                String(entry.At("service_file")) == point.serviceFile &&
                String(entry.At("required_controller_audit_parent_directory")) ==
                    point.requiredControllerAuditParentDirectory,
                "Scan deployment bundle contains an unexpected file or audit path");

        const std::string artifactBytes = ReadSealedBundleFile(point.artifactFile, 64U * 1024U * 1024U);
        Require(ConfigurationSha256(artifactBytes) == point.artifactSha256,
                "Sealed scan deployment artifact digest mismatch");
        const auto artifact = DecodeSystemExperimentArtifact(artifactBytes);
        Require(artifact.configurationSha256 == point.configurationSha256 &&
                artifact.artifactSha256 == point.artifactSha256,
                "Sealed scan deployment artifact identity mismatch");

        (void)ReadSealedBundleFile(point.serviceFile, 1024U * 1024U);
        FixedEccConfiguration service(point.serviceFile, FixedEccConfiguration::OfflineEvidenceOnly{});
        service.Check();
        const NativeEccOptions controller = service.ControllerOptions();
        const RemoteReceiverOptions receiver = service.ReceiverOptions();
        const JsonValue &receiverEntries = entry.At("receiver_plans");
        Require(controller.experimentArtifactBytes == artifactBytes &&
                controller.deploymentId == DeploymentId(snapshot.ExecutionId(), point.index) &&
                controller.auditRoot == point.controllerAuditRoot &&
                IsPointScopedRoot(controller.auditRoot, snapshot.ExecutionId(), point.index) &&
                receiverEntries.type == JsonValue::Array && !receiverEntries.items.empty() &&
                receiver.endpoints.size() == receiverEntries.items.size(),
                "Fixed ECC service is not bound to this scan deployment point");

        for (std::size_t r = 0; r < receiverEntries.items.size(); ++r) {
            const JsonValue &receiverEntry = receiverEntries.items[r];
            ExactFields(receiverEntry, {"agent_id", "plan_file", "target_plan_file", "plan_sha256",
                                        "audit_root", "required_audit_parent_directory"});
            const RemoteReceiverEndpoint &endpoint = receiver.endpoints[r];
            const std::string localPlan = root + "/point-" + Suffix(point.index) +
                "-receiver-" + Suffix(static_cast<unsigned int>(r)) + "-plan.json";
            const std::string targetPlan = String(receiverEntry.At("target_plan_file"));
            const std::string auditRoot = String(receiverEntry.At("audit_root"));
            const std::string planSha = String(receiverEntry.At("plan_sha256"));
            Path(targetPlan);
            Require(targetPlan == Parent(targetPlan) + "/scan-" + snapshot.ExecutionId() +
                    "-point-" + Suffix(point.index) + ".json",
                    "Receiver target path is not unique to this execution point");
            Path(auditRoot);
            Digest(planSha);
            Require(String(receiverEntry.At("agent_id")) == endpoint.agentId &&
                    String(receiverEntry.At("plan_file")) == localPlan &&
                    Parent(auditRoot) == String(receiverEntry.At("required_audit_parent_directory")) &&
                    IsPointScopedRoot(auditRoot, snapshot.ExecutionId(), point.index) &&
                    endpoint.planSha256 == planSha,
                    "Scan receiver deployment does not match the fixed ECC endpoint");
            const std::string planBytes = ReadSealedBundleFile(localPlan, 1024U * 1024U);
            Require(ConfigurationSha256(planBytes) == planSha,
                    "Sealed scan receiver plan digest mismatch");
            const SupervisorPlanDocument plan = DecodeSupervisorPlan(planBytes);
            Require(EncodeOptions(plan.options, plan.executableSha256) == planBytes &&
                    plan.options.deploymentId == controller.deploymentId &&
                    plan.options.configurationSha256 == point.configurationSha256 &&
                    plan.options.artifactSha256 == point.artifactSha256 &&
                    plan.options.auditRoot == auditRoot,
                    "Sealed receiver plan is not canonical or is bound to a different point");
            ScanPointReceiverDeployment target;
            target.agentId = endpoint.agentId;
            target.planFile = localPlan;
            target.targetPlanFile = targetPlan;
            target.planSha256 = planSha;
            target.auditRoot = auditRoot;
            target.requiredAuditParentDirectory = Parent(auditRoot);
            point.receiverPlans.push_back(target);
        }
        result.points.push_back(point);
    }
    directory.Check();
    return result;
}


}
