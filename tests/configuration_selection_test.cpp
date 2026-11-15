#include "configurationselection.h"
#include "configurationsha256.h"
#include <iostream>
#include <stdexcept>
using namespace GetDAQ;
namespace {
void Need(bool ok) {if (!ok) throw std::runtime_error("Selection protocol assertion failed");}
template<class F> void Reject(F action) {try {action();}catch (const std::exception &) {return;}throw std::runtime_error("Invalid selection accepted");}
std::string Replace(std::string bytes,const std::string &from,const std::string &to) {
    const auto at=bytes.find(from);Need(at!=std::string::npos);bytes.replace(at,from.size(),to);return bytes;
}
PublicationFile File(const std::string &name,char digest) {
    PublicationFile file;file.stored=name;file.sha256=std::string(64,digest);file.bytes=1;return file;
}
}
int main() {
    try {
        ConfigurationPublication p;p.rootSource="/test/root.xcfg";p.configurationSha256=std::string(64,'1');
        p.effectiveSha256=std::string(64,'2');p.snapshotSha256=std::string(64,'3');
        p.artifact=File("experiment.json",'4');p.profile=File("profile.json",'5');p.effective=File("effective.json",'6');
        PublicationSource source;source.sourcePath=p.rootSource;source.file=File("source-000.xcfg",'7');p.sources.push_back(source);
        const auto manifest=EncodeConfigurationPublication(p);
        ConfigurationRegistration r;r.repositoryId=std::string(64,'8');r.publicationSha256=ConfigurationSha256(manifest);
        r.configurationSha256=p.configurationSha256;r.artifactSha256=p.artifact.sha256;r.effectiveSha256=p.effectiveSha256;r.snapshotSha256=p.snapshotSha256;
        r.manifestFile="/test/repository/versions/"+r.publicationSha256+"/publication.json";
        const auto receipt=EncodeConfigurationRegistration(r);
        const auto selection=SelectConfigurationRegistration(receipt);
        const auto encoded=EncodeConfigurationSelection(selection);Need(EncodeConfigurationSelection(DecodeConfigurationSelection(encoded))==encoded);
        VerifyConfigurationSelection(selection,receipt,r.manifestFile,manifest,p);
        Reject([&] {DecodeConfigurationRegistration(Replace(receipt,"\"hardware_loaded_verified\":false","\"hardware_loaded_verified\":true"));});
        Reject([&] {DecodeConfigurationSelection(Replace(encoded,"\"schema_version\":1","\"schema_version\":\"1\""));});
        Reject([&] {DecodeConfigurationSelection(Replace(encoded,"\"hardware_loaded_verified\":false","\"hardware_loaded_verified\":0"));});
        Reject([&] {DecodeConfigurationSelection(Replace(encoded,"\"repository_id\":","\"extra\":true,\"repository_id\":"));});
        Reject([&] {VerifyConfigurationSelection(selection,receipt+" ",r.manifestFile,manifest,p);});
        Reject([&] {VerifyConfigurationSelection(selection,receipt,"/other/publication.json",manifest,p);});
        Reject([&] {VerifyConfigurationSelection(selection,receipt,r.manifestFile,manifest+" ",p);});
        auto different=r;different.repositoryId=std::string(64,'9');
        Reject([&] {VerifyConfigurationSelection(selection,EncodeConfigurationRegistration(different),r.manifestFile,manifest,p);});
        different=r;different.artifactSha256=std::string(64,'0');const auto wrongReceipt=EncodeConfigurationRegistration(different);
        Reject([&] {VerifyConfigurationSelection(SelectConfigurationRegistration(wrongReceipt),wrongReceipt,r.manifestFile,manifest,p);});
        for (const auto &path:{"/bad/../publication.json","/bad//publication.json","relative/publication.json","/bad/other.json"}) {
            different=r;different.manifestFile=path;Reject([&] {EncodeConfigurationRegistration(different);});
        }
        Reject([&] {DecodeConfigurationSelection(std::string(65537,' '));});
        std::cout<<"PASS configuration selection strict schema, scope, receipt bytes and repository/publication/file binding; protocol only, no hardware\n";
    } catch (const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
