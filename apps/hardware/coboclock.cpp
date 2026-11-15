// Adapted from CEA GET CoBoNode::clockInit and CoBoPLLManager.
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "hardwareclient.h"
#include "coboclockconfiguration.h"
#include <stdexcept>
namespace GetDAQ {
namespace {
void Append(std::vector<HardwareStep> &steps, HardwareStep::Kind kind, const char *device,
            const char *reg, const char *field, std::uint64_t value)
{
    HardwareStep s; s.kind = kind; s.device = device; s.reg = reg; s.field = field; s.value = value; steps.push_back(s);
}
void Verified(std::vector<HardwareStep> &steps, const char *device, const char *reg, const char *field, std::uint64_t value)
{
    Append(steps,HardwareStep::WriteValue,device,reg,field,value);
    Append(steps,HardwareStep::ExpectValue,device,reg,field,value);
}
void Select(std::vector<HardwareStep> &steps, unsigned int id)
{
    Verified(steps,"ctrl","ASAD_sc_mask","select",1U << id);
    Verified(steps,"ctrl","asadSelect","d",id);
}
}
std::vector<HardwareStep> CoBoClockInit(const CoBoClockSettings &s)
{
    const auto image = CompileCoBoClock(s); // 完整参数先计算；部署错误不能留下前段 FIFO/LCM 写入。
    bool known = image.nominalUpstreamKnown;
    for (const auto &b : image.boards) known = known && b.sampling.nominalOutputKnown;
    if (!known && !s.allowUnverifiedUpstream) throw std::invalid_argument("Unknown nominal clock output requires explicit allowUnverifiedUpstream");
    if (known && !image.nominalOutputsWithinRange) throw std::invalid_argument("Nominal AsAd clock output is outside 1..200MHz");
    for (const auto &b : image.boards) if (image.nominalUpstreamKnown && (b.effectiveNominalReadingMHz < 1 || b.effectiveNominalReadingMHz > 200 ||
        b.effectiveNominalAdcMHz < 1 || b.effectiveNominalAdcMHz > 200 ||
        (b.sampling.nominalOutputKnown && (b.effectiveNominalSamplingMHz < 1 || b.effectiveNominalSamplingMHz > 200))))
        throw std::invalid_argument("Nominal AsAd clock output is outside 1..200MHz");
    if (image.nominalKnownDifference && !s.allowNominalDifference)
        throw std::invalid_argument("Nominal clock mismatch requires explicit allowNominalDifference");
    std::vector<HardwareStep> steps;
    Append(steps,HardwareStep::ExpectValue,"ctrl","pipeCtrl","firmwareEnable",0);
    steps.back().requireBoundDescription = true;
    steps.back().annotation = std::string("CoBoClockInit; no PLL lock/frequency/phase verification; not complete Prepared; nominal_difference_ack=") +
        (s.allowNominalDifference ? "true" : "false") + "; unverified_output_ack=" + (s.allowUnverifiedUpstream ? "true" : "false") +
        "; nominal_upstream_known=" + (image.nominalUpstreamKnown ? "true" : "false") +
        "; nominal_targets_matched=" + (image.nominalTargetsMatched ? "true" : "false") +
        "; nominal_known_difference=" + (image.nominalKnownDifference ? "true" : "false") +
        "; hardware_version_check=" + (s.verifyHardwareVersion ? "true" : "false");
    Append(steps,HardwareStep::ExpectValue,"ctrl","pipeCtrl","intEnable",0);
    if (s.verifyHardwareVersion) Append(steps,HardwareStep::ExpectValue,"ctrl","hardwareVersion","",s.hardwareVersion);
    Verified(steps,"ctrl","ASAD_sc_mask","select",image.asadMask);
    if (image.programPll) {
        // FIFO 命令没有回显验证；读回 reset/data 可能消费数据或不反映写入。
        Append(steps,HardwareStep::WriteValue,"pll","WRFIFO_RST","",0x0a);
        Append(steps,HardwareStep::WriteValue,"pll","RDFIFO_RST","",0x0a);
        for (auto packet : image.pll.packets) Append(steps,HardwareStep::WriteValue,"pll","WRFIFO_DATA","",packet);
        Append(steps,HardwareStep::ReadValue,"pll","WRFIFO_SR","Full",0); // 旧逻辑仅观察，不是锁定或发送完成判据。
    } else Verified(steps,"ctrl","ckwDivider","d",image.writingDivider);
    for (const auto &b : image.boards) {
        Select(steps,b.asadId); Append(steps,HardwareStep::WriteValue,"asad","LCM1","",b.sampling.value);
    }
    Verified(steps,"ctrl","ASAD_sc_mask","select",image.asadMask);
    if (image.programReadingDivider) Verified(steps,"ctrl","ckrDivider","d",image.readingDivider);
    for (const auto &b : image.boards) {
        Select(steps,b.asadId); Append(steps,HardwareStep::WriteValue,"asad","LCM2","",b.reading.value);
    }
    // 独立调用后明确恢复全部活动 AsAd；中途失败时不自动补写恢复。
    Verified(steps,"ctrl","ASAD_sc_mask","select",image.asadMask);
    return steps;
}
}
