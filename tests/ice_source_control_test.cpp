#include "sourcecontrolservant.h"
#include "datasenderclient.h"
#include "hardwareclient.h"
#include "supervisorio.h"
#include <Ice/Ice.h>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
namespace {
using namespace GetDAQ;
void Require(bool value,const char *message) {if(!value) throw std::runtime_error(message);}
void Reject(const std::function<void()> &action) {bool rejected=false;try {action();} catch(...) {rejected=true;}Require(rejected,"Expected source-control rejection");}
std::vector<std::uint8_t> Frame(unsigned int event) {
    std::vector<std::uint8_t> bytes(4096,std::uint8_t(event));const unsigned char header[]={6,0,0,64,7,0,1,4,0,2,0,4,0,0,3,224};
    for(unsigned int i=0;i<sizeof(header);++i)bytes[i]=header[i];
    for(unsigned int i=0;i<4;++i)bytes[22+i]=std::uint8_t(event>>(24-8*i));bytes[26]=bytes[27]=0;return bytes;
}
struct Producer {
    SourceControlServant *source=nullptr;
    std::thread worker;
    std::atomic<unsigned int> starts{0},stops{0},policies{0};
    unsigned int event=0;
    bool policyFault=false,idleFault=false;
    void Idle() {Require(!worker.joinable(),"Source worker not joined");Require(!idleFault,"Synthetic retirement idle verification failure");}
    void Start() {Idle();++starts;worker=std::thread([this] {for(unsigned int i=0;i<2;++i) {const auto frame=Frame(++event);source->Feed(frame.data(),frame.size());}});}
    void Stop() {++stops;if(worker.joinable())worker.join();}
    ~Producer() {Stop();}
    IceProducerCallbacks Callbacks() {IceProducerCallbacks c;c.verifyIdle=[this] {Idle();};c.start=[this] {Start();};c.stop=[this] {Stop();};c.drain=[] {};return c;}
};
class Legacy : public mdaq::DaqCtrlNode {
public:
    unsigned int calls=0;
    void connect(const std::string &,const std::string &,const Ice::Current &) override {++calls;}
    void disconnect(const Ice::Current &) override {++calls;}
    void setAlwaysFlushData(bool,const Ice::Current &) override {++calls;}
    void setCircularBuffersEnabled(Ice::Int,const Ice::Current &) override {++calls;}
    void daqStart(const Ice::Current &) override {++calls;}
    void daqStop(const Ice::Current &) override {++calls;}
    void shutdown(const Ice::Current &) override {++calls;}
};
class LostEnd : public SourceControlServant {
public:
    LostEnd(const SourceControlOptions &o,const IceProducerCallbacks &p,const std::function<void(unsigned int,bool)> &policy):SourceControlServant(o,p,policy) {}
    void closeIceStream(const std::string &session,const Ice::Current &current) override {SourceControlServant::closeIceStream(session,current);current.con->close(true);}
};
class Observed : public SourceControlServant {
    std::mutex connectionMutex;
    Ice::ConnectionPtr ownerConnection;
public:
    std::atomic<bool> slow{false},invalid{false};
    Observed(const SourceControlOptions &o,const IceProducerCallbacks &p,const std::function<void(unsigned int,bool)> &policy):SourceControlServant(o,p,policy) {}
    SourceInfo describeSource(const Ice::Current &current) override {
        if(slow)std::this_thread::sleep_for(std::chrono::milliseconds(150));
        auto info=SourceControlServant::describeSource(current);if(invalid)info.locallySentBytes=info.issuedBytes+1;return info;
    }
    void configureSourcePolicy(Ice::Int mask,bool flush,const Ice::Current &current) override {
        SourceControlServant::configureSourcePolicy(mask,flush,current);
        std::lock_guard<std::mutex> lock(connectionMutex);ownerConnection=current.con;
    }
    void CloseOwnerConnection() {
        Ice::ConnectionPtr connection;{std::lock_guard<std::mutex> lock(connectionMutex);connection=ownerConnection;}
        Require(bool(connection),"Source original control connection missing");connection->close(true);
    }
};
class RetirementAckLoss : public Observed {
public:
    RetirementAckLoss(const SourceControlOptions &o,const IceProducerCallbacks &p,const std::function<void(unsigned int,bool)> &policy):Observed(o,p,policy){}
    FailedSourceRetirement retireFailedIceStream(const std::string &session,const Ice::Current &current) override {
        const auto result=SourceControlServant::retireFailedIceStream(session,current);current.con->close(true);return result;
    }
};
}
int main(int argc,char **argv) {
    Ice::CommunicatorPtr communicator;
    try {
        Require(argc==6,"Pass DATA_PORT SESSION ENCODING MODE ROOT");const std::string mode=argv[4],root=argv[5];
        Ice::InitializationData init;init.properties=Ice::createProperties();init.properties->setProperty("Ice.ThreadPool.Server.Size","2");init.properties->setProperty("Ice.RetryIntervals","-1");communicator=Ice::initialize(init);
        auto adapter=communicator->createObjectAdapterWithEndpoints("SourceControlTest","tcp -h 127.0.0.1 -p 0");const auto identity=communicator->stringToIdentity("DaqCtrlNode");
        Producer producer,replacement;SourceControlOptions options;options.sourceId="C0";options.receiver.port=unsigned(std::stoul(argv[1]));options.receiver.encodingMinor=unsigned(std::stoul(argv[3]));options.receiver.timeoutMs=1000;options.receiver.chunkBytes=317;
        producer.policyFault=mode=="policy-fault";
        const auto policy=[&](unsigned int mask,bool) {Require(mask==1,"Fixture policy mask differs");++producer.policies;if(producer.policyFault) throw std::runtime_error("Synthetic source policy ACK failure");};
        IceUtil::Handle<Observed> observed=new Observed(options,producer.Callbacks(),policy);
        IceUtil::Handle<SourceControlServant> server=mode=="end-ack-loss" ? static_cast<SourceControlServant *>(new LostEnd(options,producer.Callbacks(),policy)) : static_cast<SourceControlServant *>(observed.get());producer.source=server.get();
        if(mode=="retirement-ack-loss"){server=new RetirementAckLoss(options,producer.Callbacks(),policy);producer.source=server.get();}
        IceUtil::Handle<Legacy> legacy=new Legacy;
        adapter->add(mode=="legacy" ? Ice::ObjectPtr(legacy) : Ice::ObjectPtr(server),identity);adapter->activate();
        DataSenderOptions control;control.senderId=mode=="wrong-source" ? "other" : "C0";control.protocol="ICE";control.receiverAddress="127.0.0.1:"+std::string(argv[1]);
        control.controlProxy=communicator->proxyToString(adapter->createProxy(identity));control.exclusiveStoppedOwnerConfirmed=true;control.encodingMinor=int(options.receiver.encodingMinor);control.timeoutMs=1000;control.alwaysFlush=true;
        DataSenderClient client(control);
        Require(client.ObserveSource().phase=="unbound" && client.Snapshot().issuedCalls==0,"Fresh observation connected or changed control");
        if(mode=="legacy" || mode=="wrong-source") {Reject([&] {client.Validate();});Require(client.Snapshot().issuedCalls==0 && legacy->calls==0 && producer.starts==0,"Invalid source capability issued commands");}
        else if(mode=="unbound") {client.Validate();Reject([&] {client.Connect(root+"/unbound.jsonl");});Require(client.Snapshot().issuedCalls==0 && producer.policies==0,"Unbound source connected");}
        else {
            client.Validate();client.BindIceReceiverSession(mode=="wrong-session" ? std::string(64,'b') : argv[2]);
            if(mode=="wrong-session" || mode=="policy-fault") {
                Reject([&] {client.Connect(root+"/connect.jsonl");});Require(client.Snapshot().state==DataSenderSnapshot::Unknown,"Source connect/policy failure lost UNKNOWN");
                client.StopIceProductionAfterFailure(root+"/failure-stop.jsonl");Require(client.Snapshot().outcomeUnknown && client.Snapshot().failureStopAcknowledged && !client.Snapshot().sourceEndAcknowledged,"Failure stop cleared outcome or requested End");
                Reject([&] {client.StopIceProductionAfterFailure(root+"/failure-stop-again.jsonl");});Reject([&] {client.Start(root+"/retry.jsonl");});
            } else {
                client.Connect(root+"/connect.jsonl");Require(client.Snapshot().state==DataSenderSnapshot::Connected && producer.policies==1,"Source connect/policy not applied");
                if(mode=="observe-control-loss" || mode=="observe-control-loss-running") {
                    const bool running=mode=="observe-control-loss-running";
                    if(running) {
                        client.Start(root+"/start.jsonl");const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
                        while(client.ObserveSource().locallySentBytes<8192 && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
                        Require(client.ObserveSource().locallySentBytes==8192,"Initial running source data not handed off");
                    }
                    observed->CloseOwnerConnection();const auto before=client.Snapshot();
                    Require(client.ObserveSource().phase=="available" && !client.Snapshot().outcomeUnknown,"Readonly query failed or fabricated a control mutation after owner loss");
                    Reject([&] {if(running)client.Stop(root+"/stop-lost-owner.jsonl");else client.Start(root+"/start-lost-owner.jsonl");});
                    Require(client.Snapshot().outcomeUnknown && client.Snapshot().issuedCalls==before.issuedCalls && producer.starts==unsigned(running),"Lost bound control validation did not retain UNKNOWN before mutation");
                    Require(client.ObserveSource().phase=="available" && client.Snapshot().outcomeUnknown,"Readonly response cleared lost original control");
                    Reject([&] {client.StopIceProductionAfterFailure(root+"/stop-lost-owner.jsonl",true);});
                    Require(!client.Snapshot().failureStopIssued && !client.Snapshot().failureStopAcknowledged && producer.starts==unsigned(running),"Lost control was replaced or failure stop was fabricated");
                    if(running) {const auto source=client.ObserveSource();Require(source.state=="running" && !source.stopAcknowledged && !source.sourceEndAcknowledged && producer.worker.joinable(),"Lost control fabricated source stop or End");}
                } else if(mode=="observe-timeout") {
                    observed->slow=true;const auto started=std::chrono::steady_clock::now();const auto sample=client.ObserveSource(50);observed->slow=false;
                    Require(sample.phase=="unavailable" && std::chrono::steady_clock::now()-started<std::chrono::seconds(1) && client.Snapshot().issuedCalls==2 && !client.Snapshot().outcomeUnknown,"Read timeout changed control or exceeded bound");
                    Require(client.ObserveSource().phase=="available","Readonly source recovery unavailable");client.Disconnect(root+"/disconnect.jsonl");
                } else if(mode=="observe-malformed") {
                    observed->invalid=true;Require(client.ObserveSource().phase=="unavailable" && !client.Snapshot().outcomeUnknown,"Bad observation changed lifecycle");
                    Reject([&] {client.Validate();});Require(client.Snapshot().outcomeUnknown,"Failed bound control validation lost UNKNOWN");observed->invalid=false;
                    Require(client.ObserveSource().phase=="available" && client.Snapshot().outcomeUnknown,"Readonly recovery cleared unknown");
                    client.StopIceProductionAfterFailure(root+"/failure-stop.jsonl");Reject([&] {client.Start(root+"/retry.jsonl");});
                } else if(mode=="parent-failure-stop" || mode=="remote-fault" || mode.find("retirement-")==0) {
                    client.Start(root+"/start.jsonl");
                    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
                    while(client.ObserveSource().locallySentBytes<8192 && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
                    Require(client.ObserveSource().locallySentBytes==8192,"Source bytes not handed off");
                    if(mode=="remote-fault" || mode.find("retirement-")==0)server->ReportFailure("Synthetic asynchronous source device fault");
                    const auto sample=client.ObserveSource();Require(sample.phase=="available" && sample.state==((mode=="remote-fault" || mode.find("retirement-")==0) ? "unknown" : "running") && !client.Snapshot().outcomeUnknown,"Readonly fault observation mutated control");
                    Reject([&] {client.StopIceProductionAfterFailure(root+"/not-unknown.jsonl");});
                    client.StopIceProductionAfterFailure(root+"/failure-stop.jsonl",true);const auto stopped=client.ObserveSource();
                    Require(stopped.phase=="available" && stopped.state=="unknown" && stopped.failureStopAcknowledged && !stopped.sourceEndAcknowledged && !producer.worker.joinable(),"Owner failure did not join source or fabricated End");
                    Reject([&] {client.StopIceProductionAfterFailure(root+"/retry-stop.jsonl",true);});
                    if(mode.find("retirement-")==0) {
                        const auto stops=producer.stops.load();const auto issued=client.Snapshot().issuedCalls;
                        if(mode=="retirement-owner-loss")observed->CloseOwnerConnection();
                        if(mode=="retirement-idle-fault")producer.idleFault=true;
                        if(mode=="retirement-cross-controller") {
                            Ice::InitializationData otherInit;otherInit.properties=Ice::createProperties();otherInit.properties->setProperty("Ice.RetryIntervals","-1");auto other=Ice::initialize(otherInit);
                            const auto peer=DataSourceRecoveryPrx::checkedCast(other->stringToProxy(control.controlProxy)->ice_collocationOptimized(false));
                            Reject([&]{peer->retireFailedIceStream(argv[2]);});Require(!peer->describeFailedSourceRetirement(argv[2]).issued,"Another controller issued retirement");other->destroy();
                        }
                        const bool success=mode=="retirement-success" || mode=="retirement-cross-controller";
                        if(success){client.ValidateIceFailureRetirement();client.RetireIceSourceAfterFailure(root+"/retirement.jsonl");
                            Require(!client.VerifyIceFailureRetirement().empty() && !client.CompletedFailureRetirementAuditChainSha256().empty(),"Retirement lacks bound readback/audit");}
                        else Reject([&]{client.RetireIceSourceAfterFailure(root+"/retirement.jsonl");});
                        const auto status=client.Snapshot();Require(status.outcomeUnknown && status.state==DataSenderSnapshot::Unknown && !status.sourceEndAcknowledged &&
                            status.failureRetirementAcknowledged==success && producer.stops==stops,"Retirement cleared failure, sent End or repeated stop");
                        if(mode=="retirement-owner-loss")Require(!status.failureRetirementIssued && status.issuedCalls==issued,"Lost owner was reconnected for retirement");
                        else Require(status.failureRetirementIssued,"Retirement attempt missing");
                        producer.idleFault=false;Reject([&]{client.RetireIceSourceAfterFailure(root+"/repeat-retirement.jsonl");});
                        Require(producer.stops==stops,"Repeated retirement reissued producer stop");
                        SupervisorIO::WriteNew(root+"/retirement-result.json",std::string("{\"passed\":true,\"acknowledged\":")+(success ? "true" : "false")+
                            ",\"unknown_preserved\":true,\"source_end_acknowledged\":false,\"producer_stop_repeated\":false}\n");
                    }

                } else {
                if(mode=="cross-controller") {
                    Ice::InitializationData otherInit;otherInit.properties=Ice::createProperties();otherInit.properties->setProperty("Ice.RetryIntervals","-1");auto other=Ice::initialize(otherInit);
                    auto proxy=DataSourceControlPrx::uncheckedCast(other->stringToProxy(control.controlProxy)->ice_collocationOptimized(false));
                    Reject([&] {proxy->daqStart();});Reject([&] {proxy->closeIceStream(argv[2]);});Require(producer.starts==0,"Another controller started/ended source");other->destroy();
                }
                if(mode=="instance-change") {
                    adapter->remove(identity);
                    IceUtil::Handle<SourceControlServant> next=new SourceControlServant(options,replacement.Callbacks(),[](unsigned int,bool) {});replacement.source=next.get();adapter->add(next,identity);
                    Reject([&] {client.Start(root+"/start-new-instance.jsonl");});Require(client.Snapshot().outcomeUnknown && !client.Snapshot().startIssued && replacement.starts==0,"Changed source instance was adopted");
                } else {
                    if(mode!="empty") for(unsigned int i=0;i<3;++i) {client.Start(root+"/start-"+std::to_string(i)+".jsonl");client.Stop(root+"/stop-"+std::to_string(i)+".jsonl");}
                    if(mode=="end-ack-loss") {Reject([&] {client.Disconnect(root+"/disconnect.jsonl");});Require(client.Snapshot().outcomeUnknown && !client.Snapshot().sourceEndAcknowledged,"Lost source End ACK fabricated known control result");Reject([&] {client.Disconnect(root+"/retry-end.jsonl");});}
                    else {client.Disconnect(root+"/disconnect.jsonl");Require(client.Snapshot().state==DataSenderSnapshot::Disconnected && client.Snapshot().sourceEndAcknowledged,"Bound source End was not acknowledged");}
                }
                }
            }
        }
        producer.Stop();adapter->deactivate();adapter->waitForDeactivate();communicator->destroy();communicator=nullptr;
        const auto status=client.Snapshot();SupervisorIO::WriteNew(root+"/control-result.json","{\"passed\":true,\"mode\":"+JsonString(mode)+",\"unknown\":"+(status.outcomeUnknown ? "true" : "false")+",\"source_end_acknowledged\":"+(status.sourceEndAcknowledged ? "true" : "false")+",\"issued_calls\":"+std::to_string(status.issuedCalls)+",\"source_starts\":"+std::to_string(producer.starts.load())+"}\n");
        std::cout<<"PASS real source control "<<mode<<'\n';return 0;
    } catch(const std::exception &e) {if(communicator)try {communicator->destroy();} catch(...) {}std::cerr<<e.what()<<'\n';return 1;}
}
