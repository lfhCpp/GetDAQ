#ifndef GETDAQ_ECCSERVICE_P_H
#define GETDAQ_ECCSERVICE_P_H

#include <QString>
#include <string>

namespace GetDAQ {
namespace EccServiceInternal {
struct Result {
    int code = 202;
    std::string json;
};

std::string Error(const std::string &message);
std::string DecodeReceiverSourcePath(const QString &hex);
} // namespace EccServiceInternal
} // namespace GetDAQ

#endif
