#ifndef GETDAQ_WAVEFORMVIEWER_CLI_H
#define GETDAQ_WAVEFORMVIEWER_CLI_H

// Handles the standalone viewer's offline command-line operations before Qt GUI startup.
// Returns true when a command was recognized and writes its process exit code.
bool RunWaveformViewerCommandLine(int argc, char **argv, int *exitCode);

#endif
