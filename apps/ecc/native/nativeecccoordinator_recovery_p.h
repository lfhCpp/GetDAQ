#ifndef GETDAQ_NATIVEECCCOORDINATOR_RECOVERY_P_H
#define GETDAQ_NATIVEECCCOORDINATOR_RECOVERY_P_H

#include <stdexcept>
#include <string>

namespace GetDAQ {
namespace NativeEccCoordinatorRecoveryInternal {

inline void OperationId(const std::string &id) {
    if (id.empty() || id.size() > 128 ||
        id.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != std::string::npos)
        throw std::invalid_argument("Native ECC operation ID requires 1..128 ASCII letters/digits/underscore/hyphen");
}

}
}

#endif
