// Test reference only: this executable calls the received, unchanged CCfg library.
// Never link this probe, Boost, or TinyXml into a production GetDAQ target.
#include "CCfg/Io/Document.h"
#include "CCfg/CConfig.h"
#include "CCfg/Object.h"
#include "CCfg/Range.h"
#include "CCfg/TypeInfo.h"
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {
std::string Json(const std::string& value)
{
    std::ostringstream out;
    out << '"';
    for (std::string::const_iterator i=value.begin();i!=value.end();++i) {
        const unsigned char c=static_cast<unsigned char>(*i);
        if (c=='"' || c=='\\') out << '\\' << c;
        else if (c<32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
        else out << c;
    }
    out << '"';
    return out.str();
}
std::string Normalized(const CCfg::CConfig& config)
{
    std::ostringstream out;
    out << std::setprecision(17);
    switch (config.getValueTypeId()) {
    case CCfg::INTEGER: out << config.getIntValue(); break;
    case CCfg::HEXADECIMAL: out << config.getHexValue(); break;
    case CCfg::REAL: out << config.getRealValue(); break;
    case CCfg::BOOLEAN: out << (config.getBoolValue() ? "true" : "false"); break;
    case CCfg::STRING: return config.getStringValue();
    default: throw std::runtime_error("Unexpected legacy scalar type");
    }
    return out.str();
}
}

int main(int argc,char** argv)
{
    if (argc<3) { std::cerr << "Usage: legacy-configuration-probe FILE.xcfg DOT_PATH...\n"; return 2; }
    try {
        CCfg::Io::Document document;
        document.loadFromFile(argv[1]);
        std::cout << "{\"loaded\":true,\"queries\":[";
        for (int i=2;i<argc;++i) {
            if (i!=2) std::cout << ',';
            std::cout << "{\"path\":" << Json(argv[i]);
            try {
                CCfg::CConfig config(document.getConfig());
                config.appendPath(argv[i]);
                CCfg::Attribute& attribute=config.find();
                // find() resets the traversal path. Anchor all subsequent getters
                // to the concrete result, rather than accidentally reading the root.
                CCfg::CConfig resolved(&attribute);
                if (attribute.isObject()) {
                    std::cout << ",\"ok\":true,\"object\":true";
                } else {
                    const std::string value=Normalized(resolved);
                    CCfg::Range* range=resolved.getRange();
                    const std::string unit=resolved.getUnit();
                    const std::string rangeText=range ? range->toString() : "";
                    const bool inRange=!range || range->contains(resolved.getValue());
                    std::cout << ",\"ok\":true,\"object\":false,\"type\":"
                              << Json(CCfg::TypeInfo::getDisplayName(resolved.getValueTypeId()))
                              << ",\"value\":" << Json(value)
                              << ",\"unit\":" << Json(unit)
                              << ",\"range\":" << Json(rangeText)
                              << ",\"in_range\":" << (inRange ? "true" : "false");
                }
            } catch (const std::exception& error) {
                std::cout << ",\"ok\":false,\"error\":" << Json(error.what());
            }
            std::cout << '}';
        }
        std::cout << "]}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cout << "{\"loaded\":false,\"error\":" << Json(error.what()) << "}\n";
        return 1;
    }
}
