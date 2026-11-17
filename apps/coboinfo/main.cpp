// Direct board metadata only. No ECC lifecycle or hardware write entry points.
#include <Ice/Ice.h>
#include <mdaq/hw/Control.h>
#include <mdaq/utl/CmdException.h>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
std::string Json(const std::string &text){
    std::string result="\"";const char hex[]="0123456789abcdef";
    for(unsigned char c:text){if(c=='"' || c=='\\'){result+='\\';result+=char(c);}else if(c<32){result+="\\u00";result+=hex[c>>4];result+=hex[c&15];}else result+=char(c);}return result+'"';
}
void Check(bool ok,const char *message){if(!ok)throw std::invalid_argument(message);}
}
int main(int argc,char **argv){
    Ice::CommunicatorPtr communicator;
    try{
        if(argc==2 && std::string(argv[1])=="--help"){
            std::cout<<"getdaq-cobo-info devices PROXY\ngetdaq-cobo-info read-register PROXY ctrl hardwareVersion|firmwareRelease|firmwareDate\ngetdaq-cobo-info read-field PROXY ctrl pipeCtrl firmwareEnable|intEnable\nRead-only; Ice encoding 1.0, bounded RPCs, no retries.\n";return 0;
        }
        Check(argc>=3,"Expected a read-only command and HwNode proxy");
        const std::string command=argv[1],proxy=argv[2];
        const bool devices=command=="devices" && argc==3;
        const bool reg=command=="read-register" && argc==5 && std::string(argv[3])=="ctrl" && (std::string(argv[4])=="hardwareVersion" || std::string(argv[4])=="firmwareRelease" || std::string(argv[4])=="firmwareDate");
        const bool field=command=="read-field" && argc==6 && std::string(argv[3])=="ctrl" && std::string(argv[4])=="pipeCtrl" && (std::string(argv[5])=="firmwareEnable" || std::string(argv[5])=="intEnable");
        Check(devices || reg || field,"Only fixed CoBo metadata reads are supported; no write/control commands");
        Check(!proxy.empty() && proxy.size()<=512,"Invalid board proxy");
        Ice::InitializationData init;init.properties=Ice::createProperties();
        init.properties->setProperty("Ice.RetryIntervals","-1");init.properties->setProperty("Ice.Override.ConnectTimeout","4000");init.properties->setProperty("Ice.Override.Timeout","4000");init.properties->setProperty("Ice.MessageSizeMax","1024");
        communicator=Ice::initialize(init);Ice::EncodingVersion encoding;encoding.major=1;encoding.minor=0;
        const auto base=communicator->stringToProxy(proxy);Check(bool(base),"Empty board proxy");
        const auto node=mdaq::hw::NodePrx::checkedCast(base->ice_twoway()->ice_collocationOptimized(false)->ice_timeout(4000)->ice_encodingVersion(encoding));Check(bool(node),"Object does not implement mdaq::hw::Node");
        if(devices){
            const auto name=node->name();mdaq::hw::DeviceList names;node->getListOfDevices(names);Check(name.size()<=4096 && names.size()<=4096,"Board metadata exceeds limit");
            std::cout<<"{\"node\":"<<Json(name)<<",\"devices\":[";bool first=true;
            for(const auto &name:names){Check(name.size()<=4096,"Device name exceeds limit");if(!first)std::cout<<',';first=false;std::cout<<Json(name);}std::cout<<"]}\n";
        }else{
            const auto found=node->findDevice("ctrl");Check(bool(found),"Board has no ctrl device");
            const auto device=mdaq::hw::DevicePrx::uncheckedCast(found->ice_twoway()->ice_collocationOptimized(false)->ice_timeout(4000)->ice_encodingVersion(encoding));
            const auto value=reg?device->readRegister(argv[4]):device->readField("pipeCtrl",argv[5]);
            std::cout<<"{\"value\":"<<Json(std::to_string(static_cast<std::uint64_t>(value)))<<"}\n";
        }
        communicator->destroy();return 0;
    }catch(const mdaq::utl::CmdException &error){std::cerr<<error.reason<<'\n';}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';}
    if(communicator){try{communicator->destroy();}catch(...){}}
    return 1;
}
