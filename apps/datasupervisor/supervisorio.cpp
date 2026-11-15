#include "supervisorio.h"
#include "systemexperimentconfiguration.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits>
#include <poll.h>
#include <set>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
namespace GetDAQ { namespace SupervisorIO {
Fd::~Fd() { if (value>=0) ::close(value); }
bool Same(const struct stat &a,const struct stat &b) { return a.st_dev==b.st_dev && a.st_ino==b.st_ino; }
Directory::Directory(const std::string &p) : fd(::open(p.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC)),path(p) {
    if (fd.value<0 || ::fstat(fd.value,&identity)) throw std::runtime_error("Cannot open supervisor directory");
    Check();
}
void Directory::Check() const {
    struct stat current,held;
    if (::lstat(path.c_str(),&current) || ::fstat(fd.value,&held) || !S_ISDIR(current.st_mode) || !Same(identity,current) || !Same(identity,held) ||
        current.st_uid!=::geteuid() || (current.st_mode&0777)!=0700) throw std::runtime_error("Supervisor directory identity/owner/permissions changed");
}
void SyncParent(const std::string &path) {
    Fd parent(::open(path.substr(0,path.find_last_of('/')).c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));
    if (parent.value<0 || ::fsync(parent.value)) throw std::runtime_error("Cannot sync supervisor parent directory");
}
void CreateDirectory(const std::string &path) {
    Path(path);
    if (::mkdir(path.c_str(),0700)) throw std::runtime_error("Cannot exclusively create supervisor directory");
    SyncParent(path);
}
void WriteNew(const std::string &path,const std::string &bytes) {
    Fd fd(::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));
    if (fd.value<0) throw std::runtime_error("Cannot exclusively create supervisor file");
    std::size_t offset=0;
    while (offset<bytes.size()) {
        const auto n=::write(fd.value,bytes.data()+offset,bytes.size()-offset);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) throw std::runtime_error("Cannot write supervisor file");
        offset+=std::size_t(n);
    }
    if (::fsync(fd.value)) throw std::runtime_error("Cannot fsync supervisor file");
    SyncParent(path);
}
std::string ReadFile(const std::string &path,std::size_t maximum) {
    Fd fd(::open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC)); struct stat st;
    if (fd.value<0 || ::fstat(fd.value,&st) || !S_ISREG(st.st_mode) || st.st_size<0 || std::uint64_t(st.st_size)>maximum)
        throw std::runtime_error("Invalid bounded regular supervisor file");
    std::string bytes(std::size_t(st.st_size),'\0'); std::size_t offset=0;
    while (offset<bytes.size()) {
        const auto n=::read(fd.value,&bytes[offset],bytes.size()-offset);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) throw std::runtime_error("Cannot read complete supervisor file");
        offset+=std::size_t(n);
    }
    struct stat end;
    if (::fstat(fd.value,&end) || !Same(st,end) || end.st_size!=st.st_size) throw std::runtime_error("Supervisor file changed while reading");
    return bytes;
}
std::string RandomSession() {
    Fd fd(::open("/dev/urandom",O_RDONLY|O_CLOEXEC)); unsigned char bytes[32]; std::size_t count=0;
    if (fd.value<0) throw std::runtime_error("Cannot open system random source");
    while (count<sizeof(bytes)) {
        const auto n=::read(fd.value,bytes+count,sizeof(bytes)-count);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) throw std::runtime_error("Cannot obtain receiver session identity");
        count+=std::size_t(n);
    }
    const char hex[]="0123456789abcdef"; std::string result;
    for (unsigned char b:bytes) { result+=hex[b>>4]; result+=hex[b&15]; }
    return result;
}
std::string HttpStatus(unsigned int port,unsigned int timeout) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout);
    Fd fd(::socket(AF_INET,SOCK_STREAM,0));
    if (fd.value<0 || ::fcntl(fd.value,F_SETFD,FD_CLOEXEC) || ::fcntl(fd.value,F_SETFL,O_NONBLOCK)) throw std::runtime_error("Cannot open supervisor status socket");
#ifdef SO_NOSIGPIPE
    const int noSignal=1;
    if (::setsockopt(fd.value,SOL_SOCKET,SO_NOSIGPIPE,&noSignal,sizeof(noSignal))) throw std::runtime_error("Cannot suppress status SIGPIPE");
#endif
    sockaddr_in address={}; address.sin_family=AF_INET; address.sin_port=htons(port); address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    const auto wait=[&](short events) {
        for (;;) {
            const auto left=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
            if (left<=0) throw std::runtime_error("Receiver status timeout");
            pollfd p={fd.value,events,0}; const auto n=::poll(&p,1,int(std::min<long long>(left,100)));
            if (n<0 && errno==EINTR) continue;
            if (n<0) throw std::runtime_error("Receiver status poll failed");
            if (n>0) return;
        }
    };
    if (::connect(fd.value,reinterpret_cast<sockaddr *>(&address),sizeof(address))) {
        if (errno!=EINPROGRESS) throw std::runtime_error("Receiver status connect failed");
        wait(POLLOUT); int error=0; socklen_t length=sizeof(error);
        if (::getsockopt(fd.value,SOL_SOCKET,SO_ERROR,&error,&length) || error) throw std::runtime_error("Receiver status connect failed");
    }
    const std::string request="GET /status HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"; std::size_t sent=0;
    while (sent<request.size()) {
#ifdef MSG_NOSIGNAL
        const int flags=MSG_NOSIGNAL;
#else
        const int flags=0;
#endif
        const auto n=::send(fd.value,request.data()+sent,request.size()-sent,flags);
        if (n>0) sent+=std::size_t(n);
        else if (n<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR)) wait(POLLOUT);
        else throw std::runtime_error("Receiver status send failed");
    }
    std::string response; char buffer[4096];
    for (;;) {
        const auto n=::recv(fd.value,buffer,sizeof(buffer),0);
        if (n>0) { response.append(buffer,std::size_t(n)); if (response.size()>65536) throw std::runtime_error("Excessive receiver status reply"); }
        else if (n==0) break;
        else if (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR) wait(POLLIN);
        else throw std::runtime_error("Receiver status receive failed");
    }
    const auto split=response.find("\r\n\r\n");
    if (response.compare(0,17,"HTTP/1.1 200 OK\r\n") || split==std::string::npos || split>8192) throw std::runtime_error("Invalid receiver status HTTP reply");
    const auto start=response.find("\r\nContent-Length: ");
    if (start==std::string::npos || start>=split || response.find("\r\nContent-Length: ",start+1)!=std::string::npos) throw std::runtime_error("Invalid receiver status length header");
    const auto begin=start+18, end=response.find("\r\n",begin);
    JsonValue length; length.type=JsonValue::Number; length.text=response.substr(begin,end-begin);
    if (Unsigned(length)!=response.size()-split-4) throw std::runtime_error("Receiver status body length differs");
    return response.substr(split+4);
}
void ValidateOptions(const DataSupervisorOptions &o,bool requireLocalHost) {
    ValidatePlanOptions(o);
    if (!requireLocalHost) return;
    std::set<std::string> local;
    if (requireLocalHost) {
    ifaddrs *interfaces=nullptr;
    if (::getifaddrs(&interfaces)) throw std::runtime_error("Cannot inspect local receiver interfaces");
    for (auto *i=interfaces;i;i=i->ifa_next) if (i->ifa_addr && i->ifa_addr->sa_family==AF_INET) {
        char text[INET_ADDRSTRLEN]; const auto *a=reinterpret_cast<sockaddr_in *>(i->ifa_addr);
        if (::inet_ntop(AF_INET,&a->sin_addr,text,sizeof(text))) local.insert(text);
    }
    ::freeifaddrs(interfaces);
    }
    
    for (const auto &s:o.sources) {
        const auto target=s.receiverAddress.substr(0,s.receiverAddress.find(':'));
        if (!local.count(target))
            throw std::invalid_argument("Supervisor requires explicit local IPv4 target/bind/port; remote host needs a remote supervisor adapter");
    }
}
} }
