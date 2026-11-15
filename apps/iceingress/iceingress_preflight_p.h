#ifndef GETDAQ_ICEINGRESS_PREFLIGHT_P_H
#define GETDAQ_ICEINGRESS_PREFLIGHT_P_H

#include "iceingress.h"

namespace GetDAQ {
// Run after acquiring the output-directory lease and before starting the writer.
void ValidateIceIngressOutput(const IceIngressConfig &config);
}

#endif
