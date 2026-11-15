#include "controlledicesource.h"
#include "../../common/text/utf8.h"
#include <mutex>
#include <stdexcept>
namespace GetDAQ {
class ControlledIceSource::Impl {
public:
    IceProducerCallbacks callbacks;
    IceSourceClient source;
    mutable std::mutex statusMutex;
    std::mutex operationMutex,dataMutex;
    ControlledIceSourceSnapshot snapshot;
    Impl(const IceSourceOptions &options,const IceProducerCallbacks &producer) : callbacks(producer),source(options) {
        if(options.expectedSession.size()!=64 || options.expectedSession.find_first_not_of("0123456789abcdef")!=std::string::npos)
            throw std::invalid_argument("Controlled ICE source requires a fixed expected receiver session");
        if(!producer.verifyIdle || !producer.start || !producer.stop || !producer.drain)
            throw std::invalid_argument("Controlled ICE source requires idle/start/stop/drain callbacks");
    }
    ControlledIceSourceSnapshot Status() const {
        std::lock_guard<std::mutex> lock(statusMutex);auto result=snapshot;result.stream=source.Status();return result;
    }
    void State(ControlledIceSourceSnapshot::State state) {
        std::lock_guard<std::mutex> lock(statusMutex);
        if(snapshot.state==ControlledIceSourceSnapshot::Unknown) throw std::logic_error("Source fault is latched; no later state commit may clear UNKNOWN");
        snapshot.state=state;
    }
    void Unknown(const std::string &error) {
        std::lock_guard<std::mutex> lock(statusMutex);
        if(snapshot.error.empty()) snapshot.error=EccErrorText(error);
        snapshot.state=ControlledIceSourceSnapshot::Unknown;
    }
    void CheckKnown() const {if(Status().state==ControlledIceSourceSnapshot::Unknown) throw std::logic_error("Controlled ICE source UNKNOWN; no replay or automatic End");}
    void Quiesce() {
        callbacks.stop();
        {std::lock_guard<std::mutex> lock(statusMutex);snapshot.stopAcknowledged=true;}
        CheckKnown();callbacks.verifyIdle();CheckKnown();
        // No data mutex is held during stop/join or drain; the producer may Feed.
        callbacks.drain();CheckKnown();callbacks.verifyIdle();CheckKnown();
        {std::lock_guard<std::mutex> lock(statusMutex);snapshot.drainAcknowledged=true;}
    }
};
ControlledIceSource::ControlledIceSource(const IceSourceOptions &options,const IceProducerCallbacks &callbacks) : fImpl(new Impl(options,callbacks)) {}
ControlledIceSource::~ControlledIceSource() {}
ControlledIceSourceSnapshot ControlledIceSource::Status() const {return fImpl->Status();}
void ControlledIceSource::ReportFailure(const std::string &message) {fImpl->Unknown(message);}
void ControlledIceSource::Connect() {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operationMutex);
    if(p.Status().state!=ControlledIceSourceSnapshot::Fresh) throw std::logic_error("Controlled ICE source Connect requires Fresh; no replay");
    p.State(ControlledIceSourceSnapshot::Connecting);
    try {p.callbacks.verifyIdle();p.CheckKnown();p.source.Connect();p.CheckKnown();p.State(ControlledIceSourceSnapshot::Connected);}
    catch(const std::exception &e) {p.Unknown(e.what());throw;}
    catch(...) {p.Unknown("Source callback raised a nonstandard exception; outcome UNKNOWN");throw;}
}
void ControlledIceSource::Start() {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operationMutex);const auto state=p.Status().state;
    if(state!=ControlledIceSourceSnapshot::Connected && state!=ControlledIceSourceSnapshot::Stopped) throw std::logic_error("Controlled ICE source Start requires Connected/Stopped");
    try {
        p.callbacks.verifyIdle();p.CheckKnown();
        {std::lock_guard<std::mutex> lock(p.statusMutex);
            if(p.snapshot.state==ControlledIceSourceSnapshot::Unknown) throw std::logic_error("Source fault is latched before producer start");
            p.snapshot.state=ControlledIceSourceSnapshot::Starting;p.snapshot.startIssued=true;p.snapshot.stopAcknowledged=p.snapshot.drainAcknowledged=false;}
        // A producer may deliver immediately from inside start. Feed is enabled
        // before invoking it; any callback/send failure latches UNKNOWN.
        p.State(ControlledIceSourceSnapshot::Running);p.callbacks.start();p.CheckKnown();
    } catch(const std::exception &e) {p.Unknown(e.what());throw;}
    catch(...) {p.Unknown("Source callback raised a nonstandard exception; outcome UNKNOWN");throw;}
}
void ControlledIceSource::Feed(const std::uint8_t *bytes,std::size_t count) {
    auto &p=*fImpl;std::lock_guard<std::mutex> data(p.dataMutex);const auto state=p.Status().state;
    if(state!=ControlledIceSourceSnapshot::Running && state!=ControlledIceSourceSnapshot::Stopping) {
        if(count) p.Unknown("Producer delivered bytes outside Running/Stopping; source end is not permitted");
        throw std::logic_error("Controlled ICE source Feed is outside a producing/draining phase");
    }
    try {p.source.Send(bytes,count);} catch(const std::exception &e) {p.Unknown(e.what());throw;}
    catch(...) {p.Unknown("Source callback raised a nonstandard exception; outcome UNKNOWN");throw;}
}
void ControlledIceSource::Stop() {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operationMutex);
    if(p.Status().state!=ControlledIceSourceSnapshot::Running) throw std::logic_error("Controlled ICE source Stop requires Running");
    p.State(ControlledIceSourceSnapshot::Stopping);
    try {
        p.Quiesce();std::lock_guard<std::mutex> data(p.dataMutex);p.CheckKnown();p.State(ControlledIceSourceSnapshot::Stopped);
    } catch(const std::exception &e) {p.Unknown(e.what());throw;}
    catch(...) {p.Unknown("Source callback raised a nonstandard exception; outcome UNKNOWN");throw;}
}
void ControlledIceSource::Disconnect() {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operationMutex);const auto state=p.Status().state;
    if(state!=ControlledIceSourceSnapshot::Connected && state!=ControlledIceSourceSnapshot::Stopped)
        throw std::logic_error("Controlled ICE source Disconnect requires Connected/Stopped; no implicit production stop or replay");
    // Even a never-started source must verify and drain its own input. Stopping
    // allows the source-side drain callback to emit the remaining bytes.
    p.State(ControlledIceSourceSnapshot::Stopping);
    try {
        p.Quiesce();std::lock_guard<std::mutex> data(p.dataMutex);p.CheckKnown();p.State(ControlledIceSourceSnapshot::Disconnecting);
        p.source.Finish();p.State(ControlledIceSourceSnapshot::Disconnected);
    } catch(const std::exception &e) {p.Unknown(e.what());throw;}
    catch(...) {p.Unknown("Source callback raised a nonstandard exception; outcome UNKNOWN");throw;}
}
void ControlledIceSource::StopProductionAfterFailure() {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operationMutex);const auto status=p.Status();
    if(status.state!=ControlledIceSourceSnapshot::Unknown || status.failureStopIssued)
        throw std::logic_error("Failure stop requires the same UNKNOWN object with no earlier failure-stop attempt");
    {std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.failureStopIssued=true;}
    try {
        p.callbacks.stop();p.callbacks.verifyIdle();
        std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.failureStopAcknowledged=true;
    } catch(const std::exception &e) {p.Unknown(e.what());throw;}
    catch(...) {p.Unknown("Source callback raised a nonstandard exception; outcome UNKNOWN");throw;}
}
void ControlledIceSource::RetireAfterFailure() {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operationMutex);const auto before=p.Status();
    if(before.state!=ControlledIceSourceSnapshot::Unknown || !before.failureStopIssued || !before.failureStopAcknowledged || before.failureRetirementIssued)
        throw std::logic_error("Failed source retirement requires the same acknowledged failure stop and no prior retirement attempt");
    {std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.failureRetirementIssued=true;}
    try {
        p.callbacks.verifyIdle();
        {std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.retirementIdleVerified=true;}
        std::lock_guard<std::mutex> data(p.dataMutex);p.source.RetireWithoutEnd();
        std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.failureRetired=true;
    } catch(const std::exception &e){p.Unknown(e.what());throw;}
    catch(...){p.Unknown("Failed source retirement raised a nonstandard exception");throw;}
}

}
