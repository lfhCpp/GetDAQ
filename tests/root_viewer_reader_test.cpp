#include "roottreereader.h"
#include "root5_tree_fixture.h"
#include "GDataFrame.h"
#include "T2KEvent.h"
#include <TFile.h>
#include <TTree.h>

#include <cstdlib>
#include <dirent.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {
void Check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
void RemoveTree(const std::string &path) {
    DIR *directory=::opendir(path.c_str());
    if(!directory){::unlink(path.c_str());return;}
    while(dirent *entry=::readdir(directory)) {
        const std::string name=entry->d_name;if(name=="."||name=="..")continue;
        const std::string child=path+'/'+name;struct stat info;
        if(!::lstat(child.c_str(),&info)&&S_ISDIR(info.st_mode))RemoveTree(child);else ::unlink(child.c_str());
    }
    ::closedir(directory);::rmdir(path.c_str());
}
void CreateRootFile(const std::string &path) {
    TFile file(path.c_str(),"RECREATE");Check(!file.IsZombie(),"cannot create ROOT reader fixture");
    TTree tree("tree","legacy CoBo data frames");GET::GDataFrame frame;
    Check(tree.Branch("GDataFrame",&frame)!=nullptr,"cannot create legacy branch fixture");
    frame.fHeader.fEventIdx=17;frame.fHeader.fEventTime=123456;frame.fHeader.fCoboIdx=2;frame.fHeader.fAsadIdx=1;
    frame.AddSample(1,3,9,123);frame.AddSample(1,3,12,456);frame.AddSample(2,8,511,4095);
    Check(tree.Fill()>=0,"cannot fill ROOT reader fixture");
    file.cd();Check(tree.Write()>0,"cannot write ROOT reader fixture");file.Close();
}
void CreateT2KRootFile(const std::string &path, bool validDimensions) {
    TFile file(path.c_str(),"RECREATE");Check(!file.IsZombie(),"cannot create T2K ROOT reader fixture");
    TTree tree("TT2K","legacy T2K matrix events");T2KEvent event;
    Check(tree.Branch("T2KEvent",&event)!=nullptr,"cannot create T2K branch fixture");
    event.SetfNChannels(validDimensions?4*68:1);event.SetfNBins(validDimensions?512:1);
    for(int i=0;i<event.fNTBins;++i)event.fData[i]=0;
    if(validDimensions) {
        event.SetPoint(1*68+3,9,123);event.SetPoint(1*68+3,12,456);
        event.SetPoint(2*68+8,511,4095);
    }
    Check(tree.Fill()>=0,"cannot fill T2K ROOT reader fixture");
    file.cd();Check(tree.Write()>0,"cannot write T2K ROOT reader fixture");file.Close();
}
}

int main() {
    const char *temp=std::getenv("TMPDIR");if(!temp||!*temp||::access(temp,W_OK|X_OK))temp="/tmp";
    const std::string pattern=std::string(temp)+"/getdaq-root-reader-test-XXXXXX";
    std::string writable=pattern;writable.push_back('\0');
    if(!::mkdtemp(&writable[0])){std::cerr<<"cannot create ROOT reader test directory\n";return 1;}
    writable.resize(writable.find('\0'));
    try {
        const std::string path=writable+"/events.root";CreateRootFile(path);
        GetDAQ::RootTreeReader reader;std::string error;
        Check(reader.Open(path,&error),error.c_str());Check(reader.Entries()==1,"ROOT tree entry count differs");
        GetDAQ::RootEvent event;Check(reader.Read(0,&event,&error),error.c_str());
        Check(event.event==17&&event.eventTime==123456&&event.cobo==2&&event.asad==1,"ROOT event header differs");
        Check(event.channelCount==2&&event.sampleCount==3,"ROOT channel/sample counts differ");
        const auto &channel=event.channels[1][3];
        Check(channel.size()==2&&channel[0].bucket==9&&channel[0].value==123&&channel[1].bucket==12&&channel[1].value==456,
              "ROOT sample references differ");
        Check(event.channels[2][8].size()==1&&event.channels[2][8][0].bucket==511&&event.channels[2][8][0].value==4095,
              "ROOT final valid bucket/sample differs");
        Check(!reader.Read(1,&event,&error),"out-of-range ROOT entry was accepted");
        const std::string root5Path=writable+"/root5-era.root";
        CreateRoot5NamespaceTreeFixture(root5Path);
        Check(reader.Open(root5Path,&error),error.c_str());
        Check(reader.Read(0,&event,&error),error.c_str());
        Check(event.event==29&&event.eventTime==987654321&&event.cobo==3&&event.asad==2,
              "ROOT5-era get::GDataFrame header differs");
        Check(event.channelCount==2&&event.sampleCount==2&&
              event.channels[0][1].size()==1&&event.channels[0][1][0].bucket==5&&
              event.channels[0][1][0].value==321&&event.channels[3][67][0].bucket==511&&
              event.channels[3][67][0].value==4095,
              "ROOT5-era get::GDataFrame channel/sample references differ");
        const std::string invalidRoot5Path=writable+"/invalid-root5-era.root";
        CreateRoot5NamespaceTreeFixture(invalidRoot5Path,true);
        Check(reader.Open(invalidRoot5Path,&error),error.c_str());
        Check(!reader.Read(0,&event,&error),"ROOT5-era out-of-range AGET was accepted");
        const std::string t2kPath=writable+"/t2k.root";CreateT2KRootFile(t2kPath,true);
        Check(reader.Open(t2kPath,&error),error.c_str());
        Check(reader.Entries()==1,"T2K ROOT tree entry count differs");
        Check(reader.Read(0,&event,&error),error.c_str());
        Check(event.format==GetDAQ::RootEvent::Format::T2KMatrix,"T2K matrix format was not identified");
        Check(event.channelCount==272&&event.sampleCount==272u*512u,"T2K matrix dimensions differ");
        const auto &t2kChannel=event.channels[1][3];
        Check(t2kChannel.size()==512&&t2kChannel[9].value==123&&t2kChannel[12].value==456&&
              t2kChannel[10].value==0,"T2K sample positions or zero fill differ");
        Check(event.channels[2][8][511].bucket==511&&event.channels[2][8][511].value==4095,
              "T2K final valid bucket/sample differs");
        const std::string invalidT2kPath=writable+"/invalid-t2k.root";CreateT2KRootFile(invalidT2kPath,false);
        Check(reader.Open(invalidT2kPath,&error),error.c_str());
        Check(!reader.Read(0,&event,&error),"invalid T2K matrix dimensions were accepted");
        Check(!reader.Open(writable+"/missing.root",&error),"missing ROOT file was accepted");
        RemoveTree(writable);
        std::cout<<"PASS ROOT viewer reader opens GET::GDataFrame, ROOT5-era get::GDataFrame and T2K matrix trees, reads samples and zero fill, and rejects invalid entries/dimensions; temp files removed=true\n";
        return 0;
    } catch(const std::exception &exception) {
        RemoveTree(writable);std::cerr<<exception.what()<<'\n';return 1;
    }
}
