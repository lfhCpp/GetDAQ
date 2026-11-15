#ifndef GETDAQ_DATASENDERCLIENT_VALIDATION_P_H
#define GETDAQ_DATASENDERCLIENT_VALIDATION_P_H

#include <string>
#include <cstddef>
#include <vector>

namespace GetDAQ {
struct DataSenderOptions;
namespace DataSenderValidation {
void Text(const std::string &value, std::size_t maximum);
std::string Receiver(const std::string &value);
void Scalars(const DataSenderOptions &options);
}
std::vector<std::string> CanonicalDataSenderNodeKeys(const std::string &proxy, int timeoutMs);
}

#endif
