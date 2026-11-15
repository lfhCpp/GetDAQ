#include "t2keventadapter.h"

#include "T2KEvent.h"
#include <TBranch.h>
#include <TTree.h>

#include <stdexcept>
#include <utility>

namespace GetDAQ {

class T2KEventAdapter::Impl {
public:
    T2KEvent *event = nullptr;
};

T2KEventAdapter::T2KEventAdapter() : fImpl(new Impl) {}
T2KEventAdapter::~T2KEventAdapter() = default;

bool T2KEventAdapter::Supports(const TBranch *branch)
{
    return branch && std::string(branch->GetClassName()) == "T2KEvent";
}

bool T2KEventAdapter::Bind(TTree *tree)
{
    return tree && tree->SetBranchAddress("T2KEvent", &fImpl->event) >= 0;
}

bool T2KEventAdapter::Read(RootEvent *event, std::string *error)
{
    try {
        if (!event) throw std::invalid_argument("ROOT event output pointer is null");
        if (!fImpl || !fImpl->event) throw std::runtime_error("T2K event object is missing");
        const T2KEvent &source = *fImpl->event;
        if (source.fNChannels != 4 * 68 || source.fNBins != 512 ||
            source.fNTBins != source.fNChannels * source.fNBins ||
            source.fData.GetSize() != source.fNTBins)
            throw std::runtime_error("T2K matrix dimensions or data array size are invalid");
        RootEvent decoded;
        decoded.format = RootEvent::Format::T2KMatrix;
        decoded.channelCount = static_cast<unsigned>(source.fNChannels);
        decoded.sampleCount = static_cast<unsigned>(source.fNTBins);
        for (int channel = 0; channel < source.fNChannels; ++channel) {
            std::vector<RootSample> &output = decoded.channels[channel / 68][channel % 68];
            output.reserve(source.fNBins);
            for (int bucket = 0; bucket < source.fNBins; ++bucket) {
                const int value = source.fData.At(bucket + source.fNBins * channel);
                if (value < 0 || value >= 4096)
                    throw std::runtime_error("T2K matrix contains a sample outside the 12-bit ADC range");
                output.push_back(RootSample{static_cast<std::uint16_t>(bucket),
                                            static_cast<std::uint16_t>(value)});
            }
        }
        *event = std::move(decoded);
        return true;
    } catch (const std::exception &exception) {
        if (error) *error = exception.what();
        return false;
    }
}

} // namespace GetDAQ
