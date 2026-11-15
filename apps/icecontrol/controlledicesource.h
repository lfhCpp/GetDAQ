#ifndef GETDAQ_CONTROLLEDICESOURCE_H
#define GETDAQ_CONTROLLEDICESOURCE_H
#include "../iceingress/icesourceclient.h"
#include <functional>
#include <memory>
#include <string>
namespace GetDAQ {
// These callbacks run in the data-source process, never in the GUI/ECC process.
// stop must join/quiesce every producer; drain sends remaining bytes via Feed.
struct IceProducerCallbacks {
    std::function<void()> verifyIdle, start, stop, drain;
};
struct ControlledIceSourceSnapshot {
    enum State { Fresh,Connecting,Connected,Starting,Running,Stopping,Stopped,Disconnecting,Disconnected,Unknown };
    State state=Fresh;
    IceSourceSnapshot stream;
    std::string error;
    bool startIssued=false,stopAcknowledged=false,drainAcknowledged=false;
    bool failureStopIssued=false,failureStopAcknowledged=false;
    bool failureRetirementIssued=false,retirementIdleVerified=false,failureRetired=false;
};
class ControlledIceSource {
public:
    ControlledIceSource(const IceSourceOptions &options,const IceProducerCallbacks &callbacks);
    ~ControlledIceSource(); // Application must explicitly stop/join producers before destruction. No implicit End.
    ControlledIceSource(const ControlledIceSource &)=delete;
    ControlledIceSource &operator=(const ControlledIceSource &)=delete;
    void Connect(); // Requires explicit expectedSession and idle producer before any data call.
    void Start(); // Connected/Stopped; retains the same stream/connection for all cycles.
    void Feed(const std::uint8_t *bytes,std::size_t count); // Producer thread; accepted only Running/Stopping.
    void Stop(); // Quiesce/join, drain, recheck idle; no source End, supports Pause/Resume.
    void Disconnect(); // Connected/Stopped; final quiesce/drain, then same-connection Finish.
    void StopProductionAfterFailure(); // One explicit stop/join; retains UNKNOWN, no drain/End/replay.
    void RetireAfterFailure(); // Same stopped UNKNOWN stream; no drain/End; preserves original failure.
    void ReportFailure(const std::string &message); // Source application's policy/device fault; never clears UNKNOWN.
    ControlledIceSourceSnapshot Status() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
