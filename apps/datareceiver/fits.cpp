#include "fits.h"
#include "mfmstream.h"
#include "fits_writer_p.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif
#if defined(__APPLE__)
#include <stdio.h>
#endif

namespace GetDAQ {
namespace {
const std::size_t kMaximumFrameBytes=4u*1024u*1024u;
void CheckDirectory(const std::string &path) {
    struct stat info;if(::lstat(path.c_str(),&info) || !S_ISDIR(info.st_mode))throw std::runtime_error("output parent is not an existing plain directory: "+path);
}
std::string Parent(const std::string &path) {
    const auto slash=path.find_last_of('/');if(slash==std::string::npos)return ".";if(slash==0)return "/";return path.substr(0,slash);
}
std::string Leaf(const std::string &path) {
    const auto slash=path.find_last_of('/');return slash==std::string::npos?path:path.substr(slash+1);
}
std::string Stem(const std::string &path) {
    std::string name=Leaf(path);const auto dot=name.find_last_of('.');if(dot!=std::string::npos && dot!=0)name.erase(dot);return name;
}
bool SafeBaseName(const std::string &name) {
    return !name.empty() && name.size()<=128 && name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-")==std::string::npos;
}
void SyncFile(const std::string &path) {
    const int fd=::open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)throw std::runtime_error("cannot open staged FITS file for sync: "+path);
    const int result=::fsync(fd),error=errno;::close(fd);if(result)throw std::runtime_error("cannot sync staged FITS file: "+std::string(std::strerror(error)));
}
void SyncDirectory(const std::string &path) {
    const int fd=::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)throw std::runtime_error("cannot open directory for sync: "+path);
    const int result=::fsync(fd),error=errno;::close(fd);if(result)throw std::runtime_error("cannot sync directory: "+std::string(std::strerror(error)));
}
void RemoveOwnedDirectory(const std::string &path) {
    const int fd=::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(fd<0){::rmdir(path.c_str());return;}
    DIR *directory=::fdopendir(fd);if(!directory){::close(fd);return;}
    while(dirent *entry=::readdir(directory)) {
        const std::string name=entry->d_name;if(name=="." || name=="..")continue;
        struct stat info;if(::fstatat(fd,name.c_str(),&info,AT_SYMLINK_NOFOLLOW))continue;
        if(S_ISREG(info.st_mode))::unlinkat(fd,name.c_str(),0);
    }
    ::closedir(directory);::rmdir(path.c_str());
}
void PublishNoReplace(const std::string &staging,const std::string &destination) {
#if defined(__linux__) && defined(SYS_renameat2)
    if(::syscall(SYS_renameat2,AT_FDCWD,staging.c_str(),AT_FDCWD,destination.c_str(),1u)==0)return;
    if(errno==EEXIST)throw std::runtime_error("output directory appeared during conversion; no files published");
    if(errno!=ENOSYS && errno!=EINVAL)throw std::runtime_error("cannot publish FITS directory without replacing an existing path: "+std::string(std::strerror(errno)));
#elif defined(__APPLE__)
    if(::renameatx_np(AT_FDCWD,staging.c_str(),AT_FDCWD,destination.c_str(),RENAME_EXCL)==0)return;
    if(errno==EEXIST)throw std::runtime_error("output directory appeared during conversion; no files published");
    throw std::runtime_error("cannot publish FITS directory without replacing an existing path: "+std::string(std::strerror(errno)));
#endif
    throw std::runtime_error("atomic no-replace directory publication is unavailable on this platform");
}
std::uint64_t DirectoryBytes(const std::string &path) {
    std::uint64_t total=0;const int fd=::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)throw std::runtime_error("cannot inspect staged FITS directory");
    DIR *directory=::fdopendir(fd);if(!directory){::close(fd);throw std::runtime_error("cannot inspect staged FITS entries");}
    while(dirent *entry=::readdir(directory)) {
        const std::string name=entry->d_name;if(name=="." || name=="..")continue;struct stat info;
        if(::fstatat(fd,name.c_str(),&info,AT_SYMLINK_NOFOLLOW) || !S_ISREG(info.st_mode) || info.st_size<0){::closedir(directory);throw std::runtime_error("unexpected staged FITS entry");}
        const auto bytes=static_cast<std::uint64_t>(info.st_size);if(total>UINT64_MAX-bytes){::closedir(directory);throw std::runtime_error("FITS output size overflow");}total+=bytes;
    }
    ::closedir(directory);return total;
}
std::string EventName(const std::string &base,const FrameInfo &info) {
    return base+"_CoBo"+std::to_string(info.cobo)+"_AsAd"+std::to_string(info.asad)+"_EVT"+std::to_string(info.event)+".fits.gz";
}
}

FitsResult ConvertGrawToFits(const FitsOptions &options) {
    if(options.input.empty() || options.outputDirectory.empty())throw std::invalid_argument("input and output directory are required");
    struct stat existing;if(!::lstat(options.outputDirectory.c_str(),&existing))throw std::runtime_error("output directory already exists; refusing to replace it");
    if(errno!=ENOENT)throw std::runtime_error("cannot inspect FITS output directory");
    const std::string parent=Parent(options.outputDirectory),leaf=Leaf(options.outputDirectory);if(!SafeBaseName(leaf))throw std::invalid_argument("output directory must have a plain final path component");CheckDirectory(parent);
    const std::string base=options.baseName.empty()?Stem(options.input):options.baseName;if(!SafeBaseName(base))throw std::invalid_argument("basename must contain only letters, digits, dot, underscore, or hyphen");
    if(!options.maximumOutputBytes)throw std::invalid_argument("maximum output byte limit must be positive");
    std::string pattern=options.outputDirectory+".tmp.XXXXXX";std::vector<char> stagingName(pattern.begin(),pattern.end());stagingName.push_back('\0');
    char *created=::mkdtemp(stagingName.data());if(!created)throw std::runtime_error("cannot create private FITS staging directory: "+std::string(std::strerror(errno)));
    const std::string staging(created);bool published=false;
    try {
        FitsInternal::Writer writer(staging,base);
        const std::string &indexPath=writer.IndexPath();
        std::ifstream input(options.input.c_str(),std::ios::binary);if(!input)throw std::runtime_error("cannot open input file: "+options.input);
        FitsResult result;std::set<std::string> eventNames;
        MfmStream stream(kMaximumFrameBytes,[&](const std::uint8_t *frame,const FrameInfo &info) {
            if(!info.coboFrame || (info.type!=1 && info.type!=2))return;
            const std::uint64_t frameIndex=result.coboFramesSeen++;
            if(frameIndex<options.firstCoboFrame)return;
            FitsInternal::EventData event=FitsInternal::DecodeEvent(frame,info);const std::string fileName=EventName(base,info);
            if(!eventNames.insert(fileName).second)throw std::runtime_error("duplicate CoBo/AsAd/event identifiers would overwrite a FITS event file: "+fileName);
            const std::uint64_t estimated=static_cast<std::uint64_t>(event.samples)*4u+static_cast<std::uint64_t>(event.channels.size())*4096u+16384u;
            const std::uint64_t indexEstimate=(writer.IndexRows()+event.channels.size())*384u+8192u;
            if(result.outputBytes>options.maximumOutputBytes || estimated>options.maximumOutputBytes-result.outputBytes || indexEstimate>options.maximumOutputBytes-result.outputBytes-estimated)
                throw std::runtime_error("FITS output would exceed configured maximum byte limit");
            writer.WriteEvent(staging+'/'+fileName,event);
            SyncFile(staging+'/'+fileName);result.outputBytes=DirectoryBytes(staging);
            if(result.outputBytes>options.maximumOutputBytes)throw std::runtime_error("FITS output exceeded configured maximum byte limit");
            ++result.eventsWritten;result.channelsWritten+=event.channels.size();
            if(options.maximumCoboFrames && result.eventsWritten==options.maximumCoboFrames)throw std::runtime_error("selected frame count reached");
        });
        std::vector<std::uint8_t> block(64u*1024u);bool reachedCount=false;
        while(input && !reachedCount) {
            input.read(reinterpret_cast<char *>(block.data()),static_cast<std::streamsize>(block.size()));const auto count=input.gcount();
            if(count>0) {
                try {stream.Feed(block.data(),static_cast<std::size_t>(count));}
                catch(const std::runtime_error &error) {
                    if(options.maximumCoboFrames && std::string(error.what())=="selected frame count reached")reachedCount=true;
                    else throw;
                }
            }
        }
        if(input.bad())throw std::runtime_error("error while reading input file: "+options.input);
        if(!reachedCount)stream.Finish();
        input.clear();input.close();if(input.fail())throw std::runtime_error("cannot close input file: "+options.input);
        if(!result.eventsWritten)throw std::runtime_error("no supported CoBo event was selected");
        writer.Finish();
        SyncFile(indexPath);result.outputBytes=DirectoryBytes(staging);if(result.outputBytes>options.maximumOutputBytes)throw std::runtime_error("FITS output exceeds configured maximum byte limit");
        SyncDirectory(staging);PublishNoReplace(staging,options.outputDirectory);published=true;SyncDirectory(parent);return result;
    } catch(...) {if(!published)RemoveOwnedDirectory(staging);throw;}
}
}
