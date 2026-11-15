// Consistency rules adapted from received CEA GET AmcSlotManager.
// Original CeCILL license: third_party/licenses/getbench/COPYING.
#include "alignmentconfiguration.h"
#include <map>
#include <set>
#include <stdexcept>
#include <utility>
namespace GetDAQ {
namespace {
typedef std::map<unsigned int,const AlignmentSlotResult *> Slots;
Slots Index(const std::vector<AlignmentSlotResult> &results)
{
    if (results.size() > 12) throw std::invalid_argument("Alignment returned more than 12 slots");
    Slots slots;
    for (const auto &result : results) {
        if (result.slotIndex > 11 || !slots.emplace(result.slotIndex,&result).second)
            throw std::invalid_argument("Alignment slot indices must be unique and in 0..11");
    }
    return slots;
}
}
void ValidateAlignmentResults(const std::vector<AlignmentCrateResult> &crates,
                              const std::vector<LocatedCoBo> &located)
{
    if (crates.empty() || crates.size() > 16 || located.size() > 192)
        throw std::invalid_argument("Alignment result exceeds bounded host topology");
    std::map<std::string,unsigned int> expected;
    for (const auto &crate : crates) {
        if (crate.mutantId.empty() || !expected.emplace(crate.mutantId,0).second)
            throw std::invalid_argument("Alignment module IDs must be nonempty and unique");
    }
    std::set<std::string> boards;
    for (const auto &board : located) {
        if (board.coboId.empty() || !boards.insert(board.coboId).second ||
            !expected.count(board.mutantId) || board.amcSlot < 1 || board.amcSlot > 12)
            throw std::invalid_argument("Invalid or duplicate located CoBo/module/AMC slot");
        const unsigned int bit = 1u << (board.amcSlot-1);
        auto &mask = expected.at(board.mutantId);
        if (mask & bit) throw std::invalid_argument("Two active CoBo boards occupy the same AMC slot");
        mask |= bit;
    }
    for (const auto &crate : crates) {
        const auto gmc = Index(crate.gmc), wsca = Index(crate.wsca);
        if (gmc.size() != wsca.size()) throw std::runtime_error("GMC/WSCA slot key sets differ");
        unsigned int present = 0;
        for (const auto &entry : gmc) {
            const auto other = wsca.find(entry.first);
            if (other == wsca.end()) throw std::runtime_error("GMC/WSCA slot key sets differ");
            const auto &a = *entry.second, &b = *other->second;
            if (a.present != b.present) throw std::runtime_error("GMC/WSCA slot presence differs");
            // 两个Ice int的减法先提升；INT_MIN与INT_MAX不能在32位里相减。
            const std::int64_t delta = std::int64_t(a.deltaDelayPs)-std::int64_t(b.deltaDelayPs);
            if (a.present && (delta < -10000 || delta > 10000))
                throw std::runtime_error("GMC/WSCA reported delay differs by more than 10000 ps");
            if (a.present) present |= 1u << entry.first;
        }
        if (present != expected.at(crate.mutantId))
            throw std::runtime_error("Aligned presence differs from independently located active CoBo boards");
        if (crate.gmcShelfMask > 4095 || crate.wscaShelfMask > 4095 ||
            crate.gmcShelfMask != present || crate.wscaShelfMask != present)
            throw std::runtime_error("Saved clock shelf masks differ from aligned presence");
    }
}
}
