#ifndef GETDAQ_SYSTEMSCANPLAN_P_H
#define GETDAQ_SYSTEMSCANPLAN_P_H
#include "strictjson.h"
#include "systemexperimentconfiguration.h"
namespace GetDAQ { namespace SystemScanPlanInternal {
void ValidatePoint(const JsonValue &, const JsonValue &, const SystemExperimentArtifact &, unsigned int, unsigned int);
} }
#endif
