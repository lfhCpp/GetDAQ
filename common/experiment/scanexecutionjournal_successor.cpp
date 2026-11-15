#include "scanexecutionjournal.h"
#include "scanexecutionjournalinternal.h"

#include <algorithm>

namespace GetDAQ {
using namespace ScanExecutionJournalInternal;

bool ScanExecutionJournal::IsImmediateSuccessorOf(
    const ScanExecutionJournal &previous) const
{
    const bool bindingLegacySnapshot = previous.fSourceSnapshotSha256.empty() &&
        !fSourceSnapshotSha256.empty();
    const bool bindingRuntimeIdentity = previous.fRuntimeIdentitySha256.empty() &&
        !fRuntimeIdentitySha256.empty();
    const bool bindingEventCountBaseline = !previous.fHasEventCountBaseline &&
        fHasEventCountBaseline;
    const bool clearingRuntimeIdentity = !previous.fRuntimeIdentitySha256.empty() &&
        fRuntimeIdentitySha256.empty();
    const bool adoptingEcc = fEvents.size() == previous.fEvents.size() + 1 &&
        !fEvents.empty() && fEvents.back().type == Event::OperationIntent &&
        fEvents.back().operation == "adopt-ecc";
    const bool observingUnknownLifecycle = fEvents.size() == previous.fEvents.size() + 1 &&
        !fEvents.empty() && fEvents.back().type == Event::OperationObservedComplete &&
        previous.fSchemaVersion <= 5 && fSchemaVersion == 6;
    const bool preparingManagedServiceShutdown = fEvents.size() == previous.fEvents.size() + 1 &&
        !fEvents.empty() && fEvents.back().type == Event::ManagedServiceShutdownIntent &&
        previous.fSchemaVersion >= 3 && previous.fSchemaVersion <= 8 &&
        fSchemaVersion == std::max(7U, previous.fSchemaVersion);
    const bool retiringManagedService = fEvents.size() == previous.fEvents.size() + 1 &&
        !fEvents.empty() && fEvents.back().type == Event::ProcessRetirement &&
        previous.fSchemaVersion >= 7 && previous.fSchemaVersion <= 8 &&
        fSchemaVersion == previous.fSchemaVersion;
    const bool quarantiningManagedServiceShutdown =
        fEvents.size() == previous.fEvents.size() + 1 && !fEvents.empty() &&
        fEvents.back().type == Event::ManagedServiceShutdownUnknown &&
        previous.fSchemaVersion >= 7 && previous.fSchemaVersion <= 8 &&
        fSchemaVersion == previous.fSchemaVersion;
    const bool preparingManagedServiceStart = fEvents.size() == previous.fEvents.size() + 1 &&
        !fEvents.empty() && fEvents.back().type == Event::ManagedServiceStartIntent &&
        previous.fSchemaVersion <= 7 && fSchemaVersion == 8;
    const bool startingManagedService = fEvents.size() == previous.fEvents.size() + 1 &&
        !fEvents.empty() && fEvents.back().type == Event::ManagedServiceStarted &&
        previous.fSchemaVersion == 8 && fSchemaVersion == 8;
    const bool quarantiningManagedServiceStart = fEvents.size() == previous.fEvents.size() + 1 &&
        !fEvents.empty() && fEvents.back().type == Event::ManagedServiceStartUnknown &&
        previous.fSchemaVersion == 8 && fSchemaVersion == 8;
    if (fExecutionId != previous.fExecutionId ||
        fSourcePlanSha256 != previous.fSourcePlanSha256 ||
        (fSourceSnapshotSha256 != previous.fSourceSnapshotSha256 &&
         !bindingLegacySnapshot) ||
        (fSchemaVersion != previous.fSchemaVersion &&
         !(bindingLegacySnapshot && fSchemaVersion == 3) &&
         !(bindingRuntimeIdentity && fSchemaVersion == 3) &&
         !(bindingEventCountBaseline && previous.fSchemaVersion >= 3 && fSchemaVersion == 4) &&
         !(previous.fSchemaVersion >= 3 && previous.fSchemaVersion <= 4 &&
           fSchemaVersion == 5 && adoptingEcc) &&
         !preparingManagedServiceStart && !startingManagedService &&
         !quarantiningManagedServiceStart &&
         !observingUnknownLifecycle && !preparingManagedServiceShutdown &&
         !retiringManagedService) ||
        PointCount() != previous.PointCount() ||
        fEvents.size() != previous.fEvents.size() + 1)
        return false;
    for (std::size_t i = 0; i < previous.fEvents.size(); ++i) {
        const Event &left = fEvents[i];
        const Event &right = previous.fEvents[i];
        if (left.type != right.type || left.point != right.point ||
            left.from != right.from || left.to != right.to ||
            left.operationId != right.operationId || left.operation != right.operation ||
            left.actorId != right.actorId ||
            left.requestSha256 != right.requestSha256 ||
            left.responseSha256 != right.responseSha256 ||
            left.processId != right.processId || left.exitCode != right.exitCode ||
            left.serviceConfigurationSha256 != right.serviceConfigurationSha256 ||
            left.instanceId != right.instanceId ||
            left.serviceSnapshotSha256 != right.serviceSnapshotSha256 ||
            left.handoffOperationId != right.handoffOperationId ||
            left.executableSha256 != right.executableSha256 ||
            left.runtimeIdentitySha256 != right.runtimeIdentitySha256 ||
            left.operationApplied != right.operationApplied ||
            !SameEventCountBaseline(left.eventCountBaseline, right.eventCountBaseline))
            return false;
    }
    if (bindingLegacySnapshot) {
        const Event &binding = fEvents.back();
        if (binding.type != Event::SnapshotBound ||
            binding.requestSha256 != fSourceSnapshotSha256 ||
            binding.point != previous.PointIndex() ||
            binding.from != previous.State() ||
            (binding.from != ScanPointState::Planned &&
             binding.from != ScanPointState::Reviewed))
            return false;
    }
    if (bindingRuntimeIdentity) {
        const Event &binding = fEvents.back();
        if (binding.type != Event::RuntimeIdentityBound ||
            binding.requestSha256 != fRuntimeIdentitySha256 ||
            binding.point != previous.PointIndex() ||
            binding.from != ScanPointState::EccStarted)
            return false;
    } else if (bindingEventCountBaseline) {
        const Event &binding = fEvents.back();
        if (binding.type != Event::EventCountBaselineBound ||
            binding.requestSha256 != fRuntimeIdentitySha256 ||
            binding.point != previous.PointIndex() ||
            binding.from != ScanPointState::Configured ||
            !SameEventCountBaseline(binding.eventCountBaseline, fEventCountBaseline))
            return false;
    } else if (clearingRuntimeIdentity) {
        const Event &advance = fEvents.back();
        if (advance.type != Event::NextPoint ||
            advance.point != previous.PointIndex() ||
            PointIndex() != previous.PointIndex() + 1)
            return false;
    } else if (fRuntimeIdentitySha256 != previous.fRuntimeIdentitySha256) {
        return false;
    }
    if (retiringManagedService) {
        const Event &retirement = fEvents.back();
        if (previous.State() != ScanPointState::Bound ||
            previous.fEvents.empty() ||
            previous.fEvents.back().type != Event::ManagedServiceShutdownIntent ||
            retirement.point != previous.PointIndex() ||
            retirement.from != ScanPointState::Bound ||
            retirement.to != ScanPointState::Retired ||
            retirement.operationId != previous.fEvents.back().operationId ||
            retirement.processId != previous.fEvents.back().processId ||
            retirement.serviceConfigurationSha256 !=
                previous.fEvents.back().serviceConfigurationSha256 ||
            retirement.instanceId != previous.fEvents.back().instanceId ||
            retirement.serviceSnapshotSha256 !=
                previous.fEvents.back().serviceSnapshotSha256 ||
            retirement.handoffOperationId != previous.fEvents.back().handoffOperationId ||
            retirement.runtimeIdentitySha256 != previous.fRuntimeIdentitySha256 ||
            State() != ScanPointState::Retired)
            return false;
    }
    if (preparingManagedServiceShutdown) {
        const Event &intent = fEvents.back();
        if (previous.State() != ScanPointState::Bound ||
            intent.point != previous.PointIndex() || intent.from != ScanPointState::Bound ||
            intent.runtimeIdentitySha256 != previous.fRuntimeIdentitySha256 ||
            !previous.fHasEventCountBaseline || State() != previous.State() ||
            fSchemaVersion != std::max(7U, previous.fSchemaVersion))
            return false;
    }
    if (quarantiningManagedServiceShutdown) {
        const Event &unknown = fEvents.back();
        if (previous.State() != ScanPointState::Bound || previous.fEvents.empty() ||
            previous.fEvents.back().type != Event::ManagedServiceShutdownIntent ||
            unknown.point != previous.PointIndex() ||
            unknown.operationId != previous.fEvents.back().operationId ||
            unknown.from != ScanPointState::Bound ||
            State() != ScanPointState::NeedsReconciliation)
            return false;
    }
    if (fHasEventCountBaseline != previous.fHasEventCountBaseline) {
        if (previous.fHasEventCountBaseline || !bindingEventCountBaseline) {
            const Event &advance = fEvents.back();
            if (advance.type != Event::NextPoint || fHasEventCountBaseline ||
                PointIndex() != previous.PointIndex() + 1)
                return false;
        }
    } else if (fHasEventCountBaseline &&
               !SameEventCountBaseline(fEventCountBaseline, previous.fEventCountBaseline)) {
        return false;
    }
    return true;
}

} // namespace GetDAQ
