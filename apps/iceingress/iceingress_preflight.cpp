#include "iceingress_preflight_p.h"
#include "iceingress_helpers_p.h"
#include "../datareceiver/bytestore.h"
#include "../datareceiver/framestore.h"

#include <cerrno>
#include <dirent.h>
#include <stdexcept>
#include <string>

namespace GetDAQ {
namespace {
void RequireEmptyOutput(const std::string &path)
{
    DIR *directory = ::opendir(path.c_str());
    if (!directory) throw std::runtime_error("Cannot inspect ICE output directory");

    bool occupied = false;
    int scanError = 0;
    for (;;) {
        errno = 0;
        const auto entry = ::readdir(directory);
        if (!entry) {
            scanError = errno;
            break;
        }
        const std::string name = entry->d_name;
        occupied = occupied || IceIngressInternal::RawName(name) ||
            name == ".getdaq-receiver-seal.json" ||
            name == ".getdaq-ice-receiver-seal.json" ||
            name == ".getdaq-ice-receiver-seal.json.tmp";
    }
    ::closedir(directory);
    if (scanError || occupied)
        throw std::runtime_error("ICE receiver requires an output directory without existing raw/seal files");
}

void ValidateStorageMode(const IceIngressConfig &config)
{
    if (config.storageMode != "frame-storage" && config.storageMode != "byte-storage")
        throw std::invalid_argument("ICE storage mode must be frame-storage or byte-storage");
    if (config.storageMode == "byte-storage") {
        if (config.exportPort)
            throw std::invalid_argument("ICE ByteStorage cannot export parsed frames");
    }
}

void ValidateStoragePath(const IceIngressConfig &config)
{
    if (config.storageMode == "byte-storage") {
        ByteStore check(config.output, config.rotateBytes, true,
                        config.fileNaming, config.minimumFreeBytes);
    } else {
        FrameStore check(config.output, config.rotateBytes, true,
                         config.fileNaming, config.minimumFreeBytes);
    }
}
}

void ValidateIceIngressOutput(const IceIngressConfig &config)
{
    ValidateStorageMode(config);
    RequireEmptyOutput(config.output);
    ValidateStoragePath(config);
}
}
