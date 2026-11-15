#ifndef GETDAQ_ICEINGRESS_HELPERS_P_H
#define GETDAQ_ICEINGRESS_HELPERS_P_H

#include <string>

namespace GetDAQ {
namespace IceIngressInternal {
inline std::string Quote(const std::string &value)
{
    std::string text = "\"";
    const char hex[] = "0123456789abcdef";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { text += '\\'; text += char(c); }
        else if (c < 32) { text += "\\u00"; text += hex[c >> 4]; text += hex[c & 15]; }
        else text += char(c);
    }
    return text + '"';
}

inline bool RawName(const std::string &name)
{
    return name.size() >= 5 && name.substr(name.size() - 5) == ".graw";
}
}
}

#endif
