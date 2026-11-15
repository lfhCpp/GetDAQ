#ifndef GETDAQ_SUPERVISORIO_H
#define GETDAQ_SUPERVISORIO_H
#include "datasupervisor.h"
#include "supervisorcommon.h"
#include <sys/stat.h>
namespace GetDAQ { namespace SupervisorIO {
class Fd {
public:
    int value;
    explicit Fd(int fd=-1) : value(fd) {}
    ~Fd();
    Fd(const Fd &)=delete;
    Fd &operator=(const Fd &)=delete;
};
class Directory {
    Fd fd;
    struct stat identity;
    std::string path;
public:
    explicit Directory(const std::string &path);
    void Check() const;
    int Descriptor() const { return fd.value; }
    const std::string &Path() const { return path; }
};
void CreateDirectory(const std::string &path);
void WriteNew(const std::string &path,const std::string &bytes);
std::string ReadFile(const std::string &path,std::size_t maximum);
std::string RandomSession();
std::string HttpStatus(unsigned int port,unsigned int timeoutMs);
bool Same(const struct stat &a,const struct stat &b);
void ValidateOptions(const DataSupervisorOptions &options,bool requireLocalHost=true);
} }
#endif
