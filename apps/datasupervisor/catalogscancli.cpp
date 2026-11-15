#include "catalogscancli.h"
#include "receivercatalog.h"
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace GetDAQCatalogCli {
namespace {
unsigned int Decimal(const char *text, unsigned int maximum, bool canonical,
                     const std::string &error) {
    errno=0;
    char *end=nullptr;
    const auto value=std::strtoul(text,&end,10);
    if (errno || end==text || *end || value>maximum || (canonical && std::to_string(value)!=text))
        throw std::invalid_argument(error);
    return static_cast<unsigned int>(value);
}
}

bool HandleScanCommand(int argc, char **argv) {
    const std::string action=argc>1 ? argv[1] : std::string();
    if (argc==6 && action=="--bind-scan-point") {
        const auto index=Decimal(argv[3],255,false,"POINT_INDEX must be a decimal integer from 0 through 255");
        std::cout<<GetDAQ::BindScanPointToCatalog(argv[2],index,argv[4],argv[5]);
        return true;
    }
    if (argc==5 && action=="--summarize-scan") {
        std::cout<<GetDAQ::SummarizeScanBindings(argv[2],argv[3],argv[4]);
        return true;
    }
    if (argc==6 && action=="--analyze-threshold-scan") {
        const auto cobo=Decimal(argv[4],255,true,"MFM_COBO_NUMBER must be a canonical decimal integer from 0 through 255");
        std::cout<<GetDAQ::AnalyzeThresholdScan(argv[2],argv[3],cobo,argv[5]);
        return true;
    }
    if (argc==6 && action=="--analyze-autogain-waveforms") {
        const auto cobo=Decimal(argv[4],255,true,"MFM_COBO_NUMBER must be a canonical decimal integer from 0 through 255");
        std::cout<<GetDAQ::AnalyzeBoundAutoGainWaveforms(argv[2],argv[3],cobo,argv[5]);
        return true;
    }
    if (argc==9 && (action=="--analyze-pulser-scan" || action=="--analyze-peaking-scan")) {
        const unsigned int maxima[4]={255,3,3,67};
        unsigned int values[4];
        for (unsigned int i=0;i<4;++i) {
            values[i]=Decimal(argv[4+i],maxima[i],true,
                "MFM_COBO_NUMBER/ASAD/AGET/CHANNEL must be canonical decimal integers within their supported ranges");
        }
        if (action=="--analyze-pulser-scan")
            std::cout<<GetDAQ::AnalyzeBoundPulserWaveforms(argv[2],argv[3],values[0],values[1],values[2],values[3],argv[8]);
        else
            std::cout<<GetDAQ::AnalyzeBoundPeakingTimeWaveforms(argv[2],argv[3],values[0],values[1],values[2],values[3],argv[8]);
        return true;
    }
    return false;
}
}
