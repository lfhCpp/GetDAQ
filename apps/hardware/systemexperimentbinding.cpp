#include "systemexperimentbinding.h"
#include <map>
#include <stdexcept>
namespace GetDAQ {
SystemPreparationOptions BindSystemExperiment(const SystemExperimentArtifact &a,const SystemPreparationOptions &endpoints) {
    if (a.configurationSha256!=SystemExperimentDigest(a.settings) || a.artifactSha256.size()!=64 || a.artifactSha256.find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::invalid_argument("Bind requires a decoded, digest-checked system experiment artifact");
    if (endpoints.cobos.size()!=a.settings.cobos.size() || endpoints.mutants.size()!=a.settings.mutants.size())
        throw std::invalid_argument("Experiment and topology node counts differ");
    std::map<std::string,const SystemCoBoSettings *> cobos;
    std::map<std::string,const SystemMutantSettings *> mutants;
    for (const auto &c:endpoints.cobos) if (!cobos.emplace(c.id,&c).second) throw std::invalid_argument("Duplicate CoBo topology ID");
    for (const auto &m:endpoints.mutants) if (!mutants.emplace(m.endpoint.id,&m).second) throw std::invalid_argument("Duplicate MuTaNT topology ID");
    auto options=endpoints; options.configurationSha256=a.configurationSha256; options.artifactSha256=a.artifactSha256;
    options.cobos.clear(); options.mutants.clear();
    for (const auto &settings:a.settings.cobos) {
        const auto p=cobos.find(settings.id);
        if (p==cobos.end() || settings.enabled!=p->second->enabled) throw std::invalid_argument("CoBo topology ID/activity differs from experiment");
        auto c=*p->second; c.prepare.settings=settings.prepare; c.configure=settings.configure; options.cobos.push_back(std::move(c)); cobos.erase(p);
    }
    for (const auto &settings:a.settings.mutants) {
        const auto p=mutants.find(settings.id);
        if (p==mutants.end() || p->second->endpoint.master==settings.settings.isSlave) throw std::invalid_argument("MuTaNT topology ID/role differs from experiment");
        auto m=*p->second; m.settings=settings.settings; options.mutants.push_back(std::move(m)); mutants.erase(p);
    }
    if (!cobos.empty() || !mutants.empty()) throw std::invalid_argument("Duplicate/incomplete system topology binding");
    if (options.encodingMinor<0 || options.encodingMinor>1 || options.timeoutMs<100 || options.timeoutMs>60000)
        throw std::invalid_argument("Invalid bound system encoding/timeout");
    SystemAlignmentOptions topology; topology.encodingMinor=options.encodingMinor; topology.timeoutMs=options.timeoutMs;
    for (const auto &c:options.cobos) {
        ValidateHardwareDescription(c.prepare.description);
        if (c.prepare.nodeProxy.empty() || c.prepare.nodeProxy.size()>4096 || c.prepare.expectedNodeName.empty() || c.prepare.expectedNodeName.size()>128)
            throw std::invalid_argument("Invalid bound CoBo endpoint/name");
        AlignmentCoBoEndpoint e; e.id=c.id; e.enabled=c.enabled; e.expectedNodeName=c.prepare.expectedNodeName;
        e.nodeProxy=c.prepare.nodeProxy; e.linkProxy=c.linkProxy; e.description=c.prepare.description; topology.cobos.push_back(e);
        if (c.enabled) { BuildCoBoPreparePlan(c.prepare.settings); CoBoConfigure(c.configure); }
    }
    for (const auto &m:options.mutants) { topology.mutants.push_back(m.endpoint); MutantPrepare(m.settings); MutantConfigure(m.settings); }
    if (!topology.mutants.empty()) ValidateSystemAlignmentOptions(topology);
    // Callback addresses/owner declarations belong to the production controller. It must supply them before constructing a client.
    return options;
}
SystemPreparationOptions BindSystemExperiment(const SystemExperimentArtifact &a,const SystemAlignmentOptions &topology) {
    SystemPreparationOptions endpoints; endpoints.encodingMinor=topology.encodingMinor; endpoints.timeoutMs=topology.timeoutMs;
    endpoints.exclusiveOwnerConfirmed=topology.exclusiveOwnerConfirmed; endpoints.dataProcessesStopped=topology.daqStoppedConfirmed;
    for (const auto &c:topology.cobos) {
        SystemCoBoSettings e; e.id=c.id; e.enabled=c.enabled; e.linkProxy=c.linkProxy;
        e.prepare.expectedNodeName=c.expectedNodeName; e.prepare.nodeProxy=c.nodeProxy; e.prepare.description=c.description; endpoints.cobos.push_back(std::move(e));
    }
    for (const auto &m:topology.mutants) { SystemMutantSettings e; e.endpoint=m; endpoints.mutants.push_back(std::move(e)); }
    return BindSystemExperiment(a,endpoints);
}
}
