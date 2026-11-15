#include "roottreereader.h"

#include "GDataChannel.h"
#include "GDataFrame.h"
#include "GDataSample.h"
#include "root5treeadapter.h"
#include "t2keventadapter.h"
#include <TBranch.h>
#include <TClonesArray.h>
#include <TFile.h>
#include <TRefArray.h>
#include <TTree.h>

#include <limits>
#include <stdexcept>
#include <utility>

namespace GetDAQ {
class RootTreeReader::Impl {
public:
    std::unique_ptr<TFile> file;
    TTree *tree = nullptr;
    GET::GDataFrame *frame = nullptr;
    std::unique_ptr<Root5TreeAdapter> root5Adapter;
    std::unique_ptr<T2KEventAdapter> t2kAdapter;
    std::string path;
};

RootTreeReader::RootTreeReader() : fImpl(new Impl) {}
RootTreeReader::~RootTreeReader() = default;
RootTreeReader::RootTreeReader(RootTreeReader &&other) noexcept = default;
RootTreeReader &RootTreeReader::operator=(RootTreeReader &&other) noexcept = default;

bool RootTreeReader::Open(const std::string &path, std::string *error)
{
    try {
        if (path.empty()) throw std::invalid_argument("ROOT input path is empty");
        std::unique_ptr<TFile> file(TFile::Open(path.c_str(), "READ"));
        if (!file || file->IsZombie()) throw std::runtime_error("cannot open ROOT file");
        TTree *tree = nullptr;
        TBranch *branch = nullptr;
        bool isT2K = false;
        bool isRoot5 = false;
        file->GetObject("tree", tree);
        if (tree && (branch = tree->GetBranch("GDataFrame"))) {
            const std::string className = branch->GetClassName();
            if (Root5TreeAdapter::Supports(branch)) isRoot5 = true;
            else if (className != "GET::GDataFrame")
                throw std::runtime_error("GDataFrame branch class is unsupported by this ROOT 6 reader: " + className);
        } else {
            tree = nullptr;
            file->GetObject("TT2K", tree);
            if (!tree) throw std::runtime_error("ROOT file has neither a legacy 'tree' nor a 'TT2K' event tree");
            branch = tree->GetBranch("T2KEvent");
            if (!branch) throw std::runtime_error("T2K event tree has no 'T2KEvent' branch");
            if (!T2KEventAdapter::Supports(branch))
                throw std::runtime_error("T2KEvent branch class is unsupported by this ROOT 6 reader: " +
                                         std::string(branch->GetClassName()));
            isT2K = true;
        }
        if (tree->GetEntries() <= 0) throw std::runtime_error("ROOT event tree is empty");

        std::unique_ptr<Impl> opened(new Impl);
        opened->file = std::move(file);
        opened->tree = tree;
        opened->path = path;
        if (isT2K) {
            opened->t2kAdapter.reset(new T2KEventAdapter);
            if (!opened->t2kAdapter->Bind(opened->tree))
                throw std::runtime_error("cannot bind T2KEvent branch");
        } else if (isRoot5) {
            opened->root5Adapter.reset(new Root5TreeAdapter);
            if (!opened->root5Adapter->Bind(opened->tree))
                throw std::runtime_error("cannot bind ROOT5-era get::GDataFrame branch");
        } else if (opened->tree->SetBranchAddress("GDataFrame", &opened->frame) < 0) {
            throw std::runtime_error("cannot bind legacy GDataFrame branch");
        }
        fImpl.swap(opened);
        return true;
    } catch (const std::exception &exception) {
        if (error) *error = exception.what();
        return false;
    }
}

bool RootTreeReader::Read(std::int64_t entry, RootEvent *event, std::string *error)
{
    try {
        if (!event) throw std::invalid_argument("ROOT event output pointer is null");
        if (!fImpl || !fImpl->tree) throw std::runtime_error("no ROOT event tree is open");
        if (entry < 0 || entry >= fImpl->tree->GetEntries())
            throw std::out_of_range("ROOT event entry is outside the tree");
        if (fImpl->tree->GetEntry(entry) <= 0)
            throw std::runtime_error("cannot read selected ROOT event entry");

        if (fImpl->root5Adapter)
            return fImpl->root5Adapter->Read(event, error);

        if (fImpl->t2kAdapter) return fImpl->t2kAdapter->Read(event, error);
        if (!fImpl->frame) throw std::runtime_error("legacy event object is missing");
        const GET::GDataFrame &source = *fImpl->frame;
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
            GET::GDataChannel *channel = static_cast<GET::GDataChannel *>(channels->At(i));
            if (!channel) throw std::runtime_error("ROOT event contains a null channel object");
            if (channel->fAgetIdx >= 4 || channel->fChanIdx >= 68 || channel->GetNhit() < 0 || channel->GetNhit() > 512)
                throw std::runtime_error("ROOT channel identity or sample count is outside supported bounds");
            TRefArray &hits = channel->GetHits();
            if (hits.GetEntriesFast() != channel->GetNhit())
                throw std::runtime_error("ROOT channel sample reference count is inconsistent");
            std::vector<RootSample> &output = decoded.channels[channel->fAgetIdx][channel->fChanIdx];
            output.reserve(static_cast<std::size_t>(channel->GetNhit()));
            for (int hit = 0; hit < hits.GetEntriesFast(); ++hit) {
                GET::GDataSample *sample = static_cast<GET::GDataSample *>(hits.At(hit));
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

std::int64_t RootTreeReader::Entries() const
{
    return fImpl && fImpl->tree ? fImpl->tree->GetEntries() : 0;
}

std::string RootTreeReader::Path() const
{
    return fImpl ? fImpl->path : std::string();
}
}
