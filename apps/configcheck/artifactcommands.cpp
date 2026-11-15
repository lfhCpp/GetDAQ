#include "artifactcommands.h"

#include "configurationpublication.h"
#include "configurationselection.h"
#include "effectiveconfigurationrecord.h"
#include "configurationsha256.h"
#include "configurationreadback.h"

#include <iostream>
#include <stdexcept>
#include <string>

bool RunArtifactCommand(int argc, char **argv, int *exitCode)
{
    if (!exitCode || argc < 2) return false;

    const std::string command(argv[1]);
    const bool snapshot = argc == 3 && command == "snapshot";
    const bool publication = argc == 3 && command == "publication";
    const bool experiment = argc == 3 && command == "experiment";
    const bool selection = argc == 3 && command == "selection";
    const bool registration = argc == 3 && command == "registration";
    const bool readbackAudit = argc == 4 && command == "readback-audit";
    if (!snapshot && !publication && !experiment && !selection && !registration && !readbackAudit)
        return false;

    try {
        if (readbackAudit) {
            const auto evidence = GetDAQ::DecodeConfigurationReadbackAudit(
                GetDAQ::ReadConfigurationFile(argv[2]), argv[3]);
            std::cout << GetDAQ::EncodeConfigurationReadbackEvidence(evidence) << '\n';
            *exitCode = 0;
            return true;
        }

        const std::string path = argv[2];
        const std::string bytes = GetDAQ::ReadConfigurationFile(path);
        if (selection || registration) {
            const auto record = selection ? GetDAQ::DecodeConfigurationSelection(bytes).registration
                                          : GetDAQ::DecodeConfigurationRegistration(bytes);
            std::cout << "{\"valid_record\":true,\"repository_id\":"
                      << GetDAQ::EncodeJsonString(record.repositoryId)
                      << ",\"publication_sha256\":" << GetDAQ::EncodeJsonString(record.publicationSha256)
                      << ",\"publication_checked\":false,\"hardware_loaded_verified\":false}\n";
        } else if (snapshot) {
            const auto record = GetDAQ::DecodeEffectiveConfigurationRecord(bytes);
            std::cout << "{\"valid\":true,\"effective_sha256\":"
                      << GetDAQ::EncodeJsonString(record.At("effective_sha256").text)
                      << ",\"snapshot_sha256\":" << GetDAQ::EncodeJsonString(record.At("snapshot_sha256").text)
                      << ",\"effective_snapshot_semantics_recomputed\":true,\"hardware_loaded_verified\":false}\n";
        } else {
            GetDAQ::SystemExperimentArtifact artifact;
            if (publication) {
                const auto publicationRecord = GetDAQ::DecodeConfigurationPublication(bytes);
                const auto lastSeparator = path.find_last_of("/\\");
                const auto directory = lastSeparator == std::string::npos ? "." : path.substr(0, lastSeparator);
                artifact = GetDAQ::VerifyConfigurationPublication(
                    publicationRecord, [&](const GetDAQ::PublicationFile &file) {
                        return GetDAQ::ReadConfigurationFile(directory + '/' + file.stored);
                    });
            } else {
                artifact = GetDAQ::DecodeSystemExperimentArtifact(bytes);
            }
            std::cout << "{\"valid\":true,\"configuration_sha256\":"
                      << GetDAQ::EncodeJsonString(artifact.configurationSha256)
                      << ",\"artifact_sha256\":" << GetDAQ::EncodeJsonString(artifact.artifactSha256);
            if (publication) {
                std::cout << ",\"publication_sha256\":"
                          << GetDAQ::EncodeJsonString(GetDAQ::ConfigurationSha256(bytes))
                          << ",\"effective_snapshot_semantics_recomputed\":true";
            }
            std::cout << ",\"hardware_loaded_verified\":false}\n";
        }
        *exitCode = 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        *exitCode = 1;
    }
    return true;
}
