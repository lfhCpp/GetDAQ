#include <Ice/Ice.h>
#include <get/GetEcc.h>
#include <iostream>
#include <stdexcept>
#include <string>

// 显式地址、只读、默认旧版 1.0 编码；不包含任何硬件写操作。
namespace {
void Usage() {
    std::cerr << "Usage: getdaq-ice-check ping|status 'identity:tcp -h HOST -p PORT' [1.0|1.1]\n"
                 "status requires a GET StateMachine object; default encoding is 1.0.\n";
}
}

int main(int argc, char **argv) {
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        Usage();
        return 0;
    }
    if(argc < 3 || argc > 4 ||
       (std::string(argv[1]) != "ping" && std::string(argv[1]) != "status") ||
       (argc == 4 && std::string(argv[3]) != "1.0" && std::string(argv[3]) != "1.1")) {
        Usage();
        return 2;
    }
    Ice::CommunicatorPtr communicator;
    try {
        Ice::InitializationData init;
        init.properties = Ice::createProperties();
        init.properties->setProperty("Ice.RetryIntervals", "-1");
        init.properties->setProperty("Ice.Override.ConnectTimeout", "3000");
        init.properties->setProperty("Ice.Override.Timeout", "3000");
        communicator = Ice::initialize(init);
        Ice::EncodingVersion encoding; encoding.major = 1;
        encoding.minor = argc == 4 && std::string(argv[3]) == "1.1" ? 1 : 0;
        Ice::ObjectPrx proxy = communicator->stringToProxy(argv[2]);
        if(!proxy) throw std::runtime_error("Empty proxy");
        proxy = proxy->ice_collocationOptimized(false)->ice_encodingVersion(encoding);
        if(std::string(argv[1]) == "ping") {
            proxy->ice_ping();
            std::cout << "PING OK encoding=1." << int(encoding.minor) << '\n';
        } else {
            get::rc::StateMachinePrx stateMachine = get::rc::StateMachinePrx::checkedCast(proxy);
            if(!stateMachine) throw std::runtime_error("Object is not a GET StateMachine");
            get::rc::Status status;
            stateMachine->getStatus(status);
            std::cout << "state=" << int(status.s) << " error=" << int(status.e)
                      << " encoding=1." << int(encoding.minor) << '\n';
        }
        communicator->destroy();
        return 0;
    } catch(const std::exception &e) {
        std::cerr << "Ice check failed: " << e.what() << '\n';
        if(communicator) { try { communicator->destroy(); } catch(...) {} }
        return 1;
    }
}
