#ifndef GETDAQ_RECEIVERCATALOG_SCANANALYSIS_P_H
#define GETDAQ_RECEIVERCATALOG_SCANANALYSIS_P_H

#include <array>
#include <cerrno>
#include <stdexcept>
#include <string>
#include <streambuf>

#include <unistd.h>

namespace GetDAQ {
std::string BuildScanPointBinding(const std::string &scanPath,unsigned int pointIndex,const std::string &catalogPath);

namespace ReceiverCatalogScanAnalysisInternal {
void ValidateScanAnalysisPaths(const std::string &scanPath,
                               const std::string &bindingPath,
                               const std::string &outputPath);
void PublishScanAnalysisReport(const std::string &outputPath,
                               const std::string &report,
                               const std::string &analysisName,
                               const std::string &publishedObjectName);

class DescriptorInputBuffer : public std::streambuf {
public:
    explicit DescriptorInputBuffer(int descriptor) : fDescriptor(descriptor) {
        setg(fBytes.data(), fBytes.data(), fBytes.data());
    }

protected:
    int_type underflow() override {
        if (gptr() < egptr()) return traits_type::to_int_type(*gptr());
        ssize_t count;
        do { count = ::read(fDescriptor, fBytes.data(), fBytes.size()); }
        while (count < 0 && errno == EINTR);
        if (count < 0) throw std::runtime_error("Catalog raw file read failed");
        if (!count) return traits_type::eof();
        setg(fBytes.data(), fBytes.data(), fBytes.data() + count);
        return traits_type::to_int_type(*gptr());
    }

private:
    int fDescriptor;
    std::array<char, 64 * 1024> fBytes{};
};
}
}

#endif
