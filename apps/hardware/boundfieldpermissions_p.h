#ifndef GETDAQ_BOUND_FIELD_PERMISSIONS_P_H
#define GETDAQ_BOUND_FIELD_PERMISSIONS_P_H
#include "../../common/hardware/hardwaredescription.h"
#include <stdexcept>
namespace GetDAQ { namespace HardwareClientInternal {
inline void ApplyBoundFieldPolicy(FieldInfo &field,const FieldInfo &policy,bool registerReadOnly) {
    if(field.name!=policy.name || field.offset!=policy.offset || field.width!=policy.width)
        throw std::runtime_error("Remote bit field differs from bound description");
    // Legacy MDaq DeviceImpl::getFieldDescription fills name/offset/width only;
    // its wire readOnly is uninitialized. Use the explicitly bound permission
    // policy, just as for registers, after exact layout validation. Unbound
    // clients never call this helper and retain the remote restriction.
    field.readOnly=policy.readOnly || registerReadOnly;
}
} }
#endif
