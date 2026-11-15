#ifndef GETDAQ_RECEIVER_INTERNAL_H
#define GETDAQ_RECEIVER_INTERNAL_H

#include <string>

namespace GetDAQ {
namespace ReceiverInternal {

inline std::string QuoteJson(const std::string &value) {
    std::string out = "\"";
    const char hex[] = "0123456789abcdef";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
        else out += char(c);
    }
    return out + '"';
}

} // namespace ReceiverInternal
} // namespace GetDAQ

#endif
