#include "artifactcommands.h"
#include "scancommands.h"

#include <iostream>
#include <string>

namespace {
void PrintUsage()
{
    std::cout << "Usage: getdaq-config-check snapshot SNAPSHOT.json\n"
                 "       getdaq-config-check readback-audit AUDIT.jsonl TRUSTED_COMPLETED_CHAIN_SHA256\n"
                 "       getdaq-config-check publication PACKAGE/publication.json\n"
                 "       getdaq-config-check experiment EXPERIMENT.json\n"
                 "       getdaq-config-check selection SELECTION.json\n"
                 "       getdaq-config-check registration REGISTRATION.json\n"
                 "       getdaq-config-check scan-plan PLAN_DIRECTORY\n"
                 "       getdaq-config-check scan-execution-policy PLAN_DIRECTORY POLICY.json\n"
                 "       getdaq-config-check scan-execution-snapshot PLAN_DIRECTORY POLICY.json SNAPSHOT.json\n"
                 "Standard C++11 only; no Qt, Ice, hardware connection or raw data.\n";
}
}

int main(int argc, char **argv)
{
    int exitCode = 0;
    if (RunScanCommand(argc, argv, &exitCode)) return exitCode;
    if (RunArtifactCommand(argc, argv, &exitCode)) return exitCode;

    PrintUsage();
    return argc == 2 && std::string(argv[1]) == "--help" ? 0 : 2;
}
