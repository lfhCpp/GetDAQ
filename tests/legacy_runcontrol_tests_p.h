#ifndef GETDAQ_LEGACY_RUNCONTROL_TESTS_P_H
#define GETDAQ_LEGACY_RUNCONTROL_TESTS_P_H

namespace GetDAQTestInternal {

void RunPausedHardwareCanBeStopped();
void RunLegacySelectedNodeProgressAndRegressRemainIndependent();
void RunReadyDuringTransitionCannotStart();
void RunLegacySoapHttpRequestSendsQuotedEmptySoapAction();
void RunRealHttpOrdering();
void RunFailedStartNeverStartsMutant();
void RunStateConfirmationDeadline();
void RunCatalogTimeoutNotRepeated();
void RunStopWaitsForMutantState();
void RunBackupRetryDoesNotRecatalog();
void RunRunReplacementWaitsForAllCatalogsBeforeCleanupAndIncrement();

} // namespace GetDAQTestInternal

#endif
