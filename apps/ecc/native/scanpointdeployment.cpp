#include "scanpointdeployment.h"
#include "scanpointdeployment_p.h"

#include "../../../common/experiment/systemexperimentconfiguration.h"
#include "configurationsha256.h"
#include "eccprotocol.h"
#include "fixedeccconfiguration.h"
#include "planio.h"
#include "supervisorio.h"
#include "../datasupervisor/planio.h"
#include <map>
#include <set>
#include <stdexcept>

namespace GetDAQ {
namespace ScanPointDeploymentInternal {
using namespace SupervisorIO;

void Require(bool condition, const char *message)
{
    if (!condition) throw std::invalid_argument(message);
}

std::string Suffix(unsigned int value)
{
    std::string result = std::to_string(value);
    while (result.size() < 3) result.insert(result.begin(), '0');
    return result;
}

std::string DeploymentId(const std::string &executionId, unsigned int point)
{
    return "scan-" + ConfigurationSha256(executionId).substr(0, 24) + "-point-" + Suffix(point);
}

std::string Parent(const std::string &path)
{
    const std::size_t slash = path.find_last_of('/');
    return slash == 0 ? "/" : path.substr(0, slash);
}

std::string ChildRoot(const std::string &root, const std::string &executionId,
                      unsigned int point)
{
    return root + ".scan-" + executionId + "/point-" + Suffix(point);
}

}

namespace {
using namespace SupervisorIO;
using namespace ScanPointDeploymentInternal;

std::string Json(const JsonValue &value)
{
    if (value.type == JsonValue::Null) return "null";
    if (value.type == JsonValue::Boolean || value.type == JsonValue::Number) return value.text;
    if (value.type == JsonValue::String) return EncodeJsonString(value.text);
    std::string result = value.type == JsonValue::Array ? "[" : "{";
    if (value.type == JsonValue::Array) {
        for (const auto &item : value.items) {
            if (result.size() > 1) result += ',';
            result += Json(item);
        }
    } else {
        for (const auto &item : value.members) {
            if (result.size() > 1) result += ',';
            result += EncodeJsonString(item.first) + ':' + Json(item.second);
        }
    }
    return result + (value.type == JsonValue::Array ? ']' : '}');
}

JsonValue StringValue(const std::string &value)
{
    return ParseStrictJson(EncodeJsonString(value));
}

JsonValue Reference(const std::string &path, const std::string &digest)
{
    JsonValue reference;
    reference.type = JsonValue::Object;
    reference.members["path"] = StringValue(path);
    reference.members["sha256"] = StringValue(digest);
    return reference;
}

}

ScanDeploymentBundle CreateScanPointDeployments(
    const ScanExecutionSnapshot &snapshot,
    const std::string &templateServiceFile,
    const std::vector<ScanReceiverPlanTemplate> &receiverTemplates,
    const std::string &newBundleDirectory,
    const std::function<std::string(const std::string &)> &loadArtifact)
{
    Require(static_cast<bool>(loadArtifact), "Scan deployment requires an artifact loader");
    Require(snapshot.PointCount() > 0 && snapshot.PointCount() <= 256,
            "Scan deployment point count is out of range");

    FixedEccConfiguration original(templateServiceFile);
    original.Check();
    const JsonValue templateService = ParseStrictJson(ReadFile(templateServiceFile, 1024 * 1024));
    Require(!templateService.members.count("configuration_publication") &&
            !templateService.members.count("configuration_selection"),
            "Per-point scan deployments require an unpublished fixed artifact template");
    const auto endpoints = original.ReceiverOptions().endpoints;
    const auto controllerOptions = original.ControllerOptions();
    Require(receiverTemplates.size() == endpoints.size() && !endpoints.empty(),
            "Scan deployment requires one receiver-plan template per fixed agent");
    std::set<std::string> expectedSourceIds;
    std::map<std::string, const DataSenderOptions *> expectedSenders;
    for (const auto &sender : controllerOptions.senders) {
        expectedSourceIds.insert(sender.senderId);
        expectedSenders.emplace(sender.senderId, &sender);
    }
    Require(!expectedSourceIds.empty(), "Fixed ECC deployment has no data sender bindings");

    std::vector<ReceiverTemplate> receivers;
    std::set<std::string> agentIds;
    std::set<std::string> receiverAddresses;
    for (std::size_t i = 0; i < endpoints.size(); ++i) {
        const auto &input = receiverTemplates[i];
        Require(input.agentId == endpoints[i].agentId && agentIds.insert(input.agentId).second,
                "Receiver-plan templates must match the fixed agent order exactly");
        Path(input.planFile);
        Path(input.targetDirectory);
        const std::string bytes = ReadFile(input.planFile, 1024 * 1024);
        Require(ConfigurationSha256(bytes) == endpoints[i].planSha256,
                "Receiver-plan template digest differs from the fixed ECC binding");
        const SupervisorPlanDocument decoded = DecodeSupervisorPlan(bytes);
        Require(EncodeOptions(decoded.options, decoded.executableSha256) == bytes,
                "Receiver-plan template is not canonical");
        std::set<std::string> planSourceIds;
        for (const auto &source : decoded.options.sources) {
            planSourceIds.insert(source.id);
            const auto sender = expectedSenders.find(source.id);
            Require(sender != expectedSenders.end() &&
                    sender->second->receiverAddress == source.receiverAddress &&
                    sender->second->protocol == source.inputProtocol &&
                    receiverAddresses.insert(source.receiverAddress).second,
                    "Receiver-plan source target/protocol differs from the fixed ECC sender");
        }
        Require(planSourceIds == expectedSourceIds,
                "Receiver-plan template sources differ from the fixed ECC sender set");
        receivers.push_back({input.agentId, input.planFile, bytes, input.targetDirectory,
                             decoded.executableSha256, decoded.options});
    }

    // Validate every frozen point and all generated bytes before creating any
    // output. This catches stale source artifacts without leaving a partial bundle.
    std::vector<PreparedPoint> prepared;
    prepared.reserve(snapshot.PointCount());
    std::size_t totalBytes = 0;
    for (const auto &point : snapshot.Points()) {
        std::string artifactBytes = loadArtifact(point.artifact);
        Require(artifactBytes.size() <= 64U * 1024U * 1024U &&
                ConfigurationSha256(artifactBytes) == point.artifactSha256,
                "Scan point artifact no longer matches the frozen snapshot");
        const auto artifact = DecodeSystemExperimentArtifact(artifactBytes);
        Require(artifact.configurationSha256 == point.configurationSha256 &&
                artifact.artifactSha256 == point.artifactSha256,
                "Scan point artifact configuration identity differs from the frozen snapshot");

        const std::string suffix = Suffix(point.index);
        const std::string artifactName = "point-" + suffix + "-experiment.json";
        const std::string artifactPath = newBundleDirectory + "/" + artifactName;
        Path(artifactPath);
        JsonValue service = ParseStrictJson(Json(templateService));
        service.members["experiment_artifact"] = Reference(artifactPath, point.artifactSha256);
        service.members["audit_root"] = StringValue(ChildRoot(controllerOptions.auditRoot, snapshot.ExecutionId(), point.index));
        Path(service.members["audit_root"].text);
        service.members["deployment_id"] = StringValue(DeploymentId(snapshot.ExecutionId(), point.index));

        PreparedPoint output;
        output.snapshotPoint = &point;
        output.artifactBytes = artifactBytes;
        output.artifactName = artifactName;
        output.receiverBytes.reserve(receivers.size());
        JsonValue &serviceReceivers = service.members.at("remote_receivers").members.at("endpoints");
        Require(serviceReceivers.type == JsonValue::Array && serviceReceivers.items.size() == receivers.size(),
                "Fixed ECC receiver endpoint set changed during scan deployment");
        for (std::size_t i = 0; i < receivers.size(); ++i) {
            const auto &receiver = receivers[i];
            DataSupervisorOptions options = receiver.options;
            options.auditRoot = ChildRoot(options.auditRoot, snapshot.ExecutionId(), point.index);
            Path(options.auditRoot);
            options.deploymentId = DeploymentId(snapshot.ExecutionId(), point.index);
            options.configurationSha256 = point.configurationSha256;
            options.artifactSha256 = point.artifactSha256;
            const std::string bytes = EncodeOptions(options, receiver.executableSha256);
            const std::string digest = ConfigurationSha256(bytes);
            Require(serviceReceivers.items[i].At("agent_id").text == receiver.agentId,
                    "Fixed ECC receiver endpoint order changed during scan deployment");
            serviceReceivers.items[i].members["plan_sha256"] = StringValue(digest);
            output.receiverBytes.push_back(bytes);
            totalBytes += bytes.size();
        }
        output.serviceBytes = Json(service) + '\n';
        totalBytes += artifactBytes.size() + output.serviceBytes.size();
        Require(totalBytes <= 512U * 1024U * 1024U,
                "Scan deployment bundle exceeds the 512 MiB metadata/input limit");
        prepared.push_back(std::move(output));
    }

    return PublishScanPointDeploymentBundle(snapshot, original, receiverTemplates,
                                             receivers, newBundleDirectory, prepared);
}


}
