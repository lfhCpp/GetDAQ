#include "rootconverter.h"
#include "mfmstream.h"
#include "GDataFrame.h"
#include "GDataSample.h"
#include "T2KEvent.h"
#include <TFile.h>
#include <TObjString.h>
#include <TProcessID.h>
#include <TTree.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
namespace {
const std::size_t kMaximumFrameBytes=4u*1024u*1024u;
const std::uint64_t kMaximumConfigBytes=8u*1024u*1024u;
std::string Parent(const std::string &path) {
    const std::size_t slash=path.find_last_of('/');if(slash==std::string::npos)return ".";if(slash==0)return "/";return path.substr(0,slash);
}
std::string Leaf(const std::string &path) {const std::size_t slash=path.find_last_of('/');return slash==std::string::npos?path:path.substr(slash+1);}
std::string DefaultOutput(const std::string &input) {
    std::string name=Leaf(input);const std::size_t dot=name.find_last_of('.');if(dot!=std::string::npos && dot!=0)name.erase(dot);return name+".root";
}
std::uint64_t Integer(const std::uint8_t *p,unsigned n,bool little) {return MfmHeaderInteger(p,n,little);}
class OutputStage {
public:
    explicit OutputStage(const std::string &destination):fDestination(destination) {
        if(destination.empty() || Leaf(destination).empty() || Leaf(destination)=="." || Leaf(destination)=="..")throw std::invalid_argument("invalid ROOT output path");
        struct stat info;if(::lstat(destination.c_str(),&info)==0)throw std::runtime_error("ROOT output already exists; refusing to replace it");
        if(errno!=ENOENT)throw std::runtime_error("cannot inspect ROOT output path");
        const std::string parent=Parent(destination);if(::lstat(parent.c_str(),&info) || !S_ISDIR(info.st_mode))throw std::runtime_error("ROOT output parent must be an existing plain directory");
        const std::string patternText=destination+".stage.XXXXXX";
        std::vector<char> pattern(patternText.begin(),patternText.end());pattern.push_back('\0');
        char *created=::mkdtemp(pattern.data());if(!created)throw std::runtime_error("cannot create private ROOT output directory: "+std::string(std::strerror(errno)));
        fDirectory=created;fTemporary=fDirectory+'/'+Leaf(destination);
    }
    ~OutputStage(){if(!fDirectory.empty())Remove();}
    const std::string &Temporary()const{return fTemporary;}
    void Publish() {
        const int fd=::open(fTemporary.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)throw std::runtime_error("cannot open staged ROOT output for sync");
        const int synced=::fsync(fd),syncError=errno;::close(fd);if(synced)throw std::runtime_error("cannot sync staged ROOT output: "+std::string(std::strerror(syncError)));
        if(::link(fTemporary.c_str(),fDestination.c_str())!=0)throw std::runtime_error("cannot publish ROOT output without replacing an existing path: "+std::string(std::strerror(errno)));
        if(::unlink(fTemporary.c_str())!=0)throw std::runtime_error("ROOT output published but staged link cleanup failed");
        const int dir=::open(Parent(fDestination).c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(dir<0)throw std::runtime_error("cannot open ROOT parent directory for sync");
        const int result=::fsync(dir),error=errno;::close(dir);if(result)throw std::runtime_error("cannot sync ROOT parent directory: "+std::string(std::strerror(error)));
        Remove();
    }
    static void RemoveOwned(const std::string &path) {
        const int fd=::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(fd<0){::rmdir(path.c_str());return;}
        DIR *dir=::fdopendir(fd);if(!dir){::close(fd);return;}
        while(dirent *entry=::readdir(dir)) {const std::string name=entry->d_name;if(name=="."||name=="..")continue;struct stat st;if(!::fstatat(fd,name.c_str(),&st,AT_SYMLINK_NOFOLLOW)&&S_ISREG(st.st_mode))::unlinkat(fd,name.c_str(),0);}
        ::closedir(dir);::rmdir(path.c_str());
    }
private:
    void Remove(){RemoveOwned(fDirectory);fDirectory.clear();fTemporary.clear();}
    std::string fDestination,fDirectory,fTemporary;
};
std::string ReadConfig(const std::string &path) {
    if(path.empty())return std::string();
    std::ifstream file(path.c_str(),std::ios::binary);
    if(!file)throw std::runtime_error("cannot open embedded config: "+path);
    file.seekg(0,std::ios::end);const std::streamoff bytes=file.tellg();if(bytes<0 || std::uint64_t(bytes)>kMaximumConfigBytes)throw std::runtime_error("embedded config exceeds 8 MiB");
    std::string contents(static_cast<std::size_t>(bytes),'\0');file.seekg(0,std::ios::beg);if(bytes && !file.read(&contents[0],bytes))throw std::runtime_error("cannot read embedded config: "+path);return contents;
}
void EmbedConfig(TTree *tree,const std::string &contents) {if(!contents.empty())tree->GetUserInfo()->Add(new TObjString(contents.c_str()));}
void FillHeader(const std::uint8_t *frame,const FrameInfo &info,GET::GFrameHeader &header) {
    const std::uint64_t headerBytes=Integer(frame+8,2,info.littleEndian)*(std::uint64_t(1)<<(frame[0]&15u));
    if(headerBytes<75 || headerBytes>info.bytes)throw std::runtime_error("CoBo frame header is too short for legacy ROOT metadata");
    header.Clear();header.fRevision=static_cast<UShort_t>(info.revision);header.fDataSource=static_cast<UShort_t>(info.source);
    header.fEventTime=static_cast<ULong_t>(Integer(frame+16,6,info.littleEndian));header.fEventIdx=info.event;
    header.fCoboIdx=static_cast<UShort_t>(info.cobo);header.fAsadIdx=static_cast<UShort_t>(info.asad);
    header.fReadOffset=static_cast<UShort_t>(Integer(frame+28,2,info.littleEndian));header.fStatus=frame[30];
    for(unsigned aget=0;aget<4;++aget) {
        header.fHitPatterns[aget].ResetAllBits();header.fHitPatterns[aget].ResetBitNumber(68);
        const unsigned start=31+9*aget;
        for(unsigned channel=0;channel<68;++channel) {
            const unsigned byteIndex=channel/8;const unsigned offset=info.littleEndian?start+byteIndex:start+8-byteIndex;
            if((frame[offset]>>(channel%8))&1u)header.fHitPatterns[aget].SetBitNumber(67-channel,true);
        }
        header.fMult[aget]=static_cast<UShort_t>(Integer(frame+67+2*aget,2,info.littleEndian));
        header.fLastCellIdx[aget]=info.revision>=4 && headerBytes>=87?static_cast<UShort_t>(Integer(frame+79+2*aget,2,info.littleEndian)):0;
    }
    header.fWindowOut=info.revision>=3 && headerBytes>=79?static_cast<UInt_t>(Integer(frame+75,4,info.littleEndian)):0;
}
struct Sample {unsigned aget,channel,bucket,value;};
std::vector<Sample> DecodeSamples(const std::uint8_t *frame,const FrameInfo &info) {
    if(!info.coboFrame || info.revision>5 || (info.type!=1 && !(info.type==2 && info.revision==5)))throw std::runtime_error("ROOT converter supports CoBo type-1 revisions 0..5 and type-2 revision 5 only");
    const std::uint64_t headerBytes=Integer(frame+8,2,info.littleEndian)*(std::uint64_t(1)<<(frame[0]&15u));
    const std::uint64_t itemBytes=Integer(frame+10,2,info.littleEndian),itemCount=Integer(frame+12,4,info.littleEndian);
    const std::uint64_t expectedItemBytes=info.type==1?4:2;
    if(headerBytes<75 || headerBytes>info.bytes || itemBytes!=expectedItemBytes || itemCount>(info.bytes-headerBytes)/itemBytes)throw std::runtime_error("CoBo ROOT frame has inconsistent header/item bounds");
    const std::uint64_t payloadEnd=headerBytes+itemCount*itemBytes,scale=std::uint64_t(1)<<(frame[0]&15u);
    if(((payloadEnd+scale-1)/scale)*scale!=info.bytes)throw std::runtime_error("CoBo ROOT frame has invalid MFM block padding");
    if(itemCount>4u*68u*512u)throw std::runtime_error("CoBo ROOT event exceeds the bounded sample count");
    std::vector<Sample> samples;samples.reserve(static_cast<std::size_t>(itemCount));const std::uint8_t *item=frame+headerBytes;
    unsigned channel[4]={0,0,0,0},bucket[4]={0,0,0,0};
    for(std::uint64_t i=0;i<itemCount;++i,item+=itemBytes) {
        const unsigned word=static_cast<unsigned>(Integer(item,static_cast<unsigned>(itemBytes),false));Sample sample;
        if(info.type==1) {sample.aget=(word>>30)&3u;sample.channel=(word>>23)&0x7fu;sample.bucket=(word>>14)&0x1ffu;sample.value=word&0xfffu;}
        else {sample.aget=(word>>14)&1u;sample.channel=channel[sample.aget];sample.bucket=bucket[sample.aget];sample.value=word&0xfffu;if(channel[sample.aget]>=68){channel[sample.aget]=0;++bucket[sample.aget];sample.channel=0;sample.bucket=bucket[sample.aget];}++channel[sample.aget];}
        if(sample.aget>=4 || sample.channel>=68 || sample.bucket>=512 || sample.value>=4096)throw std::runtime_error("CoBo sample fields exceed legacy ROOT ranges");
        samples.push_back(sample);
    }
    return samples;
}
void AddSamples(GET::GDataFrame &frame,const std::vector<Sample> &samples) {
    for(const auto &sample:samples) {GET::GDataChannel *channel=frame.FindChannel(static_cast<UShort_t>(sample.aget),static_cast<UShort_t>(sample.channel));GET::GDataSample *value=frame.AddSample();value->Set(static_cast<UShort_t>(sample.bucket),static_cast<UShort_t>(sample.value));channel->AddSample(value);}
}
void AddT2K(T2KEvent &event,const std::vector<Sample> &samples) {
    event.SetfNChannels(4*68);event.SetfNBins(512);
    for(int channel=0;channel<4*68;++channel)
        for(int bucket=0;bucket<512;++bucket)event.SetPoint(channel,bucket,0);
    for(const auto &sample:samples)event.SetPoint(static_cast<int>(sample.aget*68+sample.channel),static_cast<int>(sample.bucket),static_cast<double>(sample.value));
}
std::uint64_t FileBytes(const std::string &path) {struct stat info;if(::stat(path.c_str(),&info)||info.st_size<0)throw std::runtime_error("cannot inspect staged ROOT output size");return static_cast<std::uint64_t>(info.st_size);}
}
RootResult ConvertGrawToRoot(const RootOptions &inputOptions) {
    RootOptions options=inputOptions;if(options.output.empty())options.output=DefaultOutput(options.input);
    if(options.input.empty() || options.output.empty())throw std::invalid_argument("input and output paths are required");
    if(!options.maximumOutputBytes)throw std::invalid_argument("maximum ROOT output size must be positive");
    const std::string config=ReadConfig(options.config);OutputStage stage(options.output);
    std::ifstream input(options.input.c_str(),std::ios::binary);if(!input)throw std::runtime_error("cannot open input file: "+options.input);
    std::unique_ptr<TFile> file(TFile::Open(stage.Temporary().c_str(),"CREATE"));if(!file || file->IsZombie())throw std::runtime_error("cannot create staged ROOT file");
    TTree *tree=options.t2k?new TTree("TT2K","A tree with T2K events."):new TTree("tree","A tree of GET data frames");
    GET::GDataFrame dataFrame;T2KEvent t2kEvent;
    if(options.t2k) {
        if(!tree->Branch("T2KEvent",&t2kEvent))throw std::runtime_error("cannot create legacy T2KEvent ROOT branch");
    } else if(!tree->Branch("GDataFrame",&dataFrame)) {
        throw std::runtime_error("cannot create legacy GDataFrame ROOT branch");
    }
    EmbedConfig(tree,config);RootResult result;std::uint64_t selected=0;bool reachedCount=false;
    MfmStream stream(kMaximumFrameBytes,[&](const std::uint8_t *frame,const FrameInfo &info) {
        const std::uint64_t ordinal=result.framesSeen++;
        if(ordinal<options.firstFrame)return;
        if(options.maximumFrames && selected>=options.maximumFrames){reachedCount=true;return;}++selected;
        if(!info.coboFrame || (info.type!=1 && info.type!=2))return;
        const std::vector<Sample> samples=DecodeSamples(frame,info);
        const UInt_t objectCount=TProcessID::GetObjectCount();
        if(options.t2k)AddT2K(t2kEvent,samples);
        else {dataFrame.Clear();FillHeader(frame,info,dataFrame.fHeader);AddSamples(dataFrame,samples);TProcessID::SetObjectCount(objectCount);}
        if(tree->Fill()<0)throw std::runtime_error("cannot append event to ROOT tree");
        ++result.eventsWritten;file->Flush();result.outputBytes=FileBytes(stage.Temporary());
        if(result.outputBytes>options.maximumOutputBytes)throw std::runtime_error("ROOT output exceeds configured byte limit");
        if(options.maximumFrames && selected>=options.maximumFrames)reachedCount=true;
    });
    std::vector<std::uint8_t> block(64u*1024u);
    while(input && !reachedCount) {input.read(reinterpret_cast<char *>(block.data()),static_cast<std::streamsize>(block.size()));const std::streamsize count=input.gcount();if(count>0)stream.Feed(block.data(),static_cast<std::size_t>(count));}
    if(input.bad())throw std::runtime_error("error reading input file: "+options.input);
    if(!reachedCount)stream.Finish();
    input.clear();input.close();
    if(input.fail())throw std::runtime_error("cannot close input file: "+options.input);
    if(!result.eventsWritten)throw std::runtime_error("input contains no supported CoBo event in the selected frame range");
    file->cd();if(tree->Write()<=0)throw std::runtime_error("cannot write ROOT event tree");file->Close();if(file->IsZombie())throw std::runtime_error("ROOT reported an error while closing output");
    result.outputBytes=FileBytes(stage.Temporary());if(result.outputBytes>options.maximumOutputBytes)throw std::runtime_error("ROOT output exceeds configured byte limit");
    stage.Publish();result.output=options.output;return result;
}
}
