#include "catalogtransfer_internal.h"
#include "controlprotocol.h"
#include "configurationsha256.h"
#include "planio.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <poll.h>
#include <set>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace GetDAQ {
namespace CatalogTransferInternal {
using namespace SupervisorIO;

void ExactRead(int fd,void *buffer,std::size_t bytes) {
    auto *out=static_cast<unsigned char *>(buffer);std::size_t done=0;
    while(done<bytes) {const auto n=::read(fd,out+done,bytes-done);if(n<0 && errno==EINTR)continue;Require(n>0,"Archive stream ended before its declared length");done+=std::size_t(n);}
}
void ExactWrite(int fd,const void *buffer,std::size_t bytes) {
    const auto *input=static_cast<const unsigned char *>(buffer);std::size_t done=0;
    while(done<bytes) {const auto n=::write(fd,input+done,bytes-done);if(n<0 && errno==EINTR)continue;Require(n>0,"Archive stream write failed");done+=std::size_t(n);}
}
std::uint32_t Read32(int fd) {std::uint32_t value;ExactRead(fd,&value,sizeof(value));return ntohl(value);}
std::uint64_t Read64(int fd) {unsigned char b[8];ExactRead(fd,b,sizeof(b));std::uint64_t v=0;for(auto c:b)v=(v<<8)|c;return v;}
void Write32(int fd,std::uint32_t value) {value=htonl(value);ExactWrite(fd,&value,sizeof(value));}
void Write64(int fd,std::uint64_t value) {unsigned char b[8];for(int i=7;i>=0;--i){b[i]=static_cast<unsigned char>(value&255);value>>=8;}ExactWrite(fd,b,sizeof(b));}
void WriteTag(int fd,const char *tag) {ExactWrite(fd,tag,8);}
void ReadTag(int fd,const char *expected) {char tag[8];ExactRead(fd,tag,sizeof(tag));Require(std::equal(tag,tag+8,expected),"Archive stream protocol tag differs");}
void WriteFailure(int fd,const std::string &message) {
    try {const auto size=std::min<std::size_t>(message.size(),1024);WriteTag(fd,"GDAQFAIL");Write32(fd,std::uint32_t(size));if(size)ExactWrite(fd,message.data(),size);} catch(...) {}
}
std::string ReadReply(int fd,const char *expected) {
    char tag[8];ExactRead(fd,tag,sizeof(tag));
    if(std::equal(tag,tag+8,"GDAQFAIL")) {const auto n=Read32(fd);Require(n<=1024,"Remote transfer error exceeds limit");std::string message(n,'\0');if(n)ExactRead(fd,&message[0],n);throw std::runtime_error("Remote archive transfer rejected: "+message);}
    Require(std::equal(tag,tag+8,expected),"Archive stream reply tag differs");return std::string();
}
void ExactReadWithProgress(int fd,void *buffer,std::size_t bytes,const CatalogTransferProgress &progress,const std::string &phase,std::uint64_t sentBytes) {
    auto *out=static_cast<unsigned char *>(buffer);std::size_t done=0;
    while(done<bytes) {
        pollfd descriptor={fd,POLLIN,0};const auto ready=::poll(&descriptor,1,1000);
        if(ready<0 && errno==EINTR)continue;
        Require(ready>=0,"Archive response poll failed");
        if(!ready){if(progress)progress(phase,sentBytes);continue;}
        const auto count=::read(fd,out+done,bytes-done);
        if(count<0 && errno==EINTR)continue;
        Require(count>0,"Archive response ended before its declared length");done+=std::size_t(count);
    }
}
void ReadReplyWithProgress(int fd,const char *expected,const CatalogTransferProgress &progress,std::uint64_t sentBytes) {
    char tag[8];ExactReadWithProgress(fd,tag,sizeof(tag),progress,"remote-verifying",sentBytes);
    if(std::equal(tag,tag+8,"GDAQFAIL")) {
        std::uint32_t encoded=0;ExactReadWithProgress(fd,&encoded,sizeof(encoded),progress,"remote-verifying",sentBytes);
        const auto size=ntohl(encoded);Require(size<=1024,"Remote transfer error exceeds limit");std::string message(size,'\0');
        if(size)ExactReadWithProgress(fd,&message[0],size,progress,"remote-verifying",sentBytes);
        throw std::runtime_error("Remote archive transfer rejected: "+message);
    }
    Require(std::equal(tag,tag+8,expected),"Archive stream reply tag differs");
}
StreamSource DecodeStreamSource(const std::string &request,const std::string &manifest,const std::string &receipt) {
    Require(request.size()<=1024*1024 && manifest.size()<=16*1024*1024 && receipt.size()<=65536,"Archive source metadata exceeds limit");
    StreamSource result;result.request=request;result.manifest=manifest;result.receipt=receipt;result.options=DecodeReceiverCatalogRequest(request);result.key=ReceiverCatalogRequestKey(result.options);
    const auto m=Json(manifest),r=Json(receipt);
    ExactSupervisorFields(m,{"schema","kind","request_key","plan_sha256","session_id","payload_method","raw_payload_bytes_read","sources"});
    ExactSupervisorFields(r,{"schema","kind","complete","request_key","manifest_sha256","operation_id","directory","payload_method","raw_payload_bytes_read","linked_files"});
    Require(Unsigned(m.At("schema"))==1 && String(m.At("kind"))=="getdaq.receiver-catalog-manifest" && String(m.At("request_key"))==result.key &&
        String(m.At("plan_sha256"))==result.options.stopped.planSha256 && String(m.At("session_id"))==result.options.stopped.sessionId &&
        String(m.At("payload_method"))=="hardlink" && Unsigned(m.At("raw_payload_bytes_read"),true)==0 && m.At("sources").type==JsonValue::Array &&
        m.At("sources").items.size()==result.options.stopped.receivers.size() && Unsigned(r.At("schema"))==1 &&
        String(r.At("kind"))=="getdaq.receiver-catalog-complete" && Boolean(r.At("complete")) && String(r.At("request_key"))==result.key &&
        String(r.At("manifest_sha256"))==ConfigurationSha256(manifest) && String(r.At("operation_id"))==result.options.operationId &&
        String(r.At("payload_method"))=="hardlink" && Unsigned(r.At("raw_payload_bytes_read"),true)==0,"Archive request/manifest/completion binding differs");
    std::set<std::string> ids;std::uint64_t count=0;
    for(std::size_t i=0;i<m.At("sources").items.size();++i) {
        const auto &row=m.At("sources").items[i];const auto &expected=result.options.stopped.receivers[i];
        ExactSupervisorFields(row,{"id","stored_directory","original_directory","seal_sha256","frames","files"});
        Require(String(row.At("id"))==expected.id && String(row.At("stored_directory"))=="source_"+std::to_string(i) &&
            String(row.At("original_directory"))==expected.outputDirectory && Unsigned(row.At("frames"),true)==expected.frames &&
            row.At("files").type==JsonValue::Array && row.At("files").items.size()==expected.files && ids.insert(expected.id).second,"Archive source mapping differs");
        const auto sealHash=String(row.At("seal_sha256"));Digest(sealHash);
        for(const auto &file:row.At("files").items) {
            ExactSupervisorFields(file,{"name","bytes","device","inode"});const auto name=String(file.At("name"));RawName(name);
            const auto bytes=Unsigned(file.At("bytes"),true);Require(++count<=65536 && result.bytes<=UINT64_MAX/2-bytes,"Archive source byte count overflows");
            result.bytes+=bytes;result.files.push_back({i,name,bytes});
        }
    }
    Require(Unsigned(r.At("linked_files"),true)==result.files.size(),"Archive linked-file count differs");return result;
}

} // namespace CatalogTransferInternal
} // namespace GetDAQ
