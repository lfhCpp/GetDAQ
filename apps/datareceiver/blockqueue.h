#ifndef GETDAQ_BLOCKQUEUE_H
#define GETDAQ_BLOCKQUEUE_H
#include <atomic>
#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace GetDAQ {
struct Block {
    explicit Block(std::size_t size) : data(size) {}
    std::vector<uint8_t> data;
    std::size_t used = 0;
    bool end = false;
};
// 固定数量的缓冲块；socket 直接读入块，线程间只移动指针。
class BlockQueue {
public:
    typedef std::unique_ptr<Block> Ptr;
    BlockQueue(std::size_t count, std::size_t size) {
        for (std::size_t i = 0; i < count; ++i) fFree.emplace_back(new Block(size));
    }
    Ptr Acquire(const std::atomic<bool> &stop) {
        std::unique_lock<std::mutex> lock(fMutex);
        while (fFree.empty() && !stop) fCondition.wait_for(lock, std::chrono::milliseconds(100));
        if (stop) return Ptr();
        Ptr p = std::move(fFree.front()); fFree.pop_front(); p->used = 0; p->end = false; return p;
    }
    void Push(Ptr p) {
        std::lock_guard<std::mutex> lock(fMutex); fReady.push_back(std::move(p)); fCondition.notify_all();
    }
    Ptr Pop() {
        std::unique_lock<std::mutex> lock(fMutex);
        fCondition.wait(lock, [this]() { return fClosed || !fReady.empty(); });
        if (fReady.empty()) return Ptr();
        Ptr p = std::move(fReady.front()); fReady.pop_front(); return p;
    }
    void Release(Ptr p) {
        std::lock_guard<std::mutex> lock(fMutex); fFree.push_back(std::move(p)); fCondition.notify_all();
    }
    void Close() { std::lock_guard<std::mutex> lock(fMutex); fClosed = true; fCondition.notify_all(); }
    std::size_t Pending() { std::lock_guard<std::mutex> lock(fMutex); return fReady.size(); }
private:
    std::mutex fMutex;
    std::condition_variable fCondition;
    std::deque<Ptr> fFree, fReady;
    bool fClosed = false;
};
}
#endif
