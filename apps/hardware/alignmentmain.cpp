#include "systemalignment.h"
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
void Usage()
{
    std::cerr << "Usage: getdaq-align validate-topology FILE\n"
        "       getdaq-align align-system FILE NEW_AUDIT_DIRECTORY --exclusive-stopped\n"
        "Requires an exclusive controlling owner and all board/sender/receiver DAQ stopped.\n"
        "A new 0700 directory records every step; failures stop without retry/restoration.\n"
        "Board-reported alignment is not full Prepare or physical phase verification.\n";
}
}
int main(int argc,char **argv)
{
    using namespace GetDAQ;
    if (argc == 2 && std::string(argv[1]) == "--help") { Usage(); return 0; }
    const bool validate = argc == 3 && std::string(argv[1]) == "validate-topology";
    const bool align = argc == 5 && std::string(argv[1]) == "align-system" && std::string(argv[4]) == "--exclusive-stopped";
    if (!validate && !align) { Usage(); return 2; }
    try {
        auto options = LoadSystemAlignmentOptions(argv[2]);
        if (validate) {
            std::cout << "{\"valid\":true,\"mutants\":" << options.mutants.size() << ",\"cobos\":" << options.cobos.size()
                << ",\"hardware_connected\":false,\"proxy_types_verified\":false}\n";
            return 0;
        }
        options.exclusiveOwnerConfirmed = options.daqStoppedConfirmed = true;
        SystemAlignmentClient client(options); const auto result = client.Align(argv[3]);
        std::cout << "{\"phase\":\"complete\",\"reported_results_consistent\":true,\"located\":[";
        for (std::size_t i = 0; i < result.located.size(); ++i) {
            if (i) std::cout << ',';
            const auto &slot = result.located[i];
            std::cout << "{\"cobo_id\":" << JsonString(slot.coboId) << ",\"mutant_id\":" << JsonString(slot.mutantId) << ",\"amc_slot\":" << slot.amcSlot << "}";
        }
        std::cout << "],\"crates\":[";
        for (std::size_t i = 0; i < result.crates.size(); ++i) {
            if (i) std::cout << ',';
            const auto &crate = result.crates[i];
            std::cout << "{\"mutant_id\":" << JsonString(crate.mutantId) << ",\"gmc_mask\":" << crate.gmcShelfMask << ",\"wsca_mask\":" << crate.wscaShelfMask << "}";
        }
        std::cout << "],\"inter_crate_steps\":" << result.interCrate.size()
            << ",\"physical_alignment_verified\":false,\"full_prepare\":false,\"sync_pulse_sent\":false}\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
