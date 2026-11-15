#include "transferjobs.h"
#include "supervisorio.h"
#include "configurationsha256.h"
#include <sys/stat.h>
#include <stdexcept>
#include <unistd.h>
#include <vector>

namespace GetDAQ {
namespace {
using namespace SupervisorIO;

void Require(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}

void Component(const std::string &value)
{
    Require(!value.empty() && value.size() <= 80 &&
            value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == std::string::npos &&
            value[0] != '_' && value[0] != '-',
            "Invalid transfer experiment/operation component");
}
}

void ValidateTransferJobOptions(const TransferJobOptions &options)
{
    Component(options.agentId);
    Text(options.deploymentId, 128);
    Require(options.deploymentId.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == std::string::npos &&
            options.deploymentId[0] != '_' && options.deploymentId[0] != '-',
            "Invalid transfer deployment");
    Path(options.sourceRoot);
    Path(options.backupRoot);
    Path(options.auditRoot);
    Path(options.executable);
    Digest(options.executableSha256);

    const std::vector<std::string> roots = {options.sourceRoot, options.backupRoot, options.auditRoot};
    for (const auto &left : roots) {
        for (const auto &right : roots) {
            if (&left != &right)
                Require(left != right && left.compare(0, right.size() + 1, right + '/') != 0,
                        "Transfer fixed roots must not overlap");
        }
    }

    Directory source(options.sourceRoot), backup(options.backupRoot), audit(options.auditRoot);
    source.Check();
    backup.Check();
    audit.Check();

    struct stat executableInfo;
    Require(!::lstat(options.executable.c_str(), &executableInfo) && S_ISREG(executableInfo.st_mode) &&
            executableInfo.st_uid == ::geteuid() && !(executableInfo.st_mode & 0022) &&
            (executableInfo.st_mode & 0111) &&
            ConfigurationSha256(ReadFile(options.executable, 64 * 1024 * 1024)) == options.executableSha256,
            "Transfer native executable binding differs");
}
}
