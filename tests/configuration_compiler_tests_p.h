#ifndef GETDAQ_CONFIGURATION_COMPILER_TESTS_P_H
#define GETDAQ_CONFIGURATION_COMPILER_TESTS_P_H

namespace GetDAQTestInternal {

void RunMutantRepository();
void RunMutantRejectedParameters();
void RunCoboCalibrationRepository();
void RunCoboCalibrationPoliciesAndCoverage();
void RunCoboCalibrationFailurePreservesOutput();
void RunCoboMonitoringRepository();
void RunCoboMonitoringModeMapping();
void RunCoboMonitoringAlarmPolicy();
void RunCoboMonitoringValidationPreservesOutputs();
void RunCoboClockConfigurationRepository();
void RunCoboClockValidationPreservesOutputs();
void RunCoboConfigureParametersRepository();
void RunCoboConfigureParametersRejectBeforeOutput();
void RunCoboReadoutConfigurationRepository();
void RunCoboReadoutConfigurationRejectsGlobalConflicts();
void RunCoboAgetConfigurationRepository();
void RunCoboAgetConfigurationMixedVersionAndOffsetConflict();
void RunAgetConfigurationRepository();
void RunAgetConfigurationRejectsInvalidSelection();
void RunAgetConfigurationIncludeAndNewOutput();
void RunHardwareDescriptionDefaultsAndNames();
void RunHardwareDescriptionInvalidIsTransactional();
void RunHardwareDescriptionRepository();

} // namespace GetDAQTestInternal

#endif
