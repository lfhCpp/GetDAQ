#include "iceingress_options.h"

#include "../../common/text/utf8.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cstdint>
#include <limits.h>
#include <ostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
namespace {
std::uint64_t Number(const std::string &value, std::uint64_t minimum, std::uint64_t maximum)
{
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("ICE receiver option requires an integer");
    const auto result = std::stoull(value);
    if (result < minimum || result > maximum) throw std::invalid_argument("ICE receiver option outside bounds");
    return result;
}

bool Digest(const std::string &value)
{
    return value.size() == 64 && value.find_first_not_of("0123456789abcdef") == std::string::npos;
}

void ValidateOutputDirectory(IceIngressConfig *config)
{
    char resolved[PATH_MAX];
    if (config->output.empty() || !::realpath(config->output.c_str(), resolved))
        throw std::invalid_argument("ICE output directory must exist");
    config->output = resolved;
    if (!ValidUtf8(config->output)) throw std::invalid_argument("ICE output path requires UTF-8");
    struct stat info;
    if (::lstat(config->output.c_str(), &info) || !S_ISDIR(info.st_mode))
        throw std::invalid_argument("ICE output is not a directory");
}
}

IceIngressOptions ParseIceIngressOptions(int argc, char **argv)
{
    IceIngressOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "--help") {
            options.help = true;
            return options;
        }
        if (option == "--once") continue;
        if (++i >= argc) throw std::invalid_argument("Missing ICE receiver option value");
        const std::string value = argv[i];
        if (option == "--output") options.config.output = value;
        else if (option == "--session-id") options.config.sessionId = value;
        else if (option == "--sender-id") options.config.senderId = value;
        else if (option == "--configuration-sha256") options.config.configurationSha256 = value;
        else if (option == "--artifact-sha256") options.config.artifactSha256 = value;
        else if (option == "--status-bind") {
            if (value != "127.0.0.1") throw std::invalid_argument("ICE status must bind localhost");
        } else if (option == "--input-protocol") {
            if (value != "ICE") throw std::invalid_argument("ICE receiver input protocol must be ICE");
        } else if (option == "--bind") options.bind = value;
        else if (option == "--port") options.port = unsigned(Number(value, 1, 65535));
        else if (option == "--status-port") options.statusPort = unsigned(Number(value, 0, 65535));
        else if (option == "--queue-mib") options.config.blocks = std::size_t(Number(value, 1, 4096) * 4);
        else if (option == "--max-frame-mib") options.config.maxFrame = std::size_t(Number(value, 1, 1024) * 1024 * 1024);
        else if (option == "--message-mib") options.messageMiB = unsigned(Number(value, 1, 64));
        else if (option == "--export-port") options.config.exportPort = unsigned(Number(value, 0, 65535));
        else if (option == "--export-queue-mib") options.config.exportBudget = std::size_t(Number(value, 1, 1024) * 1024 * 1024);
        else if (option == "--rotate-mib") options.config.rotateBytes = Number(value, 1, 1024 * 1024) * 1024 * 1024;
        else if (option == "--storage-mode") options.config.storageMode = value;
        else if (option == "--min-free-mib") options.config.minimumFreeBytes = Number(value, 0, 1024 * 1024) * 1024 * 1024;
        else if (option == "--file-naming") options.config.fileNaming = value;
        else throw std::invalid_argument("Unknown ICE receiver option");
    }

    if (!Digest(options.config.sessionId))
        throw std::invalid_argument("ICE receiver requires a lowercase HEX64 session");
    const bool managed = !options.config.senderId.empty() || !options.config.configurationSha256.empty() ||
                         !options.config.artifactSha256.empty();
    const bool controlByte = std::any_of(options.config.senderId.begin(), options.config.senderId.end(),
        [](unsigned char c) { return c < 32 || c == 127; });
    if (managed && (options.config.senderId.empty() || options.config.senderId.size() > 128 ||
        !ValidUtf8(options.config.senderId) || controlByte || !Digest(options.config.configurationSha256) ||
        !Digest(options.config.artifactSha256)))
        throw std::invalid_argument("ICE managed identity requires sender ID and both SHA256 values");

    options.config.dataBind = options.bind;
    options.config.dataPort = options.port;
    options.config.statusPort = options.statusPort;
    if (options.config.fileNaming != "native" && options.config.fileNaming != "get")
        throw std::invalid_argument("ICE file naming must be native/get");
    if (options.config.storageMode != "frame-storage" && options.config.storageMode != "byte-storage")
        throw std::invalid_argument("ICE storage mode must be frame-storage or byte-storage");
    if (options.config.storageMode == "byte-storage" && options.config.exportPort)
        throw std::invalid_argument("ICE ByteStorage cannot export parsed frames");
    in_addr address;
    if (::inet_pton(AF_INET, options.bind.c_str(), &address) != 1 || options.statusPort == options.port)
        throw std::invalid_argument("ICE bind/ports invalid");
    if (options.config.exportPort && (options.config.exportPort == options.port ||
        options.config.exportPort == options.statusPort))
        throw std::invalid_argument("ICE export port overlaps data/status");
    options.config.exportBind = options.bind;
    ValidateOutputDirectory(&options.config);
    return options;
}

void PrintIceIngressUsage(std::ostream &output)
{
    output << "getdaq-ice-receiver --output DIR --session-id HEX64 [--bind IPv4] [--port 46105] [--status-port 46115]\n"
        "  [--queue-mib 64] [--max-frame-mib 32] [--message-mib 1] [--rotate-mib 1024] [--min-free-mib 0] [--file-naming native|get]\n"
        "  [--storage-mode frame-storage|byte-storage] (byte-storage preserves ICE bytes; no MFM parsing or frame export)\n"
        "  [--export-port 0] [--export-queue-mib 16] (best-effort native length + raw frame export)\n"
        "Optional Ice 3.5 compatibility input, no Qt. One source connection per process.\n"
        "Original DataProcessor/processData is accepted. RPC blocks are not MFM frame boundaries.\n"
        "Success requires source finishStream(session, expectedBytes) on the same ordered connection.\n"
        "Legacy sender disconnect, silence or SIGTERM never manufacture EOF/completion.\n"
        "Minimum free reserve is checked before startup and each frame; 0 disables it.\n"
        "Managed ICE sources require a fixed ICE executable/SHA and distinct source-end receipt.\n"
        "  [--sender-id ID --configuration-sha256 HEX64 --artifact-sha256 HEX64]\n";
}
}
