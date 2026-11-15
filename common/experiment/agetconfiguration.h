#ifndef GETDAQ_AGETCONFIGURATION_H
#define GETDAQ_AGETCONFIGURATION_H
#include <array>
#include <stdexcept>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace GetDAQ {
// 实验参数到 AGET 寄存器的纯计算层；不包含 Qt、XML、Ice 或硬件访问。
// 映射来源：GET 2.3.0 CoBoConfigParser/CoBoNode，详见 docs/AGET_CONFIGURATION.md。
struct AgetParameter {
    std::string name, defaultValue; // 空 defaultValue 表示必须声明。
    AgetParameter(const std::string &n, const std::string &d = "") : name(n), defaultValue(d) {}
};
const std::vector<AgetParameter> &AgetParameters();
struct AgetChannel {
    enum Reading { Never = 0, Always = 1, OnlyIfHit = 2 };
    unsigned int id = 0;           // 0..67，包含 11/22/45/56 四个 FPN。
    bool active = true, selectedForTest = false;
    unsigned int gain = 0;         // 0/1/2/3 = 120fC/240fC/1pC/10pC。
    unsigned int threshold = 0;    // 0..15。
    unsigned int inhibition = 0;   // 0/1/2 = none/inhibit_channel/inhibit_trigger。
    Reading reading = Never;
};
// 对活动通道要求六个原始参数；非活动通道只要求 isActive，其他值不参与配置。
AgetChannel ParseAgetChannel(unsigned int id, const std::map<std::string, std::string> &parameters);
struct AgetSettings {
    std::uint16_t chipVersion = 0; // 显式指定 0x201/0x202/0x203；不猜测实机型号。
    bool twoPulseMode = false;
    int readoutDepth = -1;         // -1 使用旧 Module 缺省值：512/256。
    std::map<std::string, std::string> globals;
    std::vector<AgetChannel> channels; // 必须完整声明 0..67，无重复。
};
struct AgetRegisterValue {
    std::string name;
    unsigned int widthBits;
    std::uint64_t value;
    AgetRegisterValue(const std::string &n, unsigned int w, std::uint64_t v) : name(n), widthBits(w), value(v) {}
};
struct AgetRegisterImage {
    std::uint16_t chipVersion = 0;
    bool twoPulseMode = false;
    unsigned int registerStageDepth = 0; // 旧 agetSetRegisters 写 readDepth 的值。
    unsigned int moduleReadoutDepth = 0; // 旧 Configure 后续 Module.readoutDepth 覆盖值。
    std::vector<AgetRegisterValue> registers; // 按旧 Configure 的 13 个寄存器顺序。
    std::array<std::uint32_t, 3> readAlways = {{0, 0, 0}};
    std::array<std::uint32_t, 3> readIfHit = {{0, 0, 0}};
};
// 报错抛出 std::invalid_argument；合法设置保持旧映射，非法值不截断/回退。
AgetRegisterImage CompileAget(const AgetSettings &settings);
int AgetPhysicalChannel(unsigned int channelId); // FPN 返回 -1；越界拒绝。
}
#endif
