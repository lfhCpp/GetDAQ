#include "scanexecutionstate.h"

#include <stdexcept>

namespace GetDAQ {
namespace {

bool IsNext(ScanPointState current, ScanPointState next)
{
    if (next == ScanPointState::NeedsReconciliation)
        return current != ScanPointState::Retired &&
               current != ScanPointState::NeedsReconciliation;
    switch (current) {
    case ScanPointState::Planned: return next == ScanPointState::Reviewed;
    case ScanPointState::Reviewed: return next == ScanPointState::EccStarted;
    case ScanPointState::EccStarted: return next == ScanPointState::Described;
    case ScanPointState::Described: return next == ScanPointState::Prepared;
    case ScanPointState::Prepared: return next == ScanPointState::Configured;
    case ScanPointState::Configured: return next == ScanPointState::RunStarted;
    case ScanPointState::RunStarted: return next == ScanPointState::RunStopped;
    case ScanPointState::RunStopped: return next == ScanPointState::ReceiverFinished;
    case ScanPointState::ReceiverFinished: return next == ScanPointState::CatalogVerified;
    case ScanPointState::CatalogVerified: return next == ScanPointState::Bound;
    // Retirement is not an ordinary state transition: it needs evidence
    // spanning ECC, receiver/catalog and control-lease lifecycles.
    case ScanPointState::Bound: return false;
    case ScanPointState::Retired:
    case ScanPointState::NeedsReconciliation: return false;
    }
    return false;
}

bool Fail(std::string *error, const char *message)
{
    if (error) *error = message;
    return false;
}

}

ScanExecutionState::ScanExecutionState(unsigned int pointCount)
    : fPointCount(pointCount), fPointIndex(0), fState(ScanPointState::Planned)
{
    if (!pointCount || pointCount > 256)
        throw std::invalid_argument("Scan execution point count must be 1..256");
}

bool ScanExecutionState::Transition(ScanPointState expected, ScanPointState next,
                                    std::string *error)
{
    if (fState != expected)
        return Fail(error, "Scan point state changed since it was observed");
    if (!IsNext(fState, next))
        return Fail(error, "Illegal scan point state transition");
    fState = next;
    if (error) error->clear();
    return true;
}

bool ScanExecutionState::BeginNextPoint(std::string *error)
{
    if (fState != ScanPointState::Retired)
        return Fail(error, "Current scan point must be retired before advancing");
    if (fPointIndex + 1 >= fPointCount)
        return Fail(error, "No scan point remains");
    ++fPointIndex;
    fState = ScanPointState::Planned;
    if (error) error->clear();
    return true;
}

bool ScanExecutionState::ReplayLegacyRetirement(std::string *error)
{
    if (fState != ScanPointState::Bound)
        return Fail(error, "Legacy retirement history must follow a Bound point");
    fState = ScanPointState::Retired;
    if (error) error->clear();
    return true;
}

bool ScanExecutionState::RetireAfterManagedProcessExit(std::string *error)
{
    if (fState != ScanPointState::Bound)
        return Fail(error, "Managed service exit retirement requires a Bound point");
    fState = ScanPointState::Retired;
    if (error) error->clear();
    return true;
}

bool ScanExecutionState::ResolveNeedsReconciliation(ScanPointState resolved,
                                                    std::string *error)
{
    if (fState != ScanPointState::NeedsReconciliation)
        return Fail(error, "Scan point is not awaiting reconciliation");
    if (resolved == ScanPointState::NeedsReconciliation)
        return Fail(error, "Reconciliation must resolve to a concrete lifecycle state");
    fState = resolved;
    if (error) error->clear();
    return true;
}

const char *ScanPointStateName(ScanPointState state)
{
    switch (state) {
    case ScanPointState::Planned: return "Planned";
    case ScanPointState::Reviewed: return "Reviewed";
    case ScanPointState::EccStarted: return "ECCStarted";
    case ScanPointState::Described: return "Described";
    case ScanPointState::Prepared: return "Prepared";
    case ScanPointState::Configured: return "Configured";
    case ScanPointState::RunStarted: return "RunStarted";
    case ScanPointState::RunStopped: return "RunStopped";
    case ScanPointState::ReceiverFinished: return "ReceiverFinished";
    case ScanPointState::CatalogVerified: return "CatalogVerified";
    case ScanPointState::Bound: return "Bound";
    case ScanPointState::Retired: return "Retired";
    case ScanPointState::NeedsReconciliation: return "NeedsReconciliation";
    }
    return "Invalid";
}

}
