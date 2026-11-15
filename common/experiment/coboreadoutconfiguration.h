#ifndef GETDAQ_COBOREADOUTCONFIGURATION_H
#define GETDAQ_COBOREADOUTCONFIGURATION_H
#include "coboagetconfiguration.h"
#include <array>
#include <map>
namespace GetDAQ {
struct CoBoParameter {
    std::string name, defaultValue, unit;
    bool optional = false;
    CoBoParameter(const std::string &n, const std::string &d, const std::string &u, bool o)
        : name(n), defaultValue(d), unit(u), optional(o) {}
};
const std::vector<CoBoParameter> &CoBoModuleParameters();
const std::vector<CoBoParameter> &CoBoBufferParameters();
struct CoBoChipReadout {
    unsigned int asadId = 0, agetId = 0;
    bool suppressMultiplicity = false;
    // 旧零抑制配置写入全部 68 通道，包括 FPN 和不活动通道。
    std::array<unsigned int, 68> zeroSuppressionThresholds{};
};
struct CoBoBufferRange {
    std::string start, end; // 均为空表示未指定；四块必须一起提供，或一起自动计算。
};
struct CoBoReadoutSettings {
    enum BufferLayout : unsigned int { PerAsAdBuffers, SharedBuffer };
    CoBoAgetSettings aget;
    std::map<std::string, std::string> module, circularBuffer;
    std::array<CoBoBufferRange, 4> buffers;
    std::vector<CoBoChipReadout> chips; // 恰好对应 aget 中的所有活动芯片。
    BufferLayout bufferLayout = PerAsAdBuffers; // 旧固件共享内存布局须显式选择，不在写失败后切换。
};
struct CoBoRegisterValue {
    std::string device, reg, field;
    std::uint64_t value = 0;
};
struct CoBoReadoutImage {
    CoBoAgetImage aget;
    bool fullReadout = false, useExternalTimeReset = false;
    unsigned int suppressMultiplicity = 0, triggerMask = 0;
    std::array<std::uint32_t, 4> bufferStart{}, bufferEnd{};
    std::vector<CoBoRegisterValue> writes; // 零抑制、Module 和循环缓冲区的有序设置；不含 AGET 寄存器。
};
// 无硬件/文档访问。完整 Configure 还需 AsAd TCM、inspection、pulser、2p 初始化及状态约束。
CoBoReadoutImage CompileCoBoReadout(const CoBoReadoutSettings &settings);
std::uint32_t ParseCoBoUnsigned(const std::string &text, const std::string &name, std::uint32_t maximum = UINT32_MAX);
}
#endif
