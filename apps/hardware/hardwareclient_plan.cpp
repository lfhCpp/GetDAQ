#include "hardwareclient_p.h"
#include "strictjson.h"
#include "configurationsha256.h"
#include <cstdlib>
#include <fstream>
#include <iterator>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <chrono>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>
namespace GetDAQ {
namespace {
std::vector<HardwareStep> ReviewedReadbackWarnings(const std::vector<HardwareStep> &input,const std::string &proxy,const std::string &node) {
    for(const auto &step:input)if(step.warningOnReadback255)throw std::invalid_argument("Readback warning requires an explicit scoped policy file");
    auto result=input;const char *path=std::getenv("GETDAQ_READBACK_WARNING_POLICY_FILE");if(!path || !*path)return result;
#ifndef _WIN32
    struct stat info;if(::lstat(path,&info) || !S_ISREG(info.st_mode) || info.st_uid!=::geteuid() || (info.st_mode&0077) || info.st_size>65536)
        throw std::invalid_argument("Readback warning policy must be a private owned regular file");
#else
    throw std::invalid_argument("Scoped readback warning policy is currently supported by the Linux service only");
#endif
    std::ifstream file(path,std::ios::binary);if(!file)throw std::invalid_argument("Cannot read accepted warning policy");
    const std::string bytes((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    if(bytes.size()>65536)throw std::invalid_argument("Readback warning policy is too large");
    const auto policy=ParseStrictJson(bytes);
    if(policy.type!=JsonValue::Object || policy.members.size()!=5 || policy.At("schema").type!=JsonValue::Number || policy.At("kind").type!=JsonValue::String || policy.At("accepted_readback").type!=JsonValue::String || policy.At("targets").type!=JsonValue::Array || policy.At("targets").items.size()>195 || policy.At("schema").text!="1" || policy.At("kind").text!="getdaq.read-data-delay-warning" || policy.At("accepted").type!=JsonValue::Boolean || policy.At("accepted").text!="true" || policy.At("accepted_readback").text!="255")
        throw std::invalid_argument("Invalid accepted readback warning policy");
    bool matches=false;for(const auto &target:policy.At("targets").items) {
        if(target.type!=JsonValue::Object || target.members.size()!=2 || target.At("node_proxy").type!=JsonValue::String || target.At("node_name").type!=JsonValue::String)throw std::invalid_argument("Invalid scoped warning target");
        if(target.At("node_proxy").text==proxy && target.At("node_name").text==node)matches=true;
    }
    if(!matches)return result;
    const auto digest=ConfigurationSha256(bytes);
    for(auto &step:result)if(step.kind==HardwareStep::ExpectValue && step.device=="ctrl" && step.reg=="readDataDelay" &&
        step.field.size()==5 && step.field.compare(0,4,"AGET")==0 && step.field[4]>='0' && step.field[4]<='3' && step.value<=255) {
        step.warningOnReadback255=true;step.annotation+=" operator accepted legacy readDataDelay 255 sentinel; value not verified; policy_sha256="+digest;
    }
    return result;
}
}
using namespace HardwareClientInternal;
void HardwareClient::Process(const std::vector<HardwareStep> &inputSteps, const std::string &auditPath, bool execute,
                             const std::function<void()> &checkpoint) {
    std::lock_guard<std::mutex> lock(fImpl->mutex);
    if(execute)fImpl->completedAuditChain.clear();
    const auto steps=ReviewedReadbackWarnings(inputSteps,fImpl->address,fImpl->boundNodeId);
    if (steps.empty() || steps.size() > 4096) throw std::invalid_argument("Plan must contain 1..4096 steps");
    if (execute) fImpl->completedAuditChain.clear();
    ValidatePlanStructure(steps, !fImpl->expected.empty());
    std::map<std::string, Impl::PlanTarget> targets;
    get::cobo::AsAdPulserMgrPrx pulser;
    get::cobo::LedManagerPrx leds;
    get::cobo::AsAdAlarmMonitorPrx alarmMonitor;
    const auto checkTarget = [&](const std::string &device, const std::string &reg, const std::string &field, bool writing) {
        if (!targets.count(device)) {
            Impl::PlanTarget target; target.proxy = fImpl->Device(device);
            target.description = fImpl->Description(target.proxy, device); targets.emplace(device, target);
        }
        return fImpl->Width(targets.at(device).description, reg, field, writing);
    };
    std::vector<int> widths;
    std::string plan = "[";
    int totalDelay = 0;
    unsigned int totalPollReads = 0;
    // 所有元数据和输入先检查；后面的越界或只读错误不能留下前面已执行的写操作。
    for (const auto &step : steps) {
        if (widths.size()) plan += ',';
        std::string context = (step.requireBoundDescription ? ",\"require_bound_description\":true" : "") +
            (step.warningOnReadback255 ? std::string(",\"operator_accepted_255_warning\":true") : std::string()) +
            (step.annotation.empty() ? std::string() : ",\"annotation\":" + JsonString(step.annotation));
        if (step.conditionStep >= 0) {
            const auto conditionMask = step.conditionMask & Mask(widths.at(std::size_t(step.conditionStep)));
            if (!conditionMask || (step.conditionValue & conditionMask) != step.conditionValue)
                throw std::invalid_argument("Invalid condition mask/value for referenced observation");
            context += ",\"condition_step\":" + std::to_string(step.conditionStep) + ",\"condition_mask\":" + JsonString(std::to_string(conditionMask)) +
                ",\"condition_value\":" + JsonString(std::to_string(step.conditionValue));
        }
        if (step.allowedDifference && step.kind != HardwareStep::ExpectValue && step.kind != HardwareStep::ExpectObservedValue)
            throw std::invalid_argument("Allowed difference is only valid on an expected-value step");
        if (step.kind == HardwareStep::ExpectObservedValue) {
            const int width = widths.at(std::size_t(step.observationStep));
            if (!step.device.empty() || !step.reg.empty() || !step.field.empty() || step.delayMs || step.enabled || step.periodMs || step.voltageMv ||
                !(step.mask & Mask(width)) || (step.value & Mask(width)) != step.value || (step.allowedDifference & ~(step.mask & Mask(width))))
                throw std::invalid_argument("Invalid cached expectation parameters");
            widths.push_back(width);
            plan += "{\"operation\":\"expect-observed\",\"observation_step\":" + std::to_string(step.observationStep) +
                ",\"value\":" + JsonString(std::to_string(step.value)) + ",\"mask\":" + JsonString(std::to_string(step.mask & Mask(width))) +
                ",\"allowed_difference\":" + JsonString(std::to_string(step.allowedDifference)) + context + "}";
            if (plan.size() > 4*1024*1024) throw std::invalid_argument("Hardware plan exceeds 4 MiB");
            continue;
        }
        if (step.kind == HardwareStep::LedCommand || step.kind == HardwareStep::AsAdAlarmMonitorCommand) {
            if (fImpl->expected.empty()) throw std::invalid_argument("LED/alarm manager commands require a bound hardware description");
            if (!step.device.empty() || !step.reg.empty() || !step.field.empty() || step.value || step.delayMs ||
                (step.periodMs && (step.kind!=HardwareStep::LedCommand || step.ledOperation!=HardwareStep::PulseOneLed)) || step.voltageMv || step.mask != UINT64_MAX)
                throw std::invalid_argument("Invalid LED/alarm manager parameters");
            const char *operation = "asad-alarm-monitor";
            if (step.kind == HardwareStep::LedCommand) {
                if (step.ledOperation != HardwareStep::SetAllLeds && step.ledOperation != HardwareStep::ModifyLed && step.ledOperation != HardwareStep::BlinkAllLeds && step.ledOperation != HardwareStep::FlipOneLed && step.ledOperation != HardwareStep::FlipAllLeds && step.ledOperation != HardwareStep::PulseOneLed)
                    throw std::invalid_argument("Unknown LED operation");
                if (step.ledType > HardwareStep::LedD || step.ledState > HardwareStep::LedFastPulse || step.asadId > 3 ||
                    ((step.ledOperation == HardwareStep::SetAllLeds || step.ledOperation == HardwareStep::BlinkAllLeds || step.ledOperation == HardwareStep::FlipAllLeds) && (step.asadId || step.ledId || step.ledType != HardwareStep::LedP || step.ledState != HardwareStep::LedOff || step.enabled)) ||
                    (step.ledOperation == HardwareStep::ModifyLed && (step.enabled || step.ledId)) ||
                    (step.ledOperation == HardwareStep::FlipOneLed && (step.enabled || step.ledId > 15 || step.asadId || step.ledType != HardwareStep::LedP || step.ledState != HardwareStep::LedOff || step.periodMs)) ||
                    (step.ledOperation == HardwareStep::PulseOneLed && (step.enabled || step.ledId > 16 || step.asadId || step.ledType != HardwareStep::LedP || step.ledState != HardwareStep::LedOff || step.periodMs>unsigned(std::numeric_limits<int>::max()))) ||
                    (step.ledOperation != HardwareStep::PulseOneLed && step.periodMs)) throw std::invalid_argument("Invalid LED type/state/AsAd arguments");
                checkTarget("ctrl","hardwareVersion","",false); // 管理器间接读取型号，GPIO不在普通设备描述中。
                if (!leds) {
                    leds = get::cobo::LedManagerPrx::checkedCast(fImpl->node);
                    if (!leds) throw std::runtime_error("Hardware node does not implement LedManager");
                    leds = fImpl->Options(leds);
                }
                operation = step.ledOperation == HardwareStep::SetAllLeds ? "led-set-all" : step.ledOperation == HardwareStep::BlinkAllLeds ? "led-blink-all" : step.ledOperation == HardwareStep::FlipOneLed ? "led-flip-one" : step.ledOperation == HardwareStep::FlipAllLeds ? "led-flip-all" : step.ledOperation == HardwareStep::PulseOneLed ? "led-pulse" : "led-modify";
                context += ",\"led_type\":" + std::to_string(step.ledType) + ",\"led_state\":" + std::to_string(step.ledState) + ",\"asad_id\":" + std::to_string(step.asadId) + ",\"led_id\":" + std::to_string(step.ledId);
                if(step.ledOperation==HardwareStep::PulseOneLed)context += ",\"period_ms\":"+std::to_string(step.periodMs);
            } else {
                const int alarmBits = checkTarget("ctrl","asadStatus","alarm",false);
                const int enabledBits = checkTarget("ctrl","asadEnable","",false);
                const int initBits = checkTarget("ctrl","mutantConfig","initDone",true);
                if (alarmBits < 4 || enabledBits < 4 || initBits != 1) throw std::invalid_argument("AsAd alarm watcher register layout is invalid");
                if (!alarmMonitor) {
                    alarmMonitor = get::cobo::AsAdAlarmMonitorPrx::checkedCast(fImpl->node);
                    if (!alarmMonitor) throw std::runtime_error("Hardware node does not implement AsAdAlarmMonitor");
                    alarmMonitor = fImpl->Options(alarmMonitor);
                }
            }
            widths.push_back(0);
            plan += "{\"operation\":" + JsonString(operation) + ",\"enabled\":" + (step.enabled ? "true" : "false") + ",\"board_manager_call\":true" + context + "}";
            if (plan.size() > 4*1024*1024) throw std::invalid_argument("Hardware plan exceeds 4 MiB");
            continue;
        }
        if (step.kind == HardwareStep::Delay) {
            if (step.delayMs < 0 || step.delayMs > 60000 || totalDelay > 300000 - step.delayMs)
                throw std::invalid_argument("Invalid plan delay (maximum total 300 seconds)");
            totalDelay += step.delayMs; widths.push_back(0);
            plan += "{\"operation\":\"delay\",\"milliseconds\":" + std::to_string(step.delayMs) + context + "}";
            continue;
        }
        if (step.kind == HardwareStep::PulserCommand) {
            if (fImpl->expected.empty()) throw std::invalid_argument("Pulser commands require a bound hardware description");
            if (step.voltageMv < -1000 || step.voltageMv > 1000 || !step.device.empty() || !step.reg.empty() || !step.field.empty()
                || step.value || step.delayMs || step.mask != UINT64_MAX)
                throw std::invalid_argument("Invalid pulser command parameters");
            const char *operation = nullptr;
            switch (step.pulserOperation) {
            case HardwareStep::SetDefaultVoltage: operation = "pulser-set-default"; break;
            case HardwareStep::TriggerPulser: operation = "pulser-trigger"; break;
            case HardwareStep::ConfigureExternal: operation = "pulser-configure-external"; break;
            case HardwareStep::ConfigurePeriodic: operation = "pulser-configure-periodic"; break;
            case HardwareStep::ConfigureDouble: operation = "pulser-configure-double"; break;
            case HardwareStep::SetRandom: operation = "pulser-set-random"; break;
            case HardwareStep::StartPeriodic: operation = "pulser-start-periodic"; break;
            case HardwareStep::StopPeriodic: operation = "pulser-stop-periodic"; break;
            default: throw std::invalid_argument("Unknown pulser operation");
            }
            if ((step.pulserOperation == HardwareStep::ConfigurePeriodic ? step.periodMs == 0 : step.periodMs != 0)
                || (step.enabled && step.pulserOperation != HardwareStep::ConfigureExternal && step.pulserOperation != HardwareStep::SetRandom)
                || ((step.pulserOperation == HardwareStep::SetRandom || step.pulserOperation == HardwareStep::StartPeriodic || step.pulserOperation == HardwareStep::StopPeriodic) && step.voltageMv))
                throw std::invalid_argument("Unexpected or missing pulser argument");
            // 管理器会在板端访问这些目标；不得绕过本地只读/结构策略。
            if (step.pulserOperation == HardwareStep::SetDefaultVoltage || step.pulserOperation == HardwareStep::TriggerPulser || step.pulserOperation == HardwareStep::StartPeriodic) {
                checkTarget("ctrl", "triggerMask", "", false); checkTarget("ctrl", "triggerMask", "", true);
                const int highBits = checkTarget("asad", "TCM1", "", true), lowBits = checkTarget("asad", "TCM2", "", true);
                if (highBits < 6 || lowBits < 8) throw std::invalid_argument("AsAd TCM register width is too small for a pulser DAC");
                if (step.pulserOperation == HardwareStep::StartPeriodic) {
                    checkTarget("asad", "TCM0", "Trg", false); checkTarget("asad", "TCM0", "Trg", true);
                }
            }
            if (!pulser) {
                pulser = get::cobo::AsAdPulserMgrPrx::checkedCast(fImpl->node);
                if (!pulser) throw std::runtime_error("Hardware node does not implement AsAdPulserMgr");
                pulser = fImpl->Options(pulser);
            }
            widths.push_back(0);
            plan += "{\"operation\":" + JsonString(operation) + ",\"voltage_mv\":" + JsonString(std::to_string(step.voltageMv)) +
                ",\"period_ms\":" + JsonString(std::to_string(step.periodMs)) + ",\"enabled\":" + (step.enabled ? "true" : "false") +
                ",\"board_manager_call\":true" + context + "}";
            if (plan.size() > 4 * 1024 * 1024) throw std::invalid_argument("Hardware plan exceeds 4 MiB");
            continue;
        }
        if (step.kind != HardwareStep::WriteValue && step.kind != HardwareStep::ExpectValue && step.kind != HardwareStep::ReadValue && step.kind != HardwareStep::PollValue && step.kind != HardwareStep::MaskedWriteValue)
            throw std::invalid_argument("Unknown plan operation");
        if (!targets.count(step.device)) {
            Impl::PlanTarget target; target.proxy = fImpl->Device(step.device);
            target.description = fImpl->Description(target.proxy, step.device); targets.emplace(step.device, target);
        }
        const auto &target = targets.at(step.device);
        const int width = fImpl->Width(target.description, step.reg, step.field, step.kind == HardwareStep::WriteValue || step.kind == HardwareStep::MaskedWriteValue);
        if (step.kind == HardwareStep::MaskedWriteValue && (!(step.mask & Mask(width)) || (step.value & ~(step.mask & Mask(width))) ||
            step.enabled || step.delayMs || step.periodMs || step.voltageMv)) throw std::invalid_argument("Invalid masked update mask/value/parameters");
        if (step.kind != HardwareStep::ReadValue && step.kind != HardwareStep::PollValue && (step.value & Mask(width)) != step.value)
            throw std::invalid_argument("Value exceeds hardware register/field width");
        if (step.kind == HardwareStep::ExpectValue && !(step.mask & Mask(width)))
            throw std::invalid_argument("Expected-value mask has no bits in the target");
        if (step.allowedDifference & ~(step.mask & Mask(width)))
            throw std::invalid_argument("Allowed difference must be within an expected-value mask and target width");
        if (step.kind == HardwareStep::PollValue) {
            if (step.value || step.pollAttempts < 1 || step.pollAttempts > 100 || step.zeroReadRetries < 1 || step.zeroReadRetries > 10 || step.pollDelayMs > 60000 ||
                step.minimum > step.maximum || !(step.mask & Mask(width)) || (step.maximum & (step.mask & Mask(width))) != step.maximum)
                throw std::invalid_argument("Invalid bounded poll parameters or target range");
            const unsigned int reads = step.pollAttempts*step.zeroReadRetries;
            const std::uint64_t delay = std::uint64_t(step.pollAttempts)*step.pollDelayMs;
            if (reads > 4096-totalPollReads || delay > std::uint64_t(300000-totalDelay))
                throw std::invalid_argument("Poll plan exceeds 4096 reads or 300 seconds");
            totalPollReads += reads; totalDelay += int(delay);
            context += ",\"minimum\":" + JsonString(std::to_string(step.minimum)) + ",\"maximum\":" + JsonString(std::to_string(step.maximum)) +
                ",\"attempts\":" + std::to_string(step.pollAttempts) + ",\"zero_read_retries\":" + std::to_string(step.zeroReadRetries) +
                ",\"poll_delay_ms\":" + std::to_string(step.pollDelayMs);
        }
        widths.push_back(width);
        plan += "{\"operation\":" + JsonString(step.kind == HardwareStep::WriteValue ? "write" : step.kind == HardwareStep::MaskedWriteValue ? "masked-update" : step.kind == HardwareStep::ReadValue ? "read" : step.kind == HardwareStep::PollValue ? "poll" : "expect") +
            ",\"device_proxy\":" + JsonString(fImpl->communicator->proxyToString(target.proxy)) +
            ",\"device\":" + JsonString(step.device) + ",\"register\":" + JsonString(step.reg) +
            ",\"field\":" + JsonString(step.field) + ",\"value\":" + JsonString(std::to_string(step.value)) +
            ",\"mask\":" + JsonString(std::to_string(step.mask & Mask(width))) +
            ",\"allowed_difference\":" + JsonString(std::to_string(step.allowedDifference)) + context + "}";
        if (plan.size() > 4 * 1024 * 1024) throw std::invalid_argument("Hardware plan exceeds 4 MiB");
    }
    plan += ']';
    if (plan.size() > 4 * 1024 * 1024) throw std::invalid_argument("Hardware plan exceeds 4 MiB");
    if (!execute) return;
    fImpl->ExecutePlan(steps, auditPath, checkpoint, plan, widths, targets,
                       pulser, leds, alarmMonitor);
}
}
