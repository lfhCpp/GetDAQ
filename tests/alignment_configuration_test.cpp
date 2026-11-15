#include "../common/experiment/alignmentconfiguration.h"
#include <climits>
#include <iostream>
#include <stdexcept>
namespace {
using namespace GetDAQ;
void Require(bool c,const char *message) { if (!c) throw std::runtime_error(message); }
template<class F> void Rejects(F call,const std::string &message) {
    try { call(); } catch (const std::exception &e) { Require(std::string(e.what()).find(message) != std::string::npos,e.what()); return; }
    throw std::runtime_error("Expected rejection: "+message);
}
}
int main()
{
    using namespace GetDAQ;
    try {
        AlignmentCrateResult crate; crate.mutantId = "M0";
        LocatedCoBo board; board.coboId = "C0"; board.mutantId = "M0";
        for (unsigned int slot = 0; slot < 12; ++slot) {
            AlignmentSlotResult a; a.slotIndex = slot; a.present = true; a.deltaDelayPs = 10000;
            crate.gmc = {a}; a.deltaDelayPs = 0; crate.wsca = {a};
            crate.gmcShelfMask = crate.wscaShelfMask = 1u<<slot; board.amcSlot = slot+1;
            ValidateAlignmentResults({crate},{board});
            auto changed = crate; changed.wsca[0].deltaDelayPs = -1;
            Rejects([&] { ValidateAlignmentResults({changed},{board}); },"10000 ps");
            changed = crate; changed.gmc[0].deltaDelayPs = INT_MIN; changed.wsca[0].deltaDelayPs = INT_MAX;
            Rejects([&] { ValidateAlignmentResults({changed},{board}); },"10000 ps");
        }
        auto changed = crate; changed.wsca[0].slotIndex = 10;
        Rejects([&] { ValidateAlignmentResults({changed},{board}); },"key sets");
        changed = crate; changed.gmc[0].slotIndex = 12;
        Rejects([&] { ValidateAlignmentResults({changed},{board}); },"0..11");
        changed = crate; changed.wsca.push_back(changed.wsca.front());
        Rejects([&] { ValidateAlignmentResults({changed},{board}); },"unique");
        changed = crate; changed.wsca[0].present = false;
        Rejects([&] { ValidateAlignmentResults({changed},{board}); },"presence differs");
        changed = crate; changed.wscaShelfMask = 4095;
        Rejects([&] { ValidateAlignmentResults({changed},{board}); },"shelf masks");
        Rejects([&] { ValidateAlignmentResults({crate},{}); },"independently located");
        auto other = board; other.coboId = "C1";
        Rejects([&] { ValidateAlignmentResults({crate},{board,other}); },"same AMC");
        Rejects([&] { ValidateAlignmentResults({crate},{board,board}); },"duplicate located");
        Rejects([&] { ValidateAlignmentResults({crate,crate},{board}); },"unique");
        AlignmentCrateResult empty; empty.mutantId = "empty";
        ValidateAlignmentResults({empty},{});
        std::cout << "PASS alignment: all 12 slot mappings, 10000 ps boundary, int32 overflow, keys/presence/topology/masks" << std::endl;
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << std::endl; return 1; }
}
