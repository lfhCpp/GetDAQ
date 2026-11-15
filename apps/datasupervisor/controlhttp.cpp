#include "controlhttp.h"
#include "supervisorcommon.h"
#include "supervisorio.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <map>
#include <poll.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
namespace GetDAQ {
namespace {
using Clock=std::chrono::steady_clock;
void Wait(int fd,short events,const Clock::time_point &deadline) {
    for (;;) {
        const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-Clock::now()).count();
        if (remaining<=0) throw std::runtime_error("Supervisor HTTP deadline expired; execution outcome is not inferred");
        pollfd p={fd,events,0}; const int result=::poll(&p,1,int(remaining));
        if (result<0 && errno==EINTR) continue;
        if (result<0 || (p.revents&POLLNVAL)) throw std::runtime_error("Supervisor HTTP socket poll failed");
        if (result>0) return;
    }
}
std::string Lower(std::string value) {
    for (auto &c:value) if (c>='A' && c<='Z') c=char(c-'A'+'a');
    return value;
}
std::string Trim(const std::string &value) {
    const auto first=value.find_first_not_of(" \t"),last=value.find_last_not_of(" \t");
    return first==std::string::npos ? "" : value.substr(first,last-first+1);
}
std::size_t Length(const std::string &value,std::size_t maximum) {
    if (value.empty() || value.size()>10 || value.find_first_not_of("0123456789")!=std::string::npos || (value.size()>1 && value[0]=='0'))
        throw std::runtime_error("Invalid supervisor HTTP Content-Length");
    const auto count=std::stoul(value);
    if (count>maximum) throw std::runtime_error(maximum==1024*1024 ? "Supervisor HTTP response exceeds 1 MiB" : "Supervisor HTTP response exceeds its bounded metadata budget");
    return count;
}
std::size_t DecodeHeaders(const std::string &headers,unsigned int &code,std::size_t maximum) {
    const auto first=headers.find("\r\n");
    if (first==std::string::npos || first<12 || (headers.compare(0,9,"HTTP/1.1 ") && headers.compare(0,9,"HTTP/1.0 ")) ||
        headers[9]<'1' || headers[9]>'5' || headers[10]<'0' || headers[10]>'9' || headers[11]<'0' || headers[11]>'9' || (first>12 && headers[12]!=' '))
        throw std::runtime_error("Invalid supervisor HTTP response line");
    for (std::size_t i=12;i<first;++i) if (static_cast<unsigned char>(headers[i])<32 || static_cast<unsigned char>(headers[i])>126)
        throw std::runtime_error("Invalid supervisor HTTP response reason");
    code=unsigned(headers[9]-'0')*100+unsigned(headers[10]-'0')*10+unsigned(headers[11]-'0');
    std::map<std::string,std::string> fields; std::size_t start=first+2;
    while (start<headers.size()) {
        const auto end=headers.find("\r\n",start);
        if (end==std::string::npos || end==start) throw std::runtime_error("Invalid supervisor HTTP header framing");
        const auto row=headers.substr(start,end-start); const auto colon=row.find(':');
        if (!colon || colon==std::string::npos) throw std::runtime_error("Invalid supervisor HTTP header");
        const auto key=Lower(row.substr(0,colon));
        if (key.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789!#$%&'*+-.^_`|~")!=std::string::npos || fields.count(key))
            throw std::runtime_error("Invalid/duplicate supervisor HTTP header");
        const auto value=Trim(row.substr(colon+1));
        for (unsigned char c:value) if ((c<32 && c!='\t') || c==127) throw std::runtime_error("Invalid supervisor HTTP header value");
        fields.emplace(key,value); start=end+2;
    }
    if (fields.count("transfer-encoding") || !fields.count("content-length")) throw std::runtime_error("Supervisor HTTP requires a fixed Content-Length");
    return Length(fields.at("content-length"),maximum);
}
}
NativeControlHttpClient::NativeControlHttpClient(const SupervisorHttpOptions &o) : fOptions(o),fCredential(o.credentialFile) {
    std::uint32_t hostOrderAddress=0;
    if (!SupervisorIO::ParseCanonicalIPv4(o.address,hostOrderAddress) || !hostOrderAddress || !o.port || o.port>65535 || o.timeoutMs<100 || o.timeoutMs>60000)
        throw std::invalid_argument("Supervisor HTTP requires explicit canonical IPv4/port and 100..60000ms deadline");
}
SupervisorHttpResponse NativeControlHttpClient::Identity() const { return Request("GET","/v1/identity",""); }
SupervisorHttpResponse NativeControlHttpClient::Status() const { return Request("GET","/v1/status",""); }
SupervisorHttpResponse NativeControlHttpClient::ControlHandoff() const { return Request("GET","/v1/control-handoff",""); }
SupervisorHttpResponse NativeControlHttpClient::PlanRetirement() const { return Request("GET","/v1/plan-retirement",""); }
SupervisorHttpResponse NativeControlHttpClient::PlanRetirementCompletion() const { return Request("GET","/v1/plan-retirement-completion",""); }
SupervisorHttpResponse NativeControlHttpClient::ControlHandoffCompletion() const { return Request("GET","/v1/control-handoff-completion",""); }
SupervisorHttpResponse NativeControlHttpClient::Predecessor() const { return Request("GET","/v1/predecessor",""); }
SupervisorHttpResponse NativeControlHttpClient::Receivers() const { return Request("GET","/v1/receivers",""); }
SupervisorHttpResponse NativeControlHttpClient::ReceiverEvents(std::size_t sourceIndex,bool fileBytes) const { return Request("GET",std::string(fileBytes ? "/v1/receiver-file-bytes/" : "/v1/receiver-events/")+std::to_string(sourceIndex),""); }
SupervisorHttpResponse NativeControlHttpClient::Sources() const {return Request("GET","/v1/sources","");}
SupervisorHttpResponse NativeControlHttpClient::HardwareState() const {return Request("GET","/v1/hardware-state","");}
SupervisorHttpResponse NativeControlHttpClient::ReceiverFailure() const {return Request("GET","/v1/receiver-failure","");}
SupervisorHttpResponse NativeControlHttpClient::Ownership() const {return Request("GET","/v1/ownership","");}
SupervisorHttpResponse NativeControlHttpClient::ConfigureReadbacks() const {return Request("GET","/v1/configure-readbacks","");}
SupervisorHttpResponse NativeControlHttpClient::CoBoMaintenanceTargets() const {return Request("GET","/v1/cobo-maintenance-targets","");}
SupervisorHttpResponse NativeControlHttpClient::Alarms() const { return Request("GET","/v1/alarms",""); }
SupervisorHttpResponse NativeControlHttpClient::Operation(const std::string &id) const { SupervisorOperationId(id); return Request("GET","/v1/operations/"+id,""); }
SupervisorHttpResponse NativeControlHttpClient::CatalogReplacementObservation(const std::string &id) const { SupervisorOperationId(id); return Request("GET","/v1/catalog-replacement-observations/"+id,""); }
SupervisorHttpResponse NativeControlHttpClient::PostOperation(const std::string &json) const { return Request("POST","/v1/operations",json); }
SupervisorHttpResponse NativeControlHttpClient::ShutdownEccService(const std::string &instance,const std::string &snapshot,const std::string &handoff) const {
    SupervisorIO::Digest(instance);SupervisorIO::Digest(snapshot);SupervisorOperationId(handoff);
    const auto body="{\"schema\":1,\"kind\":\"getdaq.ecc-service-shutdown\",\"instance_id\":\""+instance+
        "\",\"snapshot_sha256\":\""+snapshot+"\",\"handoff_operation_id\":\""+handoff+"\"}\n";
    return Request("POST","/v1/shutdown",body);
}
SupervisorHttpResponse SupervisorHttpClient::Submit(const SupervisorControlRequest &r) const { return PostOperation(EncodeSupervisorControlRequest(r)); }
SupervisorHttpResponse NativeControlHttpClient::Request(const std::string &method,const std::string &path,const std::string &body) const try {
    using namespace SupervisorIO;
    const std::size_t maximum=method=="POST" && path=="/v1/operations" ? 2U*1024U*1024U : 8192U;
    if (body.size()>maximum) throw std::invalid_argument("Supervisor HTTP request exceeds its command endpoint bound");
    const auto token=fCredential.Token(); // 不写日志、不放到异常字符串。
    const auto deadline=Clock::now()+std::chrono::milliseconds(fOptions.timeoutMs);
    Fd fd(::socket(AF_INET,SOCK_STREAM,0));
    if (fd.value<0) throw std::runtime_error("Cannot create supervisor HTTP socket");
    const int statusFlags=::fcntl(fd.value,F_GETFL),descriptorFlags=::fcntl(fd.value,F_GETFD);
    if (statusFlags<0 || descriptorFlags<0 || ::fcntl(fd.value,F_SETFL,statusFlags|O_NONBLOCK) || ::fcntl(fd.value,F_SETFD,descriptorFlags|FD_CLOEXEC))
        throw std::runtime_error("Cannot configure supervisor HTTP socket");
#ifdef SO_NOSIGPIPE
    const int enabled=1;
    if (::setsockopt(fd.value,SOL_SOCKET,SO_NOSIGPIPE,&enabled,sizeof(enabled))) throw std::runtime_error("Cannot disable supervisor HTTP SIGPIPE");
#endif
    sockaddr_in address={}; address.sin_family=AF_INET; address.sin_port=htons(fOptions.port); ::inet_pton(AF_INET,fOptions.address.c_str(),&address.sin_addr);
    if (::connect(fd.value,reinterpret_cast<sockaddr *>(&address),sizeof(address))) {
        if (errno!=EINPROGRESS) throw std::runtime_error("Cannot connect to supervisor HTTP endpoint");
        Wait(fd.value,POLLOUT,deadline); int error=0; socklen_t size=sizeof(error);
        if (::getsockopt(fd.value,SOL_SOCKET,SO_ERROR,&error,&size) || error) throw std::runtime_error("Supervisor HTTP connect failed");
    }
    const auto request=method+' '+path+" HTTP/1.1\r\nHost: "+fOptions.address+':'+std::to_string(fOptions.port)+"\r\nAuthorization: Bearer "+token+
        "\r\nContent-Type: application/json\r\nContent-Length: "+std::to_string(body.size())+"\r\nConnection: close\r\n\r\n"+body;
    std::size_t sent=0;
    while (sent<request.size()) {
        Wait(fd.value,POLLOUT,deadline);
        int flags=0;
#ifdef MSG_NOSIGNAL
        flags=MSG_NOSIGNAL;
#endif
        const auto n=::send(fd.value,request.data()+sent,request.size()-sent,flags);
        if (n<0 && (errno==EINTR || errno==EAGAIN || errno==EWOULDBLOCK)) continue;
        if (n<=0) throw std::runtime_error("Supervisor HTTP send failed; operation may already be accepted");
        sent+=std::size_t(n);
    }
    SupervisorHttpResponse response; std::string received; std::size_t headerSize=0,bodySize=0;
    for (;;) {
        Wait(fd.value,POLLIN,deadline); char buffer[8192]; const auto n=::recv(fd.value,buffer,sizeof(buffer),0);
        if (n<0 && (errno==EINTR || errno==EAGAIN || errno==EWOULDBLOCK)) continue;
        if (n<=0) throw std::runtime_error("Incomplete supervisor HTTP response; operation outcome is unknown");
        received.append(buffer,std::size_t(n));
        if (!headerSize) {
            const auto end=received.find("\r\n\r\n");
            if ((end==std::string::npos && received.size()>16384) || (end!=std::string::npos && end>16384)) throw std::runtime_error("Supervisor HTTP headers exceed 16 KiB");
            if (end==std::string::npos) continue;
            const auto maximum=(path=="/v1/plan-retirement" || path=="/v1/predecessor") ? 5*1024*1024U : 1024*1024U;
            bodySize=DecodeHeaders(received.substr(0,end+2),response.code,maximum); headerSize=end+4;
        }
        if (received.size()>headerSize+bodySize) throw std::runtime_error("Supervisor HTTP response has extra bytes");
        if (received.size()==headerSize+bodySize) { response.body=received.substr(headerSize); return response; }
    }
} catch (const std::runtime_error &error) {
    // Fixed method/path only: never include headers, credentials or request body.
    throw std::runtime_error(std::string(error.what())+" ["+method+' '+path+"]");
}
}
