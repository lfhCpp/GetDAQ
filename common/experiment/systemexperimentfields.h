#ifndef GETDAQ_SYSTEMEXPERIMENTFIELDS_H
#define GETDAQ_SYSTEMEXPERIMENTFIELDS_H
#include "systemexperimentconfiguration.h"
namespace GetDAQ {
// 编码/严格解码共用字段清单；新增参数必须同时纳入工件，不能丢弃兼容策略。
#define GETDAQ_EXPERIMENT_FIELD(name) a.Field(#name,s.name)
template<class Archive> void ExperimentFields(Archive &a,AgetChannel &s) {
    GETDAQ_EXPERIMENT_FIELD(id); GETDAQ_EXPERIMENT_FIELD(active); GETDAQ_EXPERIMENT_FIELD(selectedForTest);
    GETDAQ_EXPERIMENT_FIELD(gain); GETDAQ_EXPERIMENT_FIELD(threshold); GETDAQ_EXPERIMENT_FIELD(inhibition); GETDAQ_EXPERIMENT_FIELD(reading);
}
template<class Archive> void ExperimentFields(Archive &a,AgetSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(chipVersion); GETDAQ_EXPERIMENT_FIELD(twoPulseMode); GETDAQ_EXPERIMENT_FIELD(readoutDepth);
    GETDAQ_EXPERIMENT_FIELD(globals); GETDAQ_EXPERIMENT_FIELD(channels);
}
template<class Archive> void ExperimentFields(Archive &a,CoBoAgetChipSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(asadId); GETDAQ_EXPERIMENT_FIELD(agetId); GETDAQ_EXPERIMENT_FIELD(settings);
}
template<class Archive> void ExperimentFields(Archive &a,CoBoAgetSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(asadMask); GETDAQ_EXPERIMENT_FIELD(allowReg2BankResetDifference); GETDAQ_EXPERIMENT_FIELD(chips);
}
template<class Archive> void ExperimentFields(Archive &a,CoBoChipReadout &s) {
    GETDAQ_EXPERIMENT_FIELD(asadId); GETDAQ_EXPERIMENT_FIELD(agetId); GETDAQ_EXPERIMENT_FIELD(suppressMultiplicity); GETDAQ_EXPERIMENT_FIELD(zeroSuppressionThresholds);
}
template<class Archive> void ExperimentFields(Archive &a,CoBoBufferRange &s) {
    GETDAQ_EXPERIMENT_FIELD(start); GETDAQ_EXPERIMENT_FIELD(end);
}
template<class Archive> void ExperimentFields(Archive &a,CoBoReadoutSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(aget); GETDAQ_EXPERIMENT_FIELD(module); GETDAQ_EXPERIMENT_FIELD(circularBuffer);
    GETDAQ_EXPERIMENT_FIELD(buffers); GETDAQ_EXPERIMENT_FIELD(chips); GETDAQ_EXPERIMENT_FIELD(bufferLayout);
}
template<class Archive> void ExperimentFields(Archive &a,CoBoAsAdBoardSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(asadId); GETDAQ_EXPERIMENT_FIELD(triggerDelay); GETDAQ_EXPERIMENT_FIELD(calibrationMode);
    GETDAQ_EXPERIMENT_FIELD(capacitor); GETDAQ_EXPERIMENT_FIELD(inspectionDevice); GETDAQ_EXPERIMENT_FIELD(isp1); GETDAQ_EXPERIMENT_FIELD(isp2);
}
template<class Archive> void ExperimentFields(Archive &a,CoBoAsAdSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(asadMask); GETDAQ_EXPERIMENT_FIELD(generator); GETDAQ_EXPERIMENT_FIELD(boards);
}
template<class Archive> void ExperimentFields(Archive &a,CoBoConfigureSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(readout); GETDAQ_EXPERIMENT_FIELD(asad);
}
template<class Archive> void ExperimentFields(Archive &a,CoBoPrepareFoundationSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(hardwareVersion); GETDAQ_EXPERIMENT_FIELD(verifyHardwareVersion); GETDAQ_EXPERIMENT_FIELD(firmwareInfoRegisters);
    GETDAQ_EXPERIMENT_FIELD(mutantStatusRegister); GETDAQ_EXPERIMENT_FIELD(interruptRateRegister); GETDAQ_EXPERIMENT_FIELD(bufferLayout); GETDAQ_EXPERIMENT_FIELD(lemo);
}
template<class Archive> void ExperimentFields(Archive &a,AsAdInputPosition &s) {
    GETDAQ_EXPERIMENT_FIELD(active); GETDAQ_EXPERIMENT_FIELD(enabled); GETDAQ_EXPERIMENT_FIELD(connectGroundToZAP); GETDAQ_EXPERIMENT_FIELD(antiSparklingEnabled);
}
template<class Archive> void ExperimentFields(Archive &a,AsAdInputBoardSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(asadId); GETDAQ_EXPERIMENT_FIELD(positions);
}
template<class Archive> void ExperimentFields(Archive &a,AsAdInputSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(asadMask); GETDAQ_EXPERIMENT_FIELD(boards);
}
template<class Archive> void ExperimentFields(Archive &a,AsAdBoardClockSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(asadId); GETDAQ_EXPERIMENT_FIELD(samplingDivider); GETDAQ_EXPERIMENT_FIELD(phaseDegrees); GETDAQ_EXPERIMENT_FIELD(delayPs);
    GETDAQ_EXPERIMENT_FIELD(samplingExpert); GETDAQ_EXPERIMENT_FIELD(readingExpert);
}
template<class Archive> void ExperimentFields(Archive &a,CoBoClockSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(hardwareVersion); GETDAQ_EXPERIMENT_FIELD(asadMask); GETDAQ_EXPERIMENT_FIELD(verifyHardwareVersion);
    GETDAQ_EXPERIMENT_FIELD(skipPllConfig); GETDAQ_EXPERIMENT_FIELD(allowNominalDifference); GETDAQ_EXPERIMENT_FIELD(allowUnverifiedUpstream);
    GETDAQ_EXPERIMENT_FIELD(writingMHz); GETDAQ_EXPERIMENT_FIELD(readingMHz); GETDAQ_EXPERIMENT_FIELD(pll); GETDAQ_EXPERIMENT_FIELD(boards);
}
template<class Archive> void ExperimentFields(Archive &a,AsAdMonitoringBoardSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(asadId); GETDAQ_EXPERIMENT_FIELD(checkPowerSupply); GETDAQ_EXPERIMENT_FIELD(alarmsEnabled);
    GETDAQ_EXPERIMENT_FIELD(averagingDisabled); GETDAQ_EXPERIMENT_FIELD(fastConversionEnabled); GETDAQ_EXPERIMENT_FIELD(singleChannelModeEnabled);
    GETDAQ_EXPERIMENT_FIELD(singleChannelItem); GETDAQ_EXPERIMENT_FIELD(vddLower); GETDAQ_EXPERIMENT_FIELD(vddUpper);
    GETDAQ_EXPERIMENT_FIELD(iddLower); GETDAQ_EXPERIMENT_FIELD(iddUpper); GETDAQ_EXPERIMENT_FIELD(vddSettleLower);
    GETDAQ_EXPERIMENT_FIELD(externalTemperatureLower); GETDAQ_EXPERIMENT_FIELD(externalTemperatureUpper); GETDAQ_EXPERIMENT_FIELD(disableAlerts);
}
template<class Archive> void ExperimentFields(Archive &a,CoBoMonitoringSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(modeMapping); GETDAQ_EXPERIMENT_FIELD(ledPolicy); GETDAQ_EXPERIMENT_FIELD(ledHardwareVersion);
    GETDAQ_EXPERIMENT_FIELD(checkAsAdAlarms); GETDAQ_EXPERIMENT_FIELD(subscribeAlarms); GETDAQ_EXPERIMENT_FIELD(asadMask);
    GETDAQ_EXPERIMENT_FIELD(powerDelayMs); GETDAQ_EXPERIMENT_FIELD(settleAttempts); GETDAQ_EXPERIMENT_FIELD(settleDelayMs);
    GETDAQ_EXPERIMENT_FIELD(zeroReadRetries); GETDAQ_EXPERIMENT_FIELD(boards);
}
template<class Archive> void ExperimentFields(Archive &a,AsAdCalibrationSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(asadId); GETDAQ_EXPERIMENT_FIELD(calibrationAgetId); GETDAQ_EXPERIMENT_FIELD(useAutomaticReadDelay);
}
template<class Archive> void ExperimentFields(Archive &a,CoBoCalibrationSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(delayLayout); GETDAQ_EXPERIMENT_FIELD(asadMask); GETDAQ_EXPERIMENT_FIELD(calibrationAgetRegister);
    GETDAQ_EXPERIMENT_FIELD(computeAutomaticReadDelay); GETDAQ_EXPERIMENT_FIELD(chipVersions); GETDAQ_EXPERIMENT_FIELD(boards);
}
template<class Archive> void ExperimentFields(Archive &a,CoBoPrepareSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(foundation); GETDAQ_EXPERIMENT_FIELD(inputs); GETDAQ_EXPERIMENT_FIELD(clock);
    GETDAQ_EXPERIMENT_FIELD(monitoring); GETDAQ_EXPERIMENT_FIELD(calibration); GETDAQ_EXPERIMENT_FIELD(manualReadDataDelay); GETDAQ_EXPERIMENT_FIELD(adcDoubleDataRate);
}
template<class Archive> void ExperimentFields(Archive &a,MutantSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(layout); GETDAQ_EXPERIMENT_FIELD(clockPolicy); GETDAQ_EXPERIMENT_FIELD(centrumScalerMode);
    GETDAQ_EXPERIMENT_FIELD(moduleCount); GETDAQ_EXPERIMENT_FIELD(slaveId); GETDAQ_EXPERIMENT_FIELD(isSlave); GETDAQ_EXPERIMENT_FIELD(slavePortA);
    GETDAQ_EXPERIMENT_FIELD(masterPortAEnabled); GETDAQ_EXPERIMENT_FIELD(masterPortBEnabled); GETDAQ_EXPERIMENT_FIELD(level0Enabled);
    GETDAQ_EXPERIMENT_FIELD(level1Enabled); GETDAQ_EXPERIMENT_FIELD(level2Enabled); GETDAQ_EXPERIMENT_FIELD(hitPatternModified);
    GETDAQ_EXPERIMENT_FIELD(level1BeforeLevel0); GETDAQ_EXPERIMENT_FIELD(twoProtonMode); GETDAQ_EXPERIMENT_FIELD(halfEventReadout);
    GETDAQ_EXPERIMENT_FIELD(level1Selection); GETDAQ_EXPERIMENT_FIELD(level0GateDelayUs); GETDAQ_EXPERIMENT_FIELD(level0GateDurationUs);
    GETDAQ_EXPERIMENT_FIELD(level1PostTriggerDelayUs); GETDAQ_EXPERIMENT_FIELD(level1bTimeoutUs); GETDAQ_EXPERIMENT_FIELD(multiplicityThresholds);
    GETDAQ_EXPERIMENT_FIELD(bucketCounts); GETDAQ_EXPERIMENT_FIELD(preloadTimestamp); GETDAQ_EXPERIMENT_FIELD(preloadEventNumber);
    GETDAQ_EXPERIMENT_FIELD(clearScalers); GETDAQ_EXPERIMENT_FIELD(clearCounters); GETDAQ_EXPERIMENT_FIELD(timestamp10ns);
    GETDAQ_EXPERIMENT_FIELD(eventNumber); GETDAQ_EXPERIMENT_FIELD(scalerEnabled); GETDAQ_EXPERIMENT_FIELD(vcxoControl);
    GETDAQ_EXPERIMENT_FIELD(sourceId); GETDAQ_EXPERIMENT_FIELD(frameType); GETDAQ_EXPERIMENT_FIELD(amcTagOnly); GETDAQ_EXPERIMENT_FIELD(userRegister);
    GETDAQ_EXPERIMENT_FIELD(readoutEnabled); GETDAQ_EXPERIMENT_FIELD(testMode); GETDAQ_EXPERIMENT_FIELD(externalClockEnabled);
    GETDAQ_EXPERIMENT_FIELD(externalClockSelection); GETDAQ_EXPERIMENT_FIELD(inspection); GETDAQ_EXPERIMENT_FIELD(gmcShelfMask); GETDAQ_EXPERIMENT_FIELD(wscaShelfMask);
    GETDAQ_EXPERIMENT_FIELD(twoProtonCounterCommand); GETDAQ_EXPERIMENT_FIELD(allowPrototypePowerDf); GETDAQ_EXPERIMENT_FIELD(verifyBoardId); GETDAQ_EXPERIMENT_FIELD(expectedBoardId);
}
template<class Archive> void ExperimentFields(Archive &a,SystemExperimentCoBo &s) {
    GETDAQ_EXPERIMENT_FIELD(id); GETDAQ_EXPERIMENT_FIELD(enabled); GETDAQ_EXPERIMENT_FIELD(prepare); GETDAQ_EXPERIMENT_FIELD(configure);
}
template<class Archive> void ExperimentFields(Archive &a,SystemExperimentMutant &s) {
    GETDAQ_EXPERIMENT_FIELD(id); GETDAQ_EXPERIMENT_FIELD(settings);
}
template<class Archive> void ExperimentFields(Archive &a,SystemExperimentSettings &s) {
    GETDAQ_EXPERIMENT_FIELD(cobos); GETDAQ_EXPERIMENT_FIELD(mutants);
}
#undef GETDAQ_EXPERIMENT_FIELD
}
#endif
