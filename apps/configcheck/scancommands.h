#ifndef GETDAQ_CONFIGCHECK_SCANCOMMANDS_H
#define GETDAQ_CONFIGCHECK_SCANCOMMANDS_H

// Handles the offline scan-plan command family. Returns false when argv does
// not select a scan command, otherwise stores the CLI exit code and returns true.
bool RunScanCommand(int argc, char **argv, int *exitCode);

#endif
