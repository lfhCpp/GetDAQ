#include "remotehandoffevidence_p.h"
#include <algorithm>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

namespace GetDAQ {
namespace RemoteEvidenceInternal {

void Require(bool value,const char *message) {
    if(!value)throw std::runtime_error(message);
}

std::string Parent(const std::string &path) {
    return path.substr(0,path.find_last_of('/'));
}

std::vector<std::string> Names(const SupervisorIO::Directory &directory) {
    directory.Check();const int fd=::openat(directory.Descriptor(),".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    Require(fd>=0,"Cannot inspect handoff evidence directory");DIR *stream=::fdopendir(fd);
    if(!stream){::close(fd);throw std::runtime_error("Cannot enumerate handoff evidence");}
    std::vector<std::string> names;
    try {
        errno=0;
        while(const auto entry=::readdir(stream)) {
            const std::string name=entry->d_name;if(name=="." || name=="..")continue;
            Require(names.size()<8192,"Handoff evidence directory exceeds entry budget");names.push_back(name);
        }
        Require(!errno,"Cannot read complete handoff evidence directory");
    }catch(...){::closedir(stream);throw;}
    ::closedir(stream);directory.Check();std::sort(names.begin(),names.end());return names;
}

std::vector<JsonValue> Journal(const std::string &bytes) {
    Require(bytes.empty() || bytes.back()=='\n',"Incomplete handoff evidence journal");
    std::vector<JsonValue> rows;std::size_t position=0;
    while(position<bytes.size()) {
        while(position<bytes.size() && (bytes[position]==' ' || bytes[position]=='\n' || bytes[position]=='\t' || bytes[position]=='\r'))++position;
        if(position==bytes.size())break;
        Require(bytes[position]=='{' && rows.size()<8192,"Invalid/budgeted handoff journal record");
        const auto begin=position;int depth=0;bool quoted=false,escape=false;
        do {
            const char c=bytes[position++];
            if(quoted) {if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quoted=false;}
            else if(c=='"')quoted=true;
            else if(c=='{' || c=='[')++depth;
            else if(c=='}' || c==']')--depth;
            Require(depth>=0 && depth<=64,"Invalid handoff journal nesting");
        }while(position<bytes.size() && (depth || quoted));
        Require(!depth && !quoted && position<bytes.size() && bytes[position]=='\n',"Incomplete/non-delimited handoff journal record");
        JsonLimits limits;limits.stringBytes=65536;rows.push_back(ParseStrictJson(bytes.substr(begin,position-begin),limits));
    }
    return rows;
}

EvidenceFile::EvidenceFile(const std::string &file)
    : path(file),fd(::open(file.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC)),bytes(ReadFile(file,1024*1024)) {
    Require(fd.value>=0 && !::fstat(fd.value,&identity),"Cannot bind handoff evidence file");Check();
}

void EvidenceFile::Check() const {
    struct stat current,opened;
    Require(!::lstat(path.c_str(),&current) && !::fstat(fd.value,&opened) && Same(current,identity) && Same(opened,identity) &&
        S_ISREG(current.st_mode) && current.st_uid==::geteuid() && (current.st_mode&0777)==0600 && current.st_nlink==1 &&
        ReadFile(path,1024*1024)==bytes,("Handoff evidence file identity/content changed: "+path).c_str());
}

} // namespace RemoteEvidenceInternal
} // namespace GetDAQ
