#include "coboclockconfiguration.h"
#include "coboclockconfiguration_p.h"

#include <stdexcept>
#include <vector>

namespace GetDAQ {
using CoBoClockInternal::Frequency;

CoBoPllImage CompileCoBoPll(float writing, const std::map<unsigned int, std::uint32_t> &overrides)
{
    Frequency(writing, "CoBo PLL CKW");
    static const std::uint32_t defaults[] = {0x00020000, 0x00000a00, 0x00000a01, 0x00000a02, 0x00000a03, 0x00000a04,
        0x00000a05, 0x11110006, 0x11110007, 0x01110008, 0x55555549, 0x9140410a, 0x0400000b, 0x0b0c006c,
        0x1b12902d, 0x1200000e, 0x8000800f, 0x01550410, 0x000000d8, 0x01010019, 0xafa8001a, 0x1c0000db,
        0x002000dc, 0x0180015d, 0x0200015e, 0x001f001f};
    std::vector<std::uint32_t> config(defaults, defaults + 26);
    CoBoPllImage image; image.divider = unsigned(2000.0f / writing);
    image.nominalDefaultWritingMHz = 2000.0 / image.divider;
    image.nonStandardFrequency = writing != 3.125f && writing != 6.25f && writing != 12.5f && writing != 25 && writing != 50 && writing != 100;
    image.expertRegisters = !overrides.empty();
    for (unsigned int i = 4; i <= 6; ++i) config[i] = (config[i] & ~UINT32_C(0xffe0)) | (image.divider << 5);
    for (const auto &entry : overrides) {
        const unsigned int id = entry.first;
        if (id > 31 || (id > 16 && id < 24)) throw std::invalid_argument("PLL override register must be R0..16 or R24..31");
        if ((entry.second & 31) != id) throw std::invalid_argument("PLL override address bits differ from register index");
        config[id <= 16 ? 1 + id : id - 6] = entry.second;
    }
    for (unsigned int i = 0; i < config.size(); ++i) {
        image.packets.push_back(config[i]);
        // 原 GetBench：R13.EN_CLKin1 为 1 时，R11 再写一次并清除 SYNC_POL_INV。
        if (i == 12 && (config[14] & (1U << 6))) image.packets.push_back(config[i] & ~(1U << 16));
    }
    return image;
}
const std::vector<CoBoPllParameter> &CoBoPllParameters()
{
    static const std::vector<CoBoPllParameter> parameters = {
        {"VCO_DIV","1","",10,8,3,false}, {"MODE","0","",11,27,5,false}, {"EN_SYNC","true","",11,26,1,true},
        {"SYNC_POL_INV","false","",11,16,1,true}, {"SYNC_EN_AUTO","false","",11,15,1,true}, {"SYNC_TYPE","0","",11,12,3,false},
        {"LD_MUX","1","",12,27,5,false}, {"LD_TYPE","3","",12,24,3,false}, {"SYNC_PLL2_DLD","false","",12,23,1,true},
        {"SYNC_PLL1_DLD","false","",12,22,1,true}, {"HOLDOVER_MUX","3","",13,27,5,false}, {"HOLDOVER_TYPE","3","",13,24,3,false},
        {"Status_CLKin1_MUX","1","",13,20,3,false}, {"Status_CLKin0_TYPE","2","",13,16,3,false},
        {"Status_CLKin0_MUX","1","",13,12,3,false}, {"CLKin_SELECT_MODE","0","CLKin_Select_MODE",13,9,3,false},
        {"EN_CLKin1","false","",13,6,1,true}, {"EN_CLKin0","true","",13,5,1,true}
    };
    return parameters;
}

} // namespace GetDAQ
