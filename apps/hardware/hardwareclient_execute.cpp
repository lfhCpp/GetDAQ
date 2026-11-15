#include "hardwareclient_p.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace GetDAQ {
using namespace HardwareClientInternal;

void HardwareClient::Impl::ExecutePlan(
    const std::vector<HardwareStep> &steps, const std::string &auditPath,
    const std::function<void()> &checkpoint, const std::string &plan,
    const std::vector<int> &widths,
    const std::map<std::string, PlanTarget> &targets,
    get::cobo::AsAdPulserMgrPrx pulser,
    get::cobo::LedManagerPrx leds,
    get::cobo::AsAdAlarmMonitorPrx alarmMonitor)
{
    Audit audit(auditPath);
    const auto started = std::chrono::system_clock::now().time_since_epoch();
    audit.Append("{\"schema_version\":1,\"phase\":\"intent\",\"node\":" + JsonString(this->address) +
        ",\"started_unix_ms\":" + JsonString(std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(started).count())) +
        ",\"encoding\":\"1." + std::to_string(this->minor) + "\",\"steps\":" + plan + "}");
    try {
        std::vector<std::uint64_t> observations(steps.size());
        const auto check = [&](std::size_t position) {
            if (!checkpoint) return;
            try { checkpoint(); }
            catch (...) {
                // 联锁失败不表示上一条RPC未知，也不能冒充硬件Stop已完成。
                try { audit.Append("{\"step\":"+std::to_string(position)+",\"phase\":\"interlock_stopped\",\"hardware_stop_sent\":false}"); } catch (...) {}
                throw;
            }
        };
        const auto wait = [&](unsigned int milliseconds, std::size_t position) {
            if (!checkpoint) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); return; }
            const auto deadline = std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);
            for (;;) {
                check(position);
                const auto remaining = deadline-std::chrono::steady_clock::now();
                if (remaining <= std::chrono::steady_clock::duration::zero()) break;
                // 不缩短旧时序；仅把主机等待分段，以便及时消费本地告警保持位。
                const auto chunk = std::min(std::chrono::duration_cast<std::chrono::milliseconds>(remaining)+std::chrono::milliseconds(1),std::chrono::milliseconds(25));
                std::this_thread::sleep_for(chunk);
            }
        };
        for (std::size_t i = 0; i < steps.size(); ++i) {
            check(i);
            const auto &step = steps[i];
            const std::string index = "\"step\":" + std::to_string(i);
            if (step.conditionStep >= 0 && (observations.at(std::size_t(step.conditionStep)) & step.conditionMask) != step.conditionValue) {
                audit.Append("{" + index + ",\"phase\":\"skipped\",\"condition_step\":" + std::to_string(step.conditionStep) +
                    ",\"condition_actual\":" + JsonString(std::to_string(observations.at(std::size_t(step.conditionStep)))) + "}");
                continue;
            }
            // begin 成功落盘以后才执行；若中断，仅凭 begin 不能判断硬件有没有生效。
            audit.Append("{" + index + ",\"phase\":\"begin\"}");
            check(i);
            if (step.kind == HardwareStep::Delay) {
                wait(unsigned(step.delayMs),i);
                audit.Append("{" + index + ",\"phase\":\"completed\"}");
                continue;
            }
            if (step.kind == HardwareStep::PulserCommand) {
                try {
                    switch (step.pulserOperation) {
                    case HardwareStep::SetDefaultVoltage: pulser->setDefaultPulserVoltage(step.voltageMv); break;
                    case HardwareStep::TriggerPulser: pulser->triggerPulser(step.voltageMv); break;
                    case HardwareStep::ConfigureExternal: pulser->configureExternalPulser(step.enabled, step.voltageMv); break;
                    case HardwareStep::ConfigurePeriodic: pulser->configurePeriodicPulser(step.voltageMv, step.periodMs); break;
                    case HardwareStep::ConfigureDouble: pulser->configureDoublePulserMode(step.voltageMv); break;
                    case HardwareStep::SetRandom: pulser->setRandomPulserEnabled(step.enabled); break;
                    case HardwareStep::StartPeriodic: pulser->startPeriodicPulser(); break;
                    case HardwareStep::StopPeriodic: pulser->stopPeriodicPulser(); break;
                    }
                } catch (const std::exception &e) {
                    const auto why = Message(e);
                    try { audit.Append("{" + index + ",\"phase\":\"unknown\",\"error\":" + JsonString(why) + "}"); } catch (...) {}
                    throw std::runtime_error("Pulser command outcome UNKNOWN at step " + std::to_string(i) + "; do not retry automatically: " + why);
                }
                try { audit.Append("{" + index + ",\"phase\":\"acknowledged\",\"readback_verified\":false}"); }
                catch (const std::exception &e) { throw std::runtime_error(std::string("Pulser acknowledged command, but audit completion failed; do not repeat command: ") + e.what()); }
                continue;
            }
            if (step.kind == HardwareStep::LedCommand || step.kind == HardwareStep::AsAdAlarmMonitorCommand) {
                try {
                    if (step.kind == HardwareStep::AsAdAlarmMonitorCommand) alarmMonitor->setAsAdAlarmMonitoringEnabled(step.enabled);
                    else if (step.ledOperation == HardwareStep::SetAllLeds) leds->setLEDs(step.enabled);
                    else if (step.ledOperation == HardwareStep::BlinkAllLeds) leds->blinkLEDs();
                    else if (step.ledOperation == HardwareStep::FlipOneLed) leds->flipLED(Ice::Int(step.ledId));
                    else if (step.ledOperation == HardwareStep::FlipAllLeds) leds->flipLEDs();
                    else if (step.ledOperation == HardwareStep::PulseOneLed) leds->pulseLED(Ice::Int(step.ledId),Ice::Int(step.periodMs));
                    else leds->modifyLED(static_cast<get::cobo::LedType>(step.ledType),int(step.asadId),static_cast<get::cobo::LedState>(step.ledState));
                } catch (const std::exception &e) {
                    const auto why = Message(e);
                    try { audit.Append("{" + index + ",\"phase\":\"unknown\",\"error\":" + JsonString(why) + "}"); } catch (...) {}
                    throw std::runtime_error("LED/alarm manager outcome UNKNOWN at step " + std::to_string(i) + "; do not retry automatically: " + why);
                }
                try { audit.Append("{" + index + ",\"phase\":\"acknowledged\",\"readback_verified\":false}"); }
                catch (const std::exception &e) { throw std::runtime_error(std::string("Manager acknowledged command, but audit completion failed; do not repeat command: ") + e.what()); }
                continue;
            }
            if (step.kind == HardwareStep::ExpectObservedValue) {
                const auto actual = observations.at(std::size_t(step.observationStep));
                const std::uint64_t difference = (actual ^ step.value) & step.mask;
                const bool matches = (difference & ~step.allowedDifference) == 0;
                const bool warning = matches && difference != 0;
                audit.Append("{" + index + ",\"phase\":" + JsonString(warning ? "matched_with_warning" : matches ? "matched" : "mismatch") +
                    ",\"observation_step\":" + std::to_string(step.observationStep) + ",\"actual\":" + JsonString(std::to_string(actual)) +
                    ",\"difference\":" + JsonString(std::to_string(difference)) + "}");
                if (!matches) throw std::runtime_error("Hardware cached expectation mismatch; prior writes are not rolled back");
                continue;
            }
            const auto target = targets.at(step.device).proxy;
            if (step.kind == HardwareStep::MaskedWriteValue) {
                // 有明确读改写语义的普通配置目标；非跨客户端原子操作，不用于命令寄存器。
                const auto read = [&] { return Bits(step.field.empty() ? target->readRegister(step.reg) : target->readField(step.reg,step.field)) & Mask(widths[i]); };
                std::uint64_t previous = 0;
                try { previous = read(); }
                catch (const std::exception &e) { throw std::runtime_error("Masked update stopped before write after read failure: " + Message(e)); }
                const auto mask = step.mask & Mask(widths[i]);
                const auto value = (previous & ~mask) | step.value;
                audit.Append("{" + index + ",\"phase\":\"observed\",\"previous\":" + JsonString(std::to_string(previous)) +
                    ",\"effective_write_value\":" + JsonString(std::to_string(value)) + "}");
                audit.Append("{" + index + ",\"phase\":\"write_begin\"}");
                check(i); // 读改写读取期间发生告警时，不继续写入。
                try {
                    if (step.field.empty()) target->writeRegister(step.reg,Wire(value)); else target->writeField(step.reg,step.field,Wire(value));
                } catch (const std::exception &e) {
                    try { audit.Append("{" + index + ",\"phase\":\"unknown\",\"error\":" + JsonString(Message(e)) + "}"); } catch (...) {}
                    throw std::runtime_error("Masked write outcome UNKNOWN; do not retry automatically: " + Message(e));
                }
                try { audit.Append("{" + index + ",\"phase\":\"acknowledged\",\"readback_verified\":false}"); }
                catch (const std::exception &e) { throw std::runtime_error("Masked write acknowledged, but audit completion failed; do not repeat command: " + Message(e)); }
                std::uint64_t actual = 0;
                check(i);
                try { actual = read(); }
                catch (const std::exception &e) { throw std::runtime_error("Masked write readback failed; prior writes are not rolled back or retried: " + Message(e)); }
                const bool matches = actual == value;
                audit.Append("{" + index + ",\"phase\":" + JsonString(matches ? "matched" : "mismatch") + ",\"actual\":" + JsonString(std::to_string(actual)) + "}");
                if (!matches) throw std::runtime_error("Masked write readback mismatch; prior writes are not rolled back");
                continue;
            }
            if (step.kind == HardwareStep::PollValue) {
                bool matches = false;
                for (unsigned int attempt = 0; attempt < step.pollAttempts; ++attempt) {
                    std::uint64_t actual = 0;
                    for (unsigned int sample = 0; sample < step.zeroReadRetries; ++sample) {
                        const std::string position = index + ",\"attempt\":" + std::to_string(attempt) + ",\"sample\":" + std::to_string(sample);
                        audit.Append("{" + position + ",\"phase\":\"poll_begin\"}");
                        check(i);
                        try { actual = Bits(step.field.empty() ? target->readRegister(step.reg) : target->readField(step.reg,step.field)) & Mask(widths[i]); }
                        catch (const std::exception &e) {
                            try { audit.Append("{" + position + ",\"phase\":\"read_failed\",\"error\":" + JsonString(Message(e)) + "}"); } catch (...) {}
                            throw std::runtime_error("Hardware poll stopped after read error; prior writes are not rolled back: " + Message(e));
                        }
                        audit.Append("{" + position + ",\"phase\":\"observed\",\"actual\":" + JsonString(std::to_string(actual)) + "}");
                        if (actual != 0) break;
                    }
                    const auto value = actual & step.mask;
                    matches = value >= step.minimum && value <= step.maximum;
                    audit.Append("{" + index + ",\"phase\":" + JsonString(matches ? "matched" : "outside_range") +
                        ",\"actual\":" + JsonString(std::to_string(actual)) + "}");
                    if (matches) break;
                    wait(step.pollDelayMs,i);
                }
                if (!matches) throw std::runtime_error("Hardware poll range not reached; prior writes are not rolled back");
                continue;
            }
            if (step.kind == HardwareStep::ExpectValue || step.kind == HardwareStep::ReadValue) {
                std::uint64_t actual = 0;
                try { actual = Bits(step.field.empty() ? target->readRegister(step.reg) : target->readField(step.reg, step.field)) & Mask(widths[i]); }
                catch (const std::exception &e) {
                    try { audit.Append("{" + index + ",\"phase\":\"read_failed\",\"error\":" + JsonString(Message(e)) + "}"); } catch (...) {}
                    throw std::runtime_error("Hardware plan stopped after read error; prior writes are not rolled back: " + Message(e));
                }
                if (step.kind == HardwareStep::ReadValue) {
                    observations[i] = actual;
                    audit.Append("{" + index + ",\"phase\":\"observed\",\"actual\":" + JsonString(std::to_string(actual)) + "}");
                    continue;
                }
                const std::uint64_t difference = (actual ^ step.value) & step.mask;
                const bool acceptedWarning=step.warningOnReadback255 && widths[i]==8 && actual==255 && difference!=0;
                const bool matches = (difference & ~step.allowedDifference) == 0 || acceptedWarning;
                const bool warning = matches && difference != 0;
                audit.Append("{" + index + ",\"phase\":" + JsonString(acceptedWarning ? "accepted_warning_unverified" : warning ? "matched_with_warning" : matches ? "matched" : "mismatch") +
                    ",\"actual\":" + JsonString(std::to_string(actual)) + ",\"difference\":" + JsonString(std::to_string(difference)) + (acceptedWarning ? ",\"operator_accepted\":true,\"readback_verified\":false" : "") + "}");
                if (!matches) throw std::runtime_error("Hardware plan expectation mismatch; prior writes are not rolled back");
                continue;
            }
            try {
                if (step.field.empty()) target->writeRegister(step.reg, Wire(step.value));
                else target->writeField(step.reg, step.field, Wire(step.value));
            } catch (const std::exception &e) {
                const std::string why = Message(e);
                try { audit.Append("{" + index + ",\"phase\":\"unknown\",\"error\":" + JsonString(why) + "}"); } catch (...) {}
                throw std::runtime_error("Hardware write outcome UNKNOWN at step " + std::to_string(i) + "; do not retry automatically: " + why);
            }
            try { audit.Append("{" + index + ",\"phase\":\"acknowledged\",\"readback_verified\":false}"); }
            catch (const std::exception &e) { throw std::runtime_error(std::string("Hardware acknowledged write, but audit completion failed; do not repeat write: ") + e.what()); }
        }
        check(steps.size()); // 最后一条应答期间收到的告警不能被标成complete。
        audit.Append("{\"phase\":\"complete\",\"automatic_readback\":false}");
        this->completedAuditChain=audit.ChainSha256();
    } catch (const std::exception &e) {
        throw std::runtime_error(std::string("Hardware plan stopped; preserve audit and do not replay prior steps: ") + e.what());
    }
}

} // namespace GetDAQ
