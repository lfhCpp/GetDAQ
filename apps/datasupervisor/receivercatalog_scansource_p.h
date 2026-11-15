#ifndef GETDAQ_RECEIVERCATALOG_SCAN_SOURCE_P_H
#define GETDAQ_RECEIVERCATALOG_SCAN_SOURCE_P_H

#include <cstdint>
#include <functional>
#include <string>

namespace GetDAQ {
namespace ReceiverCatalogScanAnalysisInternal {

// Runs a consumer over each raw file in the bound CoBo source while holding
// catalog and inode checks around the read. The descriptor is borrowed only
// for the duration of the callback.
std::uint64_t ForEachBoundScanCatalogRawFile(
    const std::string &catalogPath, const std::string &targetCobo,
    const std::function<void(int)> &consume);

}
}

#endif
