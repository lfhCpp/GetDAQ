#include "catalogtransfer_internal.h"
#include "receivercatalog.h"
#include "supervisorio.h"
#include "supervisorcommon.h"
#include "controlprotocol.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <pthread.h>
#include <signal.h>
#include <stdexcept>
#include <sys/file.h>
#include <unistd.h>
#include <vector>
#ifdef __APPLE__
#include <stdio.h>
#include <sys/stdio.h>
#endif
#ifdef __linux__
#include <linux/fs.h>
#include <sys/syscall.h>
#endif

namespace GetDAQ {
namespace {
using namespace SupervisorIO;
using namespace CatalogTransferInternal;

}
using namespace CatalogTransferInternal;

std::string ReceiveReceiverCatalogStream(const std::string &destinationRoot,int inputFd,int outputFd,bool resume,const std::string &transport,bool acceptEitherMode) {
    using namespace SupervisorIO;BlockPipeSignal signalGuard;std::unique_ptr<Directory> root;std::unique_ptr<Fd> rootLease;
    try {
        Require(transport=="openssh-stdio-v1" || transport=="tcp-plain-v1","Unsupported catalog stream transport");
        Path(destinationRoot);root.reset(new Directory(destinationRoot));rootLease=Lease(*root,".getdaq-catalog-transfer.lock",true);WriteTag(outputFd,"GDAQRDY1");
        char transferModeBytes[8];ExactRead(inputFd,transferModeBytes,sizeof(transferModeBytes));
        const std::string transferMode(transferModeBytes,sizeof(transferModeBytes));
        if(acceptEitherMode)Require(transferMode=="GDAQRES2" || transferMode=="GDAQNEW2","Archive stream transfer mode differs");
        else Require(transferMode==(resume ? "GDAQRES2" : "GDAQNEW2"),"Archive stream transfer mode differs");
        const bool streamResume=transferMode=="GDAQRES2";
        const auto requestSize=Read32(inputFd),manifestSize=Read32(inputFd),receiptSize=Read32(inputFd),checkpointSize=Read32(inputFd);
        Require(requestSize<=1024*1024 && manifestSize<=16*1024*1024 && receiptSize<=65536 && checkpointSize<=32*1024*1024,"Archive stream metadata length exceeds limit");
        StreamSource source;source.request.resize(requestSize);source.manifest.resize(manifestSize);source.receipt.resize(receiptSize);source.checkpoint.resize(checkpointSize);
        if(requestSize)ExactRead(inputFd,&source.request[0],requestSize);
        if(manifestSize)ExactRead(inputFd,&source.manifest[0],manifestSize);
        if(receiptSize)ExactRead(inputFd,&source.receipt[0],receiptSize);
        if(checkpointSize)ExactRead(inputFd,&source.checkpoint[0],checkpointSize);
        const auto sourceCheckpoint=source.checkpoint;source=DecodeStreamSource(source.request,source.manifest,source.receipt);source.checkpoint=sourceCheckpoint;
        const auto destination=RemoteDestination(destinationRoot,source.key),partial=destination+".partial";
        struct stat state;
        if(streamResume && !::lstat(destination.c_str(),&state)) {
            Require(ReadFile(destination+"/source-request.json",1024*1024)==source.request && ReadFile(destination+"/source-manifest.json",16*1024*1024)==source.manifest &&
                ReadFile(destination+"/source-catalog-complete.json",65536)==source.receipt && ReadFile(destination+"/resume-source.json",32*1024*1024)==source.checkpoint,
                "Completed remote destination belongs to a different source identity");
            const auto completion=Verify(destination,true);
            WriteTag(outputFd,"GDAQCOMP");Write32(outputFd,std::uint32_t(completion.size()));ExactWrite(outputFd,completion.data(),completion.size());return completion;
        }
        if(streamResume)Require(errno==ENOENT,"Cannot inspect completed remote destination");
        const auto intent="{\"schema\":1,\"kind\":\"getdaq.catalog-transfer-intent\",\"transport\":"+EncodeJsonString(transport)+",\"source_request_key\":"+EncodeJsonString(source.key)+
            ",\"destination\":"+EncodeJsonString(destination)+",\"automatic_retry\":false,\"explicit_resume\":true,\"source_delete\":false}\n";
        if(streamResume) {
            Require(!::lstat(partial.c_str(),&state),"No matching incomplete remote transfer exists to resume");
            root->Check();Directory targetProbe(partial);CheckTargetTree(targetProbe,source.options.stopped.receivers.size());
            Require(ReadFile(partial+"/transfer-intent.json",16384)==intent && ReadFile(partial+"/resume-source.json",32*1024*1024)==source.checkpoint &&
                ReadFile(partial+"/source-request.json",1024*1024)==source.request && ReadFile(partial+"/source-manifest.json",16*1024*1024)==source.manifest &&
                ReadFile(partial+"/source-catalog-complete.json",65536)==source.receipt,"Incomplete remote transfer is bound to a different source or intent");
        } else {
            Require(::lstat(destination.c_str(),&state) && errno==ENOENT && ::lstat(partial.c_str(),&state) && errno==ENOENT,"Archive destination or incomplete attempt already exists; inspect it, no overwrite/retry");
            CreateDirectory(partial);
        }
        Directory target(partial);auto targetLease=Lease(target,".getdaq-transfer.lock",!streamResume);
        if(!streamResume) {
            WriteNew(partial+"/transfer-intent.json",intent);WriteNew(partial+"/resume-source.json",source.checkpoint);
            WriteNew(partial+"/source-request.json",source.request);WriteNew(partial+"/source-manifest.json",source.manifest);WriteNew(partial+"/source-catalog-complete.json",source.receipt);
        }
        const auto metadataReply=[&] {WriteTag(outputFd,"GDAQACPT");};metadataReply();
        const auto sourceCount=source.options.stopped.receivers.size();std::vector<std::unique_ptr<Directory>> sourceDirectories;
        for(std::size_t i=0;i<sourceCount;++i) {
            const auto directoryName="source_"+std::to_string(i);
            if(streamResume) {if(::lstat((partial+'/'+directoryName).c_str(),&state)) {Require(errno==ENOENT,"Inspect resumed source directory");CreateDirectory(partial+'/'+directoryName);}}
            else CreateDirectory(partial+'/'+directoryName);
            sourceDirectories.emplace_back(new Directory(partial+'/'+directoryName));
            const auto sealSize=Read32(inputFd);Require(sealSize<=16*1024*1024,"Receiver seal exceeds metadata limit");std::string seal(sealSize,'\0');if(sealSize)ExactRead(inputFd,&seal[0],sealSize);
            const auto row=Json(source.manifest).At("sources").items[i];Require(ConfigurationSha256(seal)==String(row.At("seal_sha256")),"Remote receiver seal SHA-256 differs");
            Metadata(partial+'/'+directoryName+"/receiver-seal.json",seal,streamResume);Sync(*sourceDirectories.back());
        }
        struct Partial {bool exists=false;std::uint64_t bytes=0;std::string digest;struct stat info={};};
        std::vector<Partial> partials;partials.reserve(source.files.size());
        for(std::size_t index=0;index<source.files.size();++index) {
            const auto &file=source.files[index];auto &directory=*sourceDirectories[file.source];Partial item;
            if(!::fstatat(directory.Descriptor(),file.name.c_str(),&item.info,AT_SYMLINK_NOFOLLOW)) {
                item.info=Regular(directory.Descriptor(),file.name);Require(item.info.st_nlink==1 && std::uint64_t(item.info.st_size)<=file.bytes,"Remote partial is aliased or exceeds source file length");
                Fd existing(::openat(directory.Descriptor(),file.name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));Require(existing.value>=0,"Cannot open remote partial prefix");
                Sha256Stream hash;item.bytes=std::uint64_t(item.info.st_size);ReadPrefix(existing.value,item.bytes,hash);item.digest=hash.Finish();
                struct stat held;Require(!::fstat(existing.value,&held) && Unchanged(item.info,held) && Unchanged(item.info,Regular(directory.Descriptor(),file.name)),"Remote partial changed during prefix check");item.exists=true;
            } else {Require(errno==ENOENT,"Inspect remote partial file");item.digest=ConfigurationSha256(std::string());}
            const auto name="source_"+std::to_string(file.source)+'/'+file.name;
            Write32(outputFd,std::uint32_t(file.source));Write32(outputFd,std::uint32_t(name.size()));Write64(outputFd,file.bytes);Write64(outputFd,item.bytes);
            ExactWrite(outputFd,name.data(),name.size());ExactWrite(outputFd,item.digest.data(),item.digest.size());partials.push_back(item);
        }
        ReadTag(inputFd,"GDAQPRF1");
        std::string files="[";std::uint64_t copied=0;std::array<unsigned char,65536> buffer;
        for(std::size_t fileIndex=0;fileIndex<source.files.size();++fileIndex) {
            const auto &file=source.files[fileIndex];const auto sourceIndex=Read32(inputFd);const auto nameSize=Read32(inputFd);const auto fileSize=Read64(inputFd);const auto offset=Read64(inputFd);
            const auto expectedName="source_"+std::to_string(file.source)+'/'+file.name;
            Require(sourceIndex==file.source && nameSize==expectedName.size() && fileSize==file.bytes && offset==partials[fileIndex].bytes,"Archive stream file order/length/resume offset differs from source manifest");
            std::string name(nameSize,'\0');if(nameSize)ExactRead(inputFd,&name[0],nameSize);Require(name==expectedName,"Archive stream path differs from source manifest");
            auto &directory=*sourceDirectories[file.source];Fd output(::openat(directory.Descriptor(),file.name.c_str(),O_RDWR|O_NOFOLLOW|O_CLOEXEC|(partials[fileIndex].exists ? 0 : O_CREAT|O_EXCL),0600));
            if(output.value<0)throw std::runtime_error(std::string("Cannot exclusively create remote archive file ")+expectedName+": "+std::strerror(errno));
            struct stat held;Require(!::fstat(output.value,&held) && (!partials[fileIndex].exists || Unchanged(partials[fileIndex].info,held)) && held.st_nlink==1 && std::uint64_t(held.st_size)==offset,"Remote partial descriptor changed after prefix check");
            Sha256Stream hash;ReadPrefix(output.value,offset,hash);Sha256Stream prefixHash=hash;Require(prefixHash.Finish()==partials[fileIndex].digest,"Remote partial prefix changed before append");
            Require(::lseek(output.value,off_t(offset),SEEK_SET)==off_t(offset),"Cannot seek to verified remote resume offset");
            std::uint64_t received=offset;
            while(received<file.bytes) {
                const auto wanted=std::size_t(std::min<std::uint64_t>(buffer.size(),file.bytes-received));ExactRead(inputFd,buffer.data(),wanted);hash.Update(buffer.data(),wanted);std::size_t written=0;
                while(written<wanted) {const auto n=::write(output.value,buffer.data()+written,wanted-written);if(n<0 && errno==EINTR)continue;Require(n>0,"Remote archive file write failed; partial data retained");written+=std::size_t(n);}received+=wanted;
            }
            char expectedDigest[64];ExactRead(inputFd,expectedDigest,sizeof(expectedDigest));const auto digest=hash.Finish();Require(std::equal(digest.begin(),digest.end(),expectedDigest),"Archive payload digest differs while receiving");
            Require(!::fstat(output.value,&held) && S_ISREG(held.st_mode) && held.st_nlink==1 && held.st_size>=0 && std::uint64_t(held.st_size)==file.bytes && !::fsync(output.value),"Cannot sync received archive file");Sync(directory);
            if(files.size()>1)files+=',';
            files+="{\"path\":"+EncodeJsonString("source_"+std::to_string(file.source)+'/'+file.name)+",\"bytes\":"+Quote(file.bytes)+",\"sha256\":"+EncodeJsonString(digest)+"}";
            Require(files.size()<=32*1024*1024 && copied<=UINT64_MAX/2-file.bytes,"Remote transfer manifest/byte count exceeds limit");copied+=file.bytes;
        }
        ReadTag(inputFd,"GDAQEND1");files+=']';const auto transferManifest="{\"schema\":1,\"kind\":\"getdaq.catalog-transfer-manifest\",\"source_request_key\":"+EncodeJsonString(source.key)+
            ",\"source_manifest_sha256\":"+EncodeJsonString(ConfigurationSha256(source.manifest))+",\"source_receipt_sha256\":"+EncodeJsonString(ConfigurationSha256(source.receipt))+",\"files\":"+files+",\"copied_bytes\":"+Quote(copied)+"}\n";
        Metadata(partial+"/transfer-manifest.json",transferManifest,streamResume);Sync(target);Require(Verify(partial,false)==std::string(),"Remote archive content verification failed");
        const auto completion="{\"schema\":1,\"kind\":\"getdaq.catalog-transfer-complete\",\"complete\":true,\"destination\":"+EncodeJsonString(destination)+
            ",\"manifest_sha256\":"+EncodeJsonString(ConfigurationSha256(transferManifest))+",\"source_request_key\":"+EncodeJsonString(source.key)+
            ",\"copied_files\":"+Quote(source.files.size())+",\"copied_bytes\":"+Quote(copied)+",\"content_verified\":true,\"source_preserved\":true}\n";
        struct stat completeInfo;
        if(::fstatat(target.Descriptor(),"transfer-complete.json",&completeInfo,AT_SYMLINK_NOFOLLOW)) {
            Require(errno==ENOENT,"Inspect remote staged completion");Metadata(partial+"/.transfer-complete.pending",completion,streamResume);
            Require(!::linkat(target.Descriptor(),".transfer-complete.pending",target.Descriptor(),"transfer-complete.json",0),"Cannot stage remote archive completion");
            Sync(target);Require(!::unlinkat(target.Descriptor(),".transfer-complete.pending",0),"Cannot remove remote completion staging file");Sync(target);
        } else Require(S_ISREG(completeInfo.st_mode) && ReadFile(partial+"/transfer-complete.json",65536)==completion,"Staged remote completion differs from resumed transfer");
        if(!::fstatat(target.Descriptor(),".transfer-complete.pending",&completeInfo,AT_SYMLINK_NOFOLLOW)) {Require(ReadFile(partial+"/.transfer-complete.pending",65536)==completion && !::unlinkat(target.Descriptor(),".transfer-complete.pending",0),"Cannot remove verified remote completion staging file");Sync(target);}
        else Require(errno==ENOENT,"Inspect remote completion staging file");
        CheckLease(*root,".getdaq-catalog-transfer.lock",*rootLease);
        PublishDirectoryNoReplace(partial,destination);Sync(*root);
        const auto verified=Verify(destination,true,false);Require(verified==completion,"Published remote completion receipt differs");WriteTag(outputFd,"GDAQCOMP");Write32(outputFd,std::uint32_t(completion.size()));ExactWrite(outputFd,completion.data(),completion.size());return completion;
    } catch(const std::exception &error) {WriteFailure(outputFd,error.what());throw;}
}

}
