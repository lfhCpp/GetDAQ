#include "catalogtransfer.h"
#include "catalogtransfer_internal.h"
#include "receivercatalog.h"
#include "supervisorio.h"
#include "supervisorcommon.h"
#include "controlprotocol.h"
#include "systemexperimentconfiguration.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
#include <arpa/inet.h>

namespace GetDAQ {
using namespace CatalogTransferInternal;

std::string SendReceiverCatalogStream(const std::string &source,int outputFd,int inputFd,CatalogTransferProgress progress,bool resume,const std::string &transport) {
    using namespace SupervisorIO;Path(source);Input input(source);const auto key=String(input.document.At("request_key"));BlockPipeSignal signalGuard;
    Require(transport=="openssh-stdio-v1" || transport=="tcp-plain-v1","Unsupported catalog stream transport");
    const auto checkpoint=StreamSourceCheckpoint(input,transport);
    ReadReply(outputFd,"GDAQRDY1");WriteTag(inputFd,resume ? "GDAQRES2" : "GDAQNEW2");Write32(inputFd,std::uint32_t(input.request.size()));Write32(inputFd,std::uint32_t(input.manifest.size()));Write32(inputFd,std::uint32_t(input.receipt.size()));Write32(inputFd,std::uint32_t(checkpoint.size()));
    ExactWrite(inputFd,input.request.data(),input.request.size());ExactWrite(inputFd,input.manifest.data(),input.manifest.size());ExactWrite(inputFd,input.receipt.data(),input.receipt.size());ExactWrite(inputFd,checkpoint.data(),checkpoint.size());
    char startReply[8];ExactReadWithProgress(outputFd,startReply,sizeof(startReply),progress,"remote-verifying",0);
    if(std::equal(startReply,startReply+8,"GDAQFAIL")) {std::uint32_t encoded=0;ExactReadWithProgress(outputFd,&encoded,sizeof(encoded),progress,"remote-verifying",0);const auto n=ntohl(encoded);Require(n<=1024,"Remote transfer error exceeds limit");std::string message(n,'\0');if(n)ExactReadWithProgress(outputFd,&message[0],n,progress,"remote-verifying",0);throw std::runtime_error("Remote archive transfer rejected: "+message);}
    if(std::equal(startReply,startReply+8,"GDAQCOMP")) {
        Require(resume,"Remote completed an unrequested transfer");std::uint32_t encoded=0;ExactReadWithProgress(outputFd,&encoded,sizeof(encoded),progress,"remote-verifying",0);const auto size=ntohl(encoded);Require(size<=65536,"Remote archive receipt exceeds limit");std::string receipt(size,'\0');if(size)ExactReadWithProgress(outputFd,&receipt[0],size,progress,"remote-verifying",0);
        const auto result=Json(receipt);Require(Boolean(result.At("complete")) && String(result.At("source_request_key"))==key && Unsigned(result.At("copied_files"),true)==input.files.size() &&
            Unsigned(result.At("copied_bytes"),true)==input.bytes && Boolean(result.At("content_verified")) && Boolean(result.At("source_preserved")),"Existing remote completion differs from current source");
        input.Check();if(progress)progress("complete",0);return receipt;
    }
    Require(std::equal(startReply,startReply+8,"GDAQACPT"),"Archive stream start reply tag differs");
    std::uint64_t sentBytes=0,nextProgress=16*1024*1024;auto lastProgress=std::chrono::steady_clock::now();if(progress)progress("remote-accepted",0);
    for(std::size_t i=0;i<input.directories.size();++i) {
        const auto seal=ReadFile(source+"/source_"+std::to_string(i)+"/receiver-seal.json",16*1024*1024);const auto rows=input.document.At("sources").items;
        Require(ConfigurationSha256(seal)==String(rows[i].At("seal_sha256")),"Source receiver seal changed before network transfer");Write32(inputFd,std::uint32_t(seal.size()));ExactWrite(inputFd,seal.data(),seal.size());
    }
    struct ResumePrefix {std::uint64_t bytes;std::string digest;};std::vector<ResumePrefix> prefixes;prefixes.reserve(input.files.size());
    for(std::size_t index=0;index<input.files.size();++index) {
        const auto &file=input.files[index];const auto sourceIndex=Read32(outputFd),nameSize=Read32(outputFd);const auto fileSize=Read64(outputFd),offset=Read64(outputFd);
        const auto expectedName="source_"+std::to_string(file.source)+'/'+file.name;Require(sourceIndex==file.source && nameSize==expectedName.size() && fileSize==std::uint64_t(file.info.st_size) && offset<=fileSize,"Remote resume prefix identity differs from source manifest");
        std::string name(nameSize,'\0');if(nameSize)ExactRead(outputFd,&name[0],nameSize);Require(name==expectedName,"Remote resume prefix path differs from source manifest");
        std::string remoteDigest(64,'\0');ExactRead(outputFd,&remoteDigest[0],remoteDigest.size());Digest(remoteDigest);
        auto &directory=*input.directories[file.source];directory.Check();Fd fd(::openat(directory.Descriptor(),file.name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));struct stat held;
        Require(fd.value>=0 && !::fstat(fd.value,&held) && Unchanged(file.info,held),"SSH resume source changed before prefix check");Sha256Stream prefixHash;ReadPrefix(fd.value,offset,prefixHash);Sha256Stream digestHash=prefixHash;
        Require(digestHash.Finish()==remoteDigest && !::fstat(fd.value,&held) && Unchanged(file.info,held) && Unchanged(file.info,Regular(directory.Descriptor(),file.name)),"Remote partial prefix differs from bound original source");
        prefixes.push_back({offset,remoteDigest});
    }
    input.Check();WriteTag(inputFd,"GDAQPRF1");
    std::array<unsigned char,65536> buffer;
    for(std::size_t index=0;index<input.files.size();++index) {
        const auto &file=input.files[index];
        input.Check();auto &directory=*input.directories[file.source];Fd fd(::openat(directory.Descriptor(),file.name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));struct stat held;
        Require(fd.value>=0 && !::fstat(fd.value,&held) && Unchanged(file.info,held),"Archive source changed before network transfer");
        const auto name="source_"+std::to_string(file.source)+'/'+file.name;Write32(inputFd,std::uint32_t(file.source));Write32(inputFd,std::uint32_t(name.size()));Write64(inputFd,std::uint64_t(file.info.st_size));Write64(inputFd,prefixes[index].bytes);ExactWrite(inputFd,name.data(),name.size());
        Sha256Stream hash;ReadPrefix(fd.value,prefixes[index].bytes,hash);Sha256Stream prefixHash=hash;Require(prefixHash.Finish()==prefixes[index].digest,"Remote partial prefix changed after verification");
        std::uint64_t count=prefixes[index].bytes;for(;;) {
            const auto n=::read(fd.value,buffer.data(),buffer.size());if(n<0 && errno==EINTR)continue;Require(n>=0,"Archive source read failed");if(!n)break;
            Require(count<=std::uint64_t(file.info.st_size) && std::uint64_t(n)<=std::uint64_t(file.info.st_size)-count && sentBytes<=UINT64_MAX-std::uint64_t(n),"Archive source grew or total byte count overflowed while sending");hash.Update(buffer.data(),std::size_t(n));ExactWrite(inputFd,buffer.data(),std::size_t(n));count+=std::uint64_t(n);sentBytes+=std::uint64_t(n);
            const auto now=std::chrono::steady_clock::now();if(progress && (sentBytes>=nextProgress || now-lastProgress>=std::chrono::seconds(1))) {progress("sending",sentBytes);nextProgress=sentBytes+16*1024*1024;lastProgress=now;}
        }
        Require(count==std::uint64_t(file.info.st_size) && !::fstat(fd.value,&held) && Unchanged(file.info,held) && Unchanged(file.info,Regular(directory.Descriptor(),file.name)),"Archive source changed during network send");
        const auto digest=hash.Finish();ExactWrite(inputFd,digest.data(),digest.size());if(progress)progress("file-sent",sentBytes);
    }
    input.Check();WriteTag(inputFd,"GDAQEND1");
    if(progress)ReadReplyWithProgress(outputFd,"GDAQCOMP",progress,sentBytes);
    else ReadReply(outputFd,"GDAQCOMP");
    const auto receiptSize=Read32(outputFd);Require(receiptSize<=65536,"Remote archive receipt exceeds limit");
    std::string receipt(receiptSize,'\0');if(receiptSize)ExactRead(outputFd,&receipt[0],receiptSize);const auto result=Json(receipt);
    Require(Boolean(result.At("complete")) && String(result.At("source_request_key"))==key && Unsigned(result.At("copied_files"),true)==input.files.size() &&
        Unsigned(result.At("copied_bytes"),true)==input.bytes && Boolean(result.At("content_verified")) && Boolean(result.At("source_preserved")),"Remote archive completion scope differs");if(progress)progress("complete",sentBytes);return receipt;
}

}
