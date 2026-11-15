#include "cobolifecycle.h"

#include <stdexcept>

namespace GetDAQ {
namespace {
bool KnownState(CoBoLifecycleState state)
{
    switch (state) {
    case CoBoLifecycleState::Fresh:
    case CoBoLifecycleState::Preparing:
    case CoBoLifecycleState::Prepared:
    case CoBoLifecycleState::Configuring:
    case CoBoLifecycleState::Configured:
    case CoBoLifecycleState::Ready:
    case CoBoLifecycleState::Starting:
    case CoBoLifecycleState::Running:
    case CoBoLifecycleState::Paused:
    case CoBoLifecycleState::Resuming:
    case CoBoLifecycleState::Stopping:
    case CoBoLifecycleState::Stopped:
    case CoBoLifecycleState::Unknown:
    case CoBoLifecycleState::Disabled:
        return true;
    }
    return false;
}
}

const char *CoBoLifecycleStateName(CoBoLifecycleState state)
{
    switch (state) {
    case CoBoLifecycleState::Fresh: return "fresh";
    case CoBoLifecycleState::Preparing: return "preparing";
    case CoBoLifecycleState::Prepared: return "prepared";
    case CoBoLifecycleState::Configuring: return "configuring";
    case CoBoLifecycleState::Configured: return "configured";
    case CoBoLifecycleState::Ready: return "ready";
    case CoBoLifecycleState::Starting: return "starting";
    case CoBoLifecycleState::Running: return "running";
    case CoBoLifecycleState::Paused: return "paused";
    case CoBoLifecycleState::Resuming: return "resuming";
    case CoBoLifecycleState::Stopping: return "stopping";
    case CoBoLifecycleState::Stopped: return "stopped";
    case CoBoLifecycleState::Unknown: return "unknown";
    case CoBoLifecycleState::Disabled: return "disabled";
    }
    throw std::logic_error("Invalid CoBo lifecycle state");
}

CoBoLifecycleState InitialCoBoLifecycleState(bool enabled)
{
    return enabled ? CoBoLifecycleState::Fresh : CoBoLifecycleState::Disabled;
}

bool CanTransitionCoBoLifecycle(CoBoLifecycleState from, CoBoLifecycleState to)
{
    if (!KnownState(from) || !KnownState(to)) return false;
    if (from == to) return true;

    switch (from) {
    case CoBoLifecycleState::Fresh:
        return to == CoBoLifecycleState::Preparing || to == CoBoLifecycleState::Unknown;
    case CoBoLifecycleState::Preparing:
        // Reprepare can be canceled before its first hardware-changing step;
        // in that case the previous Prepared or Configured state is restored.
        return to == CoBoLifecycleState::Prepared || to == CoBoLifecycleState::Configured ||
               to == CoBoLifecycleState::Unknown;
    case CoBoLifecycleState::Prepared:
        return to == CoBoLifecycleState::Preparing || to == CoBoLifecycleState::Configuring ||
               to == CoBoLifecycleState::Fresh || to == CoBoLifecycleState::Unknown;
    case CoBoLifecycleState::Configuring:
        return to == CoBoLifecycleState::Configured || to == CoBoLifecycleState::Unknown;
    case CoBoLifecycleState::Configured:
        return to == CoBoLifecycleState::Preparing || to == CoBoLifecycleState::Configuring ||
               to == CoBoLifecycleState::Prepared || to == CoBoLifecycleState::Fresh ||
               to == CoBoLifecycleState::Unknown || to == CoBoLifecycleState::Ready ||
               to == CoBoLifecycleState::Stopping;
    case CoBoLifecycleState::Ready:
        return to == CoBoLifecycleState::Starting || to == CoBoLifecycleState::Stopping ||
               to == CoBoLifecycleState::Configured || to == CoBoLifecycleState::Prepared ||
               to == CoBoLifecycleState::Unknown;
    case CoBoLifecycleState::Starting:
        return to == CoBoLifecycleState::Running || to == CoBoLifecycleState::Stopping ||
               to == CoBoLifecycleState::Unknown;
    case CoBoLifecycleState::Running:
        return to == CoBoLifecycleState::Stopping || to == CoBoLifecycleState::Unknown;
    case CoBoLifecycleState::Paused:
        return to == CoBoLifecycleState::Resuming || to == CoBoLifecycleState::Stopping ||
               to == CoBoLifecycleState::Stopped || to == CoBoLifecycleState::Prepared ||
               to == CoBoLifecycleState::Unknown;
    case CoBoLifecycleState::Resuming:
        return to == CoBoLifecycleState::Running || to == CoBoLifecycleState::Stopping ||
               to == CoBoLifecycleState::Unknown;
    case CoBoLifecycleState::Stopping:
        return to == CoBoLifecycleState::Paused || to == CoBoLifecycleState::Stopped ||
               to == CoBoLifecycleState::Unknown;
    case CoBoLifecycleState::Stopped:
        return to == CoBoLifecycleState::Starting || to == CoBoLifecycleState::Stopping ||
               to == CoBoLifecycleState::Configured || to == CoBoLifecycleState::Prepared ||
               to == CoBoLifecycleState::Paused || to == CoBoLifecycleState::Unknown;
    case CoBoLifecycleState::Unknown:
        return to == CoBoLifecycleState::Fresh || to == CoBoLifecycleState::Stopping ||
               to == CoBoLifecycleState::Stopped || to == CoBoLifecycleState::Prepared;
    case CoBoLifecycleState::Disabled:
        return false;
    }
    return false;
}


} // namespace GetDAQ
