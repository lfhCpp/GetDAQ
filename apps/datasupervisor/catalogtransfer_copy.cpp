#include "catalogtransfer_internal.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace GetDAQ {
namespace CatalogTransferInternal {
using namespace SupervisorIO;

std::string Transfer(const std::string &source,const std::string &destination,CatalogTransferProgress progress,const std::string &expectedSourceRequestKey,bool resume) {
    Path(source);Path(destination);Require(destination!=source && destination.compare(0,source.size()+1,source+'/')!=0,"Transfer destination must be outside source catalog");Input input(source);
    if(!expectedSourceRequestKey.empty()) {Digest(expectedSourceRequestKey);Require(String(input.document.At("request_key"))==expectedSourceRequestKey,"Selected source request key changed; no destination created");}
    for(const auto &original:input.originals)Require(destination!=original->Path() && destination.compare(0,original->Path().size()+1,original->Path()+'/')!=0,"Transfer destination must be outside original receiver sessions");
    if(!resume)CreateDirectory(destination);
    Directory target(destination);auto lease=Lease(target,".getdaq-transfer.lock",!resume);
    const auto key=String(input.document.At("request_key"));
    const auto intent="{\"schema\":1,\"kind\":\"getdaq.catalog-transfer-intent\",\"source\":"+EncodeJsonString(source)+",\"destination\":"+EncodeJsonString(destination)+",\"source_request_key\":"+EncodeJsonString(key)+",\"automatic_retry\":false,\"source_delete\":false}\n";
    const auto checkpoint=SourceCheckpoint(input,target);
    if(resume) {
        Require(ReadFile(destination+"/transfer-intent.json",16384)==intent && ReadFile(destination+"/resume-source.json",32*1024*1024)==checkpoint,"Original resume source/root binding is missing or changed; no append performed");
        CheckTargetTree(target,input.directories.size());
        struct stat complete;if(!::lstat((destination+"/transfer-complete.json").c_str(),&complete))return Verify(destination,true);
        Require(errno==ENOENT,"Cannot inspect prior transfer completion");
    } else {WriteNew(destination+"/transfer-intent.json",intent);WriteNew(destination+"/resume-source.json",checkpoint);}
    Metadata(destination+"/source-request.json",input.request,resume);Metadata(destination+"/source-manifest.json",input.manifest,resume);Metadata(destination+"/source-catalog-complete.json",input.receipt,resume);
    std::vector<std::unique_ptr<Directory>> directories;
    for(std::size_t i=0;i<input.directories.size();++i) {
        const auto stored="source_"+std::to_string(i);struct stat info;if(!resume || ::lstat((destination+'/'+stored).c_str(),&info)){if(resume)Require(errno==ENOENT,"Inspect resume source directory");CreateDirectory(destination+'/'+stored);}directories.emplace_back(new Directory(destination+'/'+stored));
        Metadata(destination+'/'+stored+"/receiver-seal.json",ReadFile(source+'/'+stored+"/receiver-seal.json",16*1024*1024),resume);
    }
    const auto prefixes=resume ? CheckPrefixes(input,directories) : std::map<std::string,struct stat>();
    std::string files="[";std::uint64_t copied=0;std::vector<Payload> copiedFiles;
    for(const auto &file:input.files) {
        input.root.Check();target.Check();CheckLease(target,".getdaq-transfer.lock",*lease);auto &src=*input.directories[file.source],&dst=*directories[file.source];src.Check();dst.Check();
        Require(Unchanged(file.info,Regular(src.Descriptor(),file.name)),"Transfer source changed before copy");const auto partial=prefixes.find("source_"+std::to_string(file.source)+'/'+file.name);const auto existing=partial!=prefixes.end();const std::uint64_t prefix=existing ? std::uint64_t(partial->second.st_size) : 0;
        if(existing)Require(Unchanged(partial->second,Regular(dst.Descriptor(),file.name)),"Partial target changed after preflight");
        Fd read(::openat(src.Descriptor(),file.name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC)),write(::openat(dst.Descriptor(),file.name.c_str(),O_RDWR|O_NOFOLLOW|O_CLOEXEC|(existing ? 0 : O_CREAT|O_EXCL),0600));struct stat held;
        Require(read.value>=0 && write.value>=0 && !::fstat(read.value,&held) && Unchanged(file.info,held),"Cannot exclusively bind transfer file");std::array<unsigned char,65536> buffer;Sha256Stream hash;std::uint64_t count=0,checkpoint=1024*1024;const auto expected=std::uint64_t(file.info.st_size);
        Require(!::fstat(write.value,&held) && (existing ? Unchanged(partial->second,held) : held.st_nlink==1),"Partial target descriptor changed");
        Require(::lseek(write.value,off_t(prefix),SEEK_SET)==off_t(prefix),"Cannot seek to checked resume boundary");
        for(;;) {
            const auto n=::read(read.value,buffer.data(),buffer.size());if(n<0 && errno==EINTR)continue;Require(n>=0,"Transfer source read failed");if(!n)break;
            Require(count<=expected && std::uint64_t(n)<=expected-count,"Transfer source grew during copy");hash.Update(buffer.data(),std::size_t(n));std::size_t done=count<prefix ? std::size_t(std::min<std::uint64_t>(n,prefix-count)) : 0;
            while(done<std::size_t(n)) {const auto written=::write(write.value,buffer.data()+done,std::size_t(n)-done);if(written<0 && errno==EINTR)continue;Require(written>0,"Transfer target write failed; preserve partial destination");done+=written;}
            count+=n;copied+=n;
            if(count>=checkpoint) {src.Check();dst.Check();if(progress)progress("copying",copied);checkpoint=count+1024*1024;}
        }
        Require(count==expected && !::fstat(read.value,&held) && Unchanged(file.info,held) && Unchanged(file.info,Regular(src.Descriptor(),file.name)),"Transfer source changed or ended early");
        Require(!::fsync(write.value),"Transfer target fsync failed");const auto digest=hash.Finish();Sync(dst);
        Payload stored=file;stored.info=Regular(dst.Descriptor(),file.name);copiedFiles.push_back(stored);
        if(files.size()>1)files+=',';
        files+="{\"path\":"+EncodeJsonString("source_"+std::to_string(file.source)+'/'+file.name)+",\"bytes\":"+Quote(expected)+",\"sha256\":"+EncodeJsonString(digest)+"}";
        Require(files.size()<=32*1024*1024,"Transfer manifest exceeds 32 MiB");if(progress)progress("file-copied",copied);
    }
    files+=']';const auto manifest="{\"schema\":1,\"kind\":\"getdaq.catalog-transfer-manifest\",\"source_request_key\":"+EncodeJsonString(key)+",\"source_manifest_sha256\":"+EncodeJsonString(ConfigurationSha256(input.manifest))+
        ",\"source_receipt_sha256\":"+EncodeJsonString(ConfigurationSha256(input.receipt))+",\"files\":"+files+",\"copied_bytes\":"+Quote(copied)+"}\n";
    Metadata(destination+"/transfer-manifest.json",manifest,resume);if(progress)progress("before-complete",copied);
    const auto checkTarget=[&] {
        CheckLease(target,".getdaq-transfer.lock",*lease);
        Require(ReadFile(destination+"/transfer-manifest.json",32*1024*1024)==manifest && ReadFile(destination+"/source-request.json",1024*1024)==input.request &&
            ReadFile(destination+"/source-manifest.json",16*1024*1024)==input.manifest && ReadFile(destination+"/source-catalog-complete.json",65536)==input.receipt,"Transfer produced metadata changed; preserve incomplete destination");
        Require(ReadFile(destination+"/transfer-intent.json",16384)==intent && ReadFile(destination+"/resume-source.json",32*1024*1024)==checkpoint,"Transfer intent/source checkpoint changed");
        for(const auto &file:copiedFiles) {directories[file.source]->Check();Require(Unchanged(file.info,Regular(directories[file.source]->Descriptor(),file.name)),"Copied file identity/metadata changed before publication");}
    };
    input.Check();checkTarget();Verify(destination,false);input.Check();checkTarget();
    const auto receipt="{\"schema\":1,\"kind\":\"getdaq.catalog-transfer-complete\",\"complete\":true,\"destination\":"+EncodeJsonString(destination)+",\"manifest_sha256\":"+EncodeJsonString(ConfigurationSha256(manifest))+
        ",\"source_request_key\":"+EncodeJsonString(key)+",\"copied_files\":"+Quote(input.files.size())+",\"copied_bytes\":"+Quote(copied)+",\"content_verified\":true,\"source_preserved\":true}\n";
    Metadata(destination+"/.transfer-complete.pending",receipt,resume);Require(!::linkat(target.Descriptor(),".transfer-complete.pending",target.Descriptor(),"transfer-complete.json",0),"Cannot exclusively publish transfer completion");Sync(target);Require(!::unlinkat(target.Descriptor(),".transfer-complete.pending",0),"Cannot remove transfer publication staging link");Sync(target);
    if(progress)progress("complete",copied);
    return receipt;
}

} // namespace CatalogTransferInternal
} // namespace GetDAQ
