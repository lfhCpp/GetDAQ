#include "receivercatalog.h"
#include "supervisorio.h"
#include "systemexperimentconfiguration.h"
#include "catalogscancli.h"
#include "catalogtransfercli.h"
#include <iostream>
#include <stdexcept>
int main(int argc,char **argv) {
    try {
        if(argc==2 && std::string(argv[1])=="--help") {
            std::cout<<"Usage: getdaq-catalog --check-request FILE | --execute-request FILE | --inspect-catalog DIRECTORY\n"
                <<"       getdaq-catalog --prepare-catalog-replacement OLD_CATALOG NEW_CATALOG NEW_CANDIDATE_DIRECTORY\n"
                <<"       getdaq-catalog --publish-catalog-replacement OLD_CATALOG NEW_CATALOG NEW_CANDIDATE_DIRECTORY\n"
                <<"       getdaq-catalog --inspect-catalog-replacement OLD_CATALOG NEW_CATALOG NEW_CANDIDATE_DIRECTORY OPERATION_ID\n"
                <<"       getdaq-catalog --bind-scan-point SCAN_DIRECTORY POINT_INDEX COMPLETED_CATALOG OUTPUT_JSON\n"
                <<"       getdaq-catalog --summarize-scan SCAN_DIRECTORY BINDING_DIRECTORY OUTPUT_JSON\n"
                <<"       getdaq-catalog --analyze-threshold-scan SCAN_DIRECTORY BINDING_DIRECTORY MFM_COBO_NUMBER OUTPUT_JSON\n"
                <<"       getdaq-catalog --analyze-autogain-waveforms SCAN_DIRECTORY BINDING_DIRECTORY MFM_COBO_NUMBER OUTPUT_JSON\n"
                <<"       getdaq-catalog --analyze-pulser-scan SCAN_DIRECTORY BINDING_DIRECTORY MFM_COBO_NUMBER ASAD AGET CHANNEL OUTPUT_JSON\n"
                <<"       getdaq-catalog --analyze-peaking-scan SCAN_DIRECTORY BINDING_DIRECTORY MFM_COBO_NUMBER ASAD AGET CHANNEL OUTPUT_JSON\n"
                <<"       getdaq-catalog --copy-catalog SOURCE_CATALOG NEW_DESTINATION [--progress-file METADATA_PATH [--expected-source-key SHA256]]\n"
                <<"       getdaq-catalog --resume-catalog ORIGINAL_SOURCE PARTIAL_DESTINATION [--progress-file NEW_METADATA_PATH [--expected-source-key SHA256]]\n"
                <<"       getdaq-catalog --verify-transfer DESTINATION\n"
                <<"       getdaq-catalog --send-catalog-ssh SOURCE_CATALOG USER@HOST REMOTE_CATALOG_PROGRAM REMOTE_ROOT\n"
                <<"       getdaq-catalog --send-catalog-tcp SOURCE_CATALOG HOST PORT\n"
                <<"       getdaq-catalog --resume-catalog-tcp ORIGINAL_SOURCE HOST PORT\n"
                <<"       getdaq-catalog --listen-catalog-tcp BIND_ADDRESS PORT DESTINATION_ROOT [--once]\n"
                <<"       getdaq-catalog --resume-catalog-ssh ORIGINAL_SOURCE USER@HOST REMOTE_CATALOG_PROGRAM REMOTE_ROOT\n"
                <<"       getdaq-catalog --receive-catalog-stream PRIVATE_DESTINATION_ROOT\n"
                <<"       getdaq-catalog --resume-catalog-stream PRIVATE_DESTINATION_ROOT\n"
                <<"Native bulk transfer uses ordinary TCP/IP with no authentication or encryption. Use only on a trusted, access-controlled network.\n";return 0;
        }
        if (GetDAQCatalogCli::HandleTransferCommand(argc,argv)) return 0;
        if (GetDAQCatalogCli::HandleScanCommand(argc,argv)) return 0;
        if(argc==5 && std::string(argv[1])=="--prepare-catalog-replacement") {
            std::cout<<GetDAQ::PrepareReceiverCatalogReplacement(argv[2],argv[3],argv[4]);return 0;
        }
        if(argc==5 && std::string(argv[1])=="--publish-catalog-replacement") {
            std::cout<<GetDAQ::PublishReceiverCatalogReplacement(argv[2],argv[3],argv[4]);return 0;
        }
        if(argc==6 && std::string(argv[1])=="--inspect-catalog-replacement") {
            std::cout<<GetDAQ::InspectReceiverCatalogReplacement(argv[2],argv[3],argv[4],argv[5]);return 0;
        }
        if (argc!=3) throw std::invalid_argument("Use a documented getdaq-catalog action; replacement preparation writes a separate hardlink candidate and does not publish over the old catalog");
        const std::string action=argv[1],path=argv[2];
        if (action=="--inspect-catalog") { std::cout<<GetDAQ::InspectReceiverCatalog(path); return 0; }
        if (action!="--check-request" && action!="--execute-request") throw std::invalid_argument("Unknown catalog action");
        const auto options=GetDAQ::DecodeReceiverCatalogRequest(GetDAQ::SupervisorIO::ReadFile(path,1024*1024));
        GetDAQ::ReceiverCatalog catalog(options);
        if (action=="--check-request") { catalog.Validate(); std::cout<<"Catalog source/EOF/seal/filesystem validation passed; no target/intent created; raw payload bytes read=0\n"; }
        else std::cout<<catalog.Execute();
        return 0;
    } catch (const std::exception &error) { std::cerr<<"Catalog failed: "<<error.what()<<'\n'; return 2; }
}
