#ifndef GETDAQ_ICEINGRESS_H
#define GETDAQ_ICEINGRESS_H
#include "../datareceiver/blockqueue.h"
#include "../datareceiver/exporter.h"
#include "../../common/storage/directorylease.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace GetDAQ {
struct IceIngressConfig {
    std::string output, sessionId, fileNaming="native";
    std::string storageMode="frame-storage";
    std::string senderId, configurationSha256, artifactSha256;
    std::string dataBind="127.0.0.1";
    unsigned int dataPort=46105, statusPort=46115;
    std::size_t blockBytes=256*1024, blocks=256, maxFrame=32*1024*1024;
    std::uint64_t rotateBytes=1024ull*1024*1024;
    std::uint64_t minimumFreeBytes=0;
    std::string exportBind="127.0.0.1";
    unsigned int exportPort=0;
    std::size_t exportBudget=16*1024*1024;
};
// The data core has no Ice or Qt types. The optional servant synchronously copies
// borrowed RPC bytes into this bounded pool; only the writer touches MfmStream.
class IceIngressRecorder {
public:
    explicit IceIngressRecorder(const IceIngressConfig &config);
    ~IceIngressRecorder();
    IceIngressRecorder(const IceIngressRecorder &)=delete;
    IceIngressRecorder &operator=(const IceIngressRecorder &)=delete;
    void Feed(const std::uint8_t *data,std::size_t bytes); // Serialized by the servant.
    void EndStream(std::uint64_t expectedBytes); // Source marker and exact accepted byte count.
    void Fail(const std::string &message);
    void BindSource() { fConnections=1; }
    void InputClosed() { fInputClosed=true; }
    bool EndRequested() const { return fEnd; }
    bool Failed() const { return fStop; }
    bool WriterDone() const { return fWriterDone; }
    std::uint64_t ReceivedBytes() const { return fReceived; }
    int Finish(); // Join/drain, publish a distinct ICE receipt only after EndStream.
    std::string Status() const;
private:
    void Write();
    void WriteByteStorage();
    void WriteFrameStorage();
    void SaveReceipt();
    IceIngressConfig fConfig;
    DirectoryLease fLease;
    mutable BlockQueue fQueue;
    Exporter fExporter;
    std::thread fWriter;
    std::atomic<bool> fStop{false},fEnd{false},fWriterDone{false},fReceipt{false};
    std::atomic<bool> fInputClosed{false};
    std::atomic<unsigned int> fConnections{0};
    std::atomic<std::uint64_t> fReceived{0},fAccepted{0},fFrames{0},fFiles{0},fSynced{0};
    mutable std::mutex fErrorMutex;
    std::string fError;
};
}
#endif
