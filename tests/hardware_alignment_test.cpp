#include "../apps/hardware/systemalignment.h"
#include "hardware_fixture.h"
#include <get/cobo/MutantLinkManager.h>
#include <get/mutant/AlignmentServer.h>
#include <algorithm>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
namespace {
using namespace GetDAQ;
using namespace GetDAQTest;
void Require(bool c,const char *message) { if (!c) throw std::runtime_error(message); }
template<class F> void Rejects(F call,const std::string &message) {
    try { call(); } catch (const std::exception &e) { Require(std::string(e.what()).find(message) != std::string::npos,e.what()); return; }
    throw std::runtime_error("Expected failure: "+message);
}
std::string ReadFile(const std::string &path) { std::ifstream f(path); return std::string(std::istreambuf_iterator<char>(f),{}); }
std::uint64_t Bits(Ice::Long value) { std::uint64_t result; std::memcpy(&result,&value,8); return result; }
Ice::Long Wire(std::uint64_t value) { Ice::Long result; std::memcpy(&result,&value,8); return result; }
std::uint64_t Mask(unsigned int width) { return width == 64 ? UINT64_MAX : (1ULL<<width)-1; }
struct AlignmentFixtureState {
    std::mutex mutex;
    std::map<std::string,std::uint64_t> values;
    unsigned int modes[4] = {0,0,0,0};
    std::vector<std::string> actions;
    int links = 0, tdc = 0, multi = 0, reads = 0, writes = 0, metadata = 0, failure = 0;
    std::string auditPath;
    void Reset() {
        std::lock_guard<std::mutex> lock(mutex); values.clear(); actions.clear();
        for (auto &mode : modes) mode = 0;
        links = tdc = multi = reads = writes = metadata = failure = 0; auditPath.clear();
        for (int i = 0; i < 3; ++i) {
            const auto id = "M"+std::to_string(i)+"/";
            values[id+"CARD_B_EMC/GLOBAL_CONF"] = i == 0 ? 1 : 0;
            values[id+"CARD_B_EMC/GMC_WSCA_SEL"] = i == 0 ? 16 : unsigned(i)<<2;
            values[id+"GPIO_CLK_TREE/CLOCK_TREE_CONFIG2"] = 0x1b2|(1<<9);
        }
    }
};
class Device : public TestDevice {
    AlignmentFixtureState &fState;
    std::string fId;
    FieldDescription Field(const std::string &reg,const std::string &name) const {
        for (const auto &r : fConfig.registers) if (r.descr.name == reg)
            for (const auto &f : r.fields) if (f.name == name) return f;
        throw mdaq::utl::CmdException("Missing original fixture field");
    }
public:
    Device(const std::string &id,const DeviceInfo &device,AlignmentFixtureState &state) : TestDevice(device.registerBytes),fState(state),fId(id) {
        fConfig.descr.name = device.name; fConfig.descr.registerAccess = device.access; fConfig.descr.baseAddress = Wire(device.baseAddress);
        fConfig.registers.clear();
        for (const auto &r : device.registers) {
            RegisterConfig reg; reg.descr.name = r.name; reg.descr.offset = Wire(r.offset);
            for (const auto &f : r.fields) {
                FieldDescription field; field.name = f.name; field.offset = f.offset; field.width = f.width;
                field.readOnly = false; reg.fields.push_back(field);
            }
            fConfig.registers.push_back(reg);
        }
    }
    void getDeviceConfig(DeviceConfig &out,const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.metadata; out = fConfig;
    }
    Ice::Long readField(const std::string &reg,const std::string &name,const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.reads; const auto field = Field(reg,name);
        return Wire((fState.values[fId+'/'+fConfig.descr.name+'/'+reg]>>field.offset)&Mask(field.width));
    }
    void writeField(const std::string &reg,const std::string &name,Ice::Long value,const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState.mutex); ++fState.writes; const auto field = Field(reg,name);
        const auto mask = Mask(field.width)<<field.offset; auto &old = fState.values[fId+'/'+fConfig.descr.name+'/'+reg];
        old = (old&~mask)|((Bits(value)<<field.offset)&mask);
        fState.actions.push_back(fId+".wsca="+std::to_string(bool(old&8)));
    }
};
class Link : public get::cobo::MutantLinkManager {
    AlignmentFixtureState &fState;
    unsigned int fIndex;
public:
    Link(AlignmentFixtureState &state,unsigned int index) : fState(state),fIndex(index) {}
    void setupMutantLink(get::cobo::MutantLinkMode mode,const Ice::Current &current) override {
        bool close = false;
        {
            std::lock_guard<std::mutex> lock(fState.mutex); ++fState.links; fState.modes[fIndex] = unsigned(mode);
            fState.actions.push_back("C"+std::to_string(fIndex)+".link="+std::to_string(unsigned(mode)));
            close = fState.failure == 1;
            if (fState.failure == 12) {
                struct rlimit limit; Require(getrlimit(RLIMIT_FSIZE,&limit) == 0,"Get audit limit");
                limit.rlim_cur = ReadFile(fState.auditPath).size()+1;
                Require(setrlimit(RLIMIT_FSIZE,&limit) == 0,"Limit audit completion");
            }
        }
        if (close) current.con->close(true);
    }
};
class Alignment : public get::mutant::AlignmentServer {
    AlignmentFixtureState &fState;
    unsigned int fIndex;
    void Multi(unsigned int step,get::mutant::Clock clock,get::mutant::MultiShelveAlignmentResults &out,const Ice::Current &current) {
        bool close = false;
        {
            std::lock_guard<std::mutex> lock(fState.mutex); ++fState.multi;
            fState.actions.push_back("M"+std::to_string(fIndex)+".multi="+std::to_string(unsigned(clock))+":"+std::to_string(step));
            fState.values["M"+std::to_string(fIndex)+"/GPIO_CLK_TREE/CLOCK_TREE_CONFIG1"] = 0xffffff;
            out.measuredDelay = -123; out.programmedDelay = 42; out.comment = fState.failure == 8 ? std::string(4097,'x') : "fixture report / 中文";
            close = fState.failure == 11 && fIndex == 1 && step == 2;
        }
        if (close) current.con->close(true);
    }
public:
    Alignment(AlignmentFixtureState &state,unsigned int index) : fState(state),fIndex(index) {}
    void alignDelayTDC(get::mutant::Clock clock,bool verbose,get::mutant::AlignmentResults &out,const Ice::Current &current) override {
        bool close = false, alarm = false;
        {
            std::lock_guard<std::mutex> lock(fState.mutex); Require(!verbose,"Single-crate verbose differs from received old host"); ++fState.tdc;
            fState.actions.push_back("M"+std::to_string(fIndex)+".tdc="+std::to_string(unsigned(clock)));
            unsigned int presentMask = 0;
            const unsigned int module[] = {0,1,2,1}, slots[] = {0,11,5,10};
            for (unsigned int slot = 0; slot < 12; ++slot) {
                get::mutant::SlotAlignmentResult row; row.isAmcPresent = false; row.amcDeltaDelay = 100; row.amcProgDelay = 7;
                for (unsigned int cobo = 0; cobo < 4; ++cobo)
                    if (module[cobo] == fIndex && slots[cobo] == slot && fState.modes[cobo] == unsigned(clock)+2) row.isAmcPresent = true;
                if (row.isAmcPresent) presentMask |= 1u<<slot;
                if (fState.failure == 6 && clock == get::mutant::WSCA) row.amcDeltaDelay = 10101;
                out[Ice::Short(slot)] = row;
            }
            if (fState.failure == 4 && clock == get::mutant::WSCA && fIndex == 0) {
                std::swap(out[0].isAmcPresent,out[1].isAmcPresent);
            }
            if (fState.failure == 9) { out[-1] = out[0]; out.erase(0); }
            if (fState.failure == 5) out[12] = out[0];
            auto &mask = fState.values["M"+std::to_string(fIndex)+"/GPIO_CLK_TREE/CLOCK_TREE_CONFIG1"];
            const unsigned int shift = unsigned(clock)*12;
            mask = (mask&~(0xfffULL<<shift))|(std::uint64_t(presentMask)<<shift);
            if (fState.failure == 7) mask ^= 1u<<shift;
            close = fState.failure == 2; alarm = fState.failure == 3;
        }
        if (alarm) throw get::mutant::AlignmentAlarm("Board procedure may have changed delay lines before alarm");
        if (close) current.con->close(true);
    }
    void multiShelveAlignDelayStep1(get::mutant::Clock c,bool v,get::mutant::MultiShelveAlignmentResults &r,const Ice::Current &x) override { Require(v,"multi verbose"); Multi(1,c,r,x); }
    void multiShelveAlignDelayStep2(get::mutant::Clock c,bool v,get::mutant::MultiShelveAlignmentResults &r,const Ice::Current &x) override { Require(v,"multi verbose"); Multi(2,c,r,x); }
    void multiShelveAlignDelayStep3(get::mutant::Clock c,bool v,get::mutant::MultiShelveAlignmentResults &r,const Ice::Current &x) override { Require(v,"multi verbose"); Multi(3,c,r,x); }
};
SystemAlignmentOptions Install(Ice::CommunicatorPtr communicator,Ice::ObjectAdapterPtr adapter,AlignmentFixtureState &state,const HardwareDescription &mutant,const HardwareDescription &cobo)
{
    SystemAlignmentOptions options; options.timeoutMs = 1000; options.exclusiveOwnerConfirmed = options.daqStoppedConfirmed = true;
    for (unsigned int i = 0; i < 3; ++i) {
        AlignmentMutantEndpoint m; m.id = m.expectedNodeName = "M"+std::to_string(i); m.master = i == 0; m.description = mutant;
        IceUtil::Handle<TestNode> node = new TestNode; node->fName = m.id;
        for (const auto &device : mutant.devices) node->fDevices[device.name] = mdaq::hw::DevicePrx::uncheckedCast(adapter->addWithUUID(new Device(m.id,device,state)));
        m.nodeProxy = communicator->proxyToString(adapter->addWithUUID(node));
        m.alignmentProxy = communicator->proxyToString(adapter->addWithUUID(new Alignment(state,i))); options.mutants.push_back(m);
    }
    for (unsigned int i = 0; i < 4; ++i) {
        AlignmentCoBoEndpoint c; c.id = c.expectedNodeName = "C"+std::to_string(i); c.description = cobo; c.enabled = i != 3;
        IceUtil::Handle<TestNode> node = new TestNode; node->fName = c.id;
        for (const auto &device : cobo.devices) node->fDevices[device.name] = mdaq::hw::DevicePrx::uncheckedCast(adapter->addWithUUID(new Device(c.id,device,state)));
        c.nodeProxy = communicator->proxyToString(adapter->addWithUUID(node));
        c.linkProxy = communicator->proxyToString(adapter->addWithUUID(new Link(state,i))); options.cobos.push_back(c);
    }
    return options;
}
std::string TopologyJson(const SystemAlignmentOptions &options,const std::string &mutantPath,const std::string &coboPath)
{
    std::string json = "{\"schema_version\":1,\"kind\":\"getdaq.alignment-topology\",\"encoding\":\"1."+std::to_string(options.encodingMinor)+"\",\"timeout_ms\":1000,\"mutants\":[";
    for (std::size_t i = 0; i < options.mutants.size(); ++i) {
        if (i) json += ',';
        const auto &m = options.mutants[i];
        json += "{\"id\":"+JsonString(m.id)+",\"expected_node_name\":"+JsonString(m.expectedNodeName)+",\"node_proxy\":"+JsonString(m.nodeProxy)+
            ",\"alignment_proxy\":"+JsonString(m.alignmentProxy)+",\"role\":"+JsonString(m.master ? "master" : "slave")+",\"hardware_description_file\":"+JsonString(mutantPath)+"}";
    }
    json += "],\"cobos\":[";
    for (std::size_t i = 0; i < options.cobos.size(); ++i) {
        if (i) json += ',';
        const auto &c = options.cobos[i];
        json += "{\"id\":"+JsonString(c.id)+",\"expected_node_name\":"+JsonString(c.expectedNodeName)+",\"node_proxy\":"+JsonString(c.nodeProxy)+
            ",\"link_proxy\":"+JsonString(c.linkProxy)+",\"enabled\":"+(c.enabled ? "true" : "false")+",\"hardware_description_file\":"+JsonString(coboPath)+"}";
    }
    return json+"]}";
}
}
int main(int argc,char **argv)
{
    Ice::CommunicatorPtr communicator;
    try {
        Require(argc == 3 || argc == 6 || argc == 7,"Pass fixtures; optional --serve DIRECTORY FAILURE or --record OUTPUT MODULES ACTIVE_MASK");
        const auto mutant = LoadHardwareDescription(argv[1]), cobo = LoadHardwareDescription(argv[2]);
        Require(mutant.devices.size() == 5 && cobo.devices.size() == 11,"Wrong original description fixtures");
        communicator = Ice::initialize(); auto adapter = communicator->createObjectAdapterWithEndpoints("AlignmentTest","tcp -h 127.0.0.1 -p 0");
        AlignmentFixtureState state; auto options = Install(communicator,adapter,state,mutant,cobo); adapter->activate();
        if (argc == 7) {
            Require(std::string(argv[3]) == "--record","Expected --record");
            const auto modules = ParseUnsigned(argv[5]), enabled = ParseUnsigned(argv[6]);
            Require(modules >= 1 && modules <= 3 && enabled < (1u<<modules),"Recording topology bounds");
            state.Reset(); options.mutants.resize(std::size_t(modules));
            state.values["M0/CARD_B_EMC/GMC_WSCA_SEL"] = modules > 1 ? 16 : 0;
            for (unsigned int i = 0; i < 4; ++i) options.cobos[i].enabled = (enabled>>i)&1;
            char folder[] = "/tmp/getdaq-alignment-record-XXXXXX"; Require(mkdtemp(folder) != nullptr,"Recording root");
            SystemAlignmentClient client(options); const auto result = client.Align(std::string(folder)+"/audit");
            std::ofstream output(argv[4]); output << "{\"actions\":[";
            for (std::size_t i = 0; i < state.actions.size(); ++i) { if (i) output << ','; output << JsonString(state.actions[i]); }
            output << "],\"located\":[";
            for (std::size_t i = 0; i < result.located.size(); ++i) {
                if (i) output << ',';
                const auto &r = result.located[i]; output << '[' << JsonString(r.coboId) << ',' << JsonString(r.mutantId) << ',' << r.amcSlot << ']';
            }
            output << "],\"masks\":[";
            for (std::size_t i = 0; i < result.crates.size(); ++i) {
                if (i) output << ',';
                output << '[' << result.crates[i].gmcShelfMask << ',' << result.crates[i].wscaShelfMask << ']';
            }
            output << "]}\n"; output.close(); Require(bool(output),"Write reference action projection");
            communicator->destroy(); return 0;
        }
        if (argc == 6) {
            Require(std::string(argv[3]) == "--serve","Expected --serve"); state.Reset(); state.failure = int(ParseUnsigned(argv[5]));
            const std::string directory = argv[4]; std::ofstream topology(directory+"/topology.json");
            topology << TopologyJson(options,argv[1],argv[2]); topology.close(); Require(bool(topology),"Write fixture topology");
            std::cout << "{\"ready\":true}" << std::endl; std::string stop; std::getline(std::cin,stop);
            std::cout << "{\"links\":" << state.links << ",\"tdc\":" << state.tdc << ",\"multi\":" << state.multi << ",\"writes\":" << state.writes << "}" << std::endl;
            communicator->destroy(); return 0;
        }
        char folder[] = "/tmp/getdaq-alignment-tcp-XXXXXX"; Require(mkdtemp(folder) != nullptr,"Create alignment test root");
        unsigned int number = 0; const auto directory = [&] { return std::string(folder)+"/run-"+std::to_string(number++); };
        for (int minor : {0,1}) {
            options.encodingMinor = minor; state.Reset();
            // Validate共享执行预检查必须只有元数据，没有寄存器读写或经理RPC。
            HardwareClient check(options.mutants[0].nodeProxy,minor,1000); check.UseDescription(mutant); check.Validate(MutantWscaClock(true));
            Require(state.metadata && !state.reads && !state.writes && !state.links && !state.tdc,"Validate executed hardware operations");
            state.Reset(); SystemAlignmentClient client(options); const auto path = directory(); const auto result = client.Align(path);
            Require(result.located.size() == 3 && result.located[0].amcSlot == 1 && result.located[1].amcSlot == 12 && result.located[2].amcSlot == 6,"AMC slot convention differs");
            Require(result.crates[0].gmcShelfMask == 1 && result.crates[1].gmcShelfMask == 2048 && result.crates[2].gmcShelfMask == 32,"Local masks lost before multi-crate overwrite");
            Require(result.interCrate.size() == 24 && state.multi == 24 && state.writes == 6,"Multi-crate actions/WSCA sequence differs");
            for (auto mode : state.modes) Require(mode == 0,"Inactive CoBo not restored at successful completion");
            for (int m = 0; m < 3; ++m) Require(state.values["M"+std::to_string(m)+"/GPIO_CLK_TREE/CLOCK_TREE_CONFIG2"] == (0x1ba|(1<<9)),"WSCA changed unrelated bits");
            for (std::size_t i = 0; i < result.interCrate.size(); i += 2) {
                const auto &a = result.interCrate[i], &b = result.interCrate[i+1];
                Require(a.mutantId == "M0" && b.mutantId != "M0" && a.clock == b.clock && a.step == b.step,"Master/slave RPC order differs");
            }
            const auto journal = ReadFile(path+"/system.jsonl");
            Require(journal.find("\"physical_alignment_verified\":false") != std::string::npos && journal.find("\"bound_description\"") != std::string::npos,"Audit lost boundary or bound policy");
            struct stat info; Require(stat(path.c_str(),&info) == 0 && (info.st_mode&0777) == 0700,"Audit directory permissions");
            Require(client.GetState() == SystemAlignmentClient::State::Complete,"Success state");
            const auto calls = state.actions.size(); Rejects([&] { client.Align(directory()); },"single-use"); Require(calls == state.actions.size(),"Complete operation replayed");
            std::cout << "PASS three-crate topology/AMC1,12,6/preserved masks/multi order encoding 1." << minor << std::endl;
            state.Reset(); auto single = options; single.mutants.resize(1); single.cobos.resize(1);
            state.values["M0/CARD_B_EMC/GMC_WSCA_SEL"] = 0;
            SystemAlignmentClient mono(single); Require(mono.Align(directory()).interCrate.empty() && !state.multi && state.writes == 1,"Single crate called multi or omitted final WSCA");
            state.Reset(); auto gate = options; gate.exclusiveOwnerConfirmed = false; SystemAlignmentClient noOwner(gate);
            Rejects([&] { noOwner.Align(directory()); },"explicit exclusive"); Require(!state.metadata && !state.actions.size(),"Missing owner connected hardware");
            auto invalid = options; invalid.mutants[1].master = true;
            Rejects([&] { SystemAlignmentClient bad(invalid); },"one explicit master");
            invalid = options; for (auto &cobo : invalid.cobos) cobo.enabled = false;
            Rejects([&] { SystemAlignmentClient noActive(invalid); },"at least one active CoBo");
            state.Reset(); invalid = options; invalid.cobos.back().expectedNodeName = "wrong";
            SystemAlignmentClient wrong(invalid); Rejects([&] { wrong.Align(directory()); },"name differs"); Require(state.actions.empty(),"Late wrong node allowed early mutation");
            state.Reset(); invalid = options; invalid.cobos.back().linkProxy = invalid.cobos.back().nodeProxy;
            SystemAlignmentClient missing(invalid); Rejects([&] { missing.Align(directory()); },"implement MutantLinkManager"); Require(state.actions.empty(),"Late manager type allowed mutation");
            state.Reset(); invalid = options;
            for (auto &d : invalid.mutants.back().description.devices) for (auto &r : d.registers) if (r.name == "CLOCK_TREE_CONFIG2") r.readOnly = true;
            SystemAlignmentClient denied(invalid); Rejects([&] { denied.Align(directory()); },"read-only"); Require(state.actions.empty(),"Late local read-only allowed early link writes");
            state.Reset(); state.values["C3/ctrl/pipeCtrl"] = 1;
            // 使用收到的实际firmwareEnable位置，不依赖猜测。
            for (const auto &d : cobo.devices) if (d.name == "ctrl") for (const auto &r : d.registers) if (r.name == "pipeCtrl")
                for (const auto &f : r.fields) if (f.name == "firmwareEnable") state.values["C3/ctrl/pipeCtrl"] = 1ULL<<f.offset;
            SystemAlignmentClient running(options); Rejects([&] { running.Align(directory()); },"both be zero"); Require(state.actions.empty(),"Inactive running CoBo allowed writes");
            state.Reset(); state.values["M2/CARD_B_EMC/GLOBAL_CONF"] = 1;
            SystemAlignmentClient role(options); Rejects([&] { role.Align(directory()); },"role/clock"); Require(state.actions.empty(),"Incorrect actual slave role wrote links");
            const int failures[] = {1,2,3,4,5,6,7,8,9,11};
            const char *messages[] = {"UNKNOWN","UNKNOWN","UNKNOWN","different AMC","more than 12","10000 ps","shelf masks","comment exceeds","outside 0..11","UNKNOWN"};
            for (unsigned int i = 0; i < sizeof(failures)/sizeof(failures[0]); ++i) {
                state.Reset(); state.failure = failures[i]; SystemAlignmentClient broken(options); const auto failed = directory();
                Rejects([&] { broken.Align(failed); },messages[i]); Require(broken.GetState() == SystemAlignmentClient::State::Failed,"Failure state absent");
                const auto stopped = state.actions.size(); Rejects([&] { broken.Align(directory()); },"single-use"); Require(stopped == state.actions.size(),"Failure replayed or restored");
                if (failures[i] == 1) Require(state.links == 1 && !state.tdc && !state.writes,"Lost link reply replayed");
                if (failures[i] == 2 || failures[i] == 3) Require(state.tdc == 1 && !state.writes && state.modes[0] == 2,"Failed TDC automatically restored");
                if (failures[i] == 11) Require(state.multi == 4 && !state.writes,"Lost multi reply replayed or restored");
                Require(ReadFile(failed+"/system.jsonl").find("\"automatic_restoration\":false") != std::string::npos,"Failure policy not durable");
            }
            state.Reset(); const auto existing = directory(); Require(mkdir(existing.c_str(),0700) == 0,"Create collision");
            SystemAlignmentClient collision(options); Rejects([&] { collision.Align(existing); },"exclusively create"); Require(state.actions.empty() && !state.metadata,"Existing audit dir contacted nodes");
            state.Reset(); const auto truncated = directory(); state.auditPath = truncated+"/system.jsonl"; state.failure = 12;
            struct rlimit original; Require(getrlimit(RLIMIT_FSIZE,&original) == 0,"Get file size limit"); const auto oldSignal = std::signal(SIGXFSZ,SIG_IGN);
            SystemAlignmentClient auditFailure(options); bool caught = false;
            try { auditFailure.Align(truncated); } catch (const std::exception &e) { caught = std::string(e.what()).find("acknowledged but audit failed") != std::string::npos; }
            Require(setrlimit(RLIMIT_FSIZE,&original) == 0,"Restore file size limit"); std::signal(SIGXFSZ,oldSignal);
            Require(caught && state.links == 1 && !state.tdc,"Acknowledged link replayed after partial audit failure");
            std::cout << "PASS preflight/owner/stopped/roles/unknown/no restoration/audit partial write encoding 1." << minor << std::endl;
        }
        communicator->destroy(); std::cout << "All native alignment tests passed; no real hardware used" << std::endl; return 0;
    } catch (const std::exception &e) { if (communicator) communicator->destroy(); std::cerr << e.what() << std::endl; return 1; }
}
