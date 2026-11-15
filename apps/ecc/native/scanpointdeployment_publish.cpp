#include "scanpointdeployment.h"
#include "scanpointdeployment_p.h"
#include "fixedeccconfiguration.h"
#include "supervisorio.h"
#include "configurationsha256.h"
#include "../../../common/experiment/systemexperimentconfiguration.h"
#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
namespace ScanPointDeploymentInternal {
using namespace SupervisorIO;

namespace {

void Seal(const std::string &path)
{
    Fd file(::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC));
    if (file.value < 0 || ::fchmod(file.value, 0400) || ::fsync(file.value))
        throw std::runtime_error("Cannot seal scan deployment input");
}

void WriteSealed(const std::string &path, const std::string &bytes)
{
    WriteNew(path, bytes);
    Seal(path);
}

std::string TargetFile(const ScanReceiverPlanTemplate &input,
                       const std::string &executionId, unsigned int point)
{
    return input.targetDirectory + "/scan-" + executionId + "-point-" +
        Suffix(point) + ".json";
}

}

ScanDeploymentBundle PublishScanPointDeploymentBundle(
    const ScanExecutionSnapshot &snapshot,
    const FixedEccConfiguration &original,
    const std::vector<ScanReceiverPlanTemplate> &receiverTemplates,
    const std::vector<ReceiverTemplate> &receivers,
    const std::string &newBundleDirectory,
    const std::vector<PreparedPoint> &prepared)
{
    const auto controllerOptions = original.ControllerOptions();
    original.Check();
    for (const auto &receiver : receivers)
        Require(ReadFile(receiver.planFile, 1024 * 1024) == receiver.bytes,
                "Receiver-plan template changed during scan deployment review");
    Path(newBundleDirectory);
    CreateDirectory(newBundleDirectory);
    Directory bundle(newBundleDirectory);
    ScanDeploymentBundle result;
    result.executionId = snapshot.ExecutionId();
    result.points.reserve(prepared.size());
    std::string manifest = "{\"schema\":1,\"kind\":\"getdaq.scan-deployment-bundle\",\"execution_id\":" +
        EncodeJsonString(snapshot.ExecutionId()) + ",\"snapshot_sha256\":" +
        EncodeJsonString(ConfigurationSha256(snapshot.Encode())) + ",\"points\":[";
    for (std::size_t p = 0; p < prepared.size(); ++p) {
        const auto &source = prepared[p];
        const auto &point = *source.snapshotPoint;
        ScanPointDeployment deployment;
        deployment.index = point.index;
        deployment.artifactFile = newBundleDirectory + "/" + source.artifactName;
        deployment.serviceFile = newBundleDirectory + "/point-" + Suffix(point.index) + "-service.json";
        Path(deployment.serviceFile);
        deployment.controllerAuditRoot = ChildRoot(controllerOptions.auditRoot, snapshot.ExecutionId(), point.index);
        deployment.requiredControllerAuditParentDirectory = Parent(deployment.controllerAuditRoot);
        deployment.configurationSha256 = point.configurationSha256;
        deployment.artifactSha256 = point.artifactSha256;
        WriteSealed(deployment.artifactFile, source.artifactBytes);
        for (std::size_t i = 0; i < receivers.size(); ++i) {
            const std::string local = newBundleDirectory + "/point-" + Suffix(point.index) +
                "-receiver-" + Suffix(static_cast<unsigned int>(i)) + "-plan.json";
            Path(local);
            WriteSealed(local, source.receiverBytes[i]);
            ScanPointReceiverDeployment target;
            target.agentId = receivers[i].agentId;
            target.planFile = local;
            target.targetPlanFile = TargetFile(receiverTemplates[i], snapshot.ExecutionId(), point.index);
            Path(target.targetPlanFile);
            target.planSha256 = ConfigurationSha256(source.receiverBytes[i]);
            target.auditRoot = receivers[i].options.auditRoot + ".scan-" + snapshot.ExecutionId() +
                "/point-" + Suffix(point.index);
            target.requiredAuditParentDirectory = Parent(target.auditRoot);
            deployment.receiverPlans.push_back(target);
        }
        WriteSealed(deployment.serviceFile, source.serviceBytes);
        FixedEccConfiguration checked(deployment.serviceFile);
        checked.Check();
        Require(checked.ControllerOptions().experimentArtifactBytes == source.artifactBytes,
                "Generated fixed ECC deployment changed the scan artifact");
        if (p) manifest += ',';
        manifest += "{\"index\":" + std::to_string(point.index) +
            ",\"configuration_sha256\":" + EncodeJsonString(point.configurationSha256) +
            ",\"artifact_sha256\":" + EncodeJsonString(point.artifactSha256) +
            ",\"artifact_file\":" + EncodeJsonString(deployment.artifactFile) +
            ",\"service_file\":" + EncodeJsonString(deployment.serviceFile) +
            ",\"controller_audit_root\":" + EncodeJsonString(deployment.controllerAuditRoot) +
            ",\"required_controller_audit_parent_directory\":" + EncodeJsonString(deployment.requiredControllerAuditParentDirectory) +
            ",\"receiver_plans\":[";
        for (std::size_t i = 0; i < deployment.receiverPlans.size(); ++i) {
            if (i) manifest += ',';
            const auto &receiver = deployment.receiverPlans[i];
            manifest += "{\"agent_id\":" + EncodeJsonString(receiver.agentId) +
                ",\"plan_file\":" + EncodeJsonString(receiver.planFile) +
                ",\"target_plan_file\":" + EncodeJsonString(receiver.targetPlanFile) +
                ",\"plan_sha256\":" + EncodeJsonString(receiver.planSha256) +
                ",\"audit_root\":" + EncodeJsonString(receiver.auditRoot) +
                ",\"required_audit_parent_directory\":" + EncodeJsonString(receiver.requiredAuditParentDirectory) + "}";
        }
        manifest += "]}";
        result.points.push_back(std::move(deployment));
    }
    original.Check();
    for (const auto &receiver : receivers)
        Require(ReadFile(receiver.planFile, 1024 * 1024) == receiver.bytes,
                "Receiver-plan template changed while publishing scan deployment");
    manifest += "]}\n";
    result.manifestFile = newBundleDirectory + "/manifest.json";
    Path(result.manifestFile);
    WriteSealed(result.manifestFile, manifest);
    if (::fsync(bundle.Descriptor())) throw std::runtime_error("Cannot sync scan deployment bundle");
    bundle.Check();
    return result;
}

}
}
