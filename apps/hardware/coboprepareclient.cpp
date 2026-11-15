// CoBoNode::impl_prepare host orchestration adapted from received CEA GET source.
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "coboprepareclient.h"
#include "hardwareaudit.h"
#include <atomic>
#include <stdexcept>
namespace GetDAQ {
namespace {
void Text(const std::string &value, std::size_t maximum, bool emptyAllowed = false)
{
    if ((!emptyAllowed && value.empty()) || value.size() > maximum) throw std::invalid_argument("Invalid bounded CoBo Prepare option");
    for (unsigned char c : value) if (c < 32 || c == 127) throw std::invalid_argument("Control byte in CoBo Prepare option");
}
void Expect(std::vector<HardwareStep> &steps, const char *reg, const char *field, std::uint64_t value)
{
    HardwareStep step; step.kind = HardwareStep::ExpectValue; step.device = "ctrl"; step.reg = reg; step.field = field;
    step.value = value; step.requireBoundDescription = true; steps.push_back(step);
}
}
class CoBoPrepareClient::Impl {
public:
    CoBoPrepareOptions options;
    CoBoPreparePlan plan;
    std::atomic<State> state{Fresh};
    std::unique_ptr<HardwareClient> hardware;
    std::unique_ptr<CoBoAlarmClient> alarms;
    std::unique_ptr<HardwareAudit> audit;
    std::string directory;
    std::atomic<bool> subscribed{false};
    void CheckAlarm(bool requireActive = true) const {
        if (!alarms) return;
        const auto snapshot = alarms->Snapshot();
        if (snapshot.subscription != (requireActive ? CoBoAlarmSnapshot::Active : CoBoAlarmSnapshot::Fresh) || snapshot.stopRequested)
            throw std::runtime_error("CoBo Prepare alarm interlock stopped; hardware Stop/recovery must be coordinated explicitly");
    }
};
CoBoPrepareClient::CoBoPrepareClient(const CoBoPrepareOptions &options) : fImpl(new Impl)
{
    Text(options.nodeProxy,4096); Text(options.expectedNodeName,128);
    Text(options.callbackEndpoints,4096,!options.settings.monitoring.subscribeAlarms); Text(options.publishedEndpoints,4096,true);
    if (options.encodingMinor < 0 || options.encodingMinor > 1 || options.timeoutMs < 100 || options.timeoutMs > 60000 ||
        !options.maxPendingAlarms || options.maxPendingAlarms > 4096 || options.maxAlarmAuditBytes < 65536 || options.maxAlarmAuditBytes > 64*1024*1024)
        throw std::invalid_argument("Invalid CoBo Prepare encoding/timeout/alarm budget");
    ValidateHardwareDescription(options.description);
    fImpl->plan = BuildCoBoPreparePlan(options.settings);
    fImpl->options = options; // 复制本次输入；外部修改不改变已校验的计划或绑定权限。
}
CoBoPrepareClient::~CoBoPrepareClient() {}
CoBoPrepareClient::State CoBoPrepareClient::Status() const { return fImpl->state.load(); }
CoBoAlarmClient *CoBoPrepareClient::Alarms() const
{
    const auto state = Status();
    if (state != Complete && state != Failed && state != Preflighted) throw std::logic_error("Alarm owner is available only after Preflight/Prepare terminates");
    return fImpl->alarms.get();
}
void CoBoPrepareClient::Prepare(const std::string &newAuditDirectory)
{
    if (Status() != Fresh) throw std::logic_error("CoBo Prepare client is single-use; recovery is required before another attempt");
    Preflight(newAuditDirectory);
    Prepare(std::function<void()>());
}
void CoBoPrepareClient::CheckInterlock() const
{
    if (Status() == Failed) throw std::runtime_error("CoBo Prepare failed; explicit hardware Stop/recovery is required");
    fImpl->CheckAlarm(fImpl->subscribed.load());
}
void CoBoPrepareClient::Preflight(const std::string &newAuditDirectory,const std::vector<HardwareStep> &additionalSteps)
{
    State expected = Fresh;
    if (!fImpl->state.compare_exchange_strong(expected,Running)) throw std::logic_error("CoBo Prepare client is single-use; recovery is required before another attempt");
    auto &p = *fImpl; const auto &o = p.options;
    auto &audit = p.audit;
    try {
        if (!o.ownsStoppedHardware || !o.dataProcessesStopped)
            throw std::invalid_argument("CoBo Prepare requires exclusive stopped hardware and stopped data processes declarations");
        CreateHardwareAuditDirectory(newAuditDirectory);
        p.directory = newAuditDirectory;
        audit.reset(new HardwareAudit(newAuditDirectory+"/prepare.jsonl"));
        audit->Append("{\"schema_version\":1,\"phase\":\"intent\",\"operation\":\"cobo-prepare\",\"expected_node_name\":"+JsonString(o.expectedNodeName)+
            ",\"node_proxy\":"+JsonString(o.nodeProxy)+",\"encoding_minor\":"+std::to_string(o.encodingMinor)+",\"timeout_ms\":"+std::to_string(o.timeoutMs)+
            ",\"hardware_policy\":"+HardwarePolicyJson(o.description)+",\"owner_and_stopped_are_declarations\":true,\"subscribe_alarms\":"+(o.settings.monitoring.subscribeAlarms ? "true" : "false")+
            ",\"check_asad_alarms\":"+(o.settings.monitoring.checkAsAdAlarms ? "true" : "false")+",\"adc_channels_reapplied_after_reset\":true,\"full_system_prepared\":false}");
        p.hardware.reset(new HardwareClient(o.nodeProxy,o.encodingMinor,o.timeoutMs));
        p.hardware->UseDescription(o.description);
        if (p.hardware->Name() != o.expectedNodeName) throw std::runtime_error("CoBo Prepare actual node name differs from expected name");
        auto complete = p.plan.beforeSubscription; AppendHardwareSteps(complete,p.plan.afterSubscription);
        p.hardware->Validate(complete); // 包括最后的阈值/延迟/板端检查器权限；先于任何硬件写入。
        if (!additionalSteps.empty()) p.hardware->Validate(additionalSteps);
        if (o.settings.monitoring.subscribeAlarms) {
            CoBoAlarmOptions options; options.nodeProxy = o.nodeProxy; options.expectedCoBoId = o.expectedNodeName;
            options.encodingMinor = o.encodingMinor; options.timeoutMs = o.timeoutMs; options.callbackEndpoints = o.callbackEndpoints;
            options.publishedEndpoints = o.publishedEndpoints; options.maxPendingEvents = o.maxPendingAlarms; options.maxEventAuditBytes = o.maxAlarmAuditBytes;
            options.eventAuditPath = newAuditDirectory+"/alarms.jsonl";
            p.alarms.reset(new CoBoAlarmClient(options)); p.alarms->ValidateService();
        }
        std::vector<HardwareStep> premises;
        Expect(premises,"pipeCtrl","firmwareEnable",0); Expect(premises,"pipeCtrl","intEnable",0);
        if (o.settings.foundation.verifyHardwareVersion) Expect(premises,"hardwareVersion","",o.settings.foundation.hardwareVersion);
        p.hardware->Execute(premises,newAuditDirectory+"/premises.jsonl");
        audit->Append("{\"phase\":\"preflight_complete\",\"register_values_read\":true,\"hardware_changed\":false,\"remote_lock_implemented\":false}");
        p.state = Preflighted;
    } catch (...) {
        if (audit) try { audit->Append("{\"phase\":\"preflight_failed\",\"hardware_changed\":false}"); } catch (...) {}
        p.state = Failed; throw;
    }
}
void CoBoPrepareClient::Prepare(const std::function<void()> &checkpoint)
{
    State expected = Preflighted;
    if (!fImpl->state.compare_exchange_strong(expected,Running)) throw std::logic_error("CoBo Prepare requires its completed Preflight and cannot be replayed");
    auto &p = *fImpl; auto &audit = p.audit; const auto &newAuditDirectory = p.directory;
    try {
        const std::function<void()> check = [&] { p.CheckAlarm(p.subscribed.load()); if (checkpoint) checkpoint(); };
        // 预检查与执行之间可能经过其他板操作；在第一项写入前重新观察停止/板型。
        std::vector<HardwareStep> premises;
        Expect(premises,"pipeCtrl","firmwareEnable",0); Expect(premises,"pipeCtrl","intEnable",0);
        if (p.options.settings.foundation.verifyHardwareVersion) Expect(premises,"hardwareVersion","",p.options.settings.foundation.hardwareVersion);
        p.hardware->Execute(premises,newAuditDirectory+"/execution-premises.jsonl",check);
        p.hardware->Execute(p.plan.beforeSubscription,newAuditDirectory+"/before-subscription.jsonl",check);
        audit->Append("{\"phase\":\"before_subscription_complete\"}");
        if (p.alarms) {
            check();
            p.alarms->Subscribe(newAuditDirectory+"/subscribe.jsonl");
            p.subscribed = true;
            p.CheckAlarm();
        }
        p.hardware->Execute(p.plan.afterSubscription,newAuditDirectory+"/after-subscription.jsonl",check);
        check();
        audit->Append("{\"phase\":\"complete\",\"owned_alarm_listener_retained\":"+std::string(p.alarms ? "true" : "false")+
            ",\"full_system_prepared\":false,\"physical_clock_or_calibration_verified\":false,\"sync_pulse_sent\":false}");
        p.state = Complete;
    } catch (...) {
        if (audit) try { audit->Append("{\"phase\":\"stopped\",\"automatic_retry\":false,\"automatic_unsubscribe\":false,\"hardware_stop_sent\":false,\"full_system_prepared\":false}"); } catch (...) {}
        p.state = Failed; throw;
    }
}
}
