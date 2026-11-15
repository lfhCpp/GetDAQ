#ifndef GETDAQ_SUPERVISORCONTROLHTTP_H
#define GETDAQ_SUPERVISORCONTROLHTTP_H
#include "controlprotocol.h"
#include "credential.h"
#include <cstddef>
namespace GetDAQ {
struct SupervisorHttpOptions {
    std::string address,credentialFile;
    unsigned int port=0,timeoutMs=2000;
};
struct SupervisorHttpResponse {
    unsigned int code=0;
    std::string body;
};
// 原生低频客户端，无Qt/curl。每次调用只连接/发送一次，不重试POST。
// 传输失败并不证明服务未执行；调用方必须保留操作ID，通过只读查询核对。
class NativeControlHttpClient {
public:
    explicit NativeControlHttpClient(const SupervisorHttpOptions &options);
    SupervisorHttpResponse Identity() const;
    SupervisorHttpResponse Status() const;
    SupervisorHttpResponse ControlHandoff() const; // Original live ECC completed receipt; read-only.
    SupervisorHttpResponse PlanRetirement() const; // Original data service complete normal plan retirement.
    SupervisorHttpResponse PlanRetirementCompletion() const; // Explicit completed metadata repair; never submits it.
    SupervisorHttpResponse ControlHandoffCompletion() const; // Explicitly completed original live control handoff; read-only.
    SupervisorHttpResponse Predecessor() const; // Activated data-plan predecessor, bounded metadata only.
    SupervisorHttpResponse Receivers() const; // 自有子进程即时观察，不触发数据或硬件操作。
    SupervisorHttpResponse ReceiverEvents(std::size_t sourceIndex,bool fileBytes=false) const; // One selected receiver's checked count; metadata only.
    SupervisorHttpResponse Sources() const; // Bounded read-only source state.
    SupervisorHttpResponse HardwareState() const; // Fixed low-frequency hardware bits through ECC only.
    SupervisorHttpResponse ReceiverFailure() const; // ECC cached failure policy; no source/receiver action.
    SupervisorHttpResponse Ownership() const; // ECC cached host reservation and explicit scope.
    SupervisorHttpResponse ConfigureReadbacks() const; // Historical Configure checks; never reads hardware.
    SupervisorHttpResponse CoBoMaintenanceTargets() const; // Fixed active IDs/names; no hardware read.
    SupervisorHttpResponse Alarms() const; // ECC可选缓存端点；不发硬件或原始数据请求。
    SupervisorHttpResponse Operation(const std::string &id) const;
    SupervisorHttpResponse CatalogReplacementObservation(const std::string &publishOperationId) const;
    SupervisorHttpResponse PostOperation(const std::string &requestJson) const;
    SupervisorHttpResponse ShutdownEccService(const std::string &instanceId,const std::string &snapshotSha256,
                                               const std::string &handoffOperationId) const; // Requires complete fenced handoff.
private:
    SupervisorHttpOptions fOptions;
    SupervisorCredential fCredential;
    SupervisorHttpResponse Request(const std::string &method,const std::string &path,const std::string &body) const;
};
// 数据服务的类型化入口继续使用相同的有界、单次发送传输。
class SupervisorHttpClient : public NativeControlHttpClient {
public:
    explicit SupervisorHttpClient(const SupervisorHttpOptions &options) : NativeControlHttpClient(options) {}
    SupervisorHttpResponse Submit(const SupervisorControlRequest &request) const;
};
}
#endif
