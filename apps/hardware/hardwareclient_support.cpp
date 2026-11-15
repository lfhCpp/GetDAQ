#include "hardwareclient_p.h"
#include <cstring>
#include <limits>
#include <stdexcept>

namespace GetDAQ {
namespace HardwareClientInternal {

void CheckName(const std::string &name) {
    if (name.empty() || name.size() > 512 || name.find('\0') != std::string::npos)
        throw std::invalid_argument("Hardware name must contain 1..512 non-NUL bytes");
}

std::uint64_t Bits(Ice::Long value) {
    static_assert(sizeof(Ice::Long) == sizeof(std::uint64_t), "GET long must have 64 bits");
    std::uint64_t bits; std::memcpy(&bits, &value, sizeof(bits)); return bits;
}

Ice::Long Wire(std::uint64_t value) {
    Ice::Long wire; std::memcpy(&wire, &value, sizeof(wire)); return wire;
}

std::uint64_t Mask(int bits) {
    return bits == 64 ? std::numeric_limits<std::uint64_t>::max() : (std::uint64_t(1) << bits) - 1;
}

std::string Message(const std::exception &e) {
    const auto *command = dynamic_cast<const mdaq::utl::CmdException *>(&e);
    return command ? command->reason : e.what();
}

} // namespace HardwareClientInternal
} // namespace GetDAQ
