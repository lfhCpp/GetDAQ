// Test-only oracle: call the received CCfg BinaryFormat directly, unchanged.
#include "CCfg/Io/Document.h"
#include "CCfg/Io/BinaryFormat.h"
#include "CCfg/Io/FileStorage.h"
#include "CCfg/Io/IncludeHandler.h"
#include "CCfg/Object.h"
#include <iostream>
int main(int argc,char **argv) {
    if(argc!=4)return 2;
    try {
        CCfg::Io::Document document;document.loadFromFile(argv[1]);
        if(std::string(argv[2])!="root")return 2;
        CCfg::Io::FileStorage output(argv[3],CCfg::Io::FileStorage::Write|CCfg::Io::FileStorage::Binary);
        CCfg::Io::BinaryFormat format;format.format(document.getConfig(),output,CCfg::Io::NoOpIncludeHandler::getInstance());
        return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
