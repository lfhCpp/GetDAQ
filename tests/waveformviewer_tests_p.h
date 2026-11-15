#ifndef GETDAQ_WAVEFORMVIEWER_TESTS_P_H
#define GETDAQ_WAVEFORMVIEWER_TESTS_P_H

#include "waveformviewer_testfixtures_p.h"

namespace GetDAQTestInternal {

void RunWaveformViewerIndexesRawAndLayeredFrames();
void RunWaveformViewerDecodesCompactCoboFrames();
void RunWaveformViewerRejectsTruncatedMfmInput();
void RunWaveformViewerComputesLegacyPulseMetrics();
void RunWaveformViewerSummarizesPeakingTimeScan();
void RunWaveformViewerSummarizesPulserResponse();
void RunWaveformViewerNavigatesAndWrapsFrames();
void RunWaveformViewerExportsSelectedSamplesAsCsv();

} // namespace GetDAQTestInternal

#endif
