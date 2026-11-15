#ifndef GETDAQ_COBO_LIFECYCLE_PROJECTION_H
#define GETDAQ_COBO_LIFECYCLE_PROJECTION_H
#include "../../../common/control/eccprotocol.h"

namespace GetDAQ {

// Project fixed-target CoBo lifecycle evidence from the persisted ECC command
// journal. This is historical controller state, never hardware readback.
void ProjectCoBoLifecycle(EccControlMaintenanceTarget &target,
                          const EccControlControllerStatus &controller,
                          const std::vector<const EccControlOperation *> &operations);
void ProjectCoBoLifecycles(std::vector<EccControlMaintenanceTarget> &targets,
                           const EccControlControllerStatus &controller,
                           const std::vector<const EccControlOperation *> &operations);

}
#endif
