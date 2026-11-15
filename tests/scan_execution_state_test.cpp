#include "scanexecutionstate.h"
#include "scanexecutionjournal.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void Require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

template <typename F>
void Rejects(F action)
{
    try { action(); }
    catch (const std::exception &) { return; }
    throw std::runtime_error("Invalid scan execution accepted");
}

std::string Replace(std::string value, const std::string &from,
                    const std::string &to)
{
    const std::size_t offset = value.find(from);
    Require(offset != std::string::npos, "Journal mutation target not found");
    value.replace(offset, from.size(), to);
    return value;
}

void AdvanceToBound(GetDAQ::ScanExecutionState &execution)
{
    using GetDAQ::ScanPointState;
    const ScanPointState states[] = {
        ScanPointState::Reviewed, ScanPointState::EccStarted,
        ScanPointState::Described, ScanPointState::Prepared,
        ScanPointState::Configured, ScanPointState::RunStarted,
        ScanPointState::RunStopped, ScanPointState::ReceiverFinished,
        ScanPointState::CatalogVerified, ScanPointState::Bound
    };
    for (ScanPointState next : states) {
        const ScanPointState current = execution.State();
        Require(execution.Transition(current, next), "Valid lifecycle transition rejected");
    }
}

GetDAQ::ScanExecutionJournal CompleteJournalPoint(
    GetDAQ::ScanExecutionJournal journal)
{
    using GetDAQ::ScanPointState;
    if (journal.State() == ScanPointState::Planned)
        journal = journal.WithTransition(ScanPointState::Planned, ScanPointState::Reviewed);
    const struct Step { ScanPointState from, to; const char *operation; } steps[] = {
        {ScanPointState::Reviewed, ScanPointState::EccStarted, "start-ecc"},
        {ScanPointState::EccStarted, ScanPointState::Described, "describe"},
        {ScanPointState::Described, ScanPointState::Prepared, "prepare"},
        {ScanPointState::Prepared, ScanPointState::Configured, "configure"},
        {ScanPointState::Configured, ScanPointState::RunStarted, "run-start"},
        {ScanPointState::RunStarted, ScanPointState::RunStopped, "run-stop"},
        {ScanPointState::RunStopped, ScanPointState::ReceiverFinished, "receiver-finish"},
        {ScanPointState::ReceiverFinished, ScanPointState::CatalogVerified, "catalog-verify"},
        {ScanPointState::CatalogVerified, ScanPointState::Bound, "bind"}
    };
    unsigned int sequence = 0;
    for (const Step &step : steps) {
        if (journal.State() != step.from) continue;
        const std::string id = "op-" + std::to_string(journal.PointIndex()) + "-" +
                               std::to_string(sequence++);
        journal = journal.WithOperationIntent(id, step.operation, std::string(64, 'c'));
        journal = journal.WithOperationComplete(id, std::string(64, 'd'), step.to);
        if (std::string(step.operation) == "start-ecc")
            journal = journal.WithRuntimeIdentityBinding(std::string(64, 'e'));
    }
    return journal;
}
}

int main()
{
    try {
        using namespace GetDAQ;
        Rejects([] { ScanExecutionState zero(0); });
        Rejects([] { ScanExecutionState tooMany(257); });

        ScanExecutionState execution(2);
        std::string error;
        Require(!execution.Transition(ScanPointState::Planned, ScanPointState::Configured, &error) &&
                !error.empty() && execution.State() == ScanPointState::Planned,
                "Skipped lifecycle steps changed state");
        Require(!execution.Transition(ScanPointState::Reviewed, ScanPointState::EccStarted, &error),
                "Stale observed state was accepted");
        Require(!execution.BeginNextPoint(&error) && execution.PointIndex() == 0,
                "Advanced before retiring current point");

        AdvanceToBound(execution);
        Require(execution.State() == ScanPointState::Bound,
                "Ordinary transitions advanced beyond the evidence-backed boundary");
        Require(!execution.Transition(ScanPointState::Bound, ScanPointState::Retired, &error) &&
                execution.State() == ScanPointState::Bound,
                "In-memory state API accepted evidence-free retirement");
        Require(!execution.BeginNextPoint(&error) && execution.PointIndex() == 0,
                "Advanced a point without retirement evidence");

        ScanExecutionState unknown(1);
        Require(unknown.Transition(ScanPointState::Planned,
                                   ScanPointState::NeedsReconciliation),
                "Could not quarantine unknown result");
        Require(!unknown.Transition(ScanPointState::NeedsReconciliation,
                                   ScanPointState::Reviewed) &&
                !unknown.BeginNextPoint(&error),
                "Unknown result was automatically resumed");
        Require(std::string(ScanPointStateName(ScanPointState::NeedsReconciliation)) ==
                "NeedsReconciliation", "Stable state name changed");

        Rejects([] { ScanExecutionJournal invalid("../unsafe", std::string(64, 'a'), 1); });
        Rejects([] { ScanExecutionJournal invalid("scan-1", "not-a-digest", 1); });
        const ScanExecutionJournal initial("scan-1", std::string(64, 'a'),
                                           std::string(64, 'f'), 2);
        const auto legacyV2 = ScanExecutionJournal::Decode(Replace(
            initial.Encode(), "\"schema_version\":3", "\"schema_version\":2"));
        Require(legacyV2.SourceSnapshotSha256() == std::string(64, 'f') &&
                legacyV2.Encode().find("\"schema_version\":2") != std::string::npos,
                "Version-2 journal compatibility was not preserved");
        const ScanExecutionJournal reviewed = initial.WithTransition(
            ScanPointState::Planned, ScanPointState::Reviewed);
        Require(initial.State() == ScanPointState::Planned && initial.EventCount() == 1 &&
                initial.SourceSnapshotSha256() == std::string(64, 'f') &&
                reviewed.State() == ScanPointState::Reviewed && reviewed.EventCount() == 2,
                "Journal preview mutated the original record");
        Rejects([&] { initial.WithTransition(ScanPointState::Planned,
                                             ScanPointState::Configured); });
        ScanExecutionJournal journal = CompleteJournalPoint(initial);
        Require(journal.State() == ScanPointState::Bound,
                "Completed binding should leave the point awaiting explicit retirement evidence");
        Rejects([&] { journal.WithOperationIntent("event-retire", "retire",
                                                  std::string(64, 'c')); });
        Rejects([&] { journal.WithNextPoint(); });
        const std::string encoded = journal.Encode();
        const ScanExecutionJournal restored = ScanExecutionJournal::Decode(encoded);
        Require(restored.ExecutionId() == "scan-1" && restored.SourcePlanSha256() ==
                std::string(64, 'a') && restored.PointCount() == 2 &&
                restored.PointIndex() == 0 && restored.State() == ScanPointState::Bound &&
                restored.EventCount() == journal.EventCount(),
                "Journal encode/decode lost its identity or lifecycle state");
        Rejects([&] { ScanExecutionJournal::Decode(
            Replace(encoded, "\"to\":\"Reviewed\"", "\"to\":\"Configured\"")); });
        Rejects([&] { ScanExecutionJournal::Decode(
            Replace(encoded, "\"point_count\":2", "\"point_count\":257")); });
        Rejects([&] { ScanExecutionJournal::Decode(
            Replace(encoded, "\"schema_version\":2", "\"schema_version\":2,\"unexpected\":true")); });

        // Old journals remain readable, but an unresolved legacy retirement
        // cannot be promoted by the generic human-reconciliation API.
        std::string legacyRetirement = CompleteJournalPoint(initial).Encode();
        const std::size_t legacyRetirementEnd = legacyRetirement.rfind("]}");
        Require(legacyRetirementEnd != std::string::npos,
                "Bound journal fixture has no event list");
        legacyRetirement.insert(legacyRetirementEnd,
            ",{\"type\":\"operation-intent\",\"point_index\":0,\"state\":\"Bound\","
            "\"operation_id\":\"legacy-retire\",\"operation\":\"retire\",\"request_sha256\":\"" +
            std::string(64, 'c') + "\"},{\"type\":\"operation-unknown\",\"point_index\":0,"
            "\"state\":\"Bound\",\"operation_id\":\"legacy-retire\",\"reason_sha256\":\"" +
            std::string(64, 'd') + "\"}");
        const auto reopenedLegacyRetirement = ScanExecutionJournal::Decode(legacyRetirement);
        Require(reopenedLegacyRetirement.State() == ScanPointState::NeedsReconciliation &&
                reopenedLegacyRetirement.PendingOperationUnknown(),
                "Legacy pending retirement history was not preserved as unknown");
        Rejects([&] { reopenedLegacyRetirement.WithOperationReconciled(
            "legacy-retire", "operator-1", ScanOperationDisposition::ConfirmedApplied,
            std::string(64, 'e')); });
        const auto unresolvedLegacyRetirement = reopenedLegacyRetirement.WithOperationReconciled(
            "legacy-retire", "operator-1", ScanOperationDisposition::ConfirmedNotSent,
            std::string(64, 'e'));
        Require(unresolvedLegacyRetirement.State() == ScanPointState::Bound,
                "Confirmed-not-sent legacy retirement did not remain at Bound");

        std::string completedLegacyRetirement = CompleteJournalPoint(initial).Encode();
        const std::size_t completedLegacyEnd = completedLegacyRetirement.rfind("]}");
        Require(completedLegacyEnd != std::string::npos,
                "Completed legacy retirement fixture has no event list");
        completedLegacyRetirement.insert(completedLegacyEnd,
            ",{\"type\":\"operation-intent\",\"point_index\":0,\"state\":\"Bound\","
            "\"operation_id\":\"old-retire\",\"operation\":\"retire\",\"request_sha256\":\"" +
            std::string(64, 'c') + "\"},{\"type\":\"operation-complete\",\"point_index\":0,"
            "\"operation_id\":\"old-retire\",\"from\":\"Bound\",\"to\":\"Retired\","
            "\"response_sha256\":\"" + std::string(64, 'd') + "\"}");
        Require(ScanExecutionJournal::Decode(completedLegacyRetirement).State() ==
                ScanPointState::Retired,
                "A previously completed retirement record became unreadable");
        Rejects([&] { ScanExecutionJournal::Decode(completedLegacyRetirement).WithNextPoint(); });

        std::string reconciledLegacyRetirement = CompleteJournalPoint(initial).Encode();
        const std::size_t reconciledLegacyEnd = reconciledLegacyRetirement.rfind("]}");
        Require(reconciledLegacyEnd != std::string::npos,
                "Reconciled legacy retirement fixture has no event list");
        reconciledLegacyRetirement.insert(reconciledLegacyEnd,
            ",{\"type\":\"operation-intent\",\"point_index\":0,\"state\":\"Bound\","
            "\"operation_id\":\"old-unknown-retire\",\"operation\":\"retire\",\"request_sha256\":\"" +
            std::string(64, 'c') + "\"},{\"type\":\"operation-unknown\",\"point_index\":0,"
            "\"state\":\"Bound\",\"operation_id\":\"old-unknown-retire\",\"reason_sha256\":\"" +
            std::string(64, 'd') + "\"},{\"type\":\"operation-reconciled\",\"point_index\":0,"
            "\"from\":\"Bound\",\"to\":\"Retired\",\"operation_id\":\"old-unknown-retire\","
            "\"operator_id\":\"operator-1\",\"disposition\":\"confirmed-applied\","
            "\"evidence_sha256\":\"" + std::string(64, 'e') + "\"}");
        const auto restoredReconciledLegacyRetirement =
            ScanExecutionJournal::Decode(reconciledLegacyRetirement);
        Require(restoredReconciledLegacyRetirement.State() == ScanPointState::Retired,
                "A previously reconciled retirement record became unreadable");
        Rejects([&] { restoredReconciledLegacyRetirement.WithNextPoint(); });

        const ScanExecutionJournal legacy("scan-legacy", std::string(64, 'a'), 1);
        Rejects([&] { legacy.WithOperationIntent(
            "legacy-op", "start-ecc", std::string(64, 'c')); });
        const auto legacyReviewed = legacy.WithTransition(ScanPointState::Planned,
                                                            ScanPointState::Reviewed);
        const auto migrated = legacyReviewed.WithSnapshotBinding(std::string(64, 'e'));
        const auto restoredMigrated = ScanExecutionJournal::Decode(migrated.Encode());
        Require(restoredMigrated.SourceSnapshotSha256() == std::string(64, 'e') &&
                restoredMigrated.State() == ScanPointState::Reviewed &&
                migrated.IsImmediateSuccessorOf(legacyReviewed),
                "Legacy review journal did not safely migrate to a bound snapshot");
        Rejects([&] { restoredMigrated.WithSnapshotBinding(std::string(64, 'd')); });

        // Existing v1 operation history remains readable for recovery, but it
        // must never be retroactively attributed to a newly supplied snapshot.
        const std::string legacyOperationJournal =
            "{\"schema_version\":1,\"kind\":\"getdaq.scan-execution-journal\","
            "\"execution_id\":\"scan-legacy-operation\",\"plan_sha256\":\"" +
            std::string(64, 'a') + "\",\"point_count\":1,\"events\":["
            "{\"type\":\"transition\",\"point_index\":0,\"from\":\"Planned\",\"to\":\"Reviewed\"},"
            "{\"type\":\"operation-intent\",\"point_index\":0,\"state\":\"Reviewed\","
            "\"operation_id\":\"legacy-start\",\"operation\":\"start-ecc\",\"request_sha256\":\"" +
            std::string(64, 'c') + "\"},"
            "{\"type\":\"operation-complete\",\"point_index\":0,\"operation_id\":\"legacy-start\","
            "\"from\":\"Reviewed\",\"to\":\"ECCStarted\",\"response_sha256\":\"" +
            std::string(64, 'd') + "\"}]}";
        const auto legacyWithOperation = ScanExecutionJournal::Decode(legacyOperationJournal);
        Require(legacyWithOperation.State() == ScanPointState::EccStarted &&
                legacyWithOperation.SourceSnapshotSha256().empty(),
                "Legacy operation history was not preserved for recovery");
        Rejects([&] { legacyWithOperation.WithSnapshotBinding(std::string(64, 'e')); });

        const auto operationReady = restoredMigrated.WithOperationIntent(
            "migrated-op", "start-ecc", std::string(64, 'c'));
        Rejects([&] { ScanExecutionJournal::Decode(Replace(
            operationReady.Encode(), std::string(64, 'e'), std::string(64, 'd'))); });
        const auto reviewedJournal = initial.WithTransition(ScanPointState::Planned,
                                                             ScanPointState::Reviewed);
        Rejects([&] { reviewedJournal.WithOperationIntent(
            "wrong-step", "configure", std::string(64, 'c')); });
        const auto intent = reviewedJournal.WithOperationIntent(
            "op-1", "start-ecc", std::string(64, 'c'));
        Require(intent.State() == ScanPointState::Reviewed && intent.HasPendingOperation() &&
                intent.PendingOperationId() == "op-1" && !intent.PendingOperationUnknown(),
                "Operation intent changed state before its result");
        Rejects([&] { intent.WithOperationIntent("op-2", "start-ecc", std::string(64, 'c')); });
        Rejects([&] { intent.WithNextPoint(); });
        Rejects([&] { intent.WithOperationComplete("op-1", std::string(64, 'd'),
                                                   ScanPointState::Described); });
        const auto reopenedIntent = ScanExecutionJournal::Decode(intent.Encode());
        Require(reopenedIntent.State() == ScanPointState::NeedsReconciliation &&
                reopenedIntent.HasPendingOperation() && reopenedIntent.PendingOperationUnknown(),
                "Restart treated an uncompleted durable intent as unsent");
        Rejects([&] { reopenedIntent.WithOperationComplete("op-1", std::string(64, 'd'),
                                                          ScanPointState::EccStarted); });
        const auto completedIntent = intent.WithOperationComplete(
            "op-1", std::string(64, 'd'), ScanPointState::EccStarted);
        const auto reopenedComplete = ScanExecutionJournal::Decode(completedIntent.Encode());
        Require(reopenedComplete.State() == ScanPointState::EccStarted &&
                !reopenedComplete.HasPendingOperation(),
                "Completed operation was not replayed into its confirmed state");
        Rejects([&] { completedIntent.WithOperationIntent(
            "describe-before-bind", "describe", std::string(64, 'c')); });
        const auto runtimeBound = completedIntent.WithRuntimeIdentityBinding(
            std::string(64, '9'));
        const auto runtimeReopened = ScanExecutionJournal::Decode(runtimeBound.Encode());
        Require(runtimeReopened.RuntimeIdentitySha256() == std::string(64, '9') &&
                runtimeBound.Encode().find("\"schema_version\":3") != std::string::npos,
                "Runtime identity binding did not persist in journal schema 3");
        Rejects([&] { runtimeBound.WithRuntimeIdentityBinding(std::string(64, '8')); });
        const auto describeIntent = runtimeReopened.WithOperationIntent(
            "describe-after-bind", "describe", std::string(64, 'b'));
        Require(ScanExecutionJournal::Decode(describeIntent.Encode()).HasPendingOperation(),
                "Describe intent was not allowed after runtime identity binding");

        ScanExecutionJournal eventJournal("event-scan", std::string(64, 'a'),
            std::string(64, 'b'), 2);
        eventJournal = eventJournal.WithTransition(ScanPointState::Planned,
                                                  ScanPointState::Reviewed);
        eventJournal = eventJournal.WithOperationIntent("event-ecc", "start-ecc",
                                                        std::string(64, 'c'));
        eventJournal = eventJournal.WithOperationComplete("event-ecc", std::string(64, 'd'),
                                                          ScanPointState::EccStarted);
        eventJournal = eventJournal.WithRuntimeIdentityBinding(std::string(64, 'e'));
        const auto advanceEventOperation = [](ScanExecutionJournal journal,
                                              const std::string &id,
                                              const std::string &operation,
                                              ScanPointState next) {
            journal = journal.WithOperationIntent(id, operation, std::string(64, 'f'));
            return journal.WithOperationComplete(id, std::string(64, '1'), next);
        };
        eventJournal = advanceEventOperation(eventJournal, "event-describe", "describe",
                                             ScanPointState::Described);
        eventJournal = advanceEventOperation(eventJournal, "event-prepare", "prepare",
                                             ScanPointState::Prepared);
        eventJournal = advanceEventOperation(eventJournal, "event-configure", "configure",
                                             ScanPointState::Configured);
        ScanEventCountBaseline baseline;
        baseline.instanceId = std::string(64, '2');
        baseline.snapshotSha256 = std::string(64, '3');
        baseline.dataBindingSha256 = std::string(64, '4');
        baseline.agentId = "agent-1";
        baseline.planSha256 = std::string(64, '5');
        baseline.receiverInstanceId = std::string(64, '6');
        baseline.sourceId = "C0";
        baseline.sessionId = std::string(64, '7');
        baseline.pid = 1234;
        baseline.available = true;
        baseline.acquisitionEvents = UINT64_C(9007199254740993);
        baseline.observedAtUnixMs = UINT64_C(18446744073709550000);
        const ScanExecutionJournal beforeBaseline = eventJournal;
        eventJournal = eventJournal.WithEventCountBaseline(baseline);
        Require(eventJournal.IsImmediateSuccessorOf(beforeBaseline) &&
                eventJournal.Encode().find("\"schema_version\":4") != std::string::npos,
                "Event-count baseline was not a valid durable journal successor");
        const ScanExecutionJournal reopenedBaseline =
            ScanExecutionJournal::Decode(eventJournal.Encode());
        Require(reopenedBaseline.HasEventCountBaseline() &&
                reopenedBaseline.EventCountBaseline().acquisitionEvents == baseline.acquisitionEvents &&
                reopenedBaseline.EventCountBaseline().observedAtUnixMs == baseline.observedAtUnixMs &&
                reopenedBaseline.EventCountBaseline().sessionId == baseline.sessionId,
                "Journal baseline lost uint64 precision or receiver identity");
        Rejects([&] { reopenedBaseline.WithEventCountBaseline(baseline); });
        Rejects([&] {
            ScanEventCountBaseline unavailable = baseline;
            unavailable.available = false;
            beforeBaseline.WithEventCountBaseline(unavailable);
        });
        const auto runStart = reopenedBaseline.WithOperationIntent("event-run-start",
            "run-start", std::string(64, '8'));
        const auto reopenedRunStart = ScanExecutionJournal::Decode(runStart.Encode());
        Require(reopenedRunStart.HasEventCountBaseline() &&
                reopenedRunStart.PendingOperation() == "run-start",
                "Run start intent did not retain its event baseline through restart");
        std::string schemaFourWithoutBaseline =
            Replace(beforeBaseline.Encode(), "\"schema_version\":3", "\"schema_version\":4");
        const std::size_t eventEnd = schemaFourWithoutBaseline.rfind("]}");
        Require(eventEnd != std::string::npos, "Schema-4 fixture has no event list");
        schemaFourWithoutBaseline.insert(eventEnd,
            ",{\"type\":\"operation-intent\",\"point_index\":0,\"state\":\"Configured\","
            "\"operation_id\":\"missing-baseline-run\",\"operation\":\"run-start\","
            "\"request_sha256\":\"8888888888888888888888888888888888888888888888888888888888888888\"}");
        Rejects([&] { ScanExecutionJournal::Decode(schemaFourWithoutBaseline); });
        eventJournal = runStart.WithOperationComplete("event-run-start",
            std::string(64, '9'), ScanPointState::RunStarted);
        eventJournal = advanceEventOperation(eventJournal, "event-run-stop", "run-stop",
                                             ScanPointState::RunStopped);
        eventJournal = advanceEventOperation(eventJournal, "event-receiver-finish", "receiver-finish",
                                             ScanPointState::ReceiverFinished);
        eventJournal = advanceEventOperation(eventJournal, "event-catalog", "catalog-verify",
                                             ScanPointState::CatalogVerified);
        eventJournal = advanceEventOperation(eventJournal, "event-bind", "bind",
                                             ScanPointState::Bound);
        Rejects([&] { eventJournal.WithNextPoint(); });
        Rejects([&] { eventJournal.WithOperationIntent("event-retire", "retire",
                                                      std::string(64, 'f')); });
        Rejects([&] { ScanExecutionJournal::Decode(Replace(
            describeIntent.Encode(), "runtime-identity-bound", "snapshot-bound")); });
        const auto explicitUnknown = intent.WithOperationUnknown("op-1", std::string(64, 'e'));
        const auto reopenedUnknown = ScanExecutionJournal::Decode(explicitUnknown.Encode());
        Require(reopenedUnknown.State() == ScanPointState::NeedsReconciliation &&
                reopenedUnknown.PendingOperationUnknown(),
                "Explicit unknown result was not quarantined on replay");
        const auto quarantined = ScanExecutionJournal("scan-unknown", std::string(64, 'b'), std::string(64, 'f'), 1)
            .WithTransition(ScanPointState::Planned, ScanPointState::Reviewed)
            .WithOperationIntent("op-unknown", "start-ecc", std::string(64, 'c'))
            .WithOperationUnknown("op-unknown", std::string(64, 'e'));
        Rejects([&] { quarantined.WithTransition(ScanPointState::NeedsReconciliation,
                                                 ScanPointState::Reviewed); });
        Rejects([&] { quarantined.WithOperationReconciled(
            "wrong-operation", "operator-1", ScanOperationDisposition::ConfirmedApplied,
            std::string(64, 'f')); });
        Rejects([&] { quarantined.WithOperationReconciled(
            "op-unknown", "operator-1", ScanOperationDisposition::ConfirmedApplied,
            "bad-digest"); });
        Rejects([&] { quarantined.WithOperationReconciled(
            "op-unknown", "", ScanOperationDisposition::ConfirmedApplied,
            std::string(64, 'f')); });
        const auto reconciledApplied = quarantined.WithOperationReconciled(
            "op-unknown", "operator-1", ScanOperationDisposition::ConfirmedApplied,
            std::string(64, 'f'));
        const auto replayedApplied = ScanExecutionJournal::Decode(reconciledApplied.Encode());
        Require(replayedApplied.State() == ScanPointState::EccStarted &&
                !replayedApplied.HasPendingOperation() &&
                replayedApplied.EventCount() == quarantined.EventCount() + 1 &&
                reconciledApplied.Encode().find("\"operator_id\":\"operator-1\"") !=
                    std::string::npos,
                "Applied reconciliation did not durably advance the mapped operation");
        Rejects([&] { ScanExecutionJournal::Decode(Replace(
            reconciledApplied.Encode(), "confirmed-applied", "confirmed-not-sent")); });
        Rejects([&] { ScanExecutionJournal::Decode(Replace(
            reconciledApplied.Encode(), "\"operator_id\":\"operator-1\"",
            "\"operator_id\":\"\"")); });
        const auto reconciledNotSent = quarantined.WithOperationReconciled(
            "op-unknown", "operator-2", ScanOperationDisposition::ConfirmedNotSent,
            std::string(64, '1'));
        const auto replayedNotSent = ScanExecutionJournal::Decode(reconciledNotSent.Encode());
        Require(replayedNotSent.State() == ScanPointState::Reviewed &&
                !replayedNotSent.HasPendingOperation(),
                "Not-sent reconciliation did not restore the pre-operation state");
        Rejects([&] { replayedNotSent.WithOperationIntent(
            "op-unknown", "start-ecc", std::string(64, '2')); });
        const auto newAttempt = replayedNotSent.WithOperationIntent(
            "op-unknown-2", "start-ecc", std::string(64, '2'));
        Require(newAttempt.PendingOperationId() == "op-unknown-2",
                "A confirmed-not-sent operation could not be retried with a new ID");
        const auto reconciledAfterRestart = reopenedIntent.WithOperationReconciled(
            "op-1", "operator-1", ScanOperationDisposition::ConfirmedNotSent,
            std::string(64, '3'));
        Require(ScanExecutionJournal::Decode(reconciledAfterRestart.Encode()).State() ==
                ScanPointState::Reviewed,
                "Reconciliation after restart did not persist an explicit resolution");
        std::cout << "PASS scan lifecycle, intent-before-action journal, restart quarantine, and tamper rejection\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
