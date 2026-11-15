#ifndef GETDAQ_AUTORUN_TESTS_P_H
#define GETDAQ_AUTORUN_TESTS_P_H

namespace GetDAQTestInternal {

void RunAutomaticRunSettingsRoundTrip();
void RunAutomaticRunPolicyWaitsForCompleteAndReady();
void RunAutomaticRunEitherThresholdAndGap();
void RunAutomaticRunPolicyPauseAndFreshBytes();
void RunAutomaticRunSimulationCyclesAndStops();
void RunAutomaticRunManualPauseResumeStop();
void RunAutomaticRunCollisionAndFinalizationFailure();
void RunAutomaticRunUiPlacementAndScreenshot();
void RunAutomaticRunUiLocksSettingsThroughRunAndInterval();

} // namespace GetDAQTestInternal

#endif
