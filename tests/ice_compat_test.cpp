#include <Ice/Ice.h>
#include <get/GetEcc.h>
#include <iostream>
#include <stdexcept>

// 使用真实 GET 接口做 TCP 回环验证；所有控制操作明确拒绝。
class TestStateMachine : public get::rc::StateMachine {
public:
    void describe(const std::string &, const Ice::Current &) { Reject(); }
    void prepare(const std::string &, const Ice::Current &) { Reject(); }
    void reset(const Ice::Current &) { Reject(); }
    void configure(const std::string &, const std::string &, const Ice::Current &) { Reject(); }
    void start(const Ice::Current &) { Reject(); }
    void stop(const Ice::Current &) { Reject(); }
    void pause(const Ice::Current &) { Reject(); }
    void resume(const Ice::Current &) { Reject(); }
    void breakup(const Ice::Current &) { Reject(); }
    void getStatus(get::rc::Status &status, const Ice::Current &) {
        status.s = get::rc::Paused; status.e = get::rc::NoErr;
    }
    std::string getDataLinks(const Ice::Current &) { return "CoBo 中文 / TCP"; }
private:
    static void Reject() { throw mdaq::utl::CmdException("test rejects hardware changes / 中文"); }
};

int main() {
    Ice::CommunicatorPtr communicator;
    try {
        Ice::InitializationData init;
        init.properties = Ice::createProperties();
        init.properties->setProperty("Ice.RetryIntervals", "-1");
        communicator = Ice::initialize(init);
        Ice::ObjectAdapterPtr adapter = communicator->createObjectAdapterWithEndpoints(
            "CompatibilityTest", "tcp -h 127.0.0.1 -p 0");
        Ice::ObjectPrx base = adapter->add(new TestStateMachine, communicator->stringToIdentity("test"));
        adapter->activate();
        for(int minor = 0; minor != 2; ++minor) {
            Ice::EncodingVersion encoding; encoding.major = 1; encoding.minor = minor;
            // 禁止同进程捷径，否则不会检验网络编码。
            get::rc::StateMachinePrx proxy = get::rc::StateMachinePrx::checkedCast(
                base->ice_collocationOptimized(false)->ice_timeout(2000)->ice_encodingVersion(encoding));
            if(!proxy || !proxy->ice_getConnection()) throw std::runtime_error("No TCP connection");
            proxy->ice_ping();
            get::rc::Status status;
            proxy->getStatus(status);
            if(status.s != get::rc::Paused || status.e != get::rc::NoErr ||
               proxy->getDataLinks() != "CoBo 中文 / TCP") throw std::runtime_error("Reply mismatch");
            bool rejected = false;
            try { proxy->configure("<ConfigId>中文</ConfigId>", "unused"); }
            catch(const mdaq::utl::CmdException &e) {
                rejected = e.reason == "test rejects hardware changes / 中文";
            }
            if(!rejected) throw std::runtime_error("Typed exception mismatch");
            std::cout << "PASS GET TCP encoding 1." << minor << ": status, UTF-8, typed exception\n";
        }
        communicator->destroy();
        return 0;
    } catch(const std::exception &e) {
        std::cerr << e.what() << '\n';
        if(communicator) { try { communicator->destroy(); } catch(...) {} }
        return 1;
    }
}
