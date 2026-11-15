#include "catalogtransfercli.h"
#include "catalogtransfer.h"
#include "supervisorio.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQCatalogCli {
namespace {
unsigned int Port(const char *text,bool allowZero) {
    errno=0;
    char *end=nullptr;
    const auto value=std::strtoul(text,&end,10);
    if (errno || end==text || *end || value>65535UL || (!allowZero && !value))
        throw std::invalid_argument("Invalid TCP port");
    return static_cast<unsigned int>(value);
}
GetDAQ::CatalogTransferProgress Progress() {
    auto last=std::chrono::steady_clock::now()-std::chrono::seconds(2);
    return [&](const std::string &phase,std::uint64_t bytes) {
        const auto now=std::chrono::steady_clock::now();
        if (phase!="complete" && now-last<std::chrono::seconds(1)) return;
        last=now;
        std::cerr<<"{\"phase\":"<<GetDAQ::EncodeJsonString(phase)<<",\"transferred_bytes\":"
                 <<GetDAQ::EncodeJsonString(std::to_string(bytes))<<"}\n";
    };
}
}

bool HandleTransferCommand(int argc,char **argv) {
    const std::string action=argc>1 ? argv[1] : std::string();
    if (argc==6 && (action=="--send-catalog-ssh" || action=="--resume-catalog-ssh")) {
        const bool resume=action=="--resume-catalog-ssh";
        auto last=std::chrono::steady_clock::now()-std::chrono::seconds(2);
        GetDAQ::CatalogTransferProgress progress=[&](const std::string &phase,std::uint64_t bytes) {
            const auto now=std::chrono::steady_clock::now();
            if (phase!="complete" && now-last<std::chrono::seconds(1)) return;
            last=now;
            std::cerr<<"{\"phase\":"<<GetDAQ::EncodeJsonString(phase)<<",\"sent_bytes\":"
                     <<GetDAQ::EncodeJsonString(std::to_string(bytes))<<"}\n";
        };
        std::cout<<(resume ? GetDAQ::ResumeReceiverCatalogOverSsh(argv[2],argv[3],argv[4],argv[5],progress) :
            GetDAQ::SendReceiverCatalogOverSsh(argv[2],argv[3],argv[4],argv[5],progress));
        return true;
    }
    if (argc==5 && (action=="--send-catalog-tcp" || action=="--resume-catalog-tcp")) {
        const bool resume=action=="--resume-catalog-tcp";
        std::cout<<GetDAQ::SendReceiverCatalogOverTcp(argv[2],argv[3],Port(argv[4],false),Progress(),resume);
        return true;
    }
    if ((argc==5 || argc==6) && action=="--listen-catalog-tcp") {
        const bool once=argc==6 && std::string(argv[5])=="--once";
        if (argc==6 && !once) throw std::invalid_argument("Use optional --once");
        GetDAQ::ListenReceiverCatalogOverTcp(argv[2],Port(argv[3],true),argv[4],[](unsigned int port) {
            std::cout<<"{\"phase\":\"listening\",\"port\":"<<port<<"}\n";
            std::cout.flush();
        },once);
        return true;
    }
    if (argc==3 && (action=="--receive-catalog-stream" || action=="--resume-catalog-stream")) {
        GetDAQ::ReceiveReceiverCatalogStream(argv[2],STDIN_FILENO,STDOUT_FILENO,action=="--resume-catalog-stream");
        return true;
    }
    if ((argc==4 || argc==6 || argc==8) && (action=="--copy-catalog" || action=="--resume-catalog")) {
        GetDAQ::CatalogTransferProgress progress;
        std::unique_ptr<GetDAQ::SupervisorIO::Directory> directory;
        std::string file;
        std::chrono::steady_clock::time_point last;
        if (argc>=6) {
            if (std::string(argv[4])!="--progress-file") throw std::invalid_argument("Use --progress-file METADATA_PATH");
            file=argv[5];
            GetDAQ::SupervisorIO::Path(file);
            directory.reset(new GetDAQ::SupervisorIO::Directory(file.substr(0,file.find_last_of('/'))));
            struct stat existing;
            if (!::lstat(file.c_str(),&existing) || errno!=ENOENT)
                throw std::invalid_argument("Progress destination exists or cannot be inspected");
            progress=[&](const std::string &phase,std::uint64_t bytes) {
                const auto now=std::chrono::steady_clock::now();
                if ((phase=="copying" || phase=="file-copied") && now-last<std::chrono::seconds(1)) return;
                last=now;
                directory->Check();
                const auto pending=file+".pending";
                GetDAQ::SupervisorIO::WriteNew(pending,"{\"schema\":1,\"phase\":"+GetDAQ::EncodeJsonString(phase)+
                    ",\"copied_bytes\":"+GetDAQ::EncodeJsonString(std::to_string(bytes))+"}\n");
                if (::rename(pending.c_str(),file.c_str()) || ::fsync(directory->Descriptor()))
                    throw std::runtime_error("Cannot publish copy progress metadata");
                directory->Check();
            };
        }
        std::string expected;
        if (argc==8) {
            if (std::string(argv[6])!="--expected-source-key") throw std::invalid_argument("Use --expected-source-key SHA256");
            expected=argv[7];
        }
        std::cout<<(action=="--resume-catalog" ?
            GetDAQ::ResumeReceiverCatalog(argv[2],argv[3],progress,expected) :
            GetDAQ::CopyReceiverCatalog(argv[2],argv[3],progress,expected));
        return true;
    }
    if (argc==3 && action=="--verify-transfer") {
        std::cout<<GetDAQ::VerifyTransferredCatalog(argv[2]);
        return true;
    }
    return false;
}
}
