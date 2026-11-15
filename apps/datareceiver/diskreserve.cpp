#include "diskreserve.h"
#include <limits>

namespace GetDAQ {
bool DiskReserveAllows(uint64_t availableBytes, uint64_t minimumFreeBytes,
                       uint64_t pendingBytes, uint64_t incomingBytes)
{
    uint64_t required = minimumFreeBytes;
    const uint64_t amounts[] = {pendingBytes, incomingBytes,
                                DiskReserveMetadataBytes, DiskReserveCheckIntervalBytes};
    for (const uint64_t amount : amounts) {
        if (amount > std::numeric_limits<uint64_t>::max() - required) return false;
        required += amount;
    }
    return availableBytes >= required;
}

bool DiskReserveCheckNeeded(uint64_t bytesSinceCheck, uint64_t incomingBytes)
{
    return bytesSinceCheck >= DiskReserveCheckIntervalBytes ||
        incomingBytes > DiskReserveCheckIntervalBytes - bytesSinceCheck;
}
}
