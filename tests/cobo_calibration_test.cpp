#include "cobocalibrationconfiguration.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace GetDAQ;
namespace {
void Require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
template<class F> void Rejects(F call) { try { call(); } catch (const std::invalid_argument &) { return; } throw std::runtime_error("Expected calibration settings rejection"); }
CoBoCalibrationSettings Settings(unsigned int mask = 11) {
    CoBoCalibrationSettings s; s.asadMask = mask;
    for (unsigned int id = 0; id < 4; ++id) if (mask & (1U << id)) {
        AsAdCalibrationSettings b; b.asadId = id; s.boards.push_back(b);
        for (unsigned int chip = 0; chip < 4; ++chip) s.chipVersions[4*id+chip] = chip == 3 ? 0x203 : 0x202;
    }
    return s;
}
}
int main()
{
    try {
        for (unsigned int mask = 1; mask < 16; ++mask) {
            auto s = Settings(mask); std::reverse(s.boards.begin(),s.boards.end()); auto image = CompileCoBoCalibration(s);
            unsigned int expected = 0; for (unsigned int id = 0; id < 4; ++id) if (mask & (1U << id)) expected |= 15U << (4*id);
            Require(image.agetMask == expected && image.calibrationAsAdMask == mask,"Calibration mask coverage");
        }
        for (unsigned int subset = 1; subset < 16; ++subset) for (unsigned int requested = 0; requested < 4; ++requested) {
            auto s = Settings(1); s.chipVersions.clear();
            for (unsigned int chip = 0; chip < 4; ++chip) if (subset & (1U << chip)) s.chipVersions[chip] = 0x201;
            s.boards[0].calibrationAgetId = requested;
            if (requested && !(subset & (1U << requested))) { Rejects([&] { CompileCoBoCalibration(s); }); continue; }
            unsigned int first = 0; while (!(subset & (1U << first))) ++first;
            const auto image = CompileCoBoCalibration(s);
            Require(image.calibrationAgetIds[0] == (requested ? requested : first) && image.firstCalibrationChip == (requested ? requested : first),"Automatic/explicit selection changed");
        }
        for (unsigned int policies = 0; policies < 16; ++policies) {
            auto s = Settings(15); s.delayLayout = CoBoCalibrationSettings::PerAsAdDelayFields;
            for (auto &b : s.boards) b.useAutomaticReadDelay = (policies & (1U << b.asadId)) != 0;
            Require(CompileCoBoCalibration(s).automaticReadDelayMask == policies,"Per-AsAd policy mask");
            s.delayLayout = CoBoCalibrationSettings::GlobalDelayField;
            if (policies && policies != 15) Rejects([&] { CompileCoBoCalibration(s); });
            else Require(CompileCoBoCalibration(s).globalAutomaticReadDelay == bool(policies),"Global policy differs");
        }
        auto empty = Settings(3); for (unsigned int id = 4; id < 8; ++id) empty.chipVersions.erase(id);
        Require(CompileCoBoCalibration(empty).calibrationAsAdMask == 1 && CompileCoBoCalibration(empty).calibrationAgetIds[1] == 0,"Empty active board falsely calibrated");
        empty.boards[1].calibrationAgetId = 1; Rejects([&] { CompileCoBoCalibration(empty); });
        auto invalid = Settings(); invalid.asadMask = 0; Rejects([&] { CompileCoBoCalibration(invalid); });
        invalid = Settings(); invalid.boards.pop_back(); Rejects([&] { CompileCoBoCalibration(invalid); });
        invalid = Settings(); invalid.boards.push_back(invalid.boards[0]); Rejects([&] { CompileCoBoCalibration(invalid); });
        invalid = Settings(); invalid.boards[0].calibrationAgetId = 4; Rejects([&] { CompileCoBoCalibration(invalid); });
        invalid = Settings(); invalid.chipVersions.clear(); Rejects([&] { CompileCoBoCalibration(invalid); });
        invalid = Settings(); invalid.chipVersions[16] = 0x202; Rejects([&] { CompileCoBoCalibration(invalid); });
        invalid = Settings(); invalid.chipVersions[8] = 0x202; Rejects([&] { CompileCoBoCalibration(invalid); });
        invalid = Settings(); invalid.chipVersions[0] = 0x204; Rejects([&] { CompileCoBoCalibration(invalid); });
        invalid = Settings(); invalid.calibrationAgetRegister = false; Require(CompileCoBoCalibration(invalid).firstCalibrationChip == 0,"Explicit old register variant");
        invalid.chipVersions.erase(0); Rejects([&] { CompileCoBoCalibration(invalid); });
        invalid = Settings(); invalid.delayLayout = static_cast<CoBoCalibrationSettings::DelayLayout>(99); Rejects([&] { CompileCoBoCalibration(invalid); });
        std::cout << "PASS CoBo calibration: 15 board masks, chip selection/coverage, all delay policies and explicit old hardware bounds\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
