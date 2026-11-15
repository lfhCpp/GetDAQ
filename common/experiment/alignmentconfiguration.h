#ifndef GETDAQ_ALIGNMENTCONFIGURATION_H
#define GETDAQ_ALIGNMENTCONFIGURATION_H
#include <cstdint>
#include <string>
#include <vector>
namespace GetDAQ {
enum class AlignmentClock : unsigned int { Gmc, Wsca };
enum class CoBoLinkMode : unsigned int { Normal, Disconnected, GmcFeedback, WscaFeedback };
struct AlignmentSlotResult {
    unsigned int slotIndex = 0; // 板端字典键0..11；对外AMC编号为slotIndex+1。
    bool present = false;
    std::int32_t deltaDelayPs = 0, programmedDelay = 0;
};
struct LocatedCoBo {
    std::string coboId, mutantId;
    unsigned int amcSlot = 0; // 人类使用的1..12编号。
};
struct AlignmentCrateResult {
    std::string mutantId;
    std::vector<AlignmentSlotResult> gmc, wsca;
    unsigned int gmcShelfMask = 0, wscaShelfMask = 0;
};
// 核对两路返回键/存在性/10ns差异、已定位活动板和保存的12位输出掩码。
// 结果只是板端报告的一致性，不证明真实相位、控制权或整个Prepared状态。
void ValidateAlignmentResults(const std::vector<AlignmentCrateResult> &crates,
                              const std::vector<LocatedCoBo> &located);
}
#endif
