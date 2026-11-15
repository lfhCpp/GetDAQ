#include "scancommands.h"

#include "scanexecutionpolicy.h"
#include "scanexecutionsnapshot.h"
#include "systemexperimentconfiguration.h"
#include "systemscanplan.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace {
bool DirectoryNonLink(const std::string &path)
{
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) &&
           !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#else
    struct stat info;
    return ::lstat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

bool RegularNonLink(const std::string &path)
{
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY) &&
           !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#else
    struct stat info;
    return ::lstat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
#endif
}

std::string JoinPath(const std::string &directory, const std::string &name)
{
    if (directory.empty()) return name;
    const char last = directory[directory.size() - 1];
    return directory + (last == '/' || last == '\\' ? "" : "/") + name;
}

std::string ReadBoundedRegularFile(const std::string &path, std::size_t maximum)
{
    if (!RegularNonLink(path)) throw std::runtime_error("Expected a regular non-linked file: " + path);
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open file: " + path);
    std::string bytes(maximum + 1, '\0');
    input.read(&bytes[0], static_cast<std::streamsize>(bytes.size()));
    bytes.resize(static_cast<std::size_t>(input.gcount()));
    if (input.bad() || bytes.size() > maximum)
        throw std::runtime_error("File is unreadable or exceeds size limit: " + path);
    return bytes;
}

void RequireScanPlanDirectory(const std::string &directory)
{
    if (!DirectoryNonLink(directory)) throw std::runtime_error("Scan-plan path must be a real directory, not a link");
}

std::string ReadScanManifest(const std::string &directory)
{
    return ReadBoundedRegularFile(JoinPath(directory, "scan-manifest.json"), 1024U * 1024U);
}

GetDAQ::SystemScanPlanSummary ValidateScanPlan(const std::string &directory, const std::string &manifest)
{
    return GetDAQ::ValidateSystemScanPlan(manifest, [&](const std::string &name) {
        return ReadBoundedRegularFile(JoinPath(directory, name), 64U * 1024U * 1024U);
    });
}
}

bool RunScanCommand(int argc, char **argv, int *exitCode)
{
    if (!exitCode || argc < 2) return false;
    const std::string command(argv[1]);
    const bool scanPlan = command == "scan-plan" && argc == 3;
    const bool scanPolicy = command == "scan-execution-policy" && argc == 4;
    const bool scanSnapshot = command == "scan-execution-snapshot" && argc == 5;
    if (!scanPlan && !scanPolicy && !scanSnapshot) return false;

    try {
        const std::string directory(argv[2]);
        RequireScanPlanDirectory(directory);
        const std::string manifest = ReadScanManifest(directory);
        if (scanPlan) {
            const auto summary = ValidateScanPlan(directory, manifest);
            std::cout << "{\"valid\":true,\"kind\":" << GetDAQ::EncodeJsonString(summary.kind)
                      << ",\"target_cobo\":" << GetDAQ::EncodeJsonString(summary.targetCoBo)
                      << ",\"point_count\":" << summary.pointCount
                      << ",\"hardware_loaded_verified\":false}\n";
        } else if (scanPolicy) {
            const auto summary = ValidateScanPlan(directory, manifest);
            const std::string policyBytes = ReadBoundedRegularFile(argv[3], 4096);
            const auto policy = GetDAQ::ScanExecutionPolicy::Decode(policyBytes, manifest, summary);
            std::cout << "{\"valid\":true,\"source_plan_sha256\":"
                      << GetDAQ::EncodeJsonString(policy.SourcePlanSha256())
                      << ",\"scan_kind\":" << GetDAQ::EncodeJsonString(policy.Kind())
                      << ",\"target_cobo\":" << GetDAQ::EncodeJsonString(policy.TargetCoBo())
                      << ",\"point_count\":" << policy.PointCount() << ",\"stopping_strategy\":";
            if (policy.Strategy() == GetDAQ::ScanStopStrategy::EventCount) {
                std::cout << "\"event_count_per_point\",\"events_per_point\":"
                          << policy.EventsPerPoint() << ",\"maximum_duration_ms_per_point\":"
                          << policy.MaximumDurationMsPerPoint();
            }
            std::cout << ",\"hardware_command_sent\":false}\n";
        } else {
            const std::string policy = ReadBoundedRegularFile(argv[3], 4096U);
            const std::string snapshotBytes = ReadBoundedRegularFile(argv[4], 1024U * 1024U);
            const auto snapshot = GetDAQ::ScanExecutionSnapshot::Decode(snapshotBytes, manifest, policy,
                [&](const std::string &name) {
                    return ReadBoundedRegularFile(JoinPath(directory, name), 64U * 1024U * 1024U);
                });
            std::cout << "{\"valid\":true,\"execution_id\":" << GetDAQ::EncodeJsonString(snapshot.ExecutionId())
                      << ",\"source_plan_sha256\":" << GetDAQ::EncodeJsonString(snapshot.SourcePlanSha256())
                      << ",\"source_policy_sha256\":" << GetDAQ::EncodeJsonString(snapshot.SourcePolicySha256())
                      << ",\"scan_kind\":" << GetDAQ::EncodeJsonString(snapshot.ScanKind())
                      << ",\"target_cobo\":" << GetDAQ::EncodeJsonString(snapshot.TargetCoBo())
                      << ",\"point_count\":" << snapshot.PointCount()
                      << ",\"hardware_command_sent\":false}\n";
        }
        *exitCode = 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        *exitCode = 1;
    }
    return true;
}
