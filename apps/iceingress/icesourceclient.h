#ifndef GETDAQ_ICESOURCECLIENT_H
#define GETDAQ_ICESOURCECLIENT_H
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
namespace GetDAQ {
struct IceSourceOptions {
    std::string address="127.0.0.1",expectedSession;
    unsigned int port=46105,encodingMinor=0,timeoutMs=10000;
    std::size_t chunkBytes=256*1024;
};
struct IceSourceSnapshot {
    enum State {Fresh,Connecting,Connected,Sending,Finishing,Finished,Unknown};
    State state=Fresh;
    std::string sessionId,error;
    std::uint64_t issuedBytes=0,locallySentBytes=0,issuedChunks=0;
    bool finishIssued=false,sourceEndAcknowledged=false;
    bool failureRetirementIssued=false,dataConnectionClosed=false;
};
// Data-source library, not an ECC/GUI control API. Each stream uses one fixed
// connection. Oneway return only confirms local handoff; no automatic retry.
class IceSourceClient {
public:
    explicit IceSourceClient(const IceSourceOptions &options);
    ~IceSourceClient(); // Local resource cleanup only; never calls finishStream.
    IceSourceClient(const IceSourceClient &)=delete;
    IceSourceClient &operator=(const IceSourceClient &)=delete;
    void Connect(); // Checked interfaces/session, then freeze the actual connection.
    void Send(const std::uint8_t *data,std::size_t bytes); // Borrowed input, consumed synchronously.
    void Finish(); // Source's explicit last-byte declaration; not a file-seal proof.
    void RetireWithoutEnd(); // Explicit quiescent-owner cleanup; force-close only, no Finish/replay.
    IceSourceSnapshot Status() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
