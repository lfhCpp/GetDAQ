#ifndef GETDAQ_RECEIVER_H
#define GETDAQ_RECEIVER_H
#include "blockqueue.h"
#include "exporter.h"
#include "../../common/storage/directorylease.h"
#include <csignal>
#include <string>

namespace GetDAQ {
struct ReceiverConfig {
    std::string bind = "127.0.0.1", statusBind = "127.0.0.1", output;
    unsigned port = 46005, statusPort = 46015, exportPort = 0;
    std::size_t blockBytes = 256 * 1024, blocks = 256, maxFrame = 32 * 1024 * 1024;
    std::size_t exportBudget = 16 * 1024 * 1024;
    uint64_t rotateBytes = 1024ull * 1024 * 1024;
    uint64_t minimumFreeBytes = 0; // Optional per-volume safety reserve; zero preserves legacy behavior.
    bool once = false, durable = true;
    bool cycleControlStdin = false; // FDT多连接会话，仅从管理器继承的匿名socket请求结束。
    std::string inputProtocol = "TCP"; // TCP/ZBUF原始流，FDT仅过滤帧间控制字。
    std::string fileNaming = "native", storageMode = "frame-storage";
    std::string sessionId, senderId, configurationSha256, artifactSha256; // 可选控制身份，不能传原始帧。
};
void ValidateReceiverIdentity(const ReceiverConfig &config);
class Receiver {
public:
    explicit Receiver(const ReceiverConfig &config);
    ~Receiver();
    int Run(const volatile std::sig_atomic_t &interrupted);
private:
    void Receive();
    void Write();
    void WriteByteStorage();
    void WriteFrameStorage();
    void Fail(const std::string &message);
    std::string Status();
    void ServeStatus();
    void SaveSeal();
    ReceiverConfig fConfig;
    BlockQueue fQueue;
    Exporter fExporter;
    std::unique_ptr<DirectoryLease> fDirectoryLease;
    int fListen = -1, fStatus = -1;
    std::thread fReceiveThread, fWriteThread;
    std::atomic<bool> fStop{false}, fWriterDone{false}, fConnected{false}, fSealed{true};
    std::atomic<bool> fCycleFinish{false}, fCycleControlLost{false};
    std::atomic<bool> fCycleUnused{false}, fCycleUnusedRefused{false};
    std::atomic<bool> fCycleAborted{false}, fCycleAbortRefused{false};
    std::atomic<uint64_t> fReceived{0}, fAccepted{0}, fFrames{0}, fAcquisitionEvents{0}, fFiles{0}, fConnections{0}, fSynced{0}, fEofs{0};
    std::atomic<uint64_t> fControlBytes{0}, fHeartbeats{0}, fGoodbyes{0};
    std::mutex fErrorMutex;
    std::string fError;
    void ReadCycleControl();
};
}
#endif
