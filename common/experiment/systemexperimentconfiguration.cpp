#include "systemexperimentconfiguration.h"
#include <set>
#include <stdexcept>
namespace GetDAQ {
namespace {
void Id(const std::string &id)
{
    if (id.empty() || id.size() > 128 || id == "*" || id.find_first_of("[]/") != std::string::npos)
        throw std::invalid_argument("System experiment requires bounded concrete path-safe IDs");
    for (unsigned char c : id) if (c < 32 || c == 127) throw std::invalid_argument("Control byte in system experiment ID");
}
}
void ValidateSystemExperimentSettings(const SystemExperimentSettings &s)
{
    if (s.cobos.empty() || s.cobos.size() > 192 || s.mutants.size() > 3)
        throw std::invalid_argument("System experiment requires 1..192 CoBo and 0..3 MuTaNT");
    std::set<std::string> ids;
    std::set<unsigned int> slaveIds;
    std::set<bool> ports;
    unsigned int master = 0, active = 0;
    for (const auto &m : s.mutants) {
        Id(m.id); if (!ids.insert(m.id).second) throw std::invalid_argument("Duplicate system experiment ID");
        if (m.settings.moduleCount != s.mutants.size() || m.settings.gmcShelfMask || m.settings.wscaShelfMask)
            throw std::invalid_argument("Experiment module count must match topology; shelf masks come from new alignment");
        CompileMutant(m.settings);
        if (!m.settings.isSlave) ++master;
        else if (m.settings.slaveId != unsigned(!m.settings.slavePortA) || !slaveIds.insert(m.settings.slaveId).second || !ports.insert(m.settings.slavePortA).second)
            throw std::invalid_argument("MuTaNT slaves require unique matching ID/port: A=0, B=1");
    }
    if (!s.mutants.empty() && master != 1) throw std::invalid_argument("System experiment requires exactly one MuTaNT master");
    for (const auto &c : s.cobos) {
        Id(c.id); if (!ids.insert(c.id).second) throw std::invalid_argument("Duplicate system experiment ID");
        if (!c.enabled) continue;
        ++active; ValidateCoBoPrepareSettings(c.prepare);
        const auto prepared = CompileAsAdInputs(c.prepare.inputs);
        const auto configured = CompileCoBoConfigure(c.configure);
        if (!c.prepare.foundation.mutantStatusRegister || prepared.asadMask != configured.readout.aget.asadMask || prepared.agetMask != configured.readout.aget.agetMask ||
            c.prepare.foundation.bufferLayout != c.configure.readout.bufferLayout)
            throw std::invalid_argument("CoBo Prepare/Configure activity, buffer layout or system status support mismatch");
        for (const auto &chip : configured.readout.aget.chips)
            if (c.prepare.calibration.chipVersions.at(4*chip.asadId+chip.agetId) != chip.image.chipVersion)
                throw std::invalid_argument("CoBo Prepare/Configure expected chip version mismatch");
    }
    if (!active || (!s.mutants.empty() && active > s.mutants.size()*12)) throw std::invalid_argument("System active CoBo count is zero or exceeds available AMC slots");
}
}
