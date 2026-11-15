// Execute the received CCfg export operation without replacing its implementation.
#include "CCfg/Io/Document.h"
#include "CCfg/Io/Url.h"
#include "CCfg/CConfig.h"
#include "CCfg/Object.h"
#include <exception>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv)
{
    if(argc!=4){std::cerr<<"Usage: legacy-export INPUT.xcfg root|DOT_PATH OUTPUT.xcfg\n";return 2;}
    try {
        CCfg::Io::Document document;document.loadFromFile(argv[1]);
        CCfg::Object* object=document.getConfig();
        if(std::string(argv[2])!="root") {
            CCfg::CConfig query(object);query.appendPath(argv[2]);
            object=dynamic_cast<CCfg::Object*>(&query.find());
            if(!object)throw std::runtime_error("Legacy export requires an object");
        }
        document.exportSubConfig(*object,CCfg::Io::Url::fromPath(argv[3]));
        std::cout<<"Exported by original CCfg::Io::Document::exportSubConfig\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
