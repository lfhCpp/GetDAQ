#include "CCfg/Model/ConfigModelAccessor.h"
#include "CCfg/Model/ConfigNode.h"
#include "CCfg/Model/Command/OverrideConfigNode.h"
#include "CCfg/Io/Document.h"
#include "CCfg/Io/Url.h"
#include "CCfg/Object.h"
#include "CCfg/Path.h"
#include <QCoreApplication>
#include <iostream>
#include <memory>
#include <stdexcept>

int main(int argc,char** argv)
{
    QCoreApplication application(argc,argv);
    if(argc!=4){std::cerr<<"Usage: legacy-object-override INPUT.xcfg ROOT_DOT_PATH OUTPUT_PREFIX\n";return 2;}
    try {
        std::auto_ptr<CCfg::Io::Document> document(new CCfg::Io::Document);
        document->loadFromFile(argv[1]);CCfg::Model::ConfigModelAccessor accessor;accessor.setDocument(document);
        CCfg::Model::ConfigNode* node=accessor.pathToNode(CCfg::Path(argv[2]));
        if(!node || !node->isObject())throw std::runtime_error("Select an existing object in the original model");
        const bool inherited=node->isInherited();
        CCfg::Model::Command::OverrideConfigNode command(node,&accessor);
        command.redo();
        const bool succeeded=command.hasSucceeded();
        const std::string prefix=argv[3];
        accessor.getDocument()->exportSubConfig(*accessor.getDocument()->getConfig(),CCfg::Io::Url::fromPath(prefix+".after.xcfg"));
        command.undo();
        accessor.getDocument()->exportSubConfig(*accessor.getDocument()->getConfig(),CCfg::Io::Url::fromPath(prefix+".undo.xcfg"));
        command.redo();
        accessor.getDocument()->exportSubConfig(*accessor.getDocument()->getConfig(),CCfg::Io::Url::fromPath(prefix+".redo.xcfg"));
        std::cout<<"Original model: inherited="<<inherited<<"; override succeeded="<<succeeded<<"\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
