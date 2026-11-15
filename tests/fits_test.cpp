#include "../apps/datareceiver/fits.h"
#include <fitsio.h>
#include <dirent.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
namespace {
void Check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
void Put16(std::vector<unsigned char> &bytes,std::size_t at,unsigned value){bytes[at]=static_cast<unsigned char>(value>>8);bytes[at+1]=static_cast<unsigned char>(value);}
void Put32(std::vector<unsigned char> &bytes,std::size_t at,unsigned value){bytes[at]=static_cast<unsigned char>(value>>24);bytes[at+1]=static_cast<unsigned char>(value>>16);bytes[at+2]=static_cast<unsigned char>(value>>8);bytes[at+3]=static_cast<unsigned char>(value);}
void Put24(std::vector<unsigned char> &bytes,std::size_t at,unsigned value){bytes[at]=static_cast<unsigned char>(value>>16);bytes[at+1]=static_cast<unsigned char>(value>>8);bytes[at+2]=static_cast<unsigned char>(value);}
std::vector<unsigned char> Frame(unsigned event) {
    std::vector<unsigned char> bytes(512,0);bytes[0]=8;bytes[3]=2;Put16(bytes,5,1);bytes[7]=5;
    Put16(bytes,8,1);Put16(bytes,10,4);Put32(bytes,12,4);Put32(bytes,22,event);bytes[26]=1;bytes[27]=2;
    const unsigned words[]={ (1u<<30)|(3u<<23)|(9u<<14)|100u,
        (1u<<30)|(3u<<23)|(10u<<14)|101u,
        (2u<<30)|(4u<<23)|200u,(2u<<30)|(4u<<23)|(1u<<14)|201u };
    for(std::size_t i=0;i<4;++i)Put32(bytes,256+i*4,words[i]);
    return bytes;
}
std::vector<unsigned char> CompactFrame(unsigned event) {
    const std::size_t headerBytes=256,itemCount=std::size_t(4)*68*512;
    const std::size_t frameBytes=headerBytes+itemCount*2;
    Check(frameBytes%256==0,"compact fixture frame is not MFM block aligned");
    std::vector<unsigned char> bytes(frameBytes,0);bytes[0]=8;Put24(bytes,1,unsigned(frameBytes/256));
    Put16(bytes,5,2);bytes[7]=5;Put16(bytes,8,1);Put16(bytes,10,2);Put32(bytes,12,unsigned(itemCount));
    Put32(bytes,22,event);bytes[26]=1;bytes[27]=2;
    std::size_t offset=headerBytes;
    for(unsigned bucket=0;bucket<512;++bucket)for(unsigned channel=0;channel<68;++channel)for(unsigned aget=0;aget<4;++aget) {
        const unsigned value=(aget*1000+channel*4+bucket)&0xfffu;
        Put16(bytes,offset,aget<<14|value);offset+=2;
    }
    return bytes;
}
void Write(const std::string &path,const std::vector<unsigned char> &bytes) {
    std::ofstream output(path.c_str(),std::ios::binary);output.write(reinterpret_cast<const char *>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if(!output)throw std::runtime_error("cannot write raw fixture");
}
void RemoveTree(const std::string &path) {
    DIR *directory=::opendir(path.c_str());if(!directory){::unlink(path.c_str());return;}
    while(dirent *entry=::readdir(directory)) {
        const std::string name=entry->d_name;if(name=="."||name=="..")continue;
        const std::string child=path+'/'+name;struct stat info;
        if(!::lstat(child.c_str(),&info) && S_ISDIR(info.st_mode))RemoveTree(child);else ::unlink(child.c_str());
    }
    ::closedir(directory);::rmdir(path.c_str());
}
void FitsOk(int status,const char *message){if(!status)return;char text[FLEN_STATUS]={};fits_get_errstatus(status,text);throw std::runtime_error(std::string(message)+": "+text);}
void Verify(const std::string &directory) {
    const std::string indexPath=directory+"/run_index.fits.gz",eventPath=directory+"/run_CoBo1_AsAd2_EVT42.fits.gz";
    fitsfile *index=nullptr;int status=0;fits_open_file(&index,indexPath.c_str(),READONLY,&status);FitsOk(status,"open event index");
    fits_movnam_hdu(index,BINARY_TBL,const_cast<char *>("GROUPING"),0,&status);FitsOk(status,"find GROUPING table");
    LONGLONG rows=0;fits_get_num_rowsll(index,&rows,&status);FitsOk(status,"read grouping row count");Check(rows==276,"grouping index row count must include both type-1 and compact type-2 channels");
    unsigned int event=0;unsigned short aget=0,channel=0;int any=0;
    fits_read_col(index,TUINT,1,1,1,1,nullptr,&event,&any,&status);FitsOk(status,"read event index id");Check(event==42,"event ID differs");
    fits_read_col(index,TUSHORT,4,1,1,1,nullptr,&aget,&any,&status);FitsOk(status,"read AGET index");
    fits_read_col(index,TUSHORT,5,1,1,1,nullptr,&channel,&any,&status);FitsOk(status,"read channel index");Check(aget==1&&channel==3,"first channel identity differs");
    char member[64]={};char *memberPtr=member;fits_read_col(index,TSTRING,7,1,1,1,nullptr,&memberPtr,&any,&status);FitsOk(status,"read member name");
    Check(std::string(member)=="GET-EVTS-CHAN","grouping member extension differs");
    char location[128]={};char *locationPtr=location;fits_read_col(index,TSTRING,11,1,1,1,nullptr,&locationPtr,&any,&status);FitsOk(status,"read member location");
    Check(std::string(location)=="run_CoBo1_AsAd2_EVT42.fits.gz","grouping member location does not name its event FITS file");
    fits_close_file(index,&status);FitsOk(status,"close event index");

    fitsfile *eventFile=nullptr;status=0;fits_open_file(&eventFile,eventPath.c_str(),READONLY,&status);FitsOk(status,"open event FITS");
    int hdus=0;fits_get_num_hdus(eventFile,&hdus,&status);FitsOk(status,"read FITS HDU count");Check(hdus==3,"event file must contain primary plus two channel tables");
    fits_movabs_hdu(eventFile,1,&hdus,&status);FitsOk(status,"select primary HDU");
    int primaryDataChecksum=0,primaryHduChecksum=0;fits_verify_chksum(eventFile,&primaryDataChecksum,&primaryHduChecksum,&status);FitsOk(status,"verify primary checksum");
    Check(primaryHduChecksum==1,"primary HDU checksum is invalid");
    fits_movnam_hdu(eventFile,BINARY_TBL,const_cast<char *>("GET-EVTS-CHAN"),1,&status);FitsOk(status,"select first channel table");
    rows=0;fits_get_num_rowsll(eventFile,&rows,&status);FitsOk(status,"read channel row count");Check(rows==2,"channel table sample count differs");
    unsigned short buckets[2]={},values[2]={};fits_read_col(eventFile,TUSHORT,1,1,1,2,nullptr,buckets,&any,&status);FitsOk(status,"read buckets");
    fits_read_col(eventFile,TUSHORT,2,1,1,2,nullptr,values,&any,&status);FitsOk(status,"read samples");
    Check(buckets[0]==9&&buckets[1]==10&&values[0]==100&&values[1]==101,"FITS channel values differ from the source frame");
    int dataChecksum=0,hduChecksum=0;fits_verify_chksum(eventFile,&dataChecksum,&hduChecksum,&status);FitsOk(status,"verify FITS checksum");Check(dataChecksum==1&&hduChecksum==1,"channel HDU checksum is invalid");
    fits_close_file(eventFile,&status);FitsOk(status,"close event FITS");
}
void VerifyCompact(const std::string &directory) {
    const std::string indexPath=directory+"/run_index.fits.gz",eventPath=directory+"/run_CoBo1_AsAd2_EVT99.fits.gz";
    fitsfile *index=nullptr;int status=0;fits_open_file(&index,indexPath.c_str(),READONLY,&status);FitsOk(status,"open compact event index");
    fits_movnam_hdu(index,BINARY_TBL,const_cast<char *>("GROUPING"),0,&status);FitsOk(status,"find compact GROUPING table");
    LONGLONG rows=0;fits_get_num_rowsll(index,&rows,&status);FitsOk(status,"read compact grouping row count");
    Check(rows==276,"compact readout must create one index row per channel and preserve preceding events");
    fits_close_file(index,&status);FitsOk(status,"close compact event index");

    fitsfile *eventFile=nullptr;status=0;fits_open_file(&eventFile,eventPath.c_str(),READONLY,&status);FitsOk(status,"open compact event FITS");
    int hdus=0;fits_get_num_hdus(eventFile,&hdus,&status);FitsOk(status,"read compact FITS HDUs");
    Check(hdus==273,"compact event FITS must contain a primary HDU and 272 channel tables");
    fits_movnam_hdu(eventFile,BINARY_TBL,const_cast<char *>("GET-EVTS-CHAN"),71,&status);FitsOk(status,"select compact AGET/channel extension");
    unsigned short aget=0,channel=0;fits_read_key(eventFile,TUSHORT,const_cast<char *>("AGET"),&aget,nullptr,&status);FitsOk(status,"read compact AGET key");
    fits_read_key(eventFile,TUSHORT,const_cast<char *>("CHAN"),&channel,nullptr,&status);FitsOk(status,"read compact channel key");
    Check(aget==2&&channel==17,"compact FITS extension order or channel metadata differs");
    fits_get_num_rowsll(eventFile,&rows,&status);FitsOk(status,"read compact channel sample count");Check(rows==512,"compact FITS channel must contain all 512 buckets");
    unsigned short buckets[2]={},values[2]={};int any=0;
    fits_read_col(eventFile,TUSHORT,1,1,1,1,nullptr,buckets,&any,&status);FitsOk(status,"read compact first bucket");
    fits_read_col(eventFile,TUSHORT,2,1,1,1,nullptr,values,&any,&status);FitsOk(status,"read compact first sample");
    Check(buckets[0]==0&&values[0]==2068,"compact first bucket/sample differs");
    fits_read_col(eventFile,TUSHORT,1,512,1,1,nullptr,buckets+1,&any,&status);FitsOk(status,"read compact last bucket");
    fits_read_col(eventFile,TUSHORT,2,512,1,1,nullptr,values+1,&any,&status);FitsOk(status,"read compact last sample");
    Check(buckets[1]==511&&values[1]==2579,"compact final bucket/sample differs");
    int dataChecksum=0,hduChecksum=0;fits_verify_chksum(eventFile,&dataChecksum,&hduChecksum,&status);FitsOk(status,"verify compact FITS checksum");
    Check(dataChecksum==1&&hduChecksum==1,"compact channel HDU checksum is invalid");
    fits_close_file(eventFile,&status);FitsOk(status,"close compact event FITS");
}
void ExpectFailure(const GetDAQ::FitsOptions &options,const char *message) {
    bool failed=false;try{GetDAQ::ConvertGrawToFits(options);}catch(const std::exception &){failed=true;}
    Check(failed,message);Check(::access(options.outputDirectory.c_str(),F_OK)!=0,"failed FITS conversion published output");
}
}
int main() {
    const char *temp=std::getenv("TMPDIR");if(!temp||!*temp||::access(temp,W_OK|X_OK))temp="/tmp";
    std::string pattern=std::string(temp)+"/getdaq-fits-test-XXXXXX";std::vector<char> writable(pattern.begin(),pattern.end());writable.push_back('\0');
    if(!::mkdtemp(writable.data())){std::cerr<<"cannot create FITS test directory\n";return 1;}
    const std::string root=writable.data();
    try {
        const std::string input=root+"/synthetic.graw";auto frame=Frame(42);Write(input,frame);auto second=Frame(43);frame.insert(frame.end(),second.begin(),second.end());
        const auto compact=CompactFrame(99);frame.insert(frame.end(),compact.begin(),compact.end());Write(input,frame);
        GetDAQ::FitsOptions options;options.input=input;options.outputDirectory=root+"/published";options.baseName="run";
        const auto result=GetDAQ::ConvertGrawToFits(options);
        Check(result.coboFramesSeen==3&&result.eventsWritten==3&&result.channelsWritten==276,"unexpected FITS conversion counters");
        Verify(options.outputDirectory);
        VerifyCompact(options.outputDirectory);
        bool existing=false;try{GetDAQ::ConvertGrawToFits(options);}catch(const std::exception &){existing=true;}
        Check(existing,"existing FITS output directory was replaced");

        GetDAQ::FitsOptions selected=options;selected.outputDirectory=root+"/selected";selected.maximumCoboFrames=1;
        const auto limited=GetDAQ::ConvertGrawToFits(selected);Check(limited.eventsWritten==1&&limited.coboFramesSeen==1,"count limit did not stop after the selected event");
        GetDAQ::FitsOptions capped=options;capped.outputDirectory=root+"/capped";capped.maximumOutputBytes=1;ExpectFailure(capped,"output byte cap was not enforced");
        GetDAQ::FitsOptions incomplete=options;incomplete.input=root+"/incomplete-type2.graw";incomplete.outputDirectory=root+"/incomplete-type2";
        auto incompleteFrame=CompactFrame(100);Put32(incompleteFrame,12,unsigned(4*68*512-1));Write(incomplete.input,incompleteFrame);
        ExpectFailure(incomplete,"incomplete compact type-2 frame was accepted");
        GetDAQ::FitsOptions broken=options;broken.input=root+"/truncated.graw";broken.outputDirectory=root+"/truncated";Write(broken.input,std::vector<unsigned char>(13,0));
        ExpectFailure(broken,"truncated MFM input was accepted");
        RemoveTree(root);std::cout<<"PASS FITS type-1 and compact type-2 grouping/index, event/channel metadata, bucket/sample readback, checksums, frame count, no-replace publication, output cap, truncation rejection; synthetic raw bytes="<<frame.size()<<" removed=true\n";return 0;
    } catch(const std::exception &error) {RemoveTree(root);std::cerr<<error.what()<<'\n';return 1;}
}
