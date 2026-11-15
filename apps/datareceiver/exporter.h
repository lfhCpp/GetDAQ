#ifndef GETDAQ_EXPORTER_H
#define GETDAQ_EXPORTER_H
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
namespace GetDAQ {
// 在线导出为可丢弃的观察支路。慢客户端断开，绝不占住文件写盘缓冲块。
class Exporter {
public:
    Exporter(const std::string &bind, unsigned port, std::size_t budget);
    ~Exporter();
    void Publish(const uint8_t *data, std::size_t size);
    unsigned Clients() const { return fClients.load(); }
    uint64_t Disconnected() const { return fDisconnected.load(); }
    uint64_t Bytes() const { return fBytes.load(); }
    bool Failed() const { return fFailed.load(); }
private:
    struct Client {
        int fd = -1;
        std::deque<std::shared_ptr<std::vector<uint8_t>>> queue;
        std::size_t offset = 0, pending = 0;
        bool disconnectRequested = false;
    };
    void CloseClient(Client &client);
    void Work();
    int fListen = -1;
    std::size_t fBudget;
    std::mutex fMutex;
    std::vector<Client> fConnections;
    std::thread fThread;
    std::atomic<bool> fStop{false}, fFailed{false};
    std::atomic<unsigned> fClients{0};
    std::atomic<uint64_t> fDisconnected{0}, fBytes{0};
};
}
#endif
