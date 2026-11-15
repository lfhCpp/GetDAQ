#include "systemdescription.h"
#include "hardware_fixture.h"
#include "eccstatemachine.h"
#ifdef GETDAQ_TEST_ECC_JOURNAL
#include "eccjournal.h"
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>
namespace {
using namespace GetDAQ;
using namespace GetDAQTest;
void Require(bool v,const char *why) { if (!v) throw std::runtime_error(why); }
template<class F> void Rejects(F action,const std::string &text) {
    try { action(); } catch (const std::exception &e) {
        if (std::string(e.what()).find(text)==std::string::npos) throw std::runtime_error("Expected "+text+", got "+e.what());
        return;
    }
    throw std::runtime_error("Expected rejection: "+text);
}
std::string File(const std::string &path) { std::ifstream f(path); return std::string(std::istreambuf_iterator<char>(f),{}); }
Ice::Long Wire(std::uint64_t value) { Ice::Long out; std::memcpy(&out,&value,sizeof(out)); return out; }
std::uint64_t Bits(Ice::Long value) { std::uint64_t out; std::memcpy(&out,&value,sizeof(out)); return out; }
std::uint64_t Mask(unsigned int bits) { return bits==64 ? UINT64_MAX : (UINT64_C(1)<<bits)-1; }
mdaq::hw::DeviceConfig Config(const DeviceInfo &d) {
    mdaq::hw::DeviceConfig c; c.descr.name=d.name; c.descr.baseAddress=Wire(d.baseAddress);
    c.descr.registerAccess=d.access; c.descr.registerWidth=d.registerBytes;
    for (const auto &r:d.registers) {
        mdaq::hw::RegisterConfig reg; reg.descr.name=r.name; reg.descr.offset=Wire(r.offset);
        for (const auto &f:r.fields) {
            mdaq::hw::FieldDescription field; field.name=f.name; field.offset=f.offset; field.width=f.width;
            field.readOnly=false; reg.fields.push_back(field); // 收到的旧servant不保存权限。
        }
        c.registers.push_back(reg);
    }
    return c;
}
struct World {
    std::mutex mutex;
    std::vector<std::string> actions;
    void Record(const std::string &s) { std::lock_guard<std::mutex> lock(mutex); actions.push_back(s); }
    std::vector<std::string> Actions() { std::lock_guard<std::mutex> lock(mutex); return actions; }
};
struct Board {
    std::mutex mutex;
    std::map<std::string,std::uint64_t> values;
    std::atomic<int> reads{0},writes{0},metadata{0},creates{0},destroys{0};
    bool wrongType=false, failWrite=false, failCreate=false, corruptCreate=false, failDestroy=false, partialDestroy=false;
    std::function<void()> onCreate;
};
class DescriptionDevice : public TestDevice {
    Board &b; World &w; std::string id;
    mdaq::hw::FieldDescription Field(const std::string &reg,const std::string &field) {
        for (const auto &r:fConfig.registers) if (r.descr.name==reg)
            for (const auto &f:r.fields) if (f.name==field) return f;
        throw mdaq::utl::CmdException("Missing fixture field");
    }
public:
    DescriptionDevice(const mdaq::hw::DeviceConfig &d,Board &board,World &world,const std::string &name)
        : TestDevice(d.descr.registerWidth),b(board),w(world),id(name) { fConfig=d; }
    void getDeviceConfig(mdaq::hw::DeviceConfig &out,const Ice::Current &) override { ++b.metadata; out=fConfig; }
    Ice::Long readField(const std::string &reg,const std::string &field,const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(b.mutex); const auto f=Field(reg,field); ++b.reads;
        return Wire((b.values[fConfig.descr.name+'/'+reg]>>f.offset)&Mask(unsigned(f.width)));
    }
    Ice::Long readRegister(const std::string &,const Ice::Current &) override {
        throw mdaq::utl::CmdException("Describe may only read explicit stopped/status fields");
    }
    void writeField(const std::string &reg,const std::string &field,Ice::Long value,const Ice::Current &c) override {
        Require(fConfig.descr.name=="ctrl" && reg=="mutantConfig" && (field=="initDone" || field=="mutantMode") && value==0,
            "Describe wrote an unexpected hardware field");
        {
            std::lock_guard<std::mutex> lock(b.mutex); const auto f=Field(reg,field); const auto mask=Mask(unsigned(f.width))<<f.offset;
            auto &v=b.values["ctrl/"+reg]; v=(v&~mask)|((Bits(value)<<f.offset)&mask); ++b.writes;
        }
        w.Record(id+'.'+field+"=0");
        if (b.failWrite) c.con->close(true);
    }
    void writeRegister(const std::string &,Ice::Long,const Ice::Current &) override {
        throw mdaq::utl::CmdException("Describe must not clobber whole registers");
    }
};
class DescriptionNode : public BasicTestNode<mdaq::hw::Node> {
    Board &b; World &w; std::string id;
public:
    std::map<std::string,IceUtil::Handle<DescriptionDevice>> devices;
    DescriptionNode(Board &board,World &world,const std::string &name) : b(board),w(world),id(name) {}
    bool ice_isA(const std::string &type,const Ice::Current &c) const override {
        return !b.wrongType && mdaq::hw::Node::ice_isA(type,c);
    }
    void Populate(const HardwareDescription &d,Ice::ObjectAdapterPtr adapter) {
        std::lock_guard<std::mutex> lock(fMutex); fDevices.clear(); devices.clear(); fName=d.nodeId;
        for (const auto &dev:d.devices) {
            devices[dev.name]=new DescriptionDevice(Config(dev),b,w,id);
            fDevices[dev.name]=mdaq::hw::DevicePrx::uncheckedCast(adapter->addWithUUID(devices[dev.name]));
        }
    }
    void create(const mdaq::hw::NodeConfig &config,const Ice::Current &c) override {
        ++b.creates; w.Record(id+".create"); std::lock_guard<std::mutex> lock(fMutex); fName=config.id;
        for (auto d:config.devices) {
            if (fDevices.count(d.descr.name)) throw mdaq::utl::CmdException("Unexpected existing device");
            for (auto &r:d.registers) for (auto &f:r.fields) f.readOnly=false;
            if (b.corruptCreate && !d.registers.empty()) ++d.registers[0].descr.offset;
            devices[d.descr.name]=new DescriptionDevice(d,b,w,id);
            fDevices[d.descr.name]=mdaq::hw::DevicePrx::uncheckedCast(c.adapter->addWithUUID(devices[d.descr.name]));
        }
        if (b.onCreate) b.onCreate();
        if (b.failCreate) c.con->close(true);
    }
    void destroy(const Ice::Current &c) override {
        ++b.destroys; w.Record(id+".destroy");
        { std::lock_guard<std::mutex> lock(fMutex);
          if (b.partialDestroy) { if (!fDevices.empty()) fDevices.erase(fDevices.begin()); }
          else { fDevices.clear(); devices.clear(); } }
        if (b.failDestroy) c.con->close(true);
    }
};
struct Rig {
    World world;
    std::array<Board,3> boards;
    std::array<IceUtil::Handle<DescriptionNode>,3> nodes;
    SystemDescriptionOptions base;
    Ice::ObjectAdapterPtr adapter;
    Rig(Ice::CommunicatorPtr communicator,Ice::ObjectAdapterPtr a,const HardwareDescription &cobo,const HardwareDescription &mutant,int minor) : adapter(a) {
        SystemAlignmentOptions topology; topology.encodingMinor=minor; topology.timeoutMs=500;
        topology.exclusiveOwnerConfirmed=topology.daqStoppedConfirmed=true;
        for (std::size_t i=0;i<3;++i) {
            const auto id=std::string(i<2 ? "C" : "M")+std::to_string(i<2 ? i : 0);
            nodes[i]=new DescriptionNode(boards[i],world,id);
            const auto proxy=communicator->proxyToString(adapter->addWithUUID(nodes[i]));
            if (i<2) {
                AlignmentCoBoEndpoint e; e.id=id; e.expectedNodeName="CoBo["+std::to_string(i)+']'; e.nodeProxy=proxy; e.description=cobo;
                e.enabled=i==0; topology.cobos.push_back(e);
            } else {
                AlignmentMutantEndpoint e; e.id=id; e.expectedNodeName="Mutant[0]"; e.nodeProxy=proxy; e.description=mutant; e.master=true;
                topology.mutants.push_back(e);
            }
        }
        base=BuildSystemDescriptionOptions(topology,DescriptionPolicy::UseExisting); Reset(DescriptionPolicy::UseExisting);
    }
    void Reset(DescriptionPolicy policy) {
        for (std::size_t i=0;i<3;++i) {
            auto &b=boards[i]; auto &n=base.nodes[i]; n.policy=policy;
            n.currentDescription=policy==DescriptionPolicy::ReplaceExisting ? n.description : HardwareDescription();
            b.wrongType=b.failWrite=b.failCreate=b.corruptCreate=b.failDestroy=b.partialDestroy=false; b.onCreate={};
            { std::lock_guard<std::mutex> lock(b.mutex);
              b.values.clear(); if (i<2) b.values["ctrl/mutantConfig"]=UINT64_C(0x80000000)|3; }
            if (policy==DescriptionPolicy::CreateEmpty) {
                std::lock_guard<std::mutex> lock(nodes[i]->fMutex); nodes[i]->fDevices.clear(); nodes[i]->devices.clear(); nodes[i]->fName="Initial empty server";
            } else nodes[i]->Populate(n.description,adapter);
            b.reads=b.writes=b.metadata=b.creates=b.destroys=0;
        }
        std::lock_guard<std::mutex> lock(world.mutex); world.actions.clear();
    }
    int Mutations() { int count=0; for (auto &b:boards) count+=b.writes+b.creates+b.destroys; return count; }
    void NoMutations() { Require(Mutations()==0,"Preflight failure mutated an earlier node"); }
    void Order() {
        const auto a=world.Actions(); std::vector<std::string> status;
        for (const auto &s:a) if (s.find("=0")!=std::string::npos) status.push_back(s);
        Require(status==std::vector<std::string>({"C0.initDone=0","C1.initDone=0","C0.mutantMode=0","C1.mutantMode=0"}),"Describe lost all-initDone then all-mutantMode order or inactive-board status");
        for (std::size_t i=0;i<2;++i) {
            Require(boards[i].writes==2 && boards[i].values["ctrl/mutantConfig"]==UINT64_C(0x80000000),"Describe clobbered unrelated status bits");
        }
        Require(boards[2].writes==0,"Describe wrote MuTaNT hardware");
    }
};
void RemoveField(HardwareDescription &d,const std::string &field) {
    for (auto &dev:d.devices) if (dev.name=="ctrl") for (auto &reg:dev.registers) if (reg.name=="mutantConfig")
        reg.fields.erase(std::remove_if(reg.fields.begin(),reg.fields.end(),[&](const FieldInfo &f) { return f.name==field; }),reg.fields.end());
}
class Backend : public EccBackend {
    SystemDescriptionClient &client; std::string directory;
public:
    Backend(SystemDescriptionClient &c,const std::string &d) : client(c),directory(d) {}
    void Validate(EccCommand c,EccState,EccState) override {
        if (c!=EccCommand::Describe) throw std::runtime_error("Test backend only implements full Describe");
        client.Validate();
    }
    void Apply(EccCommand,EccState,EccState) override { client.Describe(directory); }
};
}
int main(int argc,char **argv) {
    Ice::CommunicatorPtr communicator;
    try {
        const bool serve=argc==8 && std::string(argv[3])=="--serve";
        Require(argc==3 || argc==4 || serve,"Expected CoBo/MuTaNT descriptions and optional audit index/serve arguments");
        const auto cobo=LoadHardwareDescription(argv[1]),mutant=LoadHardwareDescription(argv[2]);
        Ice::InitializationData init; init.properties=Ice::createProperties(); init.properties->setProperty("Ice.RetryIntervals","-1");
        init.properties->setProperty("Ice.ThreadPool.Server.Size","4"); communicator=Ice::initialize(init);
        auto adapter=communicator->createObjectAdapterWithEndpoints("DescriptionTest","tcp -h 127.0.0.1 -p 0"); adapter->activate();
        if (serve) {
            const int policy=std::stoi(argv[5]),minor=std::stoi(argv[6]),failure=std::stoi(argv[7]);
            Require(policy>=0 && policy<=2 && minor>=0 && minor<=1 && failure>=0 && failure<=4,"Invalid fixture serve mode");
            Rig r(communicator,adapter,cobo,mutant,minor); r.Reset(DescriptionPolicy(policy));
            if (failure==1) r.boards[2].wrongType=true;
            if (failure==2) r.boards[1].failCreate=true;
            if (failure==3) r.boards[0].failDestroy=true;
            if (failure==4) r.boards[1].failWrite=true;
            std::ofstream topology(std::string(argv[4])+"/topology.json");
            topology<<"{\"schema_version\":1,\"kind\":\"getdaq.alignment-topology\",\"encoding\":\"1."<<minor<<"\",\"timeout_ms\":500,\"cobos\":[";
            for (std::size_t i=0;i<2;++i) {
                const auto &n=r.base.nodes[i]; if (i) topology<<',';
                topology<<"{\"id\":"<<JsonString(n.id)<<",\"expected_node_name\":"<<JsonString(n.description.nodeId)
                    <<",\"node_proxy\":"<<JsonString(n.nodeProxy)<<",\"link_proxy\":"<<JsonString(n.nodeProxy)
                    <<",\"enabled\":"<<(i==0 ? "true" : "false")<<",\"hardware_description_file\":"<<JsonString(argv[1])<<'}';
            }
            const auto &m=r.base.nodes[2];
            topology<<"],\"mutants\":[{\"id\":"<<JsonString(m.id)<<",\"expected_node_name\":"<<JsonString(m.description.nodeId)
                <<",\"node_proxy\":"<<JsonString(m.nodeProxy)<<",\"alignment_proxy\":"<<JsonString(m.nodeProxy)
                <<",\"role\":\"master\",\"hardware_description_file\":"<<JsonString(argv[2])<<"}]}\n";
            topology.close(); Require(bool(topology),"Cannot publish fixture topology");
            std::cout<<"{\"ready\":true}"<<std::endl; std::string stop; std::getline(std::cin,stop);
            std::cout<<"{\"creates\":"<<r.boards[0].creates+r.boards[1].creates+r.boards[2].creates
                <<",\"destroys\":"<<r.boards[0].destroys+r.boards[1].destroys+r.boards[2].destroys
                <<",\"writes\":"<<r.boards[0].writes+r.boards[1].writes+r.boards[2].writes<<",\"actions\":[";
            const auto actions=r.world.Actions(); for (std::size_t i=0;i<actions.size();++i) { if (i) std::cout<<','; std::cout<<JsonString(actions[i]); }
            std::cout<<"]}"<<std::endl; communicator->destroy(); return 0;
        }
        char tmp[]="/tmp/getdaq-description-test-XXXXXX"; const auto root=::mkdtemp(tmp); Require(root,"mkdtemp failed");
        unsigned int sequence=0; const auto path=[&] { return std::string(root)+"/action-"+std::to_string(sequence++); };
        std::ofstream index; if (argc==4) { index.open(argv[3]); Require(bool(index),"Cannot open audit index"); }
        for (int minor:{0,1}) {
            Rig r(communicator,adapter,cobo,mutant,minor);
            auto bad=r.base; bad.exclusiveOwnerConfirmed=false;
            Rejects([&] { SystemDescriptionClient x(bad); },"declarations"); r.NoMutations();
            bad=r.base; RemoveField(bad.nodes[1].description,"mutantMode");
            Rejects([&] { SystemDescriptionClient x(bad); },"missing required"); r.NoMutations();
            bad=r.base; bad.nodes[2].nodeProxy=bad.nodes[0].nodeProxy;
            Rejects([&] { SystemDescriptionClient x(bad); },"Duplicate canonical"); r.NoMutations();
            bad=r.base; bad.nodes[1].description.nodeId=bad.nodes[0].description.nodeId;
            Rejects([&] { SystemDescriptionClient x(bad); },"Duplicate description"); r.NoMutations();
            bad=r.base; bad.nodes[1].currentDescription=bad.nodes[1].description;
            Rejects([&] { SystemDescriptionClient x(bad); },"only valid"); r.NoMutations();
            bad=r.base;
            for (auto &d:bad.nodes[1].description.devices) if (d.name=="ctrl") for (auto &reg:d.registers) if (reg.name=="mutantConfig")
                for (auto &f:reg.fields) if (f.name=="mutantMode") f.readOnly=true;
            Rejects([&] { SystemDescriptionClient x(bad); },"permission"); r.NoMutations();
            {
                SystemDescriptionClient x(r.base); x.Validate();
                Require(x.GetState()==SystemDescriptionClient::State::Fresh && r.boards[2].metadata>0,"Validate did not read all-node metadata"); r.NoMutations();
            }
            // 末尾节点/元数据/停止观察失败必须先于任何destroy/create/status写入。
            r.boards[2].wrongType=true;
            { SystemDescriptionClient x(r.base); Rejects([&] { x.Describe(path()); },"implement"); r.NoMutations(); }
            r.boards[2].wrongType=false; r.nodes[2]->fName="wrong";
            { SystemDescriptionClient x(r.base); Rejects([&] { x.Describe(path()); },"node name differs"); r.NoMutations(); }
            r.Reset(DescriptionPolicy::UseExisting);
            r.nodes[2]->devices.begin()->second->fConfig.descr.baseAddress+=4;
            { SystemDescriptionClient x(r.base); Rejects([&] { x.Describe(path()); },"bound description"); r.NoMutations(); }
            r.Reset(DescriptionPolicy::UseExisting);
            for (const auto &d:mutant.devices) if (d.name=="CARD_B_EMC") for (const auto &reg:d.registers) if (reg.name=="TRIGGER_CTRL")
                for (const auto &f:reg.fields) if (f.name=="run") r.boards[2].values["CARD_B_EMC/TRIGGER_CTRL"]=UINT64_C(1)<<f.offset;
            { SystemDescriptionClient x(r.base); Rejects([&] { x.Describe(path()); },"production stopped"); r.NoMutations(); }
            r.Reset(DescriptionPolicy::UseExisting);
            const auto existing=path(); Require(::mkdir(existing.c_str(),0700)==0,"mkdir failed");
            { SystemDescriptionClient x(r.base); Rejects([&] { x.Describe(existing); },"exclusively create"); r.NoMutations();
              Require(x.GetState()==SystemDescriptionClient::State::Fresh,"No-command audit rejection consumed Describe"); }
            for (auto policy:{DescriptionPolicy::UseExisting,DescriptionPolicy::CreateEmpty,DescriptionPolicy::ReplaceExisting}) {
                r.Reset(policy); const auto audit=path(); auto input=r.base; SystemDescriptionClient x(input);
                input.nodes[0].description.nodeId="changed-outside"; x.Describe(audit); r.Order();
                Require(x.Configuration().nodes[0].description.nodeId=="CoBo[0]","Description configuration was not immutable");
                Require(x.GetState()==SystemDescriptionClient::State::Described,"Describe did not complete");
                for (auto &b:r.boards) Require(b.creates==int(policy!=DescriptionPolicy::UseExisting) && b.destroys==int(policy==DescriptionPolicy::ReplaceExisting),"Description action count differs");
                const auto before=r.Mutations(); Rejects([&] { x.Describe(path()); },"cannot be replayed"); Require(r.Mutations()==before,"Completed Describe replayed");
                const auto text=File(audit+"/system-description.jsonl");
                Require(text.find("\"production_ecc_state_committed\":false")!=std::string::npos && text.find("\"board_permissions_verified\":false")!=std::string::npos,"Audit inflated hardware/ECC proof");
                if (index) index<<"{\"directory\":"<<JsonString(audit)<<",\"encoding_minor\":"<<minor<<",\"policy\":"<<int(policy)<<"}\n";
                if (policy!=DescriptionPolicy::UseExisting) {
                    const auto a=r.world.Actions(); const auto first=std::find(a.begin(),a.end(),"C0.initDone=0");
                    Require(std::find(a.begin(),first,"M0.create")!=first,"Status changed before all new descriptions were created");
                }
            }
            // 混合部署及明确不同的旧布局/名称，不隐式假定旧描述就是新描述。
            r.Reset(DescriptionPolicy::UseExisting);
            r.base.nodes[1].policy=DescriptionPolicy::CreateEmpty;
            { std::lock_guard<std::mutex> lock(r.nodes[1]->fMutex); r.nodes[1]->fDevices.clear(); r.nodes[1]->devices.clear(); }
            r.base.nodes[2].policy=DescriptionPolicy::ReplaceExisting; r.base.nodes[2].currentDescription=r.base.nodes[2].description;
            r.base.nodes[2].currentDescription.nodeId="OldMutant"; r.base.nodes[2].currentDescription.devices[0].baseAddress+=4096;
            r.nodes[2]->Populate(r.base.nodes[2].currentDescription,adapter);
            { SystemDescriptionClient x(r.base); x.Describe(path()); r.Order(); Require(r.nodes[2]->fName=="Mutant[0]","Replacement did not use final desired name"); }
            r.Reset(DescriptionPolicy::CreateEmpty); r.boards[2].wrongType=true;
            { SystemDescriptionClient x(r.base); Rejects([&] { x.Describe(path()); },"implement"); r.NoMutations(); }
            r.Reset(DescriptionPolicy::CreateEmpty); r.boards[1].failCreate=true;
            {
                SystemDescriptionClient x(r.base); Rejects([&] { x.Describe(path()); },"UNKNOWN");
                Require(x.GetState()==SystemDescriptionClient::State::Failed && r.boards[0].creates==1 && r.boards[1].creates==1 && r.boards[2].creates==0 && r.boards[0].writes==0,"Unknown create replayed/continued status");
                Rejects([&] { x.Describe(path()); },"cannot be replayed");
            }
            r.Reset(DescriptionPolicy::CreateEmpty); r.boards[1].corruptCreate=true;
            { SystemDescriptionClient x(r.base); Rejects([&] { x.Describe(path()); },"metadata/audit verification failed");
              Require(r.boards[2].creates==0 && r.boards[0].writes==0,"Create verification failure continued"); }
            r.Reset(DescriptionPolicy::CreateEmpty);
            // 空软件描述不等于物理停止；新描述装上后观察实际firmware位才能开始清除就绪。
            for (const auto &d:cobo.devices) if (d.name=="ctrl") for (const auto &reg:d.registers) if (reg.name=="pipeCtrl")
                for (const auto &f:reg.fields) if (f.name=="firmwareEnable") r.boards[1].values["ctrl/pipeCtrl"]=UINT64_C(1)<<f.offset;
            { SystemDescriptionClient x(r.base); Rejects([&] { x.Describe(path()); },"production stopped");
              Require(r.boards[2].creates==1 && r.boards[0].writes==0 && r.boards[1].writes==0,"New-node running check allowed status writes"); }
            r.Reset(DescriptionPolicy::ReplaceExisting); r.boards[0].failDestroy=true;
            {
                SystemDescriptionClient x(r.base); Rejects([&] { x.Describe(path()); },"DESTROY outcome UNKNOWN");
                Require(x.GetState()==SystemDescriptionClient::State::Failed && r.boards[0].destroys==1 && r.boards[0].creates==0 && r.boards[1].destroys==0,"Unknown destroy retried or recreated");
            }
            r.Reset(DescriptionPolicy::ReplaceExisting); r.boards[0].partialDestroy=true;
            { SystemDescriptionClient x(r.base); Rejects([&] { x.Describe(path()); },"DESTROY outcome UNKNOWN");
              Require(r.boards[0].destroys==1 && r.boards[0].creates==0,"Partial destroy followed by create"); }
            r.Reset(DescriptionPolicy::UseExisting); r.boards[1].failWrite=true;
            {
                SystemDescriptionClient x(r.base); Rejects([&] { x.Describe(path()); },"UNKNOWN");
                Require(x.GetState()==SystemDescriptionClient::State::Failed && r.boards[0].writes==1 && r.boards[1].writes==1,"Unknown status continued mutantMode");
            }
            r.Reset(DescriptionPolicy::CreateEmpty); const auto failedAudit=path();
            r.boards[0].onCreate=[&] { std::ofstream collision(failedAudit+"/step-1.jsonl"); collision<<"occupied\n"; };
            { SystemDescriptionClient x(r.base); Rejects([&] { x.Describe(failedAudit); },"exclusively create audit");
              Require(r.boards[0].creates==1 && r.boards[1].creates==0 && x.GetState()==SystemDescriptionClient::State::Failed,"Audit failure allowed next create"); }
            // 连Slice idempotent destroy也只发一次；未知且实际已清空时，不允许直接Create恢复。
            r.Reset(DescriptionPolicy::UseExisting); r.boards[0].failDestroy=true;
            {
                HardwareClient hw(r.base.nodes[0].nodeProxy,minor,500); hw.UseDescription(r.base.nodes[0].description);
                Rejects([&] { hw.DestroyDescription(path()); },"UNKNOWN");
                hw.UseDescription(r.base.nodes[0].description);
                Rejects([&] { hw.DestroyDescription(path()); },"already attempted");
                Rejects([&] { hw.Create(r.base.nodes[0].description,path()); },"UNKNOWN");
                Require(r.boards[0].destroys==1 && r.boards[0].creates==0,"Unknown description lifecycle command was replayed");
            }
#ifdef GETDAQ_TEST_ECC_JOURNAL
            for (bool failure:{false,true}) {
                r.Reset(DescriptionPolicy::CreateEmpty); r.boards[1].failCreate=failure;
                const auto journal=path(),audit=path(); SystemDescriptionClient x(r.base); Backend backend(x,audit);
                {
                    EccJournal store(journal,"description-test"); EccStateMachine machine(backend,store);
                    if (failure) {
                        Rejects([&] { machine.Execute(EccCommand::Describe,"describe-1","immutable-topology-test"); },"UNKNOWN");
                        Require(machine.Status().outcomeUnknown && machine.Status().state==EccState::Idle,"Persistent state committed partial Describe");
                    } else {
                        machine.Execute(EccCommand::Describe,"describe-1","immutable-topology-test");
                        Require(machine.Status().state==EccState::Described && !machine.Status().outcomeUnknown,"Persistent adapter did not commit after full Describe");
                    }
                }
                const auto before=r.Mutations(); EccJournal reopened(journal,"description-test");
                EccStateMachine restarted(backend,reopened);
                Require(restarted.Status().needsReconciliation && restarted.Status().outcomeUnknown==failure,"Restart silently assumed hardware state");
                Rejects([&] { restarted.Execute(EccCommand::Describe,"new-id","immutable-topology-test"); },"reconcil");
                Require(r.Mutations()==before,"Restart replayed hardware");
            }
#endif
            r.Reset(DescriptionPolicy::CreateEmpty);
            std::mutex waitMutex; std::condition_variable condition; bool entered=false,released=false;
            r.boards[0].onCreate=[&] { std::unique_lock<std::mutex> lock(waitMutex); entered=true; condition.notify_all(); condition.wait(lock,[&] { return released; }); };
            SystemDescriptionClient async(r.base); std::exception_ptr threadError;
            std::thread thread([&] { try { async.Describe(path()); } catch (...) { threadError=std::current_exception(); } });
            { std::unique_lock<std::mutex> lock(waitMutex); condition.wait(lock,[&] { return entered; });
              Require(async.GetState()==SystemDescriptionClient::State::Describing,"Snapshot blocked/lost active state");
              released=true; condition.notify_all(); }
            thread.join(); if (threadError) std::rethrow_exception(threadError); r.Order();
            std::cout<<"PASS complete system Describe TCP 1."<<minor<<": full real metadata, use/create/replace, global preflight, ACK loss, audit failure, durable state and no replay\n";
        }
        communicator->destroy(); return 0;
    } catch (const std::exception &e) {
        std::cerr<<e.what()<<'\n'; if (communicator) try { communicator->destroy(); } catch (...) {} return 1;
    }
}
