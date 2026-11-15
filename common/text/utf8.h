#ifndef GETDAQ_UTF8_H
#define GETDAQ_UTF8_H
#include <string>

namespace GetDAQ {
inline bool ValidUtf8(const std::string &text) {
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char c = text[i++];
        if (c < 0x80) continue;
        unsigned int value; int extra;
        if (c >= 0xc2 && c <= 0xdf) { value = c & 31; extra = 1; }
        else if (c >= 0xe0 && c <= 0xef) { value = c & 15; extra = 2; }
        else if (c >= 0xf0 && c <= 0xf4) { value = c & 7; extra = 3; }
        else return false;
        if (i + static_cast<unsigned int>(extra) > text.size()) return false;
        const int length = extra;
        while (extra--) {
            const unsigned char next = text[i++];
            if ((next & 0xc0) != 0x80) return false;
            value = (value << 6) | (next & 63);
        }
        if ((length == 2 && value < 0x800) || (length == 3 && value < 0x10000) || value > 0x10ffff ||
            (value >= 0xd800 && value <= 0xdfff)) return false;
    }
    return true;
}
// 异常文本不能使 unknown 记录本身失效；输入参数不使用此函数，不静默修改配置。
inline std::string EccErrorText(const std::string &text) {
    if (text.empty()) return "Unspecified backend error";
    if (text.size() <= 4096 && ValidUtf8(text) && text.find('\0') == std::string::npos) return text;
    std::string result = text.substr(0, 4000);
    for (int suffix = 0; suffix < 4; ++suffix) {
        if (ValidUtf8(result) && result.find('\0') == std::string::npos) {
            if (result.size() < text.size()) result += " [truncated]";
            return result;
        }
        if (!result.empty()) result.pop_back();
    }
    static const char hex[] = "0123456789abcdef";
    result = "Non-UTF-8 backend error bytes (hex, possibly truncated): ";
    for (std::size_t i = 0; i < text.size() && i < 1900; ++i) {
        const unsigned char c = text[i]; result += hex[c >> 4]; result += hex[c & 15];
    }
    return result;
}
inline std::string EccErrorText(const char *text) {
    if (!text) return "Unspecified backend error";
    std::string bounded;
    std::size_t i = 0;
    while (i < 4096 && text[i]) bounded += text[i++];
    if (i == 4096 && text[i]) bounded += " [truncated]";
    return EccErrorText(bounded);
}
}
#endif
