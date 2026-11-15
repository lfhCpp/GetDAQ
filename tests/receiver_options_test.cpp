#include "../apps/datareceiver/receiver_options.h"
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

template<class Function>
void ExpectError(Function function, const std::string &expected)
{
    try { function(); }
    catch (const std::exception &e) {
        Require(e.what() == expected, "CLI parser returned an unexpected diagnostic");
        return;
    }
    throw std::runtime_error("CLI parser unexpectedly accepted invalid options");
}
}

int main()
{
    try {
        const auto parsed = GetDAQ::ParseReceiverOptions({
            "--output", "incoming", "--bind", "0.0.0.0", "--port", "46006",
            "--status-port", "0", "--export-port", "46016", "--queue-mib", "8",
            "--max-frame-mib", "64", "--rotate-mib", "2", "--min-free-mib", "3",
            "--export-queue-mib", "4", "--input-protocol", "FDT", "--once",
            "--file-naming", "get", "--session-id", std::string(64, 'a'), "--sender-id", "sender",
            "--configuration-sha256", std::string(64, 'b'), "--artifact-sha256", std::string(64, 'c')});
        const auto &c = parsed.config;
        Require(!parsed.showHelp && c.output == "incoming" && c.bind == "0.0.0.0", "string option values changed");
        Require(c.port == 46006 && c.statusPort == 0 && c.exportPort == 46016, "port option conversion changed");
        Require(c.blocks == 32 && c.maxFrame == 64ull * 1024 * 1024, "queue/frame option conversion changed");
        Require(c.rotateBytes == 2ull * 1024 * 1024 && c.minimumFreeBytes == 3ull * 1024 * 1024,
                "disk size option conversion changed");
        Require(c.exportBudget == 4ull * 1024 * 1024 && c.once && c.inputProtocol == "FDT" && c.fileNaming == "get",
                "receiver flags or protocol values changed");
        Require(c.sessionId == std::string(64, 'a') && c.senderId == "sender"
                && c.configurationSha256 == std::string(64, 'b') && c.artifactSha256 == std::string(64, 'c'),
                "managed identity arguments changed");

        const auto help = GetDAQ::ParseReceiverOptions({"--help", "--not-an-option"});
        Require(help.showHelp, "help must return before parsing later arguments");
        ExpectError([] { GetDAQ::ParseReceiverOptions({"--output"}); }, "Missing value for --output");
        ExpectError([] { GetDAQ::ParseReceiverOptions({"--port", "46005"}); }, "--output DIR is required");
        ExpectError([] { GetDAQ::ParseReceiverOptions({"--output", "incoming", "--port", "0"}); },
                    "Integer outside allowed range: 0");
        ExpectError([] { GetDAQ::ParseReceiverOptions({"--output", "incoming", "--queue-mib", "1.5"}); },
                    "Invalid integer: 1.5");
        ExpectError([] { GetDAQ::ParseReceiverOptions({"--output", "incoming", "--mystery", "x"}); },
                    "Unknown option: --mystery");

        std::cout << "Receiver option parsing passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Receiver option parsing failed: " << e.what() << '\n';
        return 1;
    }
}
