#include "receiver_options.h"
#include <iostream>
#include <stdexcept>

namespace {
uint64_t Number(const std::string &text, uint64_t minimum, uint64_t maximum)
{
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("Invalid integer: " + text);
    const uint64_t value = std::stoull(text);
    if (value < minimum || value > maximum)
        throw std::runtime_error("Integer outside allowed range: " + text);
    return value;
}
}

namespace GetDAQ {
ReceiverCommandLine ParseReceiverOptions(const std::vector<std::string> &arguments)
{
    ReceiverCommandLine parsed;
    ReceiverConfig &c = parsed.config;
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const std::string &option = arguments[i];
        if (option == "--help") { parsed.showHelp = true; return parsed; }
        if (option == "--once") { c.once = true; continue; }
        if (option == "--no-fsync") { c.durable = false; continue; }
        if (option == "--cycle-control-stdin") { c.cycleControlStdin = true; continue; }
        if (++i >= arguments.size()) throw std::runtime_error("Missing value for " + option);
        const std::string &value = arguments[i];
        if (option == "--output") c.output = value;
        else if (option == "--input-protocol") c.inputProtocol = value;
        else if (option == "--storage-mode") c.storageMode = value;
        else if (option == "--file-naming") c.fileNaming = value;
        else if (option == "--session-id") c.sessionId = value;
        else if (option == "--sender-id") c.senderId = value;
        else if (option == "--configuration-sha256") c.configurationSha256 = value;
        else if (option == "--artifact-sha256") c.artifactSha256 = value;
        else if (option == "--bind") c.bind = value;
        else if (option == "--status-bind") c.statusBind = value;
        else if (option == "--port") c.port = unsigned(Number(value, 1, 65535));
        else if (option == "--status-port") c.statusPort = unsigned(Number(value, 0, 65535));
        else if (option == "--export-port") c.exportPort = unsigned(Number(value, 0, 65535));
        else if (option == "--queue-mib") c.blocks = std::size_t(Number(value, 1, 4096) * 4);
        else if (option == "--max-frame-mib") c.maxFrame = std::size_t(Number(value, 1, 1024) * 1024 * 1024);
        else if (option == "--rotate-mib") c.rotateBytes = Number(value, 1, 1024 * 1024) * 1024 * 1024;
        else if (option == "--min-free-mib") c.minimumFreeBytes = Number(value, 0, 1024 * 1024) * 1024 * 1024;
        else if (option == "--export-queue-mib") c.exportBudget = std::size_t(Number(value, 1, 1024) * 1024 * 1024);
        else throw std::runtime_error("Unknown option: " + option);
    }
    if (c.output.empty()) throw std::runtime_error("--output DIR is required");
    return parsed;
}

void PrintReceiverUsage(std::ostream &output)
{
    output << "getdaq-receiver --output DIR [--bind IP] [--port 46005] [--status-port 46015]\n"
        "  [--status-bind 127.0.0.1] [--export-port 0] [--queue-mib 64]\n"
        "  [--max-frame-mib 32] [--rotate-mib 1024] [--min-free-mib 0] [--export-queue-mib 16]\n"
        "  [--input-protocol TCP|ZBUF|FDT] [--storage-mode frame-storage|byte-storage] [--once] [--no-fsync]\n"
        "  [--cycle-control-stdin] (managed FDT private socket, no --once)\n"
        "  [--file-naming native|get] (GET timestamp + per-source padded sequence)\n"
        "  [--session-id HEX64 --sender-id ID --configuration-sha256 HEX64 --artifact-sha256 HEX64]\n"
        "One sender per process. Existing output directory required. ICE is unsupported.\n"
        "Minimum free reserve is checked before startup and during frame/block writes; 0 disables it.\n"
        "ZBUF uses raw TCP. FDT filters frame-boundary heartbeat/goodbye; TCP EOF is required.\n"
        "byte-storage writes TCP/ZBUF bytes unchanged; FDT requires frame-storage.\n"
        "Export is best-effort: 8 clients max, little-endian uint64 length + raw frame.\n"
        "Status is read-only. Stop the source before stopping this service.\n";
}
}
