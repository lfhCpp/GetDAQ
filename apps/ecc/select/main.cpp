#include "eccconfigurationselector.h"
#include <iostream>
#include <stdexcept>
int main(int argc,char **argv) {
    if (argc==2 && std::string(argv[1])=="--help") {
        std::cout<<"Usage: getdaq-ecc-select TEMPLATE.json SELECTION.json NEW_DEPLOYMENT_DIRECTORY\n"
            "Offline validation and new fixed startup input; does not connect hardware or change a running ECC.\n";
        return 0;
    }
    if (argc!=4) {std::cerr<<"Use --help\n";return 2;}
    try {std::cout<<GetDAQ::SelectEccConfiguration(argv[1],argv[2],argv[3]);return 0;}
    catch (const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
