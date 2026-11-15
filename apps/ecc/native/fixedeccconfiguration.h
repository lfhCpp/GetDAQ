#ifndef GETDAQ_FIXEDECCCONFIGURATION_H
#define GETDAQ_FIXEDECCCONFIGURATION_H
#include "nativeecccoordinator.h"
#include "remotereceivermanager.h"
#include <memory>
namespace GetDAQ {
// 本地固定配置入口：冻结文件字节/身份，不连硬件、不创建ECC或接收器审计根。
class FixedEccConfiguration {
public:
    struct OfflineEvidenceOnly {};
    explicit FixedEccConfiguration(const std::string &configurationFile);
    FixedEccConfiguration(const std::string &configurationFile,OfflineEvidenceOnly);
    ~FixedEccConfiguration();
    FixedEccConfiguration(const FixedEccConfiguration &)=delete;
    FixedEccConfiguration &operator=(const FixedEccConfiguration &)=delete;
    void Check() const; // 观察到文件变化后保持故障，恢复原文件不清门槛。
    bool ControlAccessValidated() const;
    std::string BindingSha256() const;
    NativeEccOptions ControllerOptions() const;
    RemoteReceiverOptions ReceiverOptions() const;
    std::string ServiceId() const;
    std::string ClientId() const;
    std::string BindAddress() const;
    std::string CredentialFile() const;
    unsigned int Port() const;
    bool AlarmStopEnabled() const;
    std::string SelectionJson() const; // 启动时接受的选择记录；缓存，不证明实时输入或硬件加载。
    unsigned int AlarmPollIntervalMs() const;
    bool ReceiverStopEnabled() const;
    unsigned int ReceiverPollIntervalMs() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
