#ifndef GETDAQ_CONSOLELAYOUT_TESTS_P_H
#define GETDAQ_CONSOLELAYOUT_TESTS_P_H

namespace GetDAQTestInternal {

void RunConsoleOverviewUsesConfiguredFreshCoBos();
void RunConsoleReferenceLayoutAndLiveSimulation();
void RunCompactConsoleLayoutKeepsParametersAndNodeRowsVisible();
void RunStatusTableUsesOnlyConfiguredNodeHeight();
void RunConsoleReflowsWhenConfiguredNodesShrink();
void RunManualRunNumberRollbackRemainsAvailableUntilRunStarts();
void RunElapsedShowsHoursMinutesSeconds();

} // namespace GetDAQTestInternal

#endif
