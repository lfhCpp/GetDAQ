#include "root5treeadapter.h"

#include "GDataChannel.h"
#include "GDataFrame.h"
#include "GDataSample.h"
#include <TBranch.h>
#include <TClonesArray.h>
#include <TRefArray.h>
#include <TTree.h>

#include <stdexcept>

namespace GetDAQ {

class Root5TreeAdapter::Impl {
public:
    get::GDataFrame *frame = nullptr;
};

Root5TreeAdapter::Root5TreeAdapter() : fImpl(new Impl) {}
Root5TreeAdapter::~Root5TreeAdapter() = default;

bool Root5TreeAdapter::Supports(const TBranch *branch)
{
    return branch && std::string(branch->GetClassName()) == "get::GDataFrame";
}

bool Root5TreeAdapter::Bind(TTree *tree)
{
    return tree && tree->SetBranchAddress("GDataFrame", &fImpl->frame) >= 0;
}

bool Root5TreeAdapter::Read(RootEvent *event, std::string *error)
{
    try {
        if (!event) throw std::invalid_argument("ROOT event output pointer is null");
        if (!fImpl || !fImpl->frame) throw std::runtime_error("legacy get::GDataFrame object is missing");
        const get::GDataFrame &source = *fImpl->frame;
        if (source.GetNchannels() < 0 || source.GetNchannels() > 4 * 68 ||
            source.GetNsamples() < 0 || source.GetNsamples() > 4 * 68 * 512)
            throw std::runtime_error("ROOT event exceeds the legacy AGET/channel/sample limits");

        RootEvent decoded;
        decoded.event = source.fHeader.fEventIdx;
        decoded.eventTime = source.fHeader.fEventTime;
        decoded.cobo = source.fHeader.fCoboIdx;
        decoded.asad = source.fHeader.fAsadIdx;
        TClonesArray *channels = source.GetChannels();
        TClonesArray *samples = source.GetSamples();
        if (!channels || !samples) throw std::runtime_error("ROOT event object arrays are missing");
        for (int i = 0; i < channels->GetEntriesFast(); ++i) {
            get::GDataChannel *channel = static_cast<get::GDataChannel *>(channels->At(i));
            if (!channel) throw std::runtime_error("ROOT event contains a null channel object");
            if (channel->fAgetIdx >= 4 || channel->fChanIdx >= 68 ||
                channel->GetNhit() < 0 || channel->GetNhit() > 512)
                throw std::runtime_error("ROOT channel identity or sample count is outside supported bounds");
            TRefArray &hits = channel->GetHits();
            if (hits.GetEntriesFast() != channel->GetNhit())
                throw std::runtime_error("ROOT channel sample reference count is inconsistent");
            std::vector<RootSample> &output = decoded.channels[channel->fAgetIdx][channel->fChanIdx];
            output.reserve(static_cast<std::size_t>(channel->GetNhit()));
            for (int hit = 0; hit < hits.GetEntriesFast(); ++hit) {
                get::GDataSample *sample = static_cast<get::GDataSample *>(hits.At(hit));
                if (!sample || sample->fBuckIdx >= 512 || sample->fValue >= 4096)
                    throw std::runtime_error("ROOT event contains an invalid sample object");
                output.push_back(RootSample{sample->fBuckIdx, sample->fValue});
                ++decoded.sampleCount;
            }
            if (!output.empty()) ++decoded.channelCount;
        }
        if (decoded.sampleCount != static_cast<unsigned>(source.GetNsamples()))
            throw std::runtime_error("ROOT sample references do not match the event sample array");
        *event = std::move(decoded);
        return true;
    } catch (const std::exception &exception) {
        if (error) *error = exception.what();
        return false;
    }
}

}
