#include "coboalarmclient.h"
#include "hardwareaudit.h"
#include "hardwareclient.h"
#include <Ice/Ice.h>
#include <IceUtil/UUID.h>
#include <get/cobo/CtrlNode.h>
#include <chrono>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <stdexcept>
namespace GetDAQ {
namespace {
std::uint64_t Now()
{
    const auto value = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return value < 0 ? 0 : std::uint64_t(value);
}
void Increment(std::uint64_t &value) { if (value != UINT64_MAX) ++value; }
struct CallbackState {
    mutable std::mutex mutex;
    CoBoAlarmOptions options;
    CoBoAlarmSnapshot snapshot;
    std::deque<CoBoAlarmEvent> events;
    std::unique_ptr<HardwareAudit> audit;
    bool Record(const std::string &record) {
        if (snapshot.auditFailed || snapshot.budgetExceeded) return false;
        if (!audit || audit->Bytes() > options.maxEventAuditBytes || record.size()+1 > options.maxEventAuditBytes-audit->Bytes()) {
            snapshot.budgetExceeded = snapshot.stopRequested = true; return false;
        }
        try { audit->Append(record); return true; }
        catch (...) { snapshot.auditFailed = snapshot.stopRequested = true; return false; }
    }
};
class AlarmCallback : public get::mt::AlarmCallback {
    std::shared_ptr<CallbackState> fState;
public:
    explicit AlarmCallback(std::shared_ptr<CallbackState> state) : fState(std::move(state)) {}
    void sendAsadAlarm(const std::string &id, Ice::Long wire, const Ice::Current &) override {
        std::lock_guard<std::mutex> lock(fState->mutex); auto &status = fState->snapshot;
        Increment(status.received); status.stopRequested = true;
        try {
        std::uint64_t bits = 0; static_assert(sizeof(bits) == sizeof(wire),"GET alarm mask width"); std::memcpy(&bits,&wire,sizeof(bits));
        if (id != fState->options.expectedCoBoId || !bits || bits > 15) {
            Increment(status.invalid);
            if (!fState->Record("{\"phase\":\"invalid_alarm\",\"sequence\":" + JsonString(std::to_string(status.received)) +
                ",\"reason\":\"unexpected CoBo ID or nonzero four-bit mask required\"}")) Increment(status.lost);
            return; // 不把远端任意大字符串写入日志；不把非法通知视为健康。
        }
        status.alarmMask |= unsigned(bits);
        CoBoAlarmEvent event; event.sequence = status.received; event.receivedUnixMs = Now(); event.alarmBits = unsigned(bits);
        const bool recorded = fState->Record("{\"phase\":\"alarm\",\"sequence\":" + JsonString(std::to_string(event.sequence)) +
            ",\"received_unix_ms\":" + JsonString(std::to_string(event.receivedUnixMs)) + ",\"alarm_bits\":" + JsonString(std::to_string(bits)) +
            ",\"stop_requested\":true}");
        if (recorded) Increment(status.durable);
        bool lost = !recorded;
        if (fState->events.size() >= fState->options.maxPendingEvents) { status.queueOverflow = true; lost = true; }
        else {
            try { fState->events.push_back(event); }
            catch (...) { status.queueOverflow = true; lost = true; }
        }
        if (lost) Increment(status.lost);
        } catch (...) { status.auditFailed = status.stopRequested = true; Increment(status.lost); }
    }
};
}
class CoBoAlarmClient::Impl {
public:
    std::shared_ptr<CallbackState> state{new CallbackState};
    Ice::CommunicatorPtr communicator;
    Ice::ObjectAdapterPtr adapter;
    get::cobo::CtrlNodePrx node;
    get::mt::AlarmServicePrx service;
    get::mt::AlarmCallbackPrx callback;
    std::string subscriberId, callbackAddress, closedCommandChain;
    std::mutex operationMutex;
    template<class T> T Options(T proxy) {
        Ice::EncodingVersion encoding; encoding.major = 1; encoding.minor = state->options.encodingMinor;
        return T::uncheckedCast(proxy->ice_twoway()->ice_collocationOptimized(false)->ice_timeout(state->options.timeoutMs)->ice_encodingVersion(encoding));
    }
    ~Impl() { if (communicator) { try { communicator->destroy(); } catch (...) {} } } // 不发送unsubscribe/reset。
    void DiscoverService() {
        if (node->name() != state->options.expectedCoBoId) throw std::runtime_error("Alarm node name differs from expected CoBo ID");
        const auto found = node->getAlarmService();
        if (!found) throw std::runtime_error("Hardware node returned null AlarmService");
        // Slice getter返回类型化代理；checkedCast可命中本地代理类型直接返回，
        // 即使先赋给ObjectPrx也不能强制远端检查。显式ice_isA核对实际服务。
        const auto configured = Options(found);
        if (!configured->ice_isA(get::mt::AlarmService::ice_staticId()))
            throw std::runtime_error("Returned object does not implement AlarmService");
        service = configured;
    }
    void Change(bool subscribe, const std::string &path) {
        std::lock_guard<std::mutex> operation(operationMutex);
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->snapshot.subscription != (subscribe ? CoBoAlarmSnapshot::Fresh : CoBoAlarmSnapshot::Active))
                throw std::logic_error("Alarm subscription operation is not allowed in current state; do not replay unknown operations");
        }
        if (subscribe) DiscoverService();
        HardwareAudit command(path);
        command.Append("{\"schema_version\":1,\"phase\":\"intent\",\"operation\":" + JsonString(subscribe ? "subscribe-owned-alarm" : "unsubscribe-owned-alarm") +
            ",\"subscriber_id\":" + JsonString(subscriberId) + ",\"callback_proxy\":" + JsonString(callbackAddress) +
            ",\"service_proxy\":" + JsonString(communicator->proxyToString(service)) + ",\"reset_all_subscribers\":false}");
        command.Append("{\"phase\":\"begin\"}");
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->snapshot.subscription = subscribe ? CoBoAlarmSnapshot::Subscribing : CoBoAlarmSnapshot::Unsubscribing;
        }
        try {
            if (subscribe) service->subscribe(subscriberId,callback); else service->unsubscribe(subscriberId);
            command.Append("{\"phase\":\"acknowledged\",\"callback_delivery_verified\":false}");
        } catch (const std::exception &) {
            { std::lock_guard<std::mutex> lock(state->mutex); state->snapshot.subscription = CoBoAlarmSnapshot::Unknown; state->snapshot.stopRequested = true; }
            try { command.Append("{\"phase\":\"unknown\",\"automatic_retry\":false}"); } catch (...) {}
            throw std::runtime_error("Alarm subscription outcome UNKNOWN; preserve audit, do not retry or reset subscribers");
        }
        std::lock_guard<std::mutex> lock(state->mutex);
        state->snapshot.subscription = subscribe ? CoBoAlarmSnapshot::Active : CoBoAlarmSnapshot::Closed;
        if (!state->Record("{\"phase\":" + JsonString(subscribe ? "subscription_active" : "subscription_closed") + "}"))
            throw std::runtime_error("Subscription acknowledged but event audit failed; stop request remains latched");
        if (!subscribe) closedCommandChain=command.ChainSha256();
    }
};
CoBoAlarmClient::CoBoAlarmClient(const CoBoAlarmOptions &options) : fImpl(new Impl)
{
    const auto bounded = [](const std::string &text, std::size_t maximum, bool empty) {
        return (empty || !text.empty()) && text.size() <= maximum && text.find('\0') == std::string::npos;
    };
    if (!bounded(options.nodeProxy,4096,false) || !bounded(options.expectedCoBoId,128,false) || !bounded(options.callbackEndpoints,4096,false) ||
        !bounded(options.publishedEndpoints,4096,true) || !bounded(options.eventAuditPath,4096,false) || options.encodingMinor < 0 || options.encodingMinor > 1 ||
        options.timeoutMs < 100 || options.timeoutMs > 60000 || options.maxPendingEvents < 1 || options.maxPendingEvents > 4096 ||
        options.maxEventAuditBytes < 65536 || options.maxEventAuditBytes > 64*1024*1024)
        throw std::invalid_argument("Invalid bounded CoBo alarm client options");
    for (unsigned char c : options.expectedCoBoId) if (c < 32 || c == 127) throw std::invalid_argument("Invalid CoBo ID control characters");
    fImpl->state->options = options; fImpl->subscriberId = "getdaq/" + IceUtil::generateUUID();
    fImpl->state->audit.reset(new HardwareAudit(options.eventAuditPath));
    Ice::InitializationData init; init.properties = Ice::createProperties();
    init.properties->setProperty("Ice.RetryIntervals","-1");
    init.properties->setProperty("Ice.Override.ConnectTimeout",std::to_string(options.timeoutMs));
    init.properties->setProperty("Ice.Override.Timeout",std::to_string(options.timeoutMs));
    init.properties->setProperty("Ice.MessageSizeMax","64");
    init.properties->setProperty("Ice.ThreadPool.Server.Size","1"); init.properties->setProperty("Ice.ThreadPool.Server.SizeMax","2");
    init.properties->setProperty("GetDAQAlarm.Endpoints",options.callbackEndpoints);
    if (!options.publishedEndpoints.empty()) init.properties->setProperty("GetDAQAlarm.PublishedEndpoints",options.publishedEndpoints);
    fImpl->communicator = Ice::initialize(init);
    const auto base = fImpl->communicator->stringToProxy(options.nodeProxy);
    if (!base) throw std::invalid_argument("Empty alarm node proxy");
    fImpl->node = get::cobo::CtrlNodePrx::checkedCast(fImpl->Options(base));
    if (!fImpl->node) throw std::runtime_error("Alarm target does not implement CtrlNode");
    fImpl->adapter = fImpl->communicator->createObjectAdapter("GetDAQAlarm");
    Ice::ObjectPtr servant = new AlarmCallback(fImpl->state);
    fImpl->callback = get::mt::AlarmCallbackPrx::uncheckedCast(fImpl->Options(fImpl->adapter->addWithUUID(servant)));
    fImpl->callbackAddress = fImpl->communicator->proxyToString(fImpl->callback);
    if (fImpl->callbackAddress.size() > 8192) throw std::runtime_error("Published callback proxy exceeds 8192 bytes");
    if (!fImpl->state->Record("{\"schema_version\":1,\"phase\":\"listener_prepared\",\"subscriber_id\":" + JsonString(fImpl->subscriberId) +
        ",\"expected_cobo_id\":" + JsonString(options.expectedCoBoId) + ",\"callback_proxy\":" + JsonString(fImpl->callbackAddress) + "}"))
        throw std::runtime_error("Cannot persist prepared alarm listener");
    fImpl->adapter->activate();
}
CoBoAlarmClient::~CoBoAlarmClient() {}
void CoBoAlarmClient::ValidateService() {
    std::lock_guard<std::mutex> operation(fImpl->operationMutex);
    { std::lock_guard<std::mutex> lock(fImpl->state->mutex);
      if (fImpl->state->snapshot.subscription != CoBoAlarmSnapshot::Fresh)
          throw std::logic_error("Alarm service preflight requires Fresh subscription state"); }
    fImpl->DiscoverService();
}
void CoBoAlarmClient::Subscribe(const std::string &path) { fImpl->Change(true,path); }
void CoBoAlarmClient::ValidateUnsubscribe() {
    std::lock_guard<std::mutex> operation(fImpl->operationMutex);
    { std::lock_guard<std::mutex> lock(fImpl->state->mutex);
      if (fImpl->state->snapshot.subscription != CoBoAlarmSnapshot::Active)
          throw std::logic_error("Unsubscribe preflight requires an Active owned subscription"); }
    if (fImpl->node->name() != fImpl->state->options.expectedCoBoId)
        throw std::runtime_error("Owned alarm node name changed before unsubscribe");
    if (!fImpl->service || !fImpl->service->ice_isA(get::mt::AlarmService::ice_staticId()))
        throw std::runtime_error("Owned subscription target no longer implements AlarmService");
}
void CoBoAlarmClient::Unsubscribe(const std::string &path) { fImpl->Change(false,path); }
std::string CoBoAlarmClient::ClosedCommandAuditChainSha256() const {
    std::lock_guard<std::mutex> operation(fImpl->operationMutex);
    std::lock_guard<std::mutex> lock(fImpl->state->mutex);
    if(fImpl->state->snapshot.subscription!=CoBoAlarmSnapshot::Closed || fImpl->closedCommandChain.empty())
        throw std::logic_error("No confirmed owned unsubscribe audit chain");
    return fImpl->closedCommandChain;
}
CoBoAlarmSnapshot CoBoAlarmClient::Snapshot() const {
    std::lock_guard<std::mutex> lock(fImpl->state->mutex); auto snapshot = fImpl->state->snapshot; snapshot.pending = unsigned(fImpl->state->events.size()); return snapshot;
}
std::vector<CoBoAlarmEvent> CoBoAlarmClient::Drain(unsigned int maximum) {
    if (!maximum || maximum > fImpl->state->options.maxPendingEvents) throw std::invalid_argument("Invalid bounded alarm drain count");
    std::lock_guard<std::mutex> lock(fImpl->state->mutex); std::vector<CoBoAlarmEvent> result;
    // 分配先于出队；内存分配失败不能丢弃本次已从队列移出的通知。
    result.reserve(fImpl->state->events.size() < maximum ? fImpl->state->events.size() : maximum);
    while (result.size() < maximum && !fImpl->state->events.empty()) { result.push_back(fImpl->state->events.front()); fImpl->state->events.pop_front(); }
    return result; // 清空队列不清除停止/故障保持标记。
}
std::string CoBoAlarmClient::SubscriberId() const { return fImpl->subscriberId; }
std::string CoBoAlarmClient::CallbackProxy() const { return fImpl->callbackAddress; }
}
