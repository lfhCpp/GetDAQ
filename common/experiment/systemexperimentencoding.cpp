#include "systemexperimentfields.h"
#include "strictjson.h"
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <type_traits>
namespace GetDAQ {
namespace {
std::string Quote(const std::string &s,std::size_t maximum=4096)
{
    ValidateJsonUtf8(s);
    if (s.size() > maximum) throw std::invalid_argument("Excessive experiment scalar text");
    std::string out = "\""; const char *hex = "0123456789abcdef";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c < 32) { out += "\\u00"; out += hex[c>>4]; out += hex[c&15]; }
        else out += char(c);
    }
    return out+'"';
}
class Encoder {
    std::string object = "{";
public:
    template<class T> void Field(const char *name,const T &s) {
        if (object.size() > 1) object += ',';
        object += Quote(name)+':'+Value(s);
        if (object.size() > 64*1024*1024) throw std::invalid_argument("Experiment settings exceed 64 MiB");
    }
    std::string Finish() const { return object+'}'; }
    static std::string Value(const std::string &s) { return Quote(s); }
    static std::string Value(bool s) { return s ? "true" : "false"; }
    template<class T> static typename std::enable_if<std::is_integral<T>::value && !std::is_same<T,bool>::value,std::string>::type Value(T s) {
        return Quote(std::is_signed<T>::value ? std::to_string(static_cast<long long>(s)) : std::to_string(static_cast<unsigned long long>(s)));
    }
    template<class T> static typename std::enable_if<std::is_enum<T>::value,std::string>::type Value(T s) {
        return Value(static_cast<typename std::underlying_type<T>::type>(s));
    }
    template<class T> static typename std::enable_if<std::is_floating_point<T>::value,std::string>::type Value(T s) {
        if (!std::isfinite(s)) throw std::invalid_argument("Non-finite experiment setting");
        std::ostringstream stream; stream.imbue(std::locale::classic()); stream << std::setprecision(std::numeric_limits<T>::max_digits10) << s;
        return Quote(stream.str());
    }
    template<class T> static std::string Value(const std::vector<T> &items) {
        if (items.size() > 4096) throw std::invalid_argument("Excessive experiment array");
        std::string out = "["; for (const auto &s : items) { if (out.size() > 1) out += ','; out += Value(s); } return out+']';
    }
    template<class T,std::size_t N> static std::string Value(const std::array<T,N> &items) {
        std::string out = "["; for (const auto &s : items) { if (out.size() > 1) out += ','; out += Value(s); } return out+']';
    }
    static std::string Key(const std::string &s) { return s; }
    static std::string Key(unsigned int s) { return std::to_string(s); }
    template<class K,class V> static std::string Value(const std::map<K,V> &items) {
        if (items.size() > 4096) throw std::invalid_argument("Excessive experiment map");
        std::string out = "{"; for (const auto &s : items) { if (out.size() > 1) out += ','; out += Quote(Key(s.first))+':'+Value(s.second); } return out+'}';
    }
    template<class T> static typename std::enable_if<!std::is_arithmetic<T>::value && !std::is_enum<T>::value,std::string>::type Value(const T &s) {
        T copy = s; Encoder encoder; ExperimentFields(encoder,copy); return encoder.Finish();
    }
};
}
std::string EncodeSystemExperimentSettings(const SystemExperimentSettings &settings)
{
    ValidateSystemExperimentSettings(settings);
    return Encoder::Value(settings);
}
std::string EncodeJsonString(const std::string &text) { return Quote(text); }
std::string EncodeJsonString(const std::string &text,std::size_t maximumBytes) { return Quote(text,maximumBytes); }
}
