#ifndef GETDAQ_SOURCECONTROLSERVANT_H
#define GETDAQ_SOURCECONTROLSERVANT_H
#include "controlledicesource.h"
#include "SourceControl.h"
#include <functional>
#include <mutex>
#include <set>
#include <map>
namespace GetDAQ {
struct SourceControlOptions {
    std::string sourceId;
    IceSourceOptions receiver; // Fixed allowed IPv4/port; session is supplied per controlled stream.
};
// Embed in the source/board process at its DaqCtrlNode object identity. The
// producer owns raw bytes; this servant only receives low-frequency commands.
class SourceControlServant : public DataSourceRecovery {
public:
    SourceControlServant(const SourceControlOptions &options,const IceProducerCallbacks &producer,
                         const std::function<void(unsigned int,bool)> &configurePolicy);
    void Feed(const std::uint8_t *bytes,std::size_t count);
    void ReportFailure(const std::string &message); // Device/readout thread; latch only, no implicit stop/End.
    SourceInfo describeSource(const Ice::Current &current) override;
    void connectIceStream(const std::string &,const std::string &,const std::string &,const std::string &,const Ice::Current &) override;
    void configureSourcePolicy(Ice::Int,bool,const Ice::Current &) override;
    void closeIceStream(const std::string &,const Ice::Current &) override;
    FailedSourceRetirement describeFailedSourceRetirement(const std::string &,const Ice::Current &) override;
    FailedSourceRetirement retireFailedIceStream(const std::string &,const Ice::Current &) override;
    void stopIceSourceAfterFailure(const std::string &,const Ice::Current &) override;
    void connect(const std::string &,const std::string &,const Ice::Current &) override;
    void disconnect(const Ice::Current &) override;
    void setAlwaysFlushData(bool,const Ice::Current &) override;
    void setCircularBuffersEnabled(Ice::Int,const Ice::Current &) override;
    void daqStart(const Ice::Current &) override;
    void daqStop(const Ice::Current &) override;
    void shutdown(const Ice::Current &) override;
private:
    void Owner(const Ice::Current &) const;
    std::shared_ptr<ControlledIceSource> Source() const;
    void Apply(const std::function<void()> &action);
    SourceControlOptions fOptions;
    IceProducerCallbacks fProducer;
    std::function<void(unsigned int,bool)> fPolicy;
    std::string fInstanceId,fReceiverAddress,fRequestedSession;
    mutable std::mutex fPointerMutex;
    std::mutex fOperationMutex;
    std::shared_ptr<ControlledIceSource> fSource;
    Ice::ConnectionPtr fOwner;
    std::set<std::string> fUsedSessions;
    std::map<std::string,FailedSourceRetirement> fFailedRetirements; // Bounded by the 256-session history.
};
}
#endif
