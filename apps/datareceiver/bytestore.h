#ifndef GETDAQ_BYTESTORE_H
#define GETDAQ_BYTESTORE_H
#include <cstddef>
#include <cstdint>
#include <string>

namespace GetDAQ {
// Legacy DataRouter ByteStorage equivalent: persist the incoming TCP/ZBUF byte
// stream verbatim, without parsing MFM frames. Owned by the receiver writer thread.
class ByteStore {
public:
    ByteStore(const std::string &directory, uint64_t rotateBytes, bool durable,
              const std::string &naming = "native", uint64_t minimumFreeBytes = 0);
    ~ByteStore();
    void Write(const uint8_t *data, std::size_t bytes);
    void Seal();
    void Abandon();
    uint64_t Bytes() const { return fBytes; }
    uint64_t Files() const { return fFiles; }
private:
    void OpenNext();
    void Close(bool sync);
    void CheckFreeSpace(uint64_t incomingBytes) const;
    std::string fDirectory, fPrefix, fNaming;
    uint64_t fRotate, fMinimumFree, fBytes = 0, fFiles = 0, fFileBytes = 0;
    uint64_t fBytesSinceSpaceCheck = 0;
    bool fDurable;
    int fFd = -1;
};
}
#endif
