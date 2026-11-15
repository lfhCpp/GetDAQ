#ifndef GETDAQ_CONFIGURATIONRANGEINTERNAL_P_H
#define GETDAQ_CONFIGURATIONRANGEINTERNAL_P_H

#include "configurationrange.h"

namespace GetDAQ {
namespace ConfigurationRangeInternal {

bool Equal(const ConfigurationValue &left, const ConfigurationValue &right);
bool Less(const ConfigurationValue &left, const ConfigurationValue &right);

} // namespace ConfigurationRangeInternal
} // namespace GetDAQ

#endif
