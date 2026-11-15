#include "receiver.h"
#include "../../common/text/utf8.h"

#include <stdexcept>

namespace GetDAQ {
void ValidateReceiverIdentity(const ReceiverConfig &config)
{
    if (config.storageMode != "frame-storage" && config.storageMode != "byte-storage")
        throw std::invalid_argument("Storage mode must be frame-storage or byte-storage");
    if (config.fileNaming != "native" && config.fileNaming != "get")
        throw std::invalid_argument("File naming must be native or get");
    if (config.inputProtocol != "TCP" && config.inputProtocol != "ZBUF" && config.inputProtocol != "FDT")
        throw std::invalid_argument("Input protocol must be TCP, ZBUF or FDT (ICE is not a raw TCP stream)");
    if (config.sessionId.empty() && config.senderId.empty() &&
        config.configurationSha256.empty() && config.artifactSha256.empty()) {
        if (config.cycleControlStdin)
            throw std::invalid_argument("Cycle control requires complete managed receiver identity");
        if (config.storageMode == "byte-storage" &&
            (config.inputProtocol == "FDT" || config.exportPort))
            throw std::invalid_argument("ByteStorage supports plain TCP/ZBUF only and cannot export parsed frames");
        return;
    }

    const std::string identityDigests[] = {
        config.sessionId, config.configurationSha256, config.artifactSha256
    };
    for (const auto &value : identityDigests) {
        if (value.size() != 64 || value.find_first_not_of("0123456789abcdef") != std::string::npos)
            throw std::invalid_argument("Receiver identity requires a session and two lowercase SHA-256 values");
    }
    if (config.senderId.empty() || config.senderId.size() > 128 || !ValidUtf8(config.senderId))
        throw std::invalid_argument("Receiver sender identity requires bounded UTF-8");
    for (unsigned char ch : config.senderId) {
        if (ch < 32 || ch == 127)
            throw std::invalid_argument("Control byte in receiver sender identity");
    }

    const bool cycle = config.inputProtocol == "FDT" && config.cycleControlStdin && !config.once;
    if (!config.durable || (!cycle && !config.once))
        throw std::invalid_argument("Managed receiver identity requires --once or FDT cycle control and durable fsync");
    if (config.cycleControlStdin && !cycle)
        throw std::invalid_argument("Cycle control requires managed FDT without --once");
    if (config.inputProtocol == "FDT" && !cycle)
        throw std::invalid_argument("Managed receiver plans currently require TCP/ZBUF or private FDT cycle control");
    if (config.storageMode == "byte-storage" &&
        (config.inputProtocol == "FDT" || config.exportPort || config.cycleControlStdin))
        throw std::invalid_argument("Managed ByteStorage supports plain TCP/ZBUF only, without FDT or frame export");
}
}
