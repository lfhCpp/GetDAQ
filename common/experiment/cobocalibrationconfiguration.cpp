// Adapted from CEA GET CoBoNode calibration chip selection and delay policy.
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "cobocalibrationconfiguration.h"
#include <stdexcept>
namespace GetDAQ {
CoBoCalibrationImage CompileCoBoCalibration(const CoBoCalibrationSettings &s)
{
    if (!s.asadMask || s.asadMask > 15) throw std::invalid_argument("Calibration AsAd mask must be 1..15");
    if (s.delayLayout != CoBoCalibrationSettings::GlobalDelayField && s.delayLayout != CoBoCalibrationSettings::PerAsAdDelayFields)
        throw std::invalid_argument("Unknown read delay field layout");
    if (s.chipVersions.empty() || s.chipVersions.size() > 16) throw std::invalid_argument("Calibration requires 1..16 expected active chip versions");
    CoBoCalibrationImage image; image.asadMask = s.asadMask;
    for (const auto &v : s.chipVersions) {
        if (v.first > 15 || !(s.asadMask & (1U << (v.first/4)))) throw std::invalid_argument("Calibration chip ID is outside active AsAd coverage");
        if (v.second != 0x201 && v.second != 0x202 && v.second != 0x203) throw std::invalid_argument("Unknown expected calibration AGET/ASTRE version");
        image.agetMask |= 1U << v.first; image.calibrationAsAdMask |= 1U << (v.first/4);
    }
    unsigned int seen = 0; bool first = true;
    for (const auto &b : s.boards) {
        if (b.asadId > 3 || !(s.asadMask & (1U << b.asadId)) || (seen & (1U << b.asadId)) || b.calibrationAgetId > 3)
            throw std::invalid_argument("Invalid, duplicate or inactive calibration AsAd/AGET ID");
        seen |= 1U << b.asadId;
        const unsigned int chips = (image.agetMask >> (4*b.asadId)) & 15;
        unsigned int selected = 0;
        if (chips) {
            while (!(chips & (1U << selected))) ++selected;
            if (b.calibrationAgetId) selected = b.calibrationAgetId;
            if (!(chips & (1U << selected))) throw std::invalid_argument("Requested calibration chip must be active");
        } else if (b.calibrationAgetId) throw std::invalid_argument("Calibration chip requested on an AsAd without active AGETs");
        if (!s.calibrationAgetRegister && selected) throw std::invalid_argument("Nonzero calibration chip requires calibrationAget register support");
        image.calibrationAgetIds[b.asadId] = selected;
        if (b.useAutomaticReadDelay) image.automaticReadDelayMask |= 1U << b.asadId;
        if (first) { image.globalAutomaticReadDelay = b.useAutomaticReadDelay; first = false; }
        else if (s.delayLayout == CoBoCalibrationSettings::GlobalDelayField && image.globalAutomaticReadDelay != b.useAutomaticReadDelay)
            throw std::invalid_argument("Global read delay field cannot represent different active AsAd policies");
    }
    if (seen != s.asadMask) throw std::invalid_argument("Missing active calibration AsAd settings");
    for (unsigned int id = 0; id < 4; ++id) if (image.calibrationAsAdMask & (1U << id)) {
        image.firstCalibrationChip = 4*id+image.calibrationAgetIds[id]; break;
    }
    return image;
}
}
