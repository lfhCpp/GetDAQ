#include "configurationrepository.h"
#include <stdexcept>
#include "strictjson.h"
#include <iostream>
int main(int argc,char **argv) {
    if (argc==2 && std::string(argv[1])=="--help") {
        std::cout<<"Usage: getdaq-config-repository initialize ROOT\n"
            "       getdaq-config-repository identity|list ROOT\n"
            "       getdaq-config-repository put ROOT SHA256 FILE\n"
            "       getdaq-config-repository version ROOT SHA256\n"
            "       getdaq-config-repository commit ROOT PLAN_OBJECTS.json\n"
            "Configuration metadata only; no hardware or raw DAQ.\n"; return 0;
    }
    if (argc<3) { std::cerr<<"Use --help\n";return 2; }
    try {
        const std::string command=argv[1],root=argv[2];
        if (command=="initialize" && argc==3) { GetDAQ::ConfigurationRepository::Initialize(root);std::cout<<"Initialized configuration repository\n";return 0; }
        GetDAQ::ConfigurationRepository repository(root);
        if (command=="identity" && argc==3) std::cout<<repository.IdentityJson();
        else if (command=="list" && argc==3) std::cout<<repository.ListJson();
        else if (command=="version" && argc==4) std::cout<<repository.VersionJson(argv[3]);
        else if (command=="put" && argc==5) {
            const auto bytes=GetDAQ::ReadConfigurationFile(argv[4]);repository.PutObject(argv[3],bytes);std::cout<<"Stored configuration object\n";
        } else if (command=="commit" && argc==4) {
            const auto json=GetDAQ::ParseStrictJson(GetDAQ::ReadConfigurationFile(argv[3]));
            if (json.type!=GetDAQ::JsonValue::Array) throw std::invalid_argument("Plan objects must be a JSON array");
            std::vector<std::string> objects;for (const auto &v:json.items) { if (v.type!=GetDAQ::JsonValue::String) throw std::invalid_argument("Plan object must be a string");objects.push_back(v.text); }
            std::cout<<repository.Commit(objects);
        } else { std::cerr<<"Invalid repository arguments; use --help\n";return 2; }
        return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
