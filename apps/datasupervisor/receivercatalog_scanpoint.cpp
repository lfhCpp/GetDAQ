#include "receivercatalog.h"
#include "receivercataloginternal.h"
#include "receivercatalog_scananalysis_p.h"
#include "receivercatalog_scanpoint_p.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace GetDAQ {
namespace ReceiverCatalogScanAnalysisInternal {
using namespace ReceiverCatalogInternal;
using namespace SupervisorIO;

BoundScanPointInput LoadBoundScanPointInput(
    const std::string &scanPath, const std::string &bindingPath,
    unsigned int pointIndex, const JsonValue &point, const std::string &targetCobo,
    const std::string &outputPath, const std::string &analysisName,
    const std::string &missingCoboMessage) {
    const std::string bindingName="point-"+
        (pointIndex<10 ? std::string("00") : pointIndex<100 ? std::string("0") : std::string())+
        std::to_string(pointIndex)+".json";
    const auto bindingBytes=ReadFile(bindingPath+'/'+bindingName,65536);
    auto binding=ParseStrictJson(bindingBytes);
    const auto catalogPath=String(binding.At("catalog_directory"));
    Require(outputPath.compare(0,catalogPath.size()+1,catalogPath+'/')!=0,
        "Analysis output must be outside every immutable catalog");
    if (String(binding.At("target_cobo"))!=targetCobo ||
        bindingBytes!=BuildScanPointBinding(scanPath,pointIndex,catalogPath))
        throw std::runtime_error(analysisName+
            " scan binding is missing, stale, or scoped to another CoBo: "+bindingName);

    const auto artifactName=String(point.At("artifact"));
    const auto artifactBytes=ReadFile(scanPath+'/'+artifactName,8*1024*1024);
    auto artifact=DecodeSystemExperimentArtifact(artifactBytes);
    const auto selectedCoBo=std::find_if(artifact.settings.cobos.begin(),artifact.settings.cobos.end(),
        [&](const SystemExperimentCoBo &cobo) { return cobo.id==targetCobo; });
    Require(selectedCoBo!=artifact.settings.cobos.end(),missingCoboMessage.c_str());
    const auto selectedIndex=static_cast<std::size_t>(selectedCoBo-artifact.settings.cobos.begin());
    return BoundScanPointInput{std::move(binding),catalogPath,std::move(artifact),selectedIndex};
}

}
}
