#include "icesourceclient.h"
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
namespace {
volatile std::sig_atomic_t stopped=0;
void Stop(int) {stopped=1;}
std::uint64_t Number(const std::string &value,std::uint64_t minimum,std::uint64_t maximum) {
    if(value.empty() || value.find_first_not_of("0123456789")!=std::string::npos) throw std::invalid_argument("ICE send integer option invalid");
    const auto result=std::stoull(value);if(result<minimum || result>maximum) throw std::invalid_argument("ICE send integer option out of bounds");return result;
}
class Input {
public:
    int fd=-1;bool owned=false;
    ~Input() {if(owned && fd>=0) ::close(fd);}
};
}
int main(int argc,char **argv) {
    try {
        GetDAQ::IceSourceOptions options;std::string input;std::uint64_t maximum=0;
        for(int i=1;i<argc;++i) {
            const std::string name=argv[i];
            if(name=="--help") {
                std::cout<<"getdaq-ice-send --input FILE|- [--host IPv4] [--port 46105] [--session-id HEX64]\n"
                    "  [--encoding 0|1] [--chunk-kib 256] [--timeout-ms 10000] [--max-bytes N]\n"
                    "Native data-source tool, no Qt. Input EOF explicitly ends the same ICE connection.\n"
                    "Oneway byte counts mean local handoff; finish ACK is not a disk-seal proof.\n"
                    "Read/send errors or signals never send an implicit finish; no automatic reconnect/retry.\n";return 0;
            }
            if(++i>=argc) throw std::invalid_argument("Missing ICE send argument");
            const std::string value=argv[i];
            if(name=="--input") input=value;
            else if(name=="--host") options.address=value;
            else if(name=="--port") options.port=unsigned(Number(value,1,65535));
            else if(name=="--session-id") options.expectedSession=value;
            else if(name=="--encoding") options.encodingMinor=unsigned(Number(value,0,1));
            else if(name=="--chunk-kib") options.chunkBytes=std::size_t(Number(value,1,65535)*1024);
            else if(name=="--timeout-ms") options.timeoutMs=unsigned(Number(value,100,600000));
            else if(name=="--max-bytes") maximum=Number(value,1,9223372036854775807ull);
            else throw std::invalid_argument("Unknown ICE send option");
        }
        if(input.empty()) throw std::invalid_argument("ICE send --input FILE|- is required");
        GetDAQ::IceSourceClient source(options);Input stream;
        if(input=="-") stream.fd=STDIN_FILENO;
        else {
            stream.fd=::open(input.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);stream.owned=true;struct stat info;
            if(stream.fd<0 || ::fstat(stream.fd,&info) || !S_ISREG(info.st_mode) || info.st_size<0) throw std::runtime_error("ICE send input must be a readable regular file or stdin");
            if(maximum && std::uint64_t(info.st_size)>maximum) throw std::runtime_error("ICE send input exceeds explicit byte limit before connection");
        }
        std::signal(SIGINT,Stop);std::signal(SIGTERM,Stop);std::signal(SIGPIPE,SIG_IGN);
        std::vector<std::uint8_t> block(options.chunkBytes);source.Connect();std::uint64_t bytes=0;
        for (;;) {
            if(stopped) throw std::runtime_error("ICE send interrupted; no source end issued");
            pollfd ready={stream.fd,POLLIN,0};const int available=::poll(&ready,1,100);
            if(available<0) {if(errno==EINTR) continue;throw std::runtime_error("ICE send input poll failed; no source end issued");}
            if(!available) continue;
            const auto count=::read(stream.fd,block.data(),block.size());
            if(count<0) {if(errno==EINTR) continue;throw std::runtime_error("ICE send input read failed; no source end issued");}
            if(!count) break;
            if(maximum && std::uint64_t(count)>maximum-bytes) throw std::runtime_error("ICE send stream exceeded byte limit; no source end issued");
            source.Send(block.data(),std::size_t(count));bytes+=std::uint64_t(count);
        }
        if(stopped) throw std::runtime_error("ICE send interrupted at EOF; no source end issued");
        source.Finish();const auto result=source.Status();
        std::cout<<"{\"service\":\"getdaq-ice-send\",\"locally_sent_bytes\":\""<<result.locallySentBytes<<"\",\"source_end_acknowledged\":true,\"receiver_seal_verified\":false}\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
