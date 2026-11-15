#ifndef GETDAQ_SUPERVISORCOMMON_H
#define GETDAQ_SUPERVISORCOMMON_H
#include "datasupervisor.h"
#include "strictjson.h"
#include <cstddef>
#include <cstdint>
#include <string>
namespace GetDAQ { namespace SupervisorIO {
// Pure C++11 metadata validation and canonical plan encoding. This header is
// intentionally independent of POSIX descriptors, files and sockets.
void Text(const std::string &value,std::size_t maximum);
void Digest(const std::string &value);
void Path(const std::string &value);
std::uint64_t Unsigned(const JsonValue &value,bool string=false);
std::string String(const JsonValue &value);
bool Boolean(const JsonValue &value);
bool ParseCanonicalIPv4(const std::string &text,std::uint32_t &hostOrderAddress);
void ValidatePlanOptions(const DataSupervisorOptions &options);
std::string EncodeOptions(const DataSupervisorOptions &options,const std::string &executableSha256);
} }
#endif
