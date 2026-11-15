#ifndef GETDAQ_DISKRESERVE_H
#define GETDAQ_DISKRESERVE_H

#include <cstdint>

namespace GetDAQ {
constexpr uint64_t DiskReserveMetadataBytes = 1024ull * 1024;
constexpr uint64_t DiskReserveCheckIntervalBytes = 1024ull * 1024;

bool DiskReserveAllows(uint64_t availableBytes, uint64_t minimumFreeBytes,
                      uint64_t pendingBytes, uint64_t incomingBytes);
bool DiskReserveCheckNeeded(uint64_t bytesSinceCheck, uint64_t incomingBytes);
}

#endif
