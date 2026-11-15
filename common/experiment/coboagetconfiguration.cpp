#include "coboagetconfiguration.h"
#include <algorithm>
#include <stdexcept>
namespace GetDAQ {
CoBoAgetImage CompileCoBoAget(const CoBoAgetSettings &settings)
{
    if (!settings.asadMask || settings.asadMask > 15) throw std::invalid_argument("CoBo active AsAd mask must be 1..15");
    if (settings.chips.empty() || settings.chips.size() > 16) throw std::invalid_argument("CoBo must list 1..16 active chips");
    CoBoAgetImage result; result.asadMask = settings.asadMask;
    for (const auto &chip : settings.chips) {
        if (chip.asadId > 3 || chip.agetId > 3) throw std::invalid_argument("CoBo chip AsAd/AGET ID must be 0..3");
        if (!(settings.asadMask & (1U << chip.asadId))) throw std::invalid_argument("CoBo chip belongs to an inactive AsAd");
        const unsigned int bit = 1U << (4 * chip.asadId + chip.agetId);
        if (result.agetMask & bit) throw std::invalid_argument("Duplicate active CoBo chip");
        CoBoAgetChipImage entry; entry.asadId = chip.asadId; entry.agetId = chip.agetId;
        try { entry.image = CompileAget(chip.settings); }
        catch (const std::exception &e) {
            throw std::invalid_argument("AsAd[" + std::to_string(chip.asadId) + "]/Aget[" + std::to_string(chip.agetId) + "]: " + e.what());
        }
        const unsigned int offset = static_cast<unsigned int>(entry.image.registers.back().value);
        if (result.chips.empty()) {
            result.twoPulseMode = entry.image.twoPulseMode; result.readoutOffset = offset;
            result.readoutDepth = entry.image.moduleReadoutDepth;
        } else if (result.twoPulseMode != entry.image.twoPulseMode || result.readoutOffset != offset || result.readoutDepth != entry.image.moduleReadoutDepth)
            throw std::invalid_argument("Active CoBo chips must share 2p mode, pointer offset and Module readout depth");
        result.fpnRead = result.fpnRead || (entry.image.registers.front().value & UINT64_C(0x8000));
        result.agetMask |= bit; result.chips.push_back(entry);
    }
    std::sort(result.chips.begin(), result.chips.end(), [](const CoBoAgetChipImage &a, const CoBoAgetChipImage &b) {
        return 4 * a.asadId + a.agetId < 4 * b.asadId + b.agetId;
    });
    return result;
}
}
