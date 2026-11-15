#ifndef GETDAQ_SYSTEMALIGNMENT_H
#define GETDAQ_SYSTEMALIGNMENT_H
#include "hardwareclient.h"
#include "../../common/experiment/alignmentconfiguration.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace GetDAQ {
struct AlignmentCoBoEndpoint {
    std::string id, expectedNodeName, nodeProxy, linkProxy;
    HardwareDescription description;
    bool enabled = true;
};
struct AlignmentMutantEndpoint {
    std::string id, expectedNodeName, nodeProxy, alignmentProxy;
    HardwareDescription description;
    bool master = false;
};
struct SystemAlignmentOptions {
    std::vector<AlignmentCoBoEndpoint> cobos;
    std::vector<AlignmentMutantEndpoint> mutants;
    int encodingMinor = 0, timeoutMs = 3000;
    // 由上层实际控制器提供；声明本身不实现锁、停发送器或停止接收进程。
    bool exclusiveOwnerConfirmed = false, daqStoppedConfirmed = false;
};
struct InterCrateAlignmentResult {
    std::string mutantId;
    AlignmentClock clock = AlignmentClock::Gmc;
    unsigned int step = 1;
    std::int32_t measuredDelay = 0, programmedDelay = 0;
    std::string comment;
};
struct SystemAlignmentResult {
    std::vector<LocatedCoBo> located;
    std::vector<AlignmentCrateResult> crates; // 保存机箱内对齐后的掩码，不能被后段RPC输出覆盖。
    std::vector<InterCrateAlignmentResult> interCrate;
};
void ValidateSystemAlignmentOptions(const SystemAlignmentOptions &options);
// 相对描述路径以拓扑文件所在目录为基准；只读取/校验，不连接硬件。
SystemAlignmentOptions LoadSystemAlignmentOptions(const std::string &path,bool allowStandalone = false);
// 使用已绑定的原始字节与描述读取器，不重新打开拓扑文件。
SystemAlignmentOptions DecodeSystemAlignmentOptions(const std::string &bytes,
    const std::function<HardwareDescription(const std::string &)> &descriptionReader,bool allowStandalone=false);
class SystemAlignmentClient {
public:
    enum class State : unsigned int { Fresh, Running, Complete, Failed };
    explicit SystemAlignmentClient(const SystemAlignmentOptions &options);
    ~SystemAlignmentClient();
    SystemAlignmentClient(const SystemAlignmentClient &) = delete;
    SystemAlignmentClient &operator=(const SystemAlignmentClient &) = delete;
    // 全新0700日志目录；每次RPC前持久化意图，失败停止，不补偿或重新发送。
    SystemAlignmentResult Align(const std::string &newAuditDirectory);
    // 全系统MuTaNT Prepare之前的接口/描述/名称预检查；不要求已经设置主从源。
    // 不读状态寄存器、不调用对齐/链路写入；失败后对象不可重用。
    void ValidateInterfaces();
    State GetState() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
