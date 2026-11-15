#include "../apps/datareceiver/rootconverter.h"
#include "GDataFrame.h"
#include "GDataChannel.h"
#include "GDataSample.h"
#include "T2KEvent.h"
#include <TFile.h>
#include <TObjString.h>
#include <TTree.h>
#include <TClonesArray.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
void Check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
void Put16(std::vector<unsigned char> &bytes,std::size_t at,unsigned value){bytes[at]=static_cast<unsigned char>(value>>8);bytes[at+1]=static_cast<unsigned char>(value);}
void Put32(std::vector<unsigned char> &bytes,std::size_t at,unsigned value){bytes[at]=static_cast<unsigned char>(value>>24);bytes[at+1]=static_cast<unsigned char>(value>>16);bytes[at+2]=static_cast<unsigned char>(value>>8);bytes[at+3]=static_cast<unsigned char>(value);}
void Put48(std::vector<unsigned char> &bytes,std::size_t at,unsigned long long value){for(unsigned i=0;i<6;++i)bytes[at+i]=static_cast<unsigned char>(value>>(40-8*i));}
std::vector<unsigned char> Frame(unsigned event) {
    std::vector<unsigned char> bytes(512,0);bytes[0]=8;bytes[3]=2;Put16(bytes,5,1);bytes[7]=5;
    Put16(bytes,8,1);Put16(bytes,10,4);Put32(bytes,12,4);Put48(bytes,16,event*100);Put32(bytes,22,event);bytes[26]=1;bytes[27]=2;
    bytes[31+9+8]|=1u<<3;
    const unsigned words[]={ (1u<<30)|(3u<<23)|(9u<<14)|100u,
        (1u<<30)|(3u<<23)|(10u<<14)|101u,
        (2u<<30)|(4u<<23)|200u,(2u<<30)|(4u<<23)|(1u<<14)|201u };
    for(std::size_t i=0;i<4;++i)Put32(bytes,256+i*4,words[i]);
    return bytes;
}
void Write(const std::string &path,const std::vector<unsigned char> &bytes){std::ofstream file(path.c_str(),std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));if(!file)throw std::runtime_error("cannot write synthetic raw input");}
void RemoveTree(const std::string &path){DIR *dir=::opendir(path.c_str());if(!dir){::unlink(path.c_str());return;}while(dirent *entry=::readdir(dir)){std::string name=entry->d_name;if(name=="."||name=="..")continue;std::string child=path+'/'+name;struct stat info;if(!::lstat(child.c_str(),&info)&&S_ISDIR(info.st_mode))RemoveTree(child);else ::unlink(child.c_str());}::closedir(dir);::rmdir(path.c_str());}
void CheckNoStage(const std::string &output) {
    const std::size_t slash=output.find_last_of('/');
    const std::string parent=slash==std::string::npos?".":(slash==0?"/":output.substr(0,slash));
    const std::string prefix=(slash==std::string::npos?output:output.substr(slash+1))+".stage.";
    DIR *directory=::opendir(parent.c_str());Check(directory!=nullptr,"cannot inspect ROOT conversion parent");
    bool found=false;
    while(dirent *entry=::readdir(directory))
        if(std::strncmp(entry->d_name,prefix.c_str(),prefix.size())==0)found=true;
    ::closedir(directory);
    Check(!found,"failed ROOT conversion left a staging directory");
}
void ExpectFailure(const GetDAQ::RootOptions &options,const char *message) {
    bool failed=false;
    try{GetDAQ::ConvertGrawToRoot(options);}catch(const std::exception&){failed=true;}
    Check(failed,message);
    Check(::access(options.output.c_str(),F_OK)!=0,"failed ROOT conversion published output");
    CheckNoStage(options.output);
}
void CheckObjectTree(const std::string &path) {
    TFile file(path.c_str(),"READ");Check(!file.IsZombie(),"cannot reopen legacy object ROOT file");
    TTree *tree=nullptr;file.GetObject("tree",tree);Check(tree!=nullptr,"legacy tree name differs");
    TObject *embedded=tree->GetUserInfo()->At(0);
    Check(embedded!=nullptr,"embedded configuration is missing from TTree UserInfo");
    TObjString *config=dynamic_cast<TObjString*>(embedded);
    Check(config&&std::string(config->GetString().Data())=="<run id=\"42\"/>\n","embedded configuration contents differ");
    TBranch *branch=tree->GetBranch("GDataFrame");Check(branch!=nullptr,"legacy branch is missing");
    Check(std::string(branch->GetClassName())=="GET::GDataFrame","legacy object branch class differs");
    Check(tree->GetEntries()==1,"legacy tree entry count differs");
    GET::GDataFrame *frame=nullptr;Check(tree->SetBranchAddress("GDataFrame",&frame)>=0,"cannot bind legacy object branch");
    Check(tree->GetEntry(0)>0&&frame!=nullptr,"cannot read legacy object branch");
    Check(frame->fHeader.fEventIdx==42&&frame->fHeader.fEventTime==4200,"legacy event metadata differs");
    Check(frame->fHeader.fHitPatterns[1].TestBitNumber(64),"legacy hit-pattern bit mapping differs");
    Check(frame->GetNchannels()==2&&frame->GetNsamples()==4,"legacy channel/sample object counts differ");
    GET::GDataChannel *channel=frame->FindChannel(1,3);Check(channel&&channel->GetNhit()==2,"legacy channel hit references differ");
    TClonesArray *samples=frame->GetSamples();Check(samples&&samples->GetEntriesFast()==4,"legacy sample object array differs");
    GET::GDataSample *sample=static_cast<GET::GDataSample*>(samples->At(0));
    Check(sample&&sample->fBuckIdx==9&&sample->fValue==100,"legacy sample bucket/value differs");
    tree->ResetBranchAddresses();file.Close();
}
void CheckT2KTree(const std::string &path) {
    TFile file(path.c_str(),"READ");Check(!file.IsZombie(),"cannot reopen T2K ROOT file");
    TTree *tree=nullptr;file.GetObject("TT2K",tree);Check(tree&&tree->GetEntries()==1,"legacy T2K tree differs");
    T2KEvent *event=nullptr;Check(tree->SetBranchAddress("T2KEvent",&event)>=0,"cannot bind T2K branch");
    Check(tree->GetEntry(0)>0&&event,"cannot read T2K event");
    Check(event->fNChannels==272&&event->fNBins==512,"T2K dimensions differ");
    TArrayI channel=event->GetChannel(68+3);Check(channel.GetSize()==512,"T2K channel has wrong bucket count");
    Check(channel.At(9)==100&&channel.At(10)==101&&channel.At(8)==0,"T2K sample or missing-bucket zero fill differs");
    tree->ResetBranchAddresses();file.Close();
}
}
int main(){
    const char *temp=std::getenv("TMPDIR");if(!temp||!*temp||::access(temp,W_OK|X_OK))temp="/tmp";
    std::string pattern=std::string(temp)+"/getdaq-root-test-XXXXXX";std::vector<char> writable(pattern.begin(),pattern.end());writable.push_back('\0');
    if(!::mkdtemp(writable.data())){std::cerr<<"cannot create ROOT test directory\n";return 1;}
    const std::string root=writable.data();std::uint64_t rawBytes=0;
    try {
        const std::string input=root+"/synthetic.graw";std::vector<unsigned char> bytes=Frame(42);rawBytes=bytes.size();Write(input,bytes);
        const std::string config=root+"/run.xcfg";{std::ofstream file(config.c_str(),std::ios::binary);file<<"<run id=\"42\"/>\n";}
        GetDAQ::RootOptions options;options.input=input;options.output=root+"/legacy.root";options.config=config;
        GetDAQ::RootResult result=GetDAQ::ConvertGrawToRoot(options);
        Check(result.framesSeen==1&&result.eventsWritten==1&&result.output==options.output,"ROOT conversion counters or output path differ");CheckObjectTree(options.output);
        bool existing=false;try{GetDAQ::ConvertGrawToRoot(options);}catch(const std::exception&){existing=true;}Check(existing,"existing ROOT file was replaced");
        GetDAQ::RootOptions t2k=options;t2k.t2k=true;t2k.output=root+"/t2k.root";GetDAQ::ConvertGrawToRoot(t2k);CheckT2KTree(t2k.output);
        GetDAQ::RootOptions capped=options;capped.output=root+"/capped.root";capped.maximumOutputBytes=1;ExpectFailure(capped,"ROOT output byte cap was not enforced");
        GetDAQ::RootOptions broken=options;broken.input=root+"/truncated.graw";broken.output=root+"/truncated.root";Write(broken.input,std::vector<unsigned char>(13,0));ExpectFailure(broken,"truncated MFM input was accepted");rawBytes+=13;
        RemoveTree(root);std::cout<<"PASS legacy tree/branch/class compatibility, T2K layout/zero-fill, metadata/sample readback, no-overwrite publication, output cap and truncation rejection; synthetic raw bytes="<<rawBytes<<" removed=true\n";return 0;
    } catch(const std::exception &error){RemoveTree(root);std::cerr<<error.what()<<'\n';return 1;}
}
