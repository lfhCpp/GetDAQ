#include "cobolifecycle.h"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
void Check(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}
}

int main()
{
    using GetDAQ::CoBoLifecycleState;
    try {
        const CoBoLifecycleState states[] = {CoBoLifecycleState::Fresh, CoBoLifecycleState::Preparing,
            CoBoLifecycleState::Prepared, CoBoLifecycleState::Configuring, CoBoLifecycleState::Configured,
            CoBoLifecycleState::Ready, CoBoLifecycleState::Starting, CoBoLifecycleState::Running,
            CoBoLifecycleState::Paused, CoBoLifecycleState::Resuming, CoBoLifecycleState::Stopping,
            CoBoLifecycleState::Stopped, CoBoLifecycleState::Unknown, CoBoLifecycleState::Disabled};
        const char *names[] = {"fresh", "preparing", "prepared", "configuring", "configured", "ready",
            "starting", "running", "paused", "resuming", "stopping", "stopped", "unknown", "disabled"};
        const unsigned int count = sizeof(states) / sizeof(states[0]);
        for (unsigned int i = 0; i < count; ++i)
            Check(std::string(GetDAQ::CoBoLifecycleStateName(states[i])) == names[i], "lifecycle wire name changed");
        Check(GetDAQ::InitialCoBoLifecycleState(true) == CoBoLifecycleState::Fresh, "active CoBo initial state differs");
        Check(GetDAQ::InitialCoBoLifecycleState(false) == CoBoLifecycleState::Disabled, "disabled CoBo initial state differs");

        bool allowed[14][14] = {};
        for (unsigned int i = 0; i < count; ++i) allowed[i][i] = true;
        const auto allow = [&](CoBoLifecycleState from, CoBoLifecycleState to) {
            unsigned int i = 0, j = 0;
            while (i < count && states[i] != from) ++i;
            while (j < count && states[j] != to) ++j;
            if (i == count || j == count) throw std::runtime_error("invalid test transition");
            allowed[i][j] = true;
        };
        const auto edges = {
            std::pair<CoBoLifecycleState,CoBoLifecycleState>{CoBoLifecycleState::Fresh,CoBoLifecycleState::Preparing},
            {CoBoLifecycleState::Fresh,CoBoLifecycleState::Unknown},
            {CoBoLifecycleState::Preparing,CoBoLifecycleState::Prepared},
            {CoBoLifecycleState::Preparing,CoBoLifecycleState::Configured},
            {CoBoLifecycleState::Preparing,CoBoLifecycleState::Unknown},
            {CoBoLifecycleState::Prepared,CoBoLifecycleState::Preparing},
            {CoBoLifecycleState::Prepared,CoBoLifecycleState::Configuring},
            {CoBoLifecycleState::Prepared,CoBoLifecycleState::Fresh},
            {CoBoLifecycleState::Prepared,CoBoLifecycleState::Unknown},
            {CoBoLifecycleState::Configuring,CoBoLifecycleState::Configured},
            {CoBoLifecycleState::Configuring,CoBoLifecycleState::Unknown},
            {CoBoLifecycleState::Configured,CoBoLifecycleState::Preparing},
            {CoBoLifecycleState::Configured,CoBoLifecycleState::Configuring},
            {CoBoLifecycleState::Configured,CoBoLifecycleState::Prepared},
            {CoBoLifecycleState::Configured,CoBoLifecycleState::Fresh},
            {CoBoLifecycleState::Configured,CoBoLifecycleState::Unknown},
            {CoBoLifecycleState::Configured,CoBoLifecycleState::Ready},
            {CoBoLifecycleState::Configured,CoBoLifecycleState::Stopping},
            {CoBoLifecycleState::Ready,CoBoLifecycleState::Starting},
            {CoBoLifecycleState::Ready,CoBoLifecycleState::Stopping},
            {CoBoLifecycleState::Ready,CoBoLifecycleState::Configured},
            {CoBoLifecycleState::Ready,CoBoLifecycleState::Prepared},
            {CoBoLifecycleState::Ready,CoBoLifecycleState::Unknown},
            {CoBoLifecycleState::Starting,CoBoLifecycleState::Running},
            {CoBoLifecycleState::Starting,CoBoLifecycleState::Stopping},
            {CoBoLifecycleState::Starting,CoBoLifecycleState::Unknown},
            {CoBoLifecycleState::Running,CoBoLifecycleState::Stopping},
            {CoBoLifecycleState::Running,CoBoLifecycleState::Unknown},
            {CoBoLifecycleState::Paused,CoBoLifecycleState::Resuming},
            {CoBoLifecycleState::Paused,CoBoLifecycleState::Stopping},
            {CoBoLifecycleState::Paused,CoBoLifecycleState::Stopped},
            {CoBoLifecycleState::Paused,CoBoLifecycleState::Prepared},
            {CoBoLifecycleState::Paused,CoBoLifecycleState::Unknown},
            {CoBoLifecycleState::Resuming,CoBoLifecycleState::Running},
            {CoBoLifecycleState::Resuming,CoBoLifecycleState::Stopping},
            {CoBoLifecycleState::Resuming,CoBoLifecycleState::Unknown},
            {CoBoLifecycleState::Stopping,CoBoLifecycleState::Paused},
            {CoBoLifecycleState::Stopping,CoBoLifecycleState::Stopped},
            {CoBoLifecycleState::Stopping,CoBoLifecycleState::Unknown},
            {CoBoLifecycleState::Stopped,CoBoLifecycleState::Starting},
            {CoBoLifecycleState::Stopped,CoBoLifecycleState::Stopping},
            {CoBoLifecycleState::Stopped,CoBoLifecycleState::Configured},
            {CoBoLifecycleState::Stopped,CoBoLifecycleState::Prepared},
            {CoBoLifecycleState::Stopped,CoBoLifecycleState::Paused},
            {CoBoLifecycleState::Stopped,CoBoLifecycleState::Unknown},
            {CoBoLifecycleState::Unknown,CoBoLifecycleState::Fresh},
            {CoBoLifecycleState::Unknown,CoBoLifecycleState::Stopping},
            {CoBoLifecycleState::Unknown,CoBoLifecycleState::Stopped},
            {CoBoLifecycleState::Unknown,CoBoLifecycleState::Prepared}
        };
        for (const auto &edge : edges) allow(edge.first,edge.second);
        for (unsigned int from = 0; from < count; ++from)
            for (unsigned int to = 0; to < count; ++to)
                if (GetDAQ::CanTransitionCoBoLifecycle(states[from], states[to]) != allowed[from][to]) {
                    std::cerr << "Transition mismatch " << from << " -> " << to << '\n';
                    throw std::runtime_error("lifecycle transition matrix differs");
                }
        Check(!GetDAQ::CanTransitionCoBoLifecycle(static_cast<CoBoLifecycleState>(99), CoBoLifecycleState::Unknown),
              "invalid lifecycle state was accepted");
        Check(!GetDAQ::CanTransitionCoBoLifecycle(CoBoLifecycleState::Fresh, static_cast<CoBoLifecycleState>(99)),
              "invalid lifecycle destination was accepted");

        GetDAQ::CoBoLifecycleLedger ledger({{"C0",true},{"C1",false}});
        Check(ledger.Size()==2 && ledger.IdAt(0)=="C0" && ledger.IdAt(1)=="C1", "ledger did not retain fixed CoBo ordering");
        Check(ledger.EnabledAt(0) && !ledger.EnabledAt(1) && ledger.StateAt(0)==CoBoLifecycleState::Fresh &&
              ledger.StateAt(1)==CoBoLifecycleState::Disabled, "ledger initial states differ from fixed topology");
        Check(ledger.Transition("C0",0,CoBoLifecycleState::Preparing)==CoBoLifecycleState::Fresh &&
              ledger.StateFor("C0")==CoBoLifecycleState::Preparing, "ledger did not publish a valid transition");
        const auto rejects=[&](const std::function<void()> &action,const char *message) {
            bool rejected=false; try { action(); } catch (const std::exception &) { rejected=true; }
            Check(rejected,message);
        };
        rejects([&]{ledger.Transition("C1",0,CoBoLifecycleState::Preparing);}, "ledger accepted a mismatched fixed ID/index");
        rejects([&]{ledger.Transition("C0",0,CoBoLifecycleState::Running);}, "ledger accepted an illegal transition");
        rejects([&]{ledger.Transition("C1",1,CoBoLifecycleState::Fresh);}, "ledger activated a disabled CoBo");
        rejects([&]{ledger.StateFor("missing");}, "ledger accepted an unknown CoBo ID");
        Check(ledger.Snapshot()==std::vector<CoBoLifecycleState>({CoBoLifecycleState::Preparing,CoBoLifecycleState::Disabled}),
              "rejected ledger transition changed the shared snapshot");
        rejects([&]{ledger.RequireConfiguredForAcquisition();}, "acquisition accepted a non-configured CoBo");
        Check(ledger.Snapshot()==std::vector<CoBoLifecycleState>({CoBoLifecycleState::Preparing,CoBoLifecycleState::Disabled}),
              "acquisition validation mutated the shared lifecycle");
        ledger.Transition("C0",0,CoBoLifecycleState::Configured);
        ledger.RequireConfiguredForAcquisition();

        std::cout << "PASS CoBo lifecycle matrix and fixed-topology shared ledger\n";
        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
