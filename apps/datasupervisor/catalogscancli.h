#ifndef GETDAQ_CATALOGSCANCLI_H
#define GETDAQ_CATALOGSCANCLI_H

namespace GetDAQCatalogCli {
// Run one scan command when argv selects it; return false for other commands.
bool HandleScanCommand(int argc, char **argv);
}

#endif
