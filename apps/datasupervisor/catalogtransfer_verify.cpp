#include "catalogtransfer_internal.h"
#include "controlprotocol.h"
#include "planio.h"
#include "systemexperimentconfiguration.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <map>
#include <set>
#include <stdexcept>
#include <unistd.h>

namespace GetDAQ {
namespace CatalogTransferInternal {
using namespace SupervisorIO;
std::string Hash(int fd,std::uint64_t expected) {
    Require(::lseek(fd,0,SEEK_SET)==0,"Cannot rewind transferred file");Sha256Stream hash;std::array<unsigned char,65536> buffer;std::uint64_t count=0;
    for(;;) {const auto read=::read(fd,buffer.data(),buffer.size());if(read<0 && errno==EINTR)continue;Require(read>=0,"Transfer verification read failed");if(!read)break;Require(count<=expected && std::uint64_t(read)<=expected-count,"Transfer file grew during verification");count+=read;hash.Update(buffer.data(),std::size_t(read));}
    Require(count==expected,"Transfer verification byte count differs");return hash.Finish();
}

// Validate a standalone copied dataset; original paths remain historical evidence.
std::string Verify(const std::string &path,bool requireComplete,bool verifyPayload) {
    Path(path);Directory root(path);const auto manifestBytes=ReadFile(path+"/transfer-manifest.json",32*1024*1024);const auto manifest=Json(manifestBytes);
    ExactSupervisorFields(manifest,{"schema","kind","source_request_key","source_manifest_sha256","source_receipt_sha256","files","copied_bytes"});
    Require(Unsigned(manifest.At("schema"))==1 && String(manifest.At("kind"))=="getdaq.catalog-transfer-manifest","Unsupported transfer manifest");
    const auto sourceBytes=ReadFile(path+"/source-manifest.json",16*1024*1024),receiptBytes=ReadFile(path+"/source-catalog-complete.json",65536),requestBytes=ReadFile(path+"/source-request.json",1024*1024);
    Require(ConfigurationSha256(sourceBytes)==String(manifest.At("source_manifest_sha256")) && ConfigurationSha256(receiptBytes)==String(manifest.At("source_receipt_sha256")),"Transfer original evidence hash differs");
    const auto source=Json(sourceBytes),receipt=Json(receiptBytes);const auto request=DecodeReceiverCatalogRequest(requestBytes);const auto key=ReceiverCatalogRequestKey(request);
    CheckTargetTree(root,source.At("sources").items.size());
    Require(String(manifest.At("source_request_key"))==key && String(source.At("request_key"))==key && String(receipt.At("request_key"))==key && String(receipt.At("manifest_sha256"))==ConfigurationSha256(sourceBytes) && Boolean(receipt.At("complete")),"Transfer source request/receipt binding differs");
    std::map<std::string,std::uint64_t> expected;std::map<std::string,std::set<std::string>> names;
    for(std::size_t i=0;i<source.At("sources").items.size();++i) {
        const auto &row=source.At("sources").items[i];const auto stored=String(row.At("stored_directory"));Require(stored=="source_"+std::to_string(i),"Transfer stored source differs");Directory directory(path+'/'+stored);
        Require(ConfigurationSha256(ReadFile(path+'/'+stored+"/receiver-seal.json",16*1024*1024))==String(row.At("seal_sha256")),"Transfer receiver seal changed");
        for(const auto &file:row.At("files").items) {const auto name=String(file.At("name"));RawName(name);Require(expected.emplace(stored+'/'+name,Unsigned(file.At("bytes"),true)).second,"Duplicate transfer source file");names[stored].insert(name);}
        Require(Names(directory)==names[stored],"Transferred raw name set differs");
    }
    Require(manifest.At("files").type==JsonValue::Array && manifest.At("files").items.size()==expected.size() && expected.size()<=65536,"Transfer file coverage differs");std::set<std::string> seen;std::uint64_t total=0;
    for(const auto &file:manifest.At("files").items) {
        ExactSupervisorFields(file,{"path","bytes","sha256"});const auto name=String(file.At("path"));const auto found=expected.find(name);const auto bytes=Unsigned(file.At("bytes"),true);const auto digest=String(file.At("sha256"));Digest(digest);
        Require(found!=expected.end() && seen.insert(name).second && bytes==found->second && bytes<=UINT64_MAX/2 && total<=UINT64_MAX/2-bytes,"Transfer file path/size/total differs");
        const auto slash=name.find('/');Directory directory(path+'/'+name.substr(0,slash));const auto leaf=name.substr(slash+1);const auto before=Regular(directory.Descriptor(),leaf);
        Require(before.st_nlink==1 && std::uint64_t(before.st_size)==bytes,"Transfer file is aliased or size differs");Fd input(::openat(directory.Descriptor(),leaf.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));struct stat held;
        Require(input.value>=0 && !::fstat(input.value,&held) && Unchanged(before,held) && (!verifyPayload || Hash(input.value,bytes)==digest),"Transferred content SHA-256 differs");
        Require(!::fstat(input.value,&held) && Unchanged(before,held) && Unchanged(before,Regular(directory.Descriptor(),leaf)),"Transfer file changed during verification");directory.Check();total+=bytes;
    }
    Require(total==Unsigned(manifest.At("copied_bytes"),true),"Transfer copied byte total differs");root.Check();
    if(!requireComplete)return std::string();
    const auto completeBytes=ReadFile(path+"/transfer-complete.json",65536);const auto complete=Json(completeBytes);
    ExactSupervisorFields(complete,{"schema","kind","complete","destination","manifest_sha256","source_request_key","copied_files","copied_bytes","content_verified","source_preserved"});
    Require(Unsigned(complete.At("schema"))==1 && String(complete.At("kind"))=="getdaq.catalog-transfer-complete" && Boolean(complete.At("complete")) &&
        String(complete.At("destination"))==path && String(complete.At("manifest_sha256"))==ConfigurationSha256(manifestBytes) && String(complete.At("source_request_key"))==key &&
        Unsigned(complete.At("copied_files"),true)==expected.size() && Unsigned(complete.At("copied_bytes"),true)==total && Boolean(complete.At("content_verified")) && Boolean(complete.At("source_preserved")),"Transfer completion scope differs");return completeBytes;
}
}
}
