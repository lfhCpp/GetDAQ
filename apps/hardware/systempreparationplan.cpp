// Host sequence adapted from CEA GET SystemManager (received 2021 release).
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "systempreparationplan_p.h"
#include "systempreparation_p.h"

#include <set>
#include <stdexcept>

namespace GetDAQ {
namespace SystemPreparationInternal {

CompiledSystemPreparationPlan CompileSystemPreparationPlan(const SystemPreparationOptions &options)
{
    CompiledSystemPreparationPlan plan;
    plan.options = options;
    auto &o = plan.options;
    if (!o.configurationSha256.empty() || !o.artifactSha256.empty()) {
        for (const auto &digest : {o.configurationSha256, o.artifactSha256})
            if (digest.size() != 64 || digest.find_first_not_of("0123456789abcdef") != std::string::npos)
                throw std::invalid_argument("System configuration binding requires both SHA-256 digests");
    }
    if (o.cobos.empty() || o.cobos.size() > 192 || o.mutants.size() > 3 || o.encodingMinor < 0 ||
        o.encodingMinor > 1 || o.timeoutMs < 100 || o.timeoutMs > 60000)
        throw std::invalid_argument("System preparation requires 1..192 CoBo, 0..3 MuTaNT and bounded encoding/timeout");

    std::set<std::string> ids;
    unsigned int masters = 0, active = 0;
    std::set<unsigned int> slaveIds;
    std::set<bool> slavePorts;
    auto &alignment = plan.alignmentOptions;
    alignment.encodingMinor = o.encodingMinor;
    alignment.timeoutMs = o.timeoutMs;
    alignment.exclusiveOwnerConfirmed = o.exclusiveOwnerConfirmed;
    alignment.daqStoppedConfirmed = o.dataProcessesStopped;

    for (std::size_t i = 0; i < o.mutants.size(); ++i) {
        auto &m = o.mutants[i];
        Text(m.endpoint.id, 128);
        if (!ids.insert(m.endpoint.id).second) throw std::invalid_argument("Duplicate system topology ID");
        if (m.endpoint.master == m.settings.isSlave)
            throw std::invalid_argument("MuTaNT endpoint role differs from configuration");
        m.settings.moduleCount = unsigned(o.mutants.size()); // 旧SystemManager在所有MuTaNT Prepare前设置总数。
        m.settings.gmcShelfMask = m.settings.wscaShelfMask = 0; // 唯一来源为本次对齐快照。
        CompileMutant(m.settings);
        if (m.endpoint.master) {
            ++masters;
            plan.master = i;
            plan.level2 = m.settings.level2Enabled;
        } else if (!slaveIds.insert(m.settings.slaveId).second ||
                   !slavePorts.insert(m.settings.slavePortA).second ||
                   m.settings.slaveId != unsigned(!m.settings.slavePortA)) {
            throw std::invalid_argument("MuTaNT slaves require unique matching ID/port: A=0, B=1");
        }
        plan.mutantPreparePlans.push_back(MutantPrepare(m.settings));
        alignment.mutants.push_back(m.endpoint);
    }
    if (!o.mutants.empty() && masters != 1)
        throw std::invalid_argument("System preparation requires exactly one MuTaNT master");

    for (auto &c : o.cobos) {
        Text(c.id, 128);
        Text(c.prepare.nodeProxy, 4096);
        Text(c.prepare.expectedNodeName, 128);
        ValidateHardwareDescription(c.prepare.description);
        if (!ids.insert(c.id).second) throw std::invalid_argument("Duplicate system topology ID");
        c.prepare.encodingMinor = o.encodingMinor;
        c.prepare.timeoutMs = o.timeoutMs;
        c.prepare.ownsStoppedHardware = o.exclusiveOwnerConfirmed;
        c.prepare.dataProcessesStopped = o.dataProcessesStopped;
        if (c.enabled) {
            ++active;
            ValidateCoBoPrepareSettings(c.prepare.settings);
            const auto prepared = CompileAsAdInputs(c.prepare.settings.inputs);
            const auto configured = CompileCoBoConfigure(c.configure);
            if (!c.prepare.settings.foundation.mutantStatusRegister ||
                prepared.asadMask != configured.readout.aget.asadMask ||
                prepared.agetMask != configured.readout.aget.agetMask ||
                c.prepare.settings.foundation.bufferLayout != c.configure.readout.bufferLayout)
                throw std::invalid_argument("CoBo Prepare/Configure activity, buffer layout or system status support mismatch");
            for (const auto &chip : configured.readout.aget.chips)
                if (c.prepare.settings.calibration.chipVersions.at(4 * chip.asadId + chip.agetId) != chip.image.chipVersion)
                    throw std::invalid_argument("CoBo Prepare/Configure expected chip version mismatch");
            plan.configurePlans.push_back(CoBoConfigure(c.configure));
        } else {
            plan.configurePlans.emplace_back();
        }
        AlignmentCoBoEndpoint endpoint;
        endpoint.id = c.id;
        endpoint.expectedNodeName = c.prepare.expectedNodeName;
        endpoint.nodeProxy = c.prepare.nodeProxy;
        endpoint.linkProxy = c.linkProxy;
        endpoint.description = c.prepare.description;
        endpoint.enabled = c.enabled;
        alignment.cobos.push_back(std::move(endpoint));
        plan.coboTopology.emplace_back(c.id, c.enabled);
    }
    if (!active) throw std::invalid_argument("System preparation requires at least one active CoBo");
    return plan;
}

} // namespace SystemPreparationInternal
} // namespace GetDAQ
