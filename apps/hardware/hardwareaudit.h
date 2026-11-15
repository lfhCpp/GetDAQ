#ifndef GETDAQ_HARDWAREAUDIT_H
#define GETDAQ_HARDWAREAUDIT_H
#include <cstdint>
#include <string>
namespace GetDAQ {
// 全新0700操作目录，随后fsync父目录；已存在路径拒绝，不自动删除失败记录。
void CreateHardwareAuditDirectory(const std::string &path);
// 控制命令与告警共用：全新0600文件、目录同步、完整行写入及逐条fsync。
// 不自行重试硬件动作；调用者负责串行化Append。
class HardwareAudit {
public:
    explicit HardwareAudit(const std::string &path);
    ~HardwareAudit();
    HardwareAudit(const HardwareAudit &) = delete;
    HardwareAudit &operator=(const HardwareAudit &) = delete;
    void Append(const std::string &record);
    std::uint64_t Bytes() const { return fBytes; }
    std::string ChainSha256() const { return fChainSha256; } // In-memory digest of successfully persisted records.
private:
    int fFd = -1;
    std::uint64_t fBytes = 0;
    std::string fChainSha256;
};
}
#endif
