#ifndef GETDAQ_SYSTEMDESCRIPTION_H
#define GETDAQ_SYSTEMDESCRIPTION_H
#include "systemalignment.h"
#include <utility>
namespace GetDAQ {
enum class DescriptionPolicy : unsigned int { UseExisting, CreateEmpty, ReplaceExisting };
struct SystemDescriptionNode {
    enum class Role : unsigned int { CoBo, Mutant };
    Role role = Role::CoBo;
    std::string id, nodeProxy;
    HardwareDescription description; // 最终名称与布局；直接随create发送，省去临时名称/setName。
    DescriptionPolicy policy = DescriptionPolicy::UseExisting;
    HardwareDescription currentDescription; // ReplaceExisting必须显式提供当前布局与名称。
};
struct SystemDescriptionOptions {
    std::vector<SystemDescriptionNode> nodes; // 保留给定拓扑顺序，不按代理字符串重排。
    int encodingMinor = 0, timeoutMs = 3000;
    bool exclusiveOwnerConfirmed = false, dataProcessesStopped = false;
};
// 只复制/检查本地输入；未连接目标。包含拓扑中已声明的停用实验板，它们也必须报告未就绪。
SystemDescriptionOptions BuildSystemDescriptionOptions(const SystemAlignmentOptions &topology, DescriptionPolicy policy);
void ValidateSystemDescriptionOptions(const SystemDescriptionOptions &options);
// 主机Describe的全部节点协调，不提交生产ECC Described。无Qt/原始事件流。
// 上层必须持有实际控制权、串行化所有命令，并保留失败对象/日志直到核对恢复。
class SystemDescriptionClient {
public:
    enum class State : unsigned int { Fresh, Describing, Described, Failed };
    explicit SystemDescriptionClient(const SystemDescriptionOptions &options); // 本地检查，无远端RPC。
    ~SystemDescriptionClient(); // 不隐式destroy/Stop/断开。
    SystemDescriptionClient(const SystemDescriptionClient &) = delete;
    SystemDescriptionClient &operator=(const SystemDescriptionClient &) = delete;
    void Validate(); // 全节点只读元数据/停止观察；只能Fresh，不创建日志或清除状态位。
    void Describe(const std::string &newAuditDirectory);
    // Read-only adoption after a retained Describe failure. All policies must
    // be UseExisting; verifies final metadata and every production/ready bit.
    // Writes a new observation audit, never CREATE/destroy or a register write.
    void VerifyExistingDescription(const std::string &newAuditDirectory);
    std::vector<std::pair<std::string,std::string>> VerificationAuditChains() const;
    State GetState() const;
    SystemDescriptionOptions Configuration() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
