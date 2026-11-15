#ifndef GETDAQ_COBOCLOCKCONFIGURATION_P_H
#define GETDAQ_COBOCLOCKCONFIGURATION_P_H

#include <cmath>
#include <stdexcept>
#include <string>

namespace GetDAQ {
namespace CoBoClockInternal {
inline void Frequency(float value, const char *name)
{
    if (!std::isfinite(value) || value < 1 || value > 200)
        throw std::invalid_argument(std::string(name) + " must be finite and within 1..200MHz");
}
}
}

#endif
