#include "../apps/iceingress/iceingress_options.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

GetDAQ::IceIngressOptions Parse(const std::vector<std::string> &arguments)
{
    std::vector<char *> argv;
    argv.reserve(arguments.size());
    for (const auto &argument : arguments) argv.push_back(const_cast<char *>(argument.c_str()));
    return GetDAQ::ParseIceIngressOptions(static_cast<int>(argv.size()), argv.data());
}

void Reject(const std::vector<std::string> &arguments)
{
    bool rejected = false;
    try { Parse(arguments); }
    catch (const std::exception &) { rejected = true; }
    Require(rejected, "invalid ICE receiver options were accepted");
}
}

int main()
{
    try {
        const std::string digest(64, 'a');
        auto options = Parse({"receiver", "--output", "/tmp", "--session-id", digest,
            "--sender-id", "DAQ sender", "--configuration-sha256", digest, "--artifact-sha256", digest,
            "--bind", "127.0.0.1", "--port", "46105", "--status-port", "0", "--queue-mib", "64",
            "--max-frame-mib", "32", "--message-mib", "2", "--export-port", "0",
            "--export-queue-mib", "16", "--rotate-mib", "1024", "--min-free-mib", "0",
            "--file-naming", "get", "--storage-mode", "frame-storage"});
        Require(!options.help && options.config.output == "/tmp", "output directory was not normalized");
        Require(options.config.sessionId == digest && options.config.senderId == "DAQ sender", "managed identity parse differs");
        Require(options.config.dataBind == "127.0.0.1" && options.config.dataPort == 46105 && options.config.statusPort == 0,
            "listener defaults/overrides differ");
        Require(options.config.blocks == 256 && options.config.maxFrame == 32U * 1024U * 1024U,
            "bounded queue/frame settings differ");
        Require(options.messageMiB == 2 && options.config.exportBudget == 16U * 1024U * 1024U,
            "message/export budgets differ");
        Require(options.config.rotateBytes == 1024ULL * 1024ULL * 1024ULL && options.config.fileNaming == "get",
            "storage naming/rotation settings differ");

        Require(Parse({"receiver", "--help", "--unknown"}).help, "help did not short-circuit option validation");
        Reject({"receiver", "--output", "/tmp", "--session-id", digest, "--queue-mib", "0"});
        Reject({"receiver", "--output", "/tmp", "--session-id", digest, "--port", "65536"});
        Reject({"receiver", "--output", "/tmp", "--session-id", digest, "--port", "46105", "--status-port", "46105"});
        Reject({"receiver", "--output", "/tmp", "--session-id", digest, "--storage-mode", "byte-storage", "--export-port", "12345"});
        Reject({"receiver", "--output", "/tmp", "--session-id", digest, "--sender-id", "managed"});
        Reject({"receiver", "--output", "/tmp", "--session-id", std::string(64, 'A')});
        Reject({"receiver", "--output", "/tmp", "--session-id", digest, "--unknown"});
        Reject({"receiver", "--output", "/tmp", "--session-id", digest, "--message-mib"});
        std::cout << "PASS ICE receiver option defaults, managed identity, bounds, mode conflicts and help handling\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
