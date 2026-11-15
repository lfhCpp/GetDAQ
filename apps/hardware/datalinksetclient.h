#ifndef GETDAQ_DATALINKSETCLIENT_H
#define GETDAQ_DATALINKSETCLIENT_H
#include "datasenderclient.h"
namespace GetDAQ {
// 一组独立sender→receiver连接。先检查整个集合，再连接第一块板。
// 不自动Start/Stop：CoBo流水线及MuTaNT主从次序由完整采集协调器负责。
class DataLinkSetClient {
public:
    enum class State : unsigned int { Fresh, Connecting, Connected, Disconnecting, Disconnected, Failed };
    explicit DataLinkSetClient(const std::vector<DataSenderOptions> &options);
    ~DataLinkSetClient(); // 没有远端清理。
    DataLinkSetClient(const DataLinkSetClient &) = delete;
    DataLinkSetClient &operator=(const DataLinkSetClient &) = delete;
    void Connect(const std::string &newAuditDirectory,const std::function<void()> &checkpoint = {});
    void BindIceReceiverSessions(const std::vector<IceReceiverSessionBinding> &bindings); // Validate full set before any local binding.
    void Disconnect(); // 全部本地Connected/Stopped且全部远端类型先复查；部分失败不自动补偿。
    State GetState() const;
    DataSenderClient &Sender(const std::string &id) const; // 生命周期不能超出本集合。
    std::vector<std::shared_ptr<DataSenderClient>> ObservationClients() const; // Immutable handles; safe across parent Reset.
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
