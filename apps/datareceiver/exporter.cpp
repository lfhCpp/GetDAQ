#include "exporter.h"
#include <stdexcept>
#include "netutil.h"
#include <algorithm>
#include <chrono>

namespace GetDAQ {
Exporter::Exporter(const std::string &bind, unsigned port, std::size_t budget) : fBudget(budget)
{
    if (!port) return;
    fListen = Listen(bind, port);
    try { fThread = std::thread(&Exporter::Work, this); }
    catch (...) { ::close(fListen); fListen = -1; throw; }
}
Exporter::~Exporter()
{
    fStop = true;
    if (fThread.joinable()) fThread.join();
    for (auto &c : fConnections) if (c.fd >= 0) ::close(c.fd);
    if (fListen >= 0) ::close(fListen);
}
void Exporter::Publish(const uint8_t *data, std::size_t size)
{
    if (!fClients.load() || fFailed.load()) return;
    // The caller is the receiver's frame-writing thread. Preserve the frame
    // before returning, but do not hold the publication mutex while allocating
    // and copying a potentially large frame. The exporter worker may continue
    // to drain or retire clients while this bounded copy is in progress.
    if (fBudget < 8 || size > fBudget - 8) {
        std::lock_guard<std::mutex> lock(fMutex);
        for (auto &c : fConnections)
            if (c.fd >= 0 && !c.disconnectRequested) c.disconnectRequested = true;
        return;
    }

    std::shared_ptr<std::vector<uint8_t>> message;
    message.reset(new std::vector<uint8_t>(size + 8));
    // 固定小端 uint64 长度，兼容旧 DataExporter 在 x86 上的输出。
    for (unsigned i = 0; i < 8; ++i) (*message)[i] = uint8_t(uint64_t(size) >> (8 * i));
    if (size) std::copy(data, data + size, message->begin() + 8);

    std::lock_guard<std::mutex> lock(fMutex);
    for (auto &c : fConnections) {
        if (c.fd < 0 || c.disconnectRequested) continue;
        if (c.pending > fBudget - message->size() || c.queue.size() >= 4096) {
            // Socket ownership stays with Work, including an in-flight send.
            c.disconnectRequested = true; continue;
        }
        c.pending += message->size(); c.queue.push_back(message);
    }
}
void Exporter::CloseClient(Client &client)
{
    int fd = -1;
    {
        std::lock_guard<std::mutex> lock(fMutex);
        if (client.fd < 0) return;
        fd = client.fd; client.fd = -1;
        client.queue.clear(); client.offset = client.pending = 0;
        client.disconnectRequested = false; ++fDisconnected;
    }
    ::close(fd);
}
void Exporter::Work()
{
    try {
        while (!fStop) {
            bool progress = false;
            // Only this thread changes the connection container and sockets.
            // Publish only appends owned frames or requests disconnection.
            for (auto &c : fConnections) {
                bool disconnect;
                { std::lock_guard<std::mutex> lock(fMutex); disconnect = c.disconnectRequested; }
                if (disconnect) { CloseClient(c); continue; }
                uint8_t byte = 0;
                const ssize_t n = ::recv(c.fd, &byte, 1, MSG_PEEK);
                if (n >= 0 || (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)) CloseClient(c);
            }
            {
                std::lock_guard<std::mutex> lock(fMutex);
                fConnections.erase(std::remove_if(fConnections.begin(), fConnections.end(),
                    [](const Client &c) { return c.fd < 0; }), fConnections.end());
            }
            const int fd = ::accept(fListen, nullptr, nullptr);
            if (fd >= 0) {
                if (fConnections.size() >= 8) { ::close(fd); ++fDisconnected; }
                else {
                    try { Nonblock(fd); } catch (...) { ::close(fd); throw; }
                    try { ConfigureNoSigpipe(fd); } catch (...) { ::close(fd); throw; }
                    std::lock_guard<std::mutex> lock(fMutex);
                    Client c; c.fd = fd; fConnections.push_back(std::move(c));
                }
            } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) throw std::runtime_error("Exporter accept failed");
            for (auto &c : fConnections) {
                std::size_t allowance = 1024 * 1024;
                unsigned calls = 0;
                while (c.fd >= 0 && allowance && calls++ < 256) {
                    std::shared_ptr<std::vector<uint8_t>> message;
                    std::size_t offset = 0;
                    bool disconnect;
                    {
                        std::lock_guard<std::mutex> lock(fMutex);
                        disconnect = c.disconnectRequested;
                        if (!disconnect && !c.queue.empty()) { message = c.queue.front(); offset = c.offset; }
                    }
                    if (disconnect) { CloseClient(c); break; }
                    if (!message) break;
                    const std::size_t amount = std::min(allowance, message->size() - offset);
                    // Retain the immutable frame, but never the publication lock,
                    // across network I/O. Publish cannot close/reuse this fd.
                    const ssize_t n = SendNoSigpipe(c.fd, message->data() + offset, amount);
                    if (n > 0) {
                        progress = true; allowance -= std::size_t(n); fBytes += uint64_t(n);
                        std::lock_guard<std::mutex> lock(fMutex);
                        c.offset += std::size_t(n);
                        // A partially sent allocation still consumes its full budget.
                        if (c.offset == message->size()) { c.pending -= message->size(); c.queue.pop_front(); c.offset = 0; }
                    } else {
                        if (n == 0 || (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)) CloseClient(c);
                        break;
                    }
                }
            }
            {
                std::lock_guard<std::mutex> lock(fMutex);
                fConnections.erase(std::remove_if(fConnections.begin(), fConnections.end(),
                    [](const Client &c) { return c.fd < 0; }), fConnections.end());
                fClients = unsigned(fConnections.size());
            }
            if (!progress) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            else std::this_thread::yield();
        }
    } catch (...) {
        fFailed = true;
        // The worker exclusively owns client sockets. Once it fails, close them
        // here so observers do not remain connected until receiver shutdown.
        for (auto &client : fConnections) CloseClient(client);
        fConnections.clear();
        fClients = 0;
    }
}
}
