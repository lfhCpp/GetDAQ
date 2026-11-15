#ifndef GETDAQ_CATALOGTRANSFER_INTERNAL_H
#define GETDAQ_CATALOGTRANSFER_INTERNAL_H

#include "catalogtransfer.h"
#include "receivercatalog.h"
#include "systemexperimentconfiguration.h"
#include "configurationsha256.h"
#include "supervisorio.h"
#include <map>
#include <memory>
#include <set>
#include <string>
#include <sys/stat.h>
#include <signal.h>
#include <arpa/inet.h>
#include <vector>

namespace GetDAQ {
namespace CatalogTransferInternal {

void Require(bool value,const char *message);
class BlockPipeSignal {
    sigset_t fSignalSet;
    sigset_t fPreviousMask;
    bool fWasPending;
public:
    BlockPipeSignal();
    ~BlockPipeSignal();
};
JsonValue Json(const std::string &bytes);
std::string Quote(std::uint64_t value);
void RawName(const std::string &name);
bool Unchanged(const struct stat &a,const struct stat &b);
struct stat Regular(int directory,const std::string &name);
void Sync(const SupervisorIO::Directory &directory);
std::unique_ptr<SupervisorIO::Fd> Lease(const SupervisorIO::Directory &directory,const std::string &name,bool create);
void CheckLease(const SupervisorIO::Directory &directory,const std::string &name,const SupervisorIO::Fd &fd);
std::set<std::string> Names(const SupervisorIO::Directory &directory,bool rawOnly=true);
void CheckTargetTree(const SupervisorIO::Directory &root,std::size_t sources);
void ReadPrefix(int fd,std::uint64_t bytes,Sha256Stream &hash);
void Metadata(const std::string &path,const std::string &bytes,bool resume,std::size_t maximum=32*1024*1024);
std::string Verify(const std::string &path,bool requireComplete,bool verifyPayload=true);
std::string Transfer(const std::string &source,const std::string &destination,
    CatalogTransferProgress progress,const std::string &expectedSourceRequestKey,bool resume);

struct StreamFile {std::size_t source;std::string name;std::uint64_t bytes;};
struct StreamSource {
    std::string request,manifest,receipt,key,checkpoint;
    ReceiverCatalogOptions options;
    std::vector<StreamFile> files;
    std::uint64_t bytes=0;
};

void ExactRead(int fd,void *buffer,std::size_t bytes);
void ExactWrite(int fd,const void *buffer,std::size_t bytes);
std::uint32_t Read32(int fd);
std::uint64_t Read64(int fd);
void Write32(int fd,std::uint32_t value);
void Write64(int fd,std::uint64_t value);
void WriteTag(int fd,const char *tag);
void ReadTag(int fd,const char *expected);
void WriteFailure(int fd,const std::string &message);
StreamSource DecodeStreamSource(const std::string &request,const std::string &manifest,const std::string &receipt);
std::string RemoteDestination(const std::string &root,const std::string &key);
void PublishDirectoryNoReplace(const std::string &source,const std::string &destination);
void ExactReadWithProgress(int fd,void *buffer,std::size_t bytes,const CatalogTransferProgress &progress,const std::string &phase,std::uint64_t sentBytes);
void ReadReplyWithProgress(int fd,const char *expected,const CatalogTransferProgress &progress,std::uint64_t sentBytes);
std::string ReadReply(int fd,const char *expected);
struct Payload {std::size_t source;std::string name;struct stat info;};
struct Input {
    SupervisorIO::Directory root;
    std::string request,manifest,receipt;
    JsonValue document;
    std::unique_ptr<SupervisorIO::Fd> lease;
    std::vector<std::unique_ptr<SupervisorIO::Directory>> directories,originals;
    std::vector<std::unique_ptr<SupervisorIO::Fd>> originalLeases;
    std::vector<Payload> files;
    std::uint64_t bytes=0;
    explicit Input(const std::string &path) : root(path) {
        using namespace SupervisorIO;
        receipt=InspectReceiverCatalog(path);manifest=ReadFile(path+"/manifest.json",16*1024*1024);request=ReadFile(path+"/request.json",1024*1024);document=Json(manifest);
        Require(ConfigurationSha256(manifest)==String(Json(receipt).At("manifest_sha256")),"Transfer source metadata changed");
        const auto decoded=DecodeReceiverCatalogRequest(request);
        lease=Lease(root,".getdaq-transfer.lock",true);
        for(const auto &receiver:decoded.stopped.receivers) {
            originals.emplace_back(new Directory(receiver.outputDirectory));originalLeases.push_back(Lease(*originals.back(),".getdaq-receiver.lock",false));
        }
        const auto &sources=document.At("sources").items;
        for(std::size_t i=0;i<sources.size();++i) {
            const auto stored=String(sources[i].At("stored_directory"));Require(stored=="source_"+std::to_string(i),"Transfer source directory mapping differs");directories.emplace_back(new Directory(path+'/'+stored));
            for(const auto &file:sources[i].At("files").items) {
                const auto name=String(file.At("name"));RawName(name);Payload payload;payload.source=i;payload.name=name;payload.info=Regular(directories.back()->Descriptor(),name);
                const auto size=Unsigned(file.At("bytes"),true);Require(size<=UINT64_MAX/2 && std::uint64_t(payload.info.st_size)==size && bytes<=UINT64_MAX/2-size,"Transfer input byte budget overflow");bytes+=size;files.push_back(payload);
            }
        }
        Check();
    }
    void Check() const {
        using namespace SupervisorIO;
        root.Check();CheckLease(root,".getdaq-transfer.lock",*lease);
        for(std::size_t i=0;i<originals.size();++i)CheckLease(*originals[i],".getdaq-receiver.lock",*originalLeases[i]);
        for(const auto &directory:directories)directory->Check();
        Require(ReadFile(root.Path()+"/request.json",1024*1024)==request && ReadFile(root.Path()+"/manifest.json",16*1024*1024)==manifest && InspectReceiverCatalog(root.Path())==receipt,"Transfer source metadata changed");
        for(const auto &file:files)Require(Unchanged(file.info,Regular(directories[file.source]->Descriptor(),file.name)),"Transfer source changed");
    }
};
std::string SourceCheckpoint(const Input &input,const SupervisorIO::Directory &target);
std::map<std::string,struct stat> CheckPrefixes(const Input &input,
    const std::vector<std::unique_ptr<SupervisorIO::Directory>> &directories);
std::string StreamSourceCheckpoint(const Input &input,const std::string &transport);

} // namespace CatalogTransferInternal
} // namespace GetDAQ

#endif
