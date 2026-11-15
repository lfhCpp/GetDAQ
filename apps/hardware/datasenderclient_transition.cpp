#include "datasenderclient_p.h"
#include "datasenderclient_validation_p.h"
#include "hardwareaudit.h"
#include "hardwareclient.h"
#include "strictjson.h"
#include <stdexcept>
namespace GetDAQ {
using DataSenderValidation::Text;
namespace {
std::string StateName(DataSenderSnapshot::State s) {
    switch (s) {
    case DataSenderSnapshot::Fresh: return "fresh";
    case DataSenderSnapshot::Connecting: return "connecting";
    case DataSenderSnapshot::Connected: return "connected";
    case DataSenderSnapshot::Starting: return "starting";
    case DataSenderSnapshot::Running: return "running";
    case DataSenderSnapshot::Stopping: return "stopping";
    case DataSenderSnapshot::Stopped: return "stopped";
    case DataSenderSnapshot::Disconnecting: return "disconnecting";
    case DataSenderSnapshot::Disconnected: return "disconnected";
    case DataSenderSnapshot::Unknown: return "unknown";
    }
    throw std::invalid_argument("Invalid data sender state");
}
}
void DataSenderClient::Impl::Change(DataSenderSnapshot::State target,const std::string &path,const std::function<void()> &check) {
    std::lock_guard<std::mutex> operation(operationMutex);
    const auto prior = Status(); using S = DataSenderSnapshot;
    const bool allowed = (target == S::Connected && prior.state == S::Fresh) ||
        (target == S::Running && (prior.state == S::Connected || prior.state == S::Stopped)) ||
        (target == S::Stopped && prior.state == S::Running) ||
        (target == S::Disconnected && (prior.state == S::Connected || prior.state == S::Stopped));
    if (!allowed) throw std::logic_error("Data sender transition rejected; unknown outcomes require explicit recovery");
    if(options.protocol=="ICE" && prior.receiverSession.empty()) throw std::logic_error("ICE sender requires verified receiver session binding before control");
    Text(path,4096); if (check) check(); Validate(); if (check) check();
    const std::string action = target == S::Connected ? "connect" : target == S::Running ? "start" : target == S::Stopped ? "stop" : "disconnect";
    HardwareAudit audit(path);
    const auto iceBinding=options.protocol=="ICE" ? ",\"receiver_session\":"+JsonString(Status().receiverSession)+",\"source_instance_id\":"+JsonString(Status().sourceInstanceId) : "";
    audit.Append("{\"schema_version\":1,\"phase\":\"intent\",\"operation\":"+JsonString(action)+iceBinding+
        ",\"sender_id\":"+JsonString(options.senderId)+",\"control_proxy\":"+JsonString(options.controlProxy)+
        ",\"receiver_address\":"+JsonString(options.receiverAddress)+",\"protocol\":"+JsonString(options.protocol)+",\"circular_buffer_mask\":"+
        std::to_string(options.circularBufferMask)+",\"always_flush\":"+(options.alwaysFlush ? "true" : "false")+
        ",\"encoding_minor\":"+std::to_string(options.encodingMinor)+",\"timeout_ms\":"+std::to_string(options.timeoutMs)+
        ",\"from\":"+JsonString(StateName(prior.state))+",\"target\":"+JsonString(StateName(target))+
        ",\"initial_exclusive_stopped_owner_declared\":true,\"control_lock_verified\":false,\"remote_stopped_verified\":false,\"receiver_sealed\":false}");
    {
        std::lock_guard<std::mutex> lock(statusMutex); snapshot.operation = action;
        snapshot.state = target == S::Connected ? S::Connecting : target == S::Running ? S::Starting : target == S::Stopped ? S::Stopping : S::Disconnecting;
    }
    const auto call = [&](const char *name,const std::function<void()> &send) {
        if (check) check();
        audit.Append("{\"phase\":\"call_begin\",\"call\":"+JsonString(name)+"}");
        // 每条远端动作只执行一次，包含Slice声明idempotent的disconnect。
        { std::lock_guard<std::mutex> lock(statusMutex); ++snapshot.issuedCalls; if (target==S::Running) snapshot.startIssued=true; }
        send();
        { std::lock_guard<std::mutex> lock(statusMutex); ++snapshot.acknowledgedCalls; }
        audit.Append("{\"phase\":\"call_acknowledged\",\"call\":"+JsonString(name)+"}");
    };
    try {
        if (target == S::Connected) {
            if(options.protocol=="ICE") {
                const auto bound=Status();call("connectIceStream",[&] {sourceProxy->connectIceStream(options.receiverAddress,bound.receiverSession,options.senderId,bound.sourceInstanceId);});
                call("configureSourcePolicy",[&] {sourceProxy->configureSourcePolicy(int(options.circularBufferMask),options.alwaysFlush);});
            } else {
                call("connect",[&] { proxy->connect(options.protocol,options.receiverAddress); });
                call("setCircularBuffersEnabled",[&] { proxy->setCircularBuffersEnabled(int(options.circularBufferMask)); });
                call("setAlwaysFlushData",[&] { proxy->setAlwaysFlushData(options.alwaysFlush); });
            }
        } else if (target == S::Running) call("daqStart",[&] { proxy->daqStart(); });
        else if (target == S::Stopped) call("daqStop",[&] { proxy->daqStop(); });
        else if(options.protocol=="ICE") call("closeIceStream",[&] {sourceProxy->closeIceStream(Status().receiverSession);});
        else call("disconnect",[&] { proxy->disconnect(); });
        if(options.protocol=="ICE") {
            const auto info=sourceProxy->describeSource();const auto bound=Status();
            const auto expected=target==S::Connected ? "connected" : target==S::Running ? "running" : target==S::Stopped ? "stopped" : "disconnected";
            if(info.sourceId!=options.senderId || info.instanceId!=bound.sourceInstanceId || info.receiverSession!=bound.receiverSession || info.state!=expected || !info.error.empty() ||
                (target==S::Disconnected && (!info.sourceEndAcknowledged || !info.finishIssued || !info.stopAcknowledged || !info.drainAcknowledged)))
                throw std::runtime_error("ICE source command acknowledgement lacks bound source lifecycle result");
            std::lock_guard<std::mutex> lock(statusMutex);snapshot.sourceEndAcknowledged=info.sourceEndAcknowledged;
        }
        if (check) check();
        audit.Append("{\"phase\":\"complete\",\"state\":"+JsonString(StateName(target))+
            ",\"eof_verified\":false,\"receiver_sealed\":false,\"production_ecc_state_committed\":false}");
        std::lock_guard<std::mutex> lock(statusMutex); snapshot.state = snapshot.lastConfirmedState = target;
        completedCommandChain=audit.ChainSha256();
    } catch (...) {
        const bool sent = Status().issuedCalls != prior.issuedCalls;
        { std::lock_guard<std::mutex> lock(statusMutex); snapshot.state = sent ? S::Unknown : prior.state; snapshot.outcomeUnknown = sent; }
        try { audit.Append("{\"phase\":"+JsonString(sent ? "unknown" : "aborted_before_call")+",\"automatic_retry\":false,\"automatic_cleanup\":false,\"receiver_sealed\":false}"); } catch (...) {}
        if (!sent) throw;
        throw std::runtime_error("Data sender outcome UNKNOWN during "+action+"; preserve audit and reconcile hardware/link/receiver before recovery");
    }
}
} // namespace GetDAQ
