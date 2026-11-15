#include "systemexperimentbinding.h"
#include "configurationpublication.h"
#include "configurationsha256.h"
#include "strictjson.h"
#include "effectiveconfigurationrecord.h"
#include <iostream>
int main(int argc,char **argv) {
    if (argc==2 && std::string(argv[1])=="--help") {
        std::cout<<"Usage: getdaq-system-config inspect ARTIFACT.json\n"
            "       getdaq-system-config validate-binding ARTIFACT.json TOPOLOGY.json\n"
            "       getdaq-system-config inspect-publication PUBLICATION/publication.json\n"
            "       getdaq-system-config inspect-snapshot SNAPSHOT.json\n"
            "Offline only: no hardware/data connection, control lock or ECC state change.\n"; return 0;
    }
    const bool inspect=argc==3 && std::string(argv[1])=="inspect";
    const bool bind=argc==4 && std::string(argv[1])=="validate-binding";
    const bool publication=argc==3 && std::string(argv[1])=="inspect-publication";
    const bool snapshot=argc==3 && std::string(argv[1])=="inspect-snapshot";
    if (!inspect && !bind && !publication && !snapshot) { std::cerr<<"Use --help for configuration inspection/binding commands\n"; return 2; }
    try {
        if (snapshot) {
            const auto record=GetDAQ::DecodeEffectiveConfigurationRecord(GetDAQ::ReadConfigurationFile(argv[2]));
            std::cout<<"{\"valid\":true,\"effective_sha256\":"<<GetDAQ::EncodeJsonString(record.At("effective_sha256").text)
                <<",\"snapshot_sha256\":"<<GetDAQ::EncodeJsonString(record.At("snapshot_sha256").text)
                <<",\"effective_snapshot_semantics_recomputed\":true,\"hardware_loaded_verified\":false}\n"; return 0;
        }
        if (publication) {
            const std::string path=argv[2],bytes=GetDAQ::ReadConfigurationFile(path); const auto package=GetDAQ::DecodeConfigurationPublication(bytes);
            const auto separator=path.find_last_of("/\\"); const auto directory=separator==std::string::npos ? "." : path.substr(0,separator);
            const auto artifact=GetDAQ::VerifyConfigurationPublication(package,[&](const GetDAQ::PublicationFile &file) { return GetDAQ::ReadConfigurationFile(directory+'/'+file.stored); });
            std::cout<<"{\"valid\":true,\"publication_sha256\":"<<GetDAQ::EncodeJsonString(GetDAQ::ConfigurationSha256(bytes))
                <<",\"configuration_sha256\":"<<GetDAQ::EncodeJsonString(artifact.configurationSha256)<<",\"artifact_sha256\":"<<GetDAQ::EncodeJsonString(artifact.artifactSha256)
                <<",\"effective_snapshot_semantics_recomputed\":true,\"hardware_loaded_verified\":false}\n"; return 0;
        }
        const auto a=GetDAQ::LoadSystemExperimentArtifact(argv[2]);
        if (bind) GetDAQ::BindSystemExperiment(a,GetDAQ::LoadSystemAlignmentOptions(argv[3],true));
        std::cout<<"{\"valid\":true,\"configuration_sha256\":"<<GetDAQ::EncodeJsonString(a.configurationSha256)
            <<",\"artifact_sha256\":"<<GetDAQ::EncodeJsonString(a.artifactSha256)<<",\"cobos\":"<<a.settings.cobos.size()<<",\"mutants\":"<<a.settings.mutants.size()
            <<",\"topology_bound\":"<<(bind ? "true" : "false")<<",\"hardware_connected\":false,\"production_ecc_state_committed\":false}\n";
        return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
}
