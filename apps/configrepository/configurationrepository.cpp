#include "configurationrepository.h"
#include "configurationrepository_plan_p.h"
#include "configurationrepository_files_p.h"
#include "configurationpublication.h"
#include "configurationselection.h"
#include "configurationsha256.h"
#include "strictjson.h"
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <map>
#include <set>
#include <mutex>
#include <stdexcept>
namespace GetDAQ {
using ConfigurationRepositoryInternal::Directory;
using ConfigurationRepositoryInternal::Digest;
using ConfigurationRepositoryInternal::Exists;
using ConfigurationRepositoryInternal::Fd;
using ConfigurationRepositoryInternal::MakeDirectory;
using ConfigurationRepositoryInternal::Names;
using ConfigurationRepositoryInternal::Need;
using ConfigurationRepositoryInternal::PublishFile;
using ConfigurationRepositoryInternal::Random;
using ConfigurationRepositoryInternal::Read;
using ConfigurationRepositoryInternal::Same;
using ConfigurationRepositoryInternal::ValidateRootPath;

namespace {
const std::size_t Chunk=32768,PlanLimit=1024*1024;
std::string Text(const JsonValue &v) { if (v.type!=JsonValue::String) throw std::invalid_argument("Repository plan requires string"); return v.text; }
}
class ConfigurationRepository::Impl {
public:
    std::string root,id;
    Directory rootDirectory,objectsDirectory,versionsDirectory;
    Fd lock,identityFile;
    struct stat lockIdentity,identityFileStat;
    mutable std::mutex mutex;
    mutable bool fault=false;
    std::size_t objectCount=0;
    Impl(const std::string &r):root(r),rootDirectory(r),objectsDirectory(r+"/objects"),versionsDirectory(r+"/versions"),lock(::open((r+"/lock").c_str(),O_RDWR|O_NOFOLLOW|O_CLOEXEC)),identityFile(::open((r+"/identity").c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC)) {
        ValidateRootPath(r); Need(lock.value>=0 && ::fstat(lock.value,&lockIdentity)==0 && S_ISREG(lockIdentity.st_mode) && lockIdentity.st_uid==::geteuid() && (lockIdentity.st_mode&0777)==0600,"Invalid repository lock");
        Need(::flock(lock.value,LOCK_EX|LOCK_NB)==0,"Repository already has a writer");Need(identityFile.value>=0 && ::fstat(identityFile.value,&identityFileStat)==0,"Cannot bind repository identity file");id=Read(root+"/identity",65); Digest(id);objectCount=Names(root+"/objects").size();Need(objectCount<=32768,"Repository object quota exceeded");Check();
    }
    void Check() const {
        Need(!fault,"Repository fault is latched; explicit inspection/reconciliation required");
        try {
            rootDirectory.Check();objectsDirectory.Check();versionsDirectory.Check();struct stat st,held;
            Need(::lstat((root+"/lock").c_str(),&st)==0 && ::fstat(lock.value,&held)==0 && Same(st,lockIdentity) && Same(st,held) && (st.st_mode&0777)==0600,"Repository lock identity changed");
            Need(::lstat((root+"/identity").c_str(),&st)==0 && ::fstat(identityFile.value,&held)==0 && Same(st,identityFileStat) && Same(st,held) && Read(root+"/identity",65)==id,"Repository identity changed");
        } catch (...) { fault=true;throw; }
    }
    std::string Object(const std::string &sha) const {
        Digest(sha); const auto path=root+"/objects/"+sha;
        if (!Exists(path)) throw std::out_of_range("Repository object is not present");
        try { const auto bytes=Read(path,Chunk); Need(!bytes.empty() && ConfigurationSha256(bytes)==sha,"Repository object content differs");return bytes; }
        catch (...) { fault=true;throw; }
    }
    std::string Assemble(const std::vector<std::string> &objects,std::size_t maximum,std::size_t expected=0) const {
        std::string bytes;
        for (std::size_t i=0;i<objects.size();++i) {
            const auto part=Object(objects[i]); Need(i+1==objects.size() || part.size()==Chunk,"Non-final repository object is not 32 KiB");
            Need(bytes.size()+part.size()<=maximum,"Repository assembled byte limit exceeded"); bytes+=part;
        }
        Need(!bytes.empty() && (!expected || bytes.size()==expected),"Repository assembled byte count differs"); return bytes;
    }
    std::string Version(const std::string &sha) const {
        Digest(sha); const auto directory=root+"/versions/"+sha; Directory version(directory); const auto manifest=Read(directory+"/publication.json",PlanLimit);
        Need(ConfigurationSha256(manifest)==sha,"Registered publication manifest differs");
        const auto p=DecodeConfigurationPublication(manifest); const auto artifact=VerifyConfigurationPublication(p,[&](const PublicationFile &f) { return Read(directory+'/'+f.stored,std::size_t(f.bytes)); });
        const auto receipt=Read(directory+"/registration.json",65536);const auto selected=SelectConfigurationRegistration(receipt);
        Need(selected.registration.repositoryId==id && selected.registration.configurationSha256==artifact.configurationSha256 &&
            selected.registration.artifactSha256==artifact.artifactSha256,"Registration receipt differs");
        VerifyConfigurationSelection(selected,receipt,directory+"/publication.json",manifest,p);
        version.Check(); return receipt;
    }
};
void ConfigurationRepository::Initialize(const std::string &root) {
    ValidateRootPath(root); MakeDirectory(root);MakeDirectory(root+"/objects");MakeDirectory(root+"/versions");PublishFile(root+"/identity",Random());
    Fd lock(::open((root+"/lock").c_str(),O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600)); Need(lock.value>=0 && ::fsync(lock.value)==0,"Cannot create repository lock"); Directory(root).Sync();
}
ConfigurationRepository::ConfigurationRepository(const std::string &root):fImpl(new Impl(root)) {}
ConfigurationRepository::~ConfigurationRepository() {}
std::string ConfigurationRepository::IdentityJson() const {
    std::lock_guard<std::mutex> guard(fImpl->mutex);fImpl->Check();
    return "{\"schema_version\":1,\"kind\":\"getdaq.configuration-repository\",\"repository_id\":"+EncodeJsonString(fImpl->id)+",\"chunk_bytes\":32768,\"hardware_loaded_verified\":false}\n";
}
void ConfigurationRepository::PutObject(const std::string &sha,const std::string &bytes) {
    Digest(sha); if (bytes.empty() || bytes.size()>Chunk || ConfigurationSha256(bytes)!=sha) throw std::invalid_argument("Invalid repository object bytes/SHA");
    std::lock_guard<std::mutex> guard(fImpl->mutex);fImpl->Check();const auto path=fImpl->root+"/objects/"+sha;
    try {
        if (Exists(path)) { Need(fImpl->Object(sha)==bytes,"Repository duplicate object differs");return; }
        // 配置块仓库1GiB/32768块预算，原始采集数据不属于此配额。
        Need(fImpl->objectCount<32768,"Repository object quota exceeded"); PublishFile(path,bytes);++fImpl->objectCount;fImpl->Check();
    } catch (...) { fImpl->fault=true;throw; }
}
bool ConfigurationRepository::HasObject(const std::string &sha) const {
    Digest(sha);std::lock_guard<std::mutex> guard(fImpl->mutex);fImpl->Check();if (!Exists(fImpl->root+"/objects/"+sha)) return false;
    try { fImpl->Object(sha);return true; } catch (...) { fImpl->fault=true;throw; }
}
std::string ConfigurationRepository::ReadObject(const std::string &sha) const {
    Digest(sha);std::lock_guard<std::mutex> guard(fImpl->mutex);fImpl->Check();
    const auto bytes=fImpl->Object(sha);fImpl->Check();return bytes;
}
std::string ConfigurationRepository::RegisterDeployment(const std::vector<std::string> &objects) {
    if(objects.empty() || objects.size()>32)throw std::invalid_argument("Deployment manifest object budget exceeded");
    std::lock_guard<std::mutex> guard(fImpl->mutex);fImpl->Check();
    const auto bytes=fImpl->Assemble(objects,PlanLimit);const auto manifest=ParseStrictJson(bytes);
    Need(manifest.type==JsonValue::Object && manifest.members.size()==6 && manifest.At("schema").type==JsonValue::Number && manifest.At("schema").text=="1" && Text(manifest.At("kind"))=="getdaq.distributed-release","Unsupported distributed release");
    const auto project=Text(manifest.At("project_id"));Need(!project.empty() && project.size()<=128,"Invalid distributed project identity");
    Need(manifest.At("hosts").type==JsonValue::Array && !manifest.At("hosts").items.empty() && manifest.At("hosts").items.size()<=64,"Invalid deployment host count");
    Need(manifest.At("groups").type==JsonValue::Array && !manifest.At("groups").items.empty() && manifest.At("groups").items.size()<=64,"Invalid deployment group count");
    const auto &files=manifest.At("files");Need(files.type==JsonValue::Array && !files.items.empty() && files.items.size()<=512,"Invalid deployment file count");
    std::set<std::string> names;std::size_t total=0;
    for(const auto &file:files.items) {
        Need(file.type==JsonValue::Object && file.members.size()==4,"Invalid deployment file entry");
        const auto name=Text(file.At("id"));Need(!name.empty() && name.size()<=128 && name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")==std::string::npos && name!="." && name!=".." && names.insert(name).second,"Invalid/duplicate portable file identity");
        const auto sha=Text(file.At("sha256"));Digest(sha);const auto &size=file.At("bytes");
        Need(size.type==JsonValue::Number && !size.text.empty() && size.text.find_first_not_of("0123456789")==std::string::npos && size.text.size()<10,"Invalid deployment file size");
        const auto count=std::stoul(size.text);Need(count<=16*1024*1024,"Deployment file exceeds limit");total+=count;Need(total<=64*1024*1024,"Deployment aggregate file budget exceeded");
        const auto &chunks=file.At("objects");Need(chunks.type==JsonValue::Array && (!chunks.items.empty() || count==0) && chunks.items.size()<=512,"Invalid deployment file chunks");
        std::vector<std::string> parts;for(const auto &chunk:chunks.items)parts.push_back(Text(chunk));
        Need(ConfigurationSha256(count?fImpl->Assemble(parts,count,count):std::string())==sha && (count || parts.empty()),"Deployment file digest differs");
    }
    const auto id=ConfigurationSha256(bytes),directory=fImpl->root+"/deployments";
    if(!Exists(directory))MakeDirectory(directory);
    Directory releases(directory);const auto path=directory+'/'+id;
    if(Exists(path))Need(Read(path,PlanLimit)==bytes,"Distributed release identity differs");
    else {Need(Names(directory).size()<1024,"Deployment release quota exceeded");PublishFile(path,bytes);}
    releases.Sync();fImpl->Check();return "{\"schema\":1,\"repository_id\":"+EncodeJsonString(fImpl->id)+",\"release_id\":"+EncodeJsonString(id)+",\"content_verified\":true,\"hardware_loaded_verified\":false}\n";
}
std::string ConfigurationRepository::DeploymentManifest(const std::string &sha) const {
    Digest(sha);std::lock_guard<std::mutex> guard(fImpl->mutex);fImpl->Check();
    const auto directory=fImpl->root+"/deployments",path=directory+'/'+sha;
    if(!Exists(path))throw std::out_of_range("Distributed release is not registered");
    try {Directory releases(directory);const auto bytes=Read(path,PlanLimit);Need(ConfigurationSha256(bytes)==sha,"Distributed release manifest changed");releases.Check();fImpl->Check();return bytes;}
    catch(...){fImpl->fault=true;throw;}
}
std::string ConfigurationRepository::Commit(const std::vector<std::string> &planObjects) {
    if (planObjects.empty() || planObjects.size()>32) throw std::invalid_argument("Repository plan object budget exceeded");
    std::lock_guard<std::mutex> guard(fImpl->mutex);fImpl->Check();
    const auto planBytes=fImpl->Assemble(planObjects,PlanLimit);
    const auto prepared=PrepareConfigurationRepositoryCommit(planBytes,
        [this](const std::vector<std::string> &objects,std::size_t maximum,std::size_t expected) {
            return fImpl->Assemble(objects,maximum,expected);
        });
    const auto &sha=prepared.publicationSha256;
    const auto &manifest=prepared.manifest;
    const auto &publication=prepared.publication;
    const auto &files=prepared.files;
    const auto &artifact=prepared.artifact;
    fImpl->Check();
    const auto directory=fImpl->root+"/versions/"+sha;
    try {
        if (Exists(directory) && Exists(directory+"/registration.json")) return fImpl->Version(sha);
        if (!Exists(directory)) { Need(Names(fImpl->root+"/versions").size()<1024,"Repository version quota exceeded");MakeDirectory(directory); }
        Directory version(directory);
        const auto intent="{\"schema_version\":1,\"publication_sha256\":"+EncodeJsonString(sha)+",\"plan_sha256\":"+EncodeJsonString(prepared.planSha256)+"}\n";
        if (Exists(directory+"/intent.json")) Need(Read(directory+"/intent.json",65536)==intent,"Pending version belongs to different upload plan");else PublishFile(directory+"/intent.json",intent);
        std::set<std::string> allowed={"intent.json","publication.json"};for (const auto &file:files) allowed.insert(file.first);
        for (const auto &name:Names(directory)) Need(allowed.count(name),"Unexpected pending version file");
        for (const auto &file:files) {
            version.Check();const auto path=directory+'/'+file.first;
            if (Exists(path)) Need(Read(path,file.second.size())==file.second,"Pending version file differs");else PublishFile(path,file.second);
        }
        if (Exists(directory+"/publication.json")) Need(Read(directory+"/publication.json",PlanLimit)==manifest,"Pending manifest differs");else PublishFile(directory+"/publication.json",manifest);
        version.Sync();VerifyConfigurationPublication(publication,[&](const PublicationFile &f) { return Read(directory+'/'+f.stored,std::size_t(f.bytes)); });
        ConfigurationRegistration registration;registration.repositoryId=fImpl->id;registration.publicationSha256=sha;
        registration.configurationSha256=artifact.configurationSha256;registration.artifactSha256=artifact.artifactSha256;
        registration.effectiveSha256=publication.effectiveSha256;registration.snapshotSha256=publication.snapshotSha256;registration.manifestFile=directory+"/publication.json";
        const auto receipt=EncodeConfigurationRegistration(registration);
        PublishFile(directory+"/registration.json",receipt);version.Sync();fImpl->Check();return fImpl->Version(sha);
    } catch (...) { fImpl->fault=true;throw; }
}
std::string ConfigurationRepository::VersionJson(const std::string &sha) const {
    Digest(sha);std::lock_guard<std::mutex> guard(fImpl->mutex);fImpl->Check();if (!Exists(fImpl->root+"/versions/"+sha+"/registration.json")) throw std::out_of_range("Repository version is not registered");
    try { return fImpl->Version(sha); } catch (...) { fImpl->fault=true;throw; }
}
std::string ConfigurationRepository::ListJson() const {
    std::lock_guard<std::mutex> guard(fImpl->mutex);fImpl->Check();std::string result="{\"schema_version\":1,\"repository_id\":"+EncodeJsonString(fImpl->id)+",\"versions\":[";
    try {
        for (const auto &sha:Names(fImpl->root+"/versions")) {
            Digest(sha);if (!Exists(fImpl->root+"/versions/"+sha+"/registration.json")) continue;
            Directory version(fImpl->root+"/versions/"+sha);
            const auto receipt=ParseStrictJson(Read(fImpl->root+"/versions/"+sha+"/registration.json",65536));
            Need(Text(receipt.At("repository_id"))==fImpl->id && Text(receipt.At("publication_sha256"))==sha,"Repository listing receipt differs");
            if (result.back()!='[') result+=',';
            result+="{\"publication_sha256\":"+EncodeJsonString(sha)+",\"content_verified\":false}";
        }
        return result+"]}\n";
    } catch (...) { fImpl->fault=true;throw; }
}
}
