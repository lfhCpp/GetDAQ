#include "sourcecontrolservant.h"
#include <Ice/Ice.h>
#include "../../common/text/utf8.h"
#include <algorithm>
#include <random>
#include <stdexcept>
namespace GetDAQ {
namespace {
void Digest(const std::string &value) {if(value.size()!=64 || value.find_first_not_of("0123456789abcdef")!=std::string::npos) throw std::invalid_argument("Source control requires a lowercase HEX64 session");}
std::string Instance() {
    std::random_device random;const char hex[]="0123456789abcdef";std::string result;
    for(unsigned int i=0;i<32;++i) {const auto byte=random()&255;result+=hex[byte>>4];result+=hex[byte&15];}return result;
}
const char *State(ControlledIceSourceSnapshot::State state) {
    const char *names[]={"fresh","connecting","connected","starting","running","stopping","stopped","disconnecting","disconnected","unknown"};
    if(unsigned(state)>=10) throw std::runtime_error("Invalid source state");
    return names[unsigned(state)];
}
}
SourceControlServant::SourceControlServant(const SourceControlOptions &options,const IceProducerCallbacks &producer,const std::function<void(unsigned int,bool)> &policy)
    :fOptions(options),fProducer(producer),fPolicy(policy),fInstanceId(Instance()),fReceiverAddress(options.receiver.address+':'+std::to_string(options.receiver.port)) {
    if(options.sourceId.empty() || options.sourceId.size()>128 || !ValidUtf8(options.sourceId) ||
        std::any_of(options.sourceId.begin(),options.sourceId.end(),[](unsigned char c) {return c<32 || c==127;}) || !policy || !options.receiver.expectedSession.empty())
        throw std::invalid_argument("Source service requires ID/policy and a runtime-controlled receiver session");
    auto checked=options.receiver;checked.expectedSession=std::string(64,'0');ControlledIceSource validate(checked,producer);
}
std::shared_ptr<ControlledIceSource> SourceControlServant::Source() const {
    std::lock_guard<std::mutex> lock(fPointerMutex);if(!fSource) throw std::logic_error("Source stream has not been connected");return fSource;
}
void SourceControlServant::ReportFailure(const std::string &message) {Source()->ReportFailure(message);}
void SourceControlServant::Owner(const Ice::Current &current) const {
    if(!current.con || current.con->type()!="tcp" || !fOwner || fOwner!=current.con) throw std::logic_error("Source control requires the original bound TCP controller connection");
}
void SourceControlServant::Apply(const std::function<void()> &action) {
    try {action();} catch(const std::exception &e) {throw mdaq::utl::CmdException(EccErrorText(e.what()));}
    catch(...) {throw mdaq::utl::CmdException("Source action raised a nonstandard exception; outcome remains unconfirmed");}
}
void SourceControlServant::Feed(const std::uint8_t *bytes,std::size_t count) {Source()->Feed(bytes,count);}
SourceInfo SourceControlServant::describeSource(const Ice::Current &current) {
    if(!current.con || current.con->type()!="tcp") throw std::invalid_argument("Source discovery requires actual TCP");
    SourceInfo info;info.version=1;info.sourceId=fOptions.sourceId;info.instanceId=fInstanceId;info.receiverAddress=fReceiverAddress;
    std::shared_ptr<ControlledIceSource> source;
    {std::lock_guard<std::mutex> lock(fPointerMutex);source=fSource;info.receiverSession=fRequestedSession;}
    ControlledIceSourceSnapshot snapshot;if(source) snapshot=source->Status();
    info.state=State(snapshot.state);info.error=snapshot.error;info.issuedBytes=Ice::Long(snapshot.stream.issuedBytes);info.locallySentBytes=Ice::Long(snapshot.stream.locallySentBytes);
    info.startIssued=snapshot.startIssued;info.stopAcknowledged=snapshot.stopAcknowledged;info.drainAcknowledged=snapshot.drainAcknowledged;
    info.finishIssued=snapshot.stream.finishIssued;info.sourceEndAcknowledged=snapshot.stream.sourceEndAcknowledged;
    info.failureStopIssued=snapshot.failureStopIssued;info.failureStopAcknowledged=snapshot.failureStopAcknowledged;return info;
}
void SourceControlServant::connectIceStream(const std::string &receiver,const std::string &session,const std::string &sourceId,const std::string &instance,const Ice::Current &current) {
    std::lock_guard<std::mutex> operation(fOperationMutex);
    Apply([&] {
        Digest(session);
        if(receiver!=fReceiverAddress || sourceId!=fOptions.sourceId || instance!=fInstanceId || !current.con || current.con->type()!="tcp") throw std::invalid_argument("Source/instance/receiver control binding differs");
        if(fUsedSessions.count(session) || fUsedSessions.size()>=256) throw std::logic_error("Source session replay/history budget refused");
        {std::lock_guard<std::mutex> lock(fPointerMutex);if(fSource && fSource->Status().state!=ControlledIceSourceSnapshot::Disconnected && !fSource->Status().failureRetired) throw std::logic_error("Previous source is not confirmed disconnected; no replacement");}
        auto options=fOptions.receiver;options.expectedSession=session;
        std::shared_ptr<ControlledIceSource> source(new ControlledIceSource(options,fProducer));
        fUsedSessions.insert(session);
        {std::lock_guard<std::mutex> lock(fPointerMutex);fOwner=current.con;fRequestedSession=session;fSource=source;}
        source->Connect();
    });
}
void SourceControlServant::configureSourcePolicy(Ice::Int mask,bool flush,const Ice::Current &current) {
    std::lock_guard<std::mutex> operation(fOperationMutex);
    Apply([&] {Owner(current);if(mask<1 || mask>255) throw std::invalid_argument("Source circular buffer mask outside 1..255");
        const auto source=Source();const auto state=source->Status().state;
        if(state!=ControlledIceSourceSnapshot::Connected && state!=ControlledIceSourceSnapshot::Stopped) throw std::logic_error("Source policy requires Connected/Stopped");
        try {fProducer.verifyIdle();fPolicy(unsigned(mask),flush);} catch(const std::exception &e) {source->ReportFailure(e.what());throw;}
        catch(...) {source->ReportFailure("Source policy raised a nonstandard exception");throw;}
    });
}
void SourceControlServant::closeIceStream(const std::string &session,const Ice::Current &current) {
    std::lock_guard<std::mutex> operation(fOperationMutex);Apply([&] {Owner(current);if(session!=fRequestedSession) throw std::invalid_argument("Source close session differs");Source()->Disconnect();});
}
void SourceControlServant::stopIceSourceAfterFailure(const std::string &session,const Ice::Current &current) {
    std::lock_guard<std::mutex> operation(fOperationMutex);Apply([&] {Owner(current);if(session!=fRequestedSession) throw std::invalid_argument("Source failure-stop session differs");
        const auto source=Source();source->ReportFailure("Controller declared an unknown source operation; explicit producer stop only");source->StopProductionAfterFailure();});
}
FailedSourceRetirement SourceControlServant::describeFailedSourceRetirement(const std::string &session,const Ice::Current &current) {
    if(!current.con || current.con->type()!="tcp")throw std::invalid_argument("Source retirement discovery requires actual TCP");
    Digest(session);
    FailedSourceRetirement result;result.version=1;
    std::shared_ptr<ControlledIceSource> source;
    {std::lock_guard<std::mutex> lock(fPointerMutex);
        const auto saved=fFailedRetirements.find(session);if(saved!=fFailedRetirements.end())return saved->second;
        if(session!=fRequestedSession)throw std::invalid_argument("Source retirement observation session differs");
        source=fSource;result.source.receiverSession=fRequestedSession;}
    ControlledIceSourceSnapshot snapshot;if(source)snapshot=source->Status();
    auto &info=result.source;info.version=1;info.sourceId=fOptions.sourceId;info.instanceId=fInstanceId;info.receiverAddress=fReceiverAddress;
    info.state=State(snapshot.state);info.error=snapshot.error;info.issuedBytes=Ice::Long(snapshot.stream.issuedBytes);info.locallySentBytes=Ice::Long(snapshot.stream.locallySentBytes);
    info.startIssued=snapshot.startIssued;info.stopAcknowledged=snapshot.stopAcknowledged;info.drainAcknowledged=snapshot.drainAcknowledged;
    info.finishIssued=snapshot.stream.finishIssued;info.sourceEndAcknowledged=snapshot.stream.sourceEndAcknowledged;
    info.failureStopIssued=snapshot.failureStopIssued;info.failureStopAcknowledged=snapshot.failureStopAcknowledged;
    result.issued=snapshot.failureRetirementIssued;result.producerIdleVerified=snapshot.retirementIdleVerified;
    result.dataConnectionClosed=snapshot.stream.dataConnectionClosed;result.retired=snapshot.failureRetired;
    return result;
}
FailedSourceRetirement SourceControlServant::retireFailedIceStream(const std::string &session,const Ice::Current &current) {
    std::lock_guard<std::mutex> operation(fOperationMutex);FailedSourceRetirement result;
    Apply([&]{Owner(current);if(session!=fRequestedSession)throw std::invalid_argument("Failed source retirement session differs");
        Source()->RetireAfterFailure();result=describeFailedSourceRetirement(session,current);
        std::lock_guard<std::mutex> lock(fPointerMutex);fFailedRetirements.emplace(session,result);});return result;
}
void SourceControlServant::daqStart(const Ice::Current &current) {std::lock_guard<std::mutex> operation(fOperationMutex);Apply([&] {Owner(current);Source()->Start();});}
void SourceControlServant::daqStop(const Ice::Current &current) {std::lock_guard<std::mutex> operation(fOperationMutex);Apply([&] {Owner(current);Source()->Stop();});}
void SourceControlServant::connect(const std::string &,const std::string &,const Ice::Current &) {throw mdaq::utl::CmdException("Use connectIceStream with fixed source/instance/receiver session");}
void SourceControlServant::disconnect(const Ice::Current &) {throw mdaq::utl::CmdException("Use non-idempotent closeIceStream with the original session");}
void SourceControlServant::setAlwaysFlushData(bool,const Ice::Current &) {throw mdaq::utl::CmdException("Use atomic configureSourcePolicy");}
void SourceControlServant::setCircularBuffersEnabled(Ice::Int,const Ice::Current &) {throw mdaq::utl::CmdException("Use atomic configureSourcePolicy");}
void SourceControlServant::shutdown(const Ice::Current &) {throw mdaq::utl::CmdException("Source process shutdown requires explicit application lifecycle; no implicit Stop/End");}
}
