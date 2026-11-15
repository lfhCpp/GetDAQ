#include "configurationrepository.h"
#include "repositorycredential.h"
#include "configurationpublication.h"
#include "configurationsha256.h"
#include "strictjson.h"
#include "owned_test_directory.h"
#include "effective_record_fixture.h"
#include "cobo_configure_fixture.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace GetDAQ;
namespace {
void Need(bool value,const char *why) {if (!value) throw std::runtime_error(why);}
template<class F> void Rejects(F action) {try {action();}catch (const std::exception &) {return;}throw std::runtime_error("Invalid repository action accepted");}
void Write(const std::string &path,const std::string &bytes) {std::ofstream file(path,std::ios::binary);file<<bytes;file.close();Need(bool(file),"Write fault fixture");}
std::string Array(const std::vector<std::string> &objects) {std::string out="[";for (const auto &s:objects) {if (out.back()!='[') out+=',';out+=EncodeJsonString(s);}return out+']';}
std::vector<std::string> Objects(ConfigurationRepository &repository,const std::string &bytes) {
    std::vector<std::string> objects;for (std::size_t at=0;at<bytes.size();at+=32768) {const auto part=bytes.substr(at,32768),sha=ConfigurationSha256(part);repository.PutObject(sha,part);objects.push_back(sha);}return objects;
}
std::string Plan(ConfigurationRepository &repository,const std::string &manifestFile) {
    const auto at=manifestFile.find_last_of('/');const auto directory=manifestFile.substr(0,at);const auto bytes=ReadConfigurationFile(manifestFile);const auto publication=DecodeConfigurationPublication(bytes);
    const auto manifests=Objects(repository,bytes);std::string files="[";
    VerifyConfigurationPublication(publication,[&](const PublicationFile &file) {
        const auto bytes=ReadConfigurationFile(directory+'/'+file.stored);const auto objects=Objects(repository,bytes);
        if (files.back()!='[') files+=',';
        files+="{\"stored\":"+EncodeJsonString(file.stored)+",\"objects\":"+Array(objects)+"}";return bytes;
    });
    return "{\"schema_version\":1,\"kind\":\"getdaq.configuration-upload-plan\",\"publication_sha256\":"+EncodeJsonString(ConfigurationSha256(bytes))+
        ",\"manifest_objects\":"+Array(manifests)+",\"files\":"+files+"]}";
}
PublicationFile PackageFile(const std::string &directory,const std::string &name,const std::string &bytes) {
    Write(directory+'/'+name,bytes);
    PublicationFile file;file.stored=name;file.sha256=ConfigurationSha256(bytes);file.bytes=bytes.size();return file;
}
void CreateSelfContainedPublication(const std::string &directory) {
    using namespace GetDAQ;
    Need(::mkdir(directory.c_str(),0700)==0,"Create isolated publication package");
    const std::string sourceBytes="synthetic experiment source";
    SystemExperimentArtifact artifact;artifact.profileSha256=ConfigurationSha256("reviewed-profile");
    ExperimentConfigurationSource source;source.path="synthetic.xcfg";source.sha256=ConfigurationSha256(sourceBytes);artifact.sources.push_back(source);
    SystemExperimentCoBo cobo;cobo.id="C0";cobo.configure=GetDAQTest::Configure(1);
    cobo.prepare.inputs.asadMask=cobo.prepare.clock.asadMask=cobo.prepare.monitoring.asadMask=cobo.prepare.calibration.asadMask=1;
    cobo.prepare.clock.allowNominalDifference=true;
    AsAdInputBoardSettings input;input.asadId=0;input.positions[0].active=true;cobo.prepare.inputs.boards.push_back(input);
    AsAdBoardClockSettings clock;clock.asadId=0;clock.delayPs=-5335;cobo.prepare.clock.boards.push_back(clock);
    AsAdMonitoringBoardSettings monitoring;monitoring.asadId=0;monitoring.externalTemperatureLower=-10;cobo.prepare.monitoring.boards.push_back(monitoring);
    AsAdCalibrationSettings calibration;calibration.asadId=0;cobo.prepare.calibration.boards.push_back(calibration);cobo.prepare.calibration.chipVersions[0]=0x202;
    artifact.settings.cobos.push_back(cobo);
    artifact=DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(artifact));
    const auto artifactBytes=EncodeSystemExperimentArtifact(artifact);
    const auto effective=GetDAQTest::EmptyEffectiveSnapshot(source.path,source.sha256,sourceBytes.size());
    const auto effectiveRecord=ParseStrictJson(effective);
    ConfigurationPublication publication;publication.rootSource=source.path;
    publication.configurationSha256=artifact.configurationSha256;
    publication.effectiveSha256=effectiveRecord.At("effective_sha256").text;
    publication.snapshotSha256=effectiveRecord.At("snapshot_sha256").text;
    publication.artifact=PackageFile(directory,"experiment.json",artifactBytes);
    publication.profile=PackageFile(directory,"profile.json","reviewed-profile");
    publication.effective=PackageFile(directory,"effective.json",effective);
    PublicationSource publishedSource;publishedSource.sourcePath=source.path;
    publishedSource.file=PackageFile(directory,"source-000.xcfg",sourceBytes);
    publication.sources.push_back(publishedSource);
    Write(directory+"/publication.json",EncodeConfigurationPublication(publication));
}
}
int RunRepositoryCore(const std::string &source,const std::string &parent) {
    try {
        Need(::mkdir(parent.c_str(),0700)==0,"Create new evidence parent");
        const auto root=parent+"/healthy";ConfigurationRepository::Initialize(root);std::string planBytes,receipt,publication;
        {
            ConfigurationRepository repository(root);Rejects([&] {ConfigurationRepository duplicate(root);});
            const auto object=ConfigurationSha256("small configuration object");Need(!repository.HasObject(object),"Fresh repository object present");repository.PutObject(object,"small configuration object");repository.PutObject(object,"small configuration object");Need(repository.HasObject(object),"Object missing after duplicate put");
            Rejects([&] {repository.PutObject(std::string(64,'0'),"wrong bytes");});Rejects([&] {repository.PutObject("../wrong","x");});Rejects([&] {repository.PutObject(ConfigurationSha256(std::string(32769,'x')),std::string(32769,'x'));});
            Rejects([&] {repository.Commit({ConfigurationSha256("missing plan")});});
            planBytes=Plan(repository,source);const auto chunks=Objects(repository,planBytes);receipt=repository.Commit(chunks);const auto result=ParseStrictJson(receipt);publication=result.At("publication_sha256").text;
            Need(result.At("hardware_loaded_verified").text=="false","Repository fabricated hardware loading");Need(repository.Commit(chunks)==receipt,"Duplicate commit changed receipt");Need(repository.VersionJson(publication)==receipt,"Registered version mismatch");
            Need(ParseStrictJson(repository.ListJson()).At("versions").items.size()==1,"Version registration count differs");
        }
        {ConfigurationRepository reopened(root);Need(reopened.VersionJson(publication)==receipt,"Reopen lost version");}
        // 原地篡改后改回，仍保持同一实例故障。
        {
            ConfigurationRepository repository(root);const auto manifest=DecodeConfigurationPublication(ReadConfigurationFile(source));const auto file=root+"/versions/"+publication+'/'+manifest.sources[0].file.stored,original=ReadConfigurationFile(file);
            Need(::chmod(file.c_str(),0600)==0,"Allow fault injection");Write(file,std::string(original.size(),'x'));Need(::chmod(file.c_str(),0400)==0,"Restore file mode");Rejects([&] {repository.VersionJson(publication);});
            Need(::chmod(file.c_str(),0600)==0,"Restore payload fixture");Write(file,original);Need(::chmod(file.c_str(),0400)==0,"Restore readonly fixture");Rejects([&] {repository.IdentityJson();});
        }
        // 未完成目录中只保留相同计划的部分文件，核对后完成；不同计划不接管。
        {
            const auto pendingRoot=parent+"/pending";ConfigurationRepository::Initialize(pendingRoot);ConfigurationRepository repository(pendingRoot);const auto plan=Plan(repository,source);const auto chunks=Objects(repository,plan);const auto directory=pendingRoot+"/versions/"+publication;
            Need(::mkdir(directory.c_str(),0700)==0,"Create interrupted commit fixture");const auto intent="{\"schema_version\":1,\"publication_sha256\":"+EncodeJsonString(publication)+",\"plan_sha256\":"+EncodeJsonString(ConfigurationSha256(plan))+"}\n";
            Write(directory+"/intent.json",intent);Need(::chmod((directory+"/intent.json").c_str(),0400)==0,"Seal interrupted intent");Need(repository.Commit(chunks)!=std::string(),"Same interrupted plan did not complete");
        }
        {
            const auto wrongRoot=parent+"/wrong-intent";ConfigurationRepository::Initialize(wrongRoot);ConfigurationRepository repository(wrongRoot);const auto plan=Plan(repository,source);const auto chunks=Objects(repository,plan);const auto directory=wrongRoot+"/versions/"+publication;
            Need(::mkdir(directory.c_str(),0700)==0,"Create wrong pending fixture");Write(directory+"/intent.json","different upload plan");Need(::chmod((directory+"/intent.json").c_str(),0400)==0,"Seal wrong intent");Rejects([&] {repository.Commit(chunks);});Rejects([&] {repository.IdentityJson();});
        }
        {
            const auto objectRoot=parent+"/object-corruption";ConfigurationRepository::Initialize(objectRoot);ConfigurationRepository repository(objectRoot);const auto bytes=std::string("frozen configuration"),hash=ConfigurationSha256(bytes),file=objectRoot+"/objects/"+hash;
            repository.PutObject(hash,bytes);Need(::chmod(file.c_str(),0600)==0,"Open object fault fixture");Write(file,std::string(bytes.size(),'x'));Need(::chmod(file.c_str(),0400)==0,"Seal corrupt object");Rejects([&] {repository.HasObject(hash);});
            Need(::chmod(file.c_str(),0600)==0,"Open object restore");Write(file,bytes);Need(::chmod(file.c_str(),0400)==0,"Seal restored object");Rejects([&] {repository.HasObject(hash);});
        }
        {
            const auto credential=parent+"/credential-private";RepositoryCredential::Generate(credential);RepositoryCredential checked(credential);const auto token=ReadConfigurationFile(credential).substr(0,64);Need(checked.Authorize("Bearer "+token),"Credential rejected");Need(!checked.Authorize("Bearer "+std::string(64,'0')),"Wrong credential accepted");
            Write(credential,std::string(64,'1')+'\n');Rejects([&] {checked.Authorize("Bearer "+token);});Write(credential,token+'\n');Rejects([&] {checked.Authorize("Bearer "+token);});
        }
        std::cout<<"PASS configuration repository core: lock, hash/size/path guards, duplicate objects/commit, full version/reopen, interrupted same-plan resume, foreign intent/content restoration latch and credential latch; no network/hardware/DAQ\n";
        return 0;
    } catch (const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
int main(int argc,char **argv) {
    if (argc==2 && std::string(argv[1])=="--self-contained-core") {
        try {
            OwnedTestDirectory root("/tmp/getdaq-config-repository-ctest-");
            const auto package=root.Path()+"/package";
            CreateSelfContainedPublication(package);
            return RunRepositoryCore(package+"/publication.json",root.Path()+"/evidence");
        } catch (const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    }
    if (argc==3) return RunRepositoryCore(argv[1],argv[2]);
    std::cerr<<"Pass --self-contained-core or frozen publication.json and NEW_PARENT\n";
    return 1;
}
