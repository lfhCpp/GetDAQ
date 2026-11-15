#ifndef GETDAQ_CONFIGCHECK_ARTIFACTCOMMANDS_H
#define GETDAQ_CONFIGCHECK_ARTIFACTCOMMANDS_H

// Handles offline configuration-artifact verification commands. Returns false
// when argv does not select one of these commands.
bool RunArtifactCommand(int argc, char **argv, int *exitCode);

#endif
