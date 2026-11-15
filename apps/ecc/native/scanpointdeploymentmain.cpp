#include "scanpointdeployment.h"
#include "fixedeccconfiguration.h"
#include "configurationsha256.h"
#include "eccprotocol.h"
#include "supervisorio.h"
#include "systemscanplan.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>

namespace {
using namespace GetDAQ;
using namespace GetDAQ::SupervisorIO;

std::string CanonicalDirectory(const std::string &path)
{
    if (path.empty() || path.size() > 3300) throw std::invalid_argument("Invalid scan plan directory");
    char *resolved = ::realpath(path.c_str(), nullptr);
    const bool same = resolved && path == resolved;
    const std::string canonical = resolved ? resolved : "";
    ::free(resolved);
    struct stat st;
    if (!same || ::lstat(path.c_str(), &st) || !S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode))
        throw std::invalid_argument("Scan plan directory must be canonical and not a symlink");
    return canonical;
}

std::string Quote(const std::string &value)
{
    Path(value);
    return value;
}
}

int main(int argc, char **argv)
{
    if (argc == 6 && std::string(argv[1]) == "verify") {
        try {
            const std::string planDirectory = CanonicalDirectory(argv[2]);
            const std::string planBytes = ReadFile(planDirectory + "/scan-manifest.json", 1024U * 1024U);
            const std::string policyBytes = ReadFile(argv[3], 4096);
            const std::string snapshotBytes = ReadFile(argv[4], 1024U * 1024U);
            const auto loadArtifact = [&planDirectory](const std::string &name) {
                return ReadFile(planDirectory + "/" + name, 64U * 1024U * 1024U);
            };
            const ScanExecutionSnapshot snapshot = ScanExecutionSnapshot::Decode(
                snapshotBytes, planBytes, policyBytes, loadArtifact);
            const ScanDeploymentBundle bundle = VerifyScanPointDeploymentBundle(
                snapshot, CanonicalDirectory(argv[5]));
            std::cout << "{\"verified\":true,\"kind\":\"getdaq.scan-deployment-bundle\",\"execution_id\":"
                      << EncodeJsonString(bundle.executionId) << ",\"manifest\":"
                      << EncodeJsonString(bundle.manifestFile) << ",\"point_count\":"
                      << bundle.points.size() << ",\"services_started\":false,\"hardware_commands_sent\":false}\n";
            return 0;
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            return 1;
        } catch (...) {
            std::cerr << "Non-standard scan deployment verification failure\n";
            return 1;
        }
    }
    if (argc < 9 || std::string(argv[1]) != "create" || ((argc - 7) % 2) != 0) {
        std::cerr << "Usage: getdaq-scan-deploy create PLAN_DIRECTORY POLICY.json SNAPSHOT.json "
                     "ECC_TEMPLATE.json NEW_BUNDLE_DIRECTORY RECEIVER_PLAN.json TARGET_DIRECTORY "
                     "[RECEIVER_PLAN.json TARGET_DIRECTORY ...]\n"
                     "Creates sealed offline per-point ECC/receiver inputs; does not install files, "
                     "start ECC, or send hardware commands.\n"
                     "       getdaq-scan-deploy verify PLAN_DIRECTORY POLICY.json SNAPSHOT.json BUNDLE_DIRECTORY\n"
                     "Revalidates sealed bundle files against the frozen snapshot without starting services.\n";
        return 2;
    }
    try {
        const std::string planDirectory = CanonicalDirectory(argv[2]);
        const std::string manifest = ReadFile(planDirectory + "/scan-manifest.json", 1024U * 1024U);
        const std::string policy = ReadFile(argv[3], 4096);
        const std::string snapshotBytes = ReadFile(argv[4], 1024U * 1024U);
        const std::string serviceFile = Quote(argv[5]);
        const std::string outputDirectory = Quote(argv[6]);
        const auto loadArtifact = [&planDirectory](const std::string &name) {
            return ReadFile(planDirectory + "/" + name, 64U * 1024U * 1024U);
        };
        const ScanExecutionSnapshot snapshot = ScanExecutionSnapshot::Decode(
            snapshotBytes, manifest, policy, loadArtifact);
        FixedEccConfiguration service(serviceFile);
        service.Check();
        const auto endpoints = service.ReceiverOptions().endpoints;
        const std::size_t receiverCount = static_cast<std::size_t>((argc - 7) / 2);
        if (receiverCount != endpoints.size())
            throw std::invalid_argument("Supply one receiver template and target directory per fixed ECC agent, in order");
        std::vector<ScanReceiverPlanTemplate> receivers;
        receivers.reserve(receiverCount);
        for (std::size_t i = 0; i < receiverCount; ++i)
            receivers.push_back({endpoints[i].agentId, Quote(argv[7 + i * 2]), Quote(argv[8 + i * 2])});

        const ScanDeploymentBundle bundle = CreateScanPointDeployments(
            snapshot, serviceFile, receivers, outputDirectory, loadArtifact);
        std::cout << "{\"created\":true,\"kind\":\"getdaq.scan-deployment-bundle\",\"execution_id\":"
                  << EncodeJsonString(bundle.executionId) << ",\"manifest\":"
                  << EncodeJsonString(bundle.manifestFile) << ",\"point_count\":"
                  << bundle.points.size() << ",\"remote_files_installed\":false,\"service_started\":false,"
                     "\"hardware_commands_sent\":false}\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Non-standard scan deployment bundle failure\n";
        return 1;
    }
}
