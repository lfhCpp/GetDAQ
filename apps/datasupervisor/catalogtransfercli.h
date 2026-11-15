#ifndef GETDAQ_CATALOGTRANSFERCLI_H
#define GETDAQ_CATALOGTRANSFERCLI_H

namespace GetDAQCatalogCli {
// Run a transfer command selected by argv; return false for other commands.
bool HandleTransferCommand(int argc, char **argv);
}

#endif
