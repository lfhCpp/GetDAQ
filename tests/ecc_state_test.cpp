#include "eccstatemachine.h"
#include <future>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace GetDAQ;
namespace {
void Require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
template<class F> void Reject(F f) {
    try { f(); } catch (...) { return; }
    throw std::runtime_error("Expected rejection");
}
class Backend : public EccBackend {
public:
    int validated = 0, applied = 0, failure = 0;
    std::promise<void> *entered = nullptr;
    std::shared_future<void> release;
    EccStateMachine *machine = nullptr;
    void Validate(EccCommand, EccState, EccState) override {
        ++validated;
        if (failure == 1) throw std::runtime_error("Configuration missing");
    }
    void Apply(EccCommand command, EccState from, EccState) override {
        ++applied;
        if (machine) {
            const auto status = machine->Status();
            Require(status.state == from && status.transition == static_cast<int>(command), "Committed state too early");
        }
        if (entered) { entered->set_value(); release.wait(); }
        if (failure == 2) throw std::runtime_error("Reply lost after hardware write");
        if (failure == 3) throw 17;
    }
};
}
int main() {
    try {
        // 独立枚举协议矩阵，包含非法数值、Off、重复命令及不同来源的 Reset/Breakup。
        const int expected[7][9] = {
            {-1,-1,-1,-1,-1,-1,-1,-1,-1},
            { 2,-1,-1,-1,-1,-1,-1,-1,-1},
            {-1, 3,-1,-1,-1,-1,-1,-1, 1},
            {-1,-1, 4,-1,-1,-1,-1,-1, 2},
            {-1,-1,-1, 5,-1,-1,-1, 3,-1},
            {-1,-1,-1,-1, 4, 6,-1, 3,-1},
            {-1,-1,-1,-1, 4,-1, 5, 3,-1}
        };
        for (int state = 0; state <= 6; ++state) for (int command = 1; command <= 9; ++command) {
            if (expected[state][command - 1] < 0)
                Reject([&]() { EccStateMachine::Target(static_cast<EccState>(state), static_cast<EccCommand>(command)); });
            else Require(static_cast<int>(EccStateMachine::Target(static_cast<EccState>(state), static_cast<EccCommand>(command))) ==
                         expected[state][command - 1], "Transition matrix mismatch");
        }
        Reject([]() { EccStateMachine::Target(EccState::Idle, static_cast<EccCommand>(0)); });
        for(int state=0;state<=6;++state) {
            if(state==3)Require(EccStateMachine::Target(EccState::Prepared,EccCommand::ReprepareCoBo)==EccState::Prepared,"Maintenance must retain Prepared");
            else Reject([&] {EccStateMachine::Target(static_cast<EccState>(state),EccCommand::ReprepareCoBo);});
            if(state==3)Require(EccStateMachine::Target(EccState::Prepared,EccCommand::ConfigureCoBo)==EccState::Prepared,"Single-CoBo Configure must retain system Prepared");
            else Reject([&] {EccStateMachine::Target(static_cast<EccState>(state),EccCommand::ConfigureCoBo);});
            if(state==3)Require(EccStateMachine::Target(EccState::Prepared,EccCommand::SetCoBoLed)==EccState::Prepared,"LED operation must retain Prepared");
            else Reject([&] {EccStateMachine::Target(static_cast<EccState>(state),EccCommand::SetCoBoLed);});
            if(state==3)Require(EccStateMachine::Target(EccState::Prepared,EccCommand::BlinkCoBoLeds)==EccState::Prepared,"LED blink must retain Prepared");
            else Reject([&] {EccStateMachine::Target(static_cast<EccState>(state),EccCommand::BlinkCoBoLeds);});
            if(state==3)Require(EccStateMachine::Target(EccState::Prepared,EccCommand::FlipCoBoLed)==EccState::Prepared,"LED flip must retain Prepared");
            else Reject([&] {EccStateMachine::Target(static_cast<EccState>(state),EccCommand::FlipCoBoLed);});
            if(state==3)Require(EccStateMachine::Target(EccState::Prepared,EccCommand::PulseCoBoLed)==EccState::Prepared,"LED pulse must retain Prepared");
            else Reject([&] {EccStateMachine::Target(static_cast<EccState>(state),EccCommand::PulseCoBoLed);});
            if(state==3)Require(EccStateMachine::Target(EccState::Prepared,EccCommand::TriggerAsAdPulser)==EccState::Prepared,"Pulser maintenance must retain Prepared");
            else Reject([&] {EccStateMachine::Target(static_cast<EccState>(state),EccCommand::TriggerAsAdPulser);});
            Reject([&] {EccStateMachine::Target(static_cast<EccState>(state),static_cast<EccCommand>(10));});
        }
        Reject([]() { EccStateMachine::Target(static_cast<EccState>(100), EccCommand::Describe); });
        Backend backend; EccStateMachine machine(backend); backend.machine = &machine;
        Reject([&]() { machine.Execute(EccCommand::Start); });
        Require(backend.validated == 0 && backend.applied == 0, "Illegal transition reached backend");
        for (auto command : {EccCommand::Describe, EccCommand::Prepare, EccCommand::Configure, EccCommand::Start,
                             EccCommand::Pause, EccCommand::Resume, EccCommand::Stop, EccCommand::Start,
                             EccCommand::Pause, EccCommand::Stop, EccCommand::Breakup, EccCommand::Reset, EccCommand::Reset})
            machine.Execute(command);
        Require(machine.Status().state == EccState::Idle && backend.applied == 13, "Full cycle failed");
        backend.failure = 1;
        Reject([&]() { machine.Execute(EccCommand::Describe); });
        Require(machine.Status().error == 1 && !machine.Status().outcomeUnknown && backend.applied == 13, "Preflight failure altered hardware/state");
        backend.failure = 0; machine.Execute(EccCommand::Describe);
        Require(machine.Status().error == 0 && machine.Status().message.empty(), "Preflight error did not clear on success");
        // Apply 异常（含非 std::exception）后禁止 Reset 或重发。
        for (int failure : {2, 3}) {
            Backend uncertain; uncertain.failure = failure; EccStateMachine stopped(uncertain);
            Reject([&]() { stopped.Execute(EccCommand::Describe); });
            const auto status = stopped.Status();
            Require(status.state == EccState::Idle && status.transition == 0 && status.error == 1 && status.outcomeUnknown,
                    "Unknown result reported as confirmed state");
            Reject([&]() { stopped.Execute(EccCommand::Describe); });
            Reject([&]() { stopped.Execute(EccCommand::Reset); });
            Require(uncertain.applied == 1, "Unknown operation replayed");
        }
        // 用同步门而不是时间假设验证并发；后端慢调用期间状态查询不被锁住。
        Backend blocking; std::promise<void> entered, release;
        blocking.entered = &entered; blocking.release = release.get_future().share();
        EccStateMachine concurrent(blocking); blocking.machine = &concurrent;
        auto running = std::async(std::launch::async, [&]() { concurrent.Execute(EccCommand::Describe); });
        entered.get_future().wait();
        const auto busy = concurrent.Status();
        bool rejected = false;
        try { concurrent.Execute(EccCommand::Describe); } catch (...) { rejected = true; }
        release.set_value(); running.get();
        Require(busy.state == EccState::Idle && busy.transition == 1 && rejected && blocking.applied == 1, "Concurrent transition was queued or committed early");
        std::cout << "PASS ECC state matrix, full cycle, preflight, unknown-result fence, concurrent status/command\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
