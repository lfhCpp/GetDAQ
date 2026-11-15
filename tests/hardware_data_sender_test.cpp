#include "datasenderclient.h"
#include "datalinksetclient.h"
#include "hardwareclient.h"
#include <Ice/Ice.h>
#include <mdaq/DaqControl.h>
#include <arpa/inet.h>
#include <atomic>
#include <csignal>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>
namespace {
using namespace GetDAQ;
class PlainObject : public Ice::Object {};
void Require(bool ok,const std::string &message) { if (!ok) throw std::runtime_error(message); }
template<class F> void Rejects(F call,const std::string &message) {
    try { call(); } catch (const std::exception &e) { Require(std::string(e.what()).find(message) != std::string::npos,e.what()); return; }
    throw std::runtime_error("Expected rejection: "+message);
}
std::string File(const std::string &path) { std::ifstream in(path); return {std::istreambuf_iterator<char>(in),{}}; }
// 夹具仅替代VxWorks板端：控制走真实Ice TCP，数据另开plain TCP。不链接Qt。
class Sender : public mdaq::DaqCtrlNode {
public:
    mutable std::mutex mutex;
    std::vector<std::string> calls;
    std::atomic<bool> validType{true};
    std::atomic<int> failure{0}, failKind{0};
    std::atomic<int> starts{0}, mask{0};
    std::atomic<bool> always{false}, running{false};
    bool raw = false, truncated = false;
    int fd = -1;
    ~Sender() { if (fd >= 0) ::close(fd); }
    bool ice_isA(const std::string &id,const Ice::Current &c) const override {
        return validType && mdaq::DaqCtrlNode::ice_isA(id,c);
    }
    void Reset() {
        std::lock_guard<std::mutex> lock(mutex); calls.clear(); failure = failKind = starts = mask = 0;
        always = running = false; validType = true;
    }
    std::vector<std::string> Calls() { std::lock_guard<std::mutex> lock(mutex); return calls; }
    void Record(const std::string &text,const Ice::Current &c) {
        std::lock_guard<std::mutex> lock(mutex);
        calls.push_back(text+"/encoding="+std::to_string(c.encoding.minor));
    }
    void Fail(int action,const Ice::Current &c) {
        if (failure != action) return;
        if (failKind == 1) throw mdaq::utl::CmdException("Applied before error");
        if (failKind == 2) throw Ice::OperationNotExistException(__FILE__,__LINE__,c.id,c.facet,c.operation);
        if (failKind == 3) { ::usleep(350000); return; }
        c.con->close(true); // 应用后丢失应答，验证idempotent disconnect也不自动重发。
    }
    void connect(const std::string &type,const std::string &address,const Ice::Current &c) override {
        Record("connect:"+type+":"+address,c);
        if (raw) {
            if (fd >= 0) ::close(fd);
            fd = ::socket(AF_INET,SOCK_STREAM,0); if (fd < 0) throw mdaq::utl::CmdException("socket failed");
            const auto colon = address.find(':'); sockaddr_in target{}; target.sin_family = AF_INET;
            target.sin_port = htons(static_cast<uint16_t>(std::stoul(address.substr(colon+1))));
            ::inet_pton(AF_INET,address.substr(0,colon).c_str(),&target.sin_addr);
            if (::connect(fd,reinterpret_cast<sockaddr *>(&target),sizeof(target)) != 0) throw mdaq::utl::CmdException("raw connect failed");
        }
        Fail(1,c);
    }
    void setCircularBuffersEnabled(int value,const Ice::Current &c) override { mask = value; Record("mask:"+std::to_string(value),c); Fail(2,c); }
    void setAlwaysFlushData(bool value,const Ice::Current &c) override { always = value; Record(value ? "flush:true" : "flush:false",c); Fail(3,c); }
    void daqStart(const Ice::Current &c) override { running = true; ++starts; Record("start",c); Send(unsigned(starts),false); Fail(4,c); }
    void daqStop(const Ice::Current &c) override { running = false; Record("stop",c); Send(100+unsigned(starts),truncated); Fail(5,c); }
    void disconnect(const Ice::Current &c) override {
        Record("disconnect",c); if (fd >= 0) { ::close(fd); fd = -1; } Fail(6,c);
    }
    void shutdown(const Ice::Current &) override { throw mdaq::utl::CmdException("Do not shutdown test board"); }
    void Send(unsigned int event,bool shortFrame) {
        if (!raw) return;
        // 独立按收到的CoboFormats-Rev-4写头：4096字节，block64/header128/event@22。
        std::vector<unsigned char> data(4096,static_cast<unsigned char>(event));
        data[0] = 6; data[1] = data[2] = 0; data[3] = 64; data[4] = 7;
        data[5] = 0; data[6] = 1; data[7] = 4; data[8] = 0; data[9] = 2;
        data[10] = 0; data[11] = 4; const unsigned int items = (4096-128)/4;
        for (unsigned int i = 0; i < 4; ++i) { data[12+i] = static_cast<unsigned char>(items >> (24-8*i)); data[22+i] = static_cast<unsigned char>(event >> (24-8*i)); }
        data[26] = data[27] = 0; const std::size_t total = shortFrame ? 100 : data.size();
        std::size_t done = 0;
        while (done < total) {
            // 首段故意小于帧头，测试跨TCP读取的帧拼接。
            const auto count = done == 0 ? std::size_t(7) : total-done;
            const auto n = ::send(fd,data.data()+done,count,0);
            if (n <= 0) throw mdaq::utl::CmdException("raw send failed");
            done += static_cast<std::size_t>(n);
        }
    }
};
std::string Snapshot(DataSenderClient &client,Sender &sender) {
    const auto s = client.Snapshot();
    std::string json = "{\"state\":"+std::to_string(s.state)+",\"last_confirmed\":"+std::to_string(s.lastConfirmedState)+
        ",\"unknown\":"+(s.outcomeUnknown ? "true" : "false")+",\"issued\":"+std::to_string(s.issuedCalls)+
        ",\"acknowledged\":"+std::to_string(s.acknowledgedCalls)+",\"calls\":[";
    const auto calls = sender.Calls(); for (std::size_t i = 0; i < calls.size(); ++i) { if (i) json += ','; json += JsonString(calls[i]); }
    return json+"]}";
}
}
int main(int argc,char **argv) {
    std::signal(SIGPIPE,SIG_IGN);
    Ice::CommunicatorPtr communicator;
    try {
        communicator = Ice::initialize(); const auto adapter = communicator->createObjectAdapterWithEndpoints("DataSenderTest","tcp -h 127.0.0.1 -p 0");
        IceUtil::Handle<Sender> sender = new Sender;
        const auto proxy = communicator->proxyToString(adapter->addWithUUID(sender));
        const auto wrong = communicator->proxyToString(adapter->addWithUUID(new PlainObject)); adapter->activate();
        DataSenderOptions o; o.senderId = "CoBo0"; o.controlProxy = proxy; o.receiverAddress = "127.0.0.1:46005";
        o.circularBufferMask = 9; o.exclusiveStoppedOwnerConfirmed = true; o.timeoutMs = 1000;
        if (argc == 6 && std::string(argv[1]) == "--bridge") {
            o.receiverAddress = argv[2]; o.encodingMinor = std::stoi(argv[3]); const std::string directory = argv[4];
            sender->raw = true; sender->truncated = std::string(argv[5]) == "truncated";
            DataSenderClient client(o); std::cout << Snapshot(client,*sender) << std::endl;
            unsigned int sequence = 0; std::string command;
            while (std::getline(std::cin,command) && command != "quit") {
                try {
                    const auto audit = directory+"/operation-"+std::to_string(sequence++)+".jsonl";
                    if (command == "connect") client.Connect(audit);
                    else if (command == "start") client.Start(audit);
                    else if (command == "stop") client.Stop(audit);
                    else if (command == "disconnect") client.Disconnect(audit);
                    else if (command == "fail-disconnect") sender->failure = 6;
                    else if (command != "status") throw std::invalid_argument("Unknown test bridge command");
                    std::cout << Snapshot(client,*sender) << std::endl;
                } catch (const std::exception &e) { std::cout << "{\"error\":" << JsonString(e.what()) << ",\"snapshot\":" << Snapshot(client,*sender) << '}' << std::endl; }
            }
            communicator->destroy(); return 0;
        }
        Require(argc == 1,"Use no arguments, or --bridge IPv4:port ENCODING AUDITDIR normal|truncated");
        char folder[] = "/tmp/getdaq-data-sender-XXXXXX"; Require(mkdtemp(folder) != nullptr,"mkdtemp failed");
        unsigned int sequence = 0; const auto audit = [&] { return std::string(folder)+"/operation-"+std::to_string(sequence++)+".jsonl"; };
        for (int minor : {0,1}) {
            o.encodingMinor = minor; sender->Reset();
            DataSenderClient client(o); Require(sender->Calls().empty(),"Constructor sent a mutating call"); client.Validate();
            client.Connect(audit()); Require(sender->mask == 9 && !sender->always,"Connect policy not applied");
            client.Start(audit()); Rejects([&] { client.Disconnect(audit()); },"transition rejected");
            client.Stop(audit()); client.Start(audit()); client.Stop(audit()); client.Disconnect(audit());
            const auto calls = sender->Calls(); Require(calls.size() == 8 && calls[0].find("connect:TCP:") == 0 && calls[7].find("disconnect") == 0,"Call sequence/extra operation");
            for (const auto &call : calls) Require(call.find("encoding="+std::to_string(minor)) != std::string::npos,"Wrong Ice encoding");
            Require(client.Snapshot().state == DataSenderSnapshot::Disconnected && client.Snapshot().acknowledgedCalls == 8,"Successful state/ack count");
            Rejects([&] { client.Connect(audit()); },"transition rejected");
            sender->Reset();auto zbuf=o;zbuf.protocol="ZBUF";
            DataSenderClient zbufClient(zbuf);const auto zbufAudit=audit();zbufClient.Connect(zbufAudit);zbufClient.Start(audit());zbufClient.Stop(audit());zbufClient.Start(audit());zbufClient.Stop(audit());zbufClient.Disconnect(audit());
            Require(sender->Calls().size()==8 && sender->Calls()[0].find("connect:ZBUF:")==0,"ZBUF control/pause/resume lifecycle differs");
            Require(File(zbufAudit).find("\"protocol\":\"ZBUF\"")!=std::string::npos,"ZBUF actual control intent lacks protocol");
            sender->Reset();auto fdt=o;fdt.protocol="FDT";DataSenderClient fdtClient(fdt);fdtClient.Connect(audit());
            Require(!fdtClient.Snapshot().startIssued,"Connect fabricated a Start intent");
            fdtClient.Start(audit());fdtClient.Stop(audit());fdtClient.Start(audit());fdtClient.Stop(audit());fdtClient.Disconnect(audit());
            Require(fdtClient.Snapshot().startIssued && sender->Calls().size()==8 && sender->Calls()[0].find("connect:FDT:")==0,"FDT control/Start history differs");
            for(const auto &protocol:std::vector<std::string>{"zbuf","fdt","ice",""}) {auto invalid=o;invalid.protocol=protocol;Rejects([&] {DataSenderClient bad(invalid);},"protocol");}
            sender->Reset(); { DataSenderClient idle(o); idle.Connect(audit()); }
            Require(sender->Calls().size() == 3,"Destructor sent Stop/Disconnect");
            sender->Reset(); DataSenderClient untouched(o); const auto path = audit(); { std::ofstream existing(path); existing << "preserve"; }
            Rejects([&] { untouched.Connect(path); },"exclusively create"); Require(sender->Calls().empty() && File(path) == "preserve","Existing audit caused RPC/overwrite");
            const auto failBefore = [] { throw std::runtime_error("latched alarm"); };
            Rejects([&] { untouched.Connect(audit(),failBefore); },"latched alarm"); Require(untouched.Snapshot().state == DataSenderSnapshot::Fresh && sender->Calls().empty(),"Early checkpoint mutated");
            unsigned int checks = 0;
            Rejects([&] { untouched.Connect(audit(),[&] { if (++checks == 3) failBefore(); }); },"latched alarm");
            Require(untouched.Snapshot().state == DataSenderSnapshot::Fresh && !untouched.Snapshot().outcomeUnknown && sender->Calls().empty(),"Before-call checkpoint was marked remote unknown");
            checks = 0;
            Rejects([&] { untouched.Connect(audit(),[&] { if (++checks == 4) failBefore(); }); },"UNKNOWN");
            Require(sender->Calls().size() == 1 && untouched.Snapshot().lastConfirmedState == DataSenderSnapshot::Fresh,"Mid-connect checkpoint continued/committed");
            for (int action : {1,2,3,4,5,6}) {
                sender->Reset(); DataSenderClient failed(o);
                if (action >= 4) failed.Connect(audit());
                if (action >= 5) failed.Start(audit());
                if (action == 6) failed.Stop(audit());
                const auto previous = failed.Snapshot(); sender->failure = action; const auto failureAudit = audit();
                Rejects([&] {
                    if (action <= 3) failed.Connect(failureAudit);
                    else if (action == 4) failed.Start(failureAudit);
                    else if (action == 5) failed.Stop(failureAudit);
                    else failed.Disconnect(failureAudit);
                },"UNKNOWN");
                const auto snapshot = failed.Snapshot();
                const std::size_t expected = std::size_t(action);
                Require(sender->Calls().size() == expected && snapshot.outcomeUnknown && snapshot.state == DataSenderSnapshot::Unknown && snapshot.lastConfirmedState == previous.state,"Applied reply loss replayed/continued/committed");
                Require(snapshot.startIssued==(action>=4),"Reply loss erased or fabricated Start history used by FDT unused-source termination");
                Require(File(failureAudit).find("\"phase\":\"unknown\"") != std::string::npos,"Unknown not recorded");
                Rejects([&] { failed.Stop(audit()); },"transition rejected");
                Rejects([&] { failed.Disconnect(audit()); },"transition rejected");
                Require(sender->Calls().size() == expected,"Unknown was replayed during recovery attempt");
            }
            for (int kind : {1,2,3}) {
                sender->Reset(); auto timeout = o; if (kind == 3) timeout.timeoutMs = 100;
                DataSenderClient failed(timeout); failed.Connect(audit()); sender->failure = 4; sender->failKind = kind;
                Rejects([&] { failed.Start(audit()); },"UNKNOWN");
                if (kind == 3) ::usleep(400000); // 等夹具确实结束，禁止与下一场景并行改变它。
                Require(sender->Calls().size() == 4 && sender->running,"Typed exception/missing method/timeout replayed or rolled back");
            }
            sender->Reset(); sender->validType = false; DataSenderClient wrongType(o);
            const auto absent = audit(); Rejects([&] { wrongType.Connect(absent); },"does not implement");
            Require(::access(absent.c_str(),F_OK) != 0 && sender->Calls().empty() && wrongType.Snapshot().state == DataSenderSnapshot::Fresh,"Preflight type failure created intent or mutated");
            sender->validType = true; DataSenderClient changed(o); changed.Connect(audit()); sender->validType = false;
            Rejects([&] { changed.Start(audit()); },"does not implement"); Require(changed.Snapshot().state == DataSenderSnapshot::Connected && sender->Calls().size() == 3,"Later interface not rechecked");
            sender->validType = true;
            sender->Reset(); auto flush = o; flush.alwaysFlush = true; flush.circularBufferMask = 15;
            DataSenderClient immediate(flush); immediate.Connect(audit()); Require(sender->always && sender->mask == 15,"Explicit immediate flush/mask ignored");
            sender->Reset(); DataSenderClient ackFailed(o); struct rlimit saved; Require(::getrlimit(RLIMIT_FSIZE,&saved) == 0,"Cannot read file limit");
            std::signal(SIGXFSZ,SIG_IGN); bool limited = false; unsigned int at = 0;
            try {
                Rejects([&] { ackFailed.Connect(audit(),[&] {
                    if (++at == 6) { auto limit = saved; limit.rlim_cur = 1; Require(::setrlimit(RLIMIT_FSIZE,&limit) == 0,"Cannot set file limit"); limited = true; }
                }); },"UNKNOWN");
            } catch (...) { if (limited) ::setrlimit(RLIMIT_FSIZE,&saved); throw; }
            Require(::setrlimit(RLIMIT_FSIZE,&saved) == 0,"Cannot restore file limit");
            Require(sender->Calls().size() == 3 && ackFailed.Snapshot().outcomeUnknown && ackFailed.Snapshot().acknowledgedCalls == 3,"Completed RPC/failing audit committed local state or replayed");
            std::cout << "PASS sender lifecycle/unknown/preflight/checkpoint encoding 1." << minor << std::endl;
        }
        auto invalid = o; invalid.controlProxy = wrong; DataSenderClient wrongService(invalid); Rejects([&] { wrongService.Validate(); },"does not implement");
        for (const auto &address : {"", "localhost:46005", "127.0.0.1", "127.0.0.1:0", "127.0.0.1:0001", "127.0.0.1:65536", "127.0.0.1:+1", "0.0.0.0:123", "224.1.2.3:12", "255.255.255.255:12", "[::1]:123", "127.000.0.1:12"}) {
            invalid = o; invalid.receiverAddress = address; Rejects([&] { ValidateDataSenderOptions(invalid); },"receiver");
        }
        invalid = o; invalid.circularBufferMask = 0; Rejects([&] { DataSenderClient client(invalid); },"mask");
        invalid = o; invalid.circularBufferMask = 16; Rejects([&] { DataSenderClient client(invalid); },"mask");
        invalid = o; invalid.exclusiveStoppedOwnerConfirmed = false; Rejects([&] { DataSenderClient client(invalid); },"owner");
        invalid = o; invalid.encodingMinor = 2; Rejects([&] { DataSenderClient client(invalid); },"encoding");
        invalid = o; invalid.timeoutMs = 600001; Rejects([&] { DataSenderClient client(invalid); },"timeout");
        invalid = o; invalid.controlProxy = "DaqCtrlNode @ indirect"; Rejects([&] { DataSenderClient client(invalid); },"direct");
        ValidateDataSenderSet({o}); Rejects([&] { ValidateDataSenderSet({}); },"1..195");
        invalid = o; invalid.senderId = "CoBo1"; invalid.receiverAddress = "127.0.0.1:46006";
        Rejects([&] { ValidateDataSenderSet({o,invalid}); },"Duplicate");
        invalid.controlProxy = communicator->proxyToString(communicator->stringToProxy(o.controlProxy)->ice_timeout(777)->ice_compress(true));
        Rejects([&] { ValidateDataSenderSet({o,invalid}); },"Duplicate");
        invalid.controlProxy += ":tcp -h 127.0.0.2 -p 46003";
        Rejects([&] { ValidateDataSenderSet({o,invalid}); },"Duplicate");
        invalid.controlProxy = wrong; invalid.receiverAddress = o.receiverAddress; Rejects([&] { ValidateDataSenderSet({o,invalid}); },"Duplicate");
        invalid.receiverAddress = "127.0.0.1:46006"; invalid.senderId = o.senderId; Rejects([&] { ValidateDataSenderSet({o,invalid}); },"Duplicate");
        IceUtil::Handle<Sender> second = new Sender;
        invalid = o; invalid.senderId = "CoBo1"; invalid.receiverAddress = "127.0.0.1:46006";
        invalid.controlProxy = communicator->proxyToString(adapter->addWithUUID(second));
        sender->Reset(); second->validType = false; DataLinkSetClient badSet({o,invalid}); const auto noDirectory = audit();
        Rejects([&] { badSet.Connect(noDirectory); },"does not implement");
        Require(sender->Calls().empty() && ::access(noDirectory.c_str(),F_OK) != 0 && badSet.GetState() == DataLinkSetClient::State::Fresh,"Late set interface failure connected first sender");
        second->validType = true; second->failure = 2; const auto partialDirectory = audit();
        Rejects([&] { badSet.Connect(partialDirectory); },"UNKNOWN");
        Require(sender->Calls().size() == 3 && second->Calls().size() == 2 && badSet.GetState() == DataLinkSetClient::State::Failed,"Set partial failure cleaned up or continued");
        Rejects([&] { badSet.Connect(audit()); },"Fresh"); Rejects([&] { badSet.Disconnect(); },"recovery");
        sender->Reset(); second->Reset(); DataLinkSetClient goodSet({o,invalid}); const auto setDirectory = audit(); goodSet.Connect(setDirectory);
        goodSet.Sender(o.senderId).Start(audit());
        Rejects([&] { goodSet.Disconnect(); },"Connected/Stopped");
        Require(sender->Calls().size() == 4 && second->Calls().size() == 3,"Set disconnected running sender");
        goodSet.Sender(o.senderId).Stop(audit()); second->validType = false;
        Rejects([&] { goodSet.Disconnect(); },"does not implement");
        Require(sender->Calls().size() == 5,"Late disconnect interface failure partially disconnected");
        second->validType = true; goodSet.Disconnect(); Require(goodSet.GetState() == DataLinkSetClient::State::Disconnected,"Set disconnect not completed");
        Require(sender->Calls().size() == 6 && second->Calls().size() == 4,"Set disconnect sequence");
        Rejects([&] { goodSet.Sender("missing"); },"absent");
        sender->Reset(); second->Reset(); DataLinkSetClient stoppedSet({o,invalid}); stoppedSet.Connect(audit());
        sender->failure = 6; Rejects([&] { stoppedSet.Disconnect(); },"UNKNOWN");
        Require(sender->Calls().size() == 4 && second->Calls().size() == 3 && stoppedSet.GetState() == DataLinkSetClient::State::Failed,"Set disconnect replayed/continued/cleaned up after unknown");
        sender->Reset(); second->Reset(); DataLinkSetClient auditFailedSet({o,invalid}); auditFailedSet.Connect(audit());
        struct rlimit before; Require(::getrlimit(RLIMIT_FSIZE,&before) == 0,"Cannot read file limit");
        auto limited = before; limited.rlim_cur = 1; Require(::setrlimit(RLIMIT_FSIZE,&limited) == 0,"Cannot set file limit");
        try { Rejects([&] { auditFailedSet.Disconnect(); },"hardware audit"); }
        catch (...) { ::setrlimit(RLIMIT_FSIZE,&before); throw; }
        Require(::setrlimit(RLIMIT_FSIZE,&before) == 0,"Cannot restore file limit");
        Require(sender->Calls().size() == 3 && second->Calls().size() == 3 && auditFailedSet.GetState() == DataLinkSetClient::State::Failed,"Root audit failure sent disconnect or allowed journal reuse");
        Rejects([&] { auditFailedSet.Disconnect(); },"recovery");
        std::cout << "PASS global set preflight/partial connection/running guard/partial disconnect" << std::endl;
        std::cout << "PASS local bounded IPv4/options/direct endpoint/unique set validation" << std::endl;
        communicator->destroy(); return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << std::endl; if (communicator) communicator->destroy(); return 1; }
}
