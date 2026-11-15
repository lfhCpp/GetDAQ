#ifndef GETDAQ_FRAMESTORE_H
#define GETDAQ_FRAMESTORE_H
#include "diskreserve.h"
#include "mfmstream.h"
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>

namespace GetDAQ {
class DiskReserveExceeded : public std::runtime_error {
public:
    explicit DiskReserveExceeded(const std::string &message) : std::runtime_error(message) {}
};
class FrameStore {
public:
    FrameStore(const std::string &directory, uint64_t rotationBytes, bool durable, const std::string &naming = "native",
               uint64_t minimumFreeBytes = 0);
    ~FrameStore();
    void Write(const uint8_t *data, const FrameInfo &info);
    void Seal(); // 刷新、同步（按配置）、关闭；成功才可归档。
    void Abandon(); // 磁盘保护触发时关闭但不再写出缓存；此目录必须保持未封存。
    uint64_t Bytes() const { return fBytes; }
    uint64_t Files() const { return fFiles; }
    static std::string FormatGetTimestamp(uint64_t microseconds);
private:
    struct File {
        int fd = -1;
        uint64_t bytes = 0;
        uint32_t event = 0;
        bool hasEvent = false;
        std::vector<uint8_t> buffer;
        std::size_t offset = 0;
    };
    void Flush(File &file);
    void Close(File &file);
    void CheckFreeSpace(uint64_t incomingBytes) const;
    std::string fDirectory, fPrefix;
    std::string fNaming;
    uint64_t fRotation, fBytes = 0, fFiles = 0;
    uint64_t fMinimumFreeBytes = 0, fBytesSinceSpaceCheck = 0;
    bool fDurable;
    std::map<std::string, File> fOpen;
    std::map<std::string, uint64_t> fNextIndex; // GET编号跨连接保持，仍限制来源总数。
};
}
#endif
