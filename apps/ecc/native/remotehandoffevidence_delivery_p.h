#ifndef GETDAQ_REMOTEHANDOFFEVIDENCE_DELIVERY_P_H
#define GETDAQ_REMOTEHANDOFFEVIDENCE_DELIVERY_P_H

#include "remotehandoffevidence.h"
#include "strictjson.h"
#include <vector>

namespace GetDAQ { namespace RemoteEvidenceInternal {
void ApplyDeliveryHistory(std::vector<RemoteHandoffNodeEvidence> &nodes,
                          const std::vector<JsonValue> &rows, bool original);
} }

#endif
