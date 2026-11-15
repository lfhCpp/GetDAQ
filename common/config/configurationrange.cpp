#include "configurationrange.h"
#include "configurationrangeinternal_p.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace GetDAQ {
namespace ConfigurationRangeInternal {
bool Equal(const ConfigurationValue &a,const ConfigurationValue &b) {
    if (a.type!=b.type) return false;
    switch (a.type) {
    case ConfigurationValue::Integer: return a.integer==b.integer;
    case ConfigurationValue::Hexadecimal: return a.hexadecimal==b.hexadecimal;
    case ConfigurationValue::Real: return a.real==b.real;
    case ConfigurationValue::Boolean: return a.boolean==b.boolean;
    case ConfigurationValue::String: return a.text==b.text;
    }
    return false;
}
bool Less(const ConfigurationValue &a,const ConfigurationValue &b) {
    switch (a.type) {
    case ConfigurationValue::Integer: return a.integer<b.integer;
    case ConfigurationValue::Hexadecimal: return a.hexadecimal<b.hexadecimal;
    case ConfigurationValue::Real: return a.real<b.real;
    case ConfigurationValue::Boolean: return a.boolean<b.boolean;
    case ConfigurationValue::String: return a.text<b.text;
    }
    return false;
}
}
using namespace ConfigurationRangeInternal;
std::string ConfigurationValueType(const ConfigurationValue &v) {
    if (v.type<ConfigurationValue::Integer || v.type>ConfigurationValue::String) throw std::invalid_argument("Invalid configuration value type");
    static const char *names[]={"int","hex","real","bool","string"}; return names[v.type];
}
std::string NormalizeConfigurationValue(const ConfigurationValue &v) {
    if (v.type==ConfigurationValue::Integer) return std::to_string(v.integer);
    if (v.type==ConfigurationValue::Boolean) return v.boolean ? "true" : "false";
    if (v.type==ConfigurationValue::String) return v.text;
    std::ostringstream out; out.imbue(std::locale::classic());
    if (v.type==ConfigurationValue::Hexadecimal) out<<"0x"<<std::hex<<v.hexadecimal;
    else if (v.type==ConfigurationValue::Real && std::isfinite(v.real)) out<<std::setprecision(std::numeric_limits<double>::max_digits10)<<(v.real==0 ? 0.0 : v.real);
    else throw std::invalid_argument("Invalid configuration value normalization");
    return out.str();
}
bool ConfigurationRange::Contains(const ConfigurationValue &v) const {
    if (kind==Enumeration) return std::find_if(values.begin(),values.end(),[&](const ConfigurationValue &x) { return Equal(x,v); })!=values.end();
    return (lowerUnbounded || (lower.type==v.type && (Less(lower,v) || (includeLower && Equal(lower,v))))) &&
        (upperUnbounded || (upper.type==v.type && (Less(v,upper) || (includeUpper && Equal(upper,v)))));
}
bool ConfigurationRange::Equivalent(const ConfigurationRange &r) const {
    if (kind!=r.kind) return false;
    if (kind==Enumeration) return values.size()==r.values.size() && std::equal(values.begin(),values.end(),r.values.begin(),Equal);
    return lowerUnbounded==r.lowerUnbounded && upperUnbounded==r.upperUnbounded &&
        (lowerUnbounded || (includeLower==r.includeLower && Equal(lower,r.lower))) &&
        (upperUnbounded || (includeUpper==r.includeUpper && Equal(upper,r.upper)));
}
std::string EncodeConfigurationRange(const std::string &text) {
    std::string encoded;
    const auto field=[&](const std::string &s) {
        if (s.size()>1024*1024 || encoded.size()+s.size()+32>64*1024*1024) throw std::invalid_argument("Snapshot range encoding byte limit exceeded");
        encoded+=std::to_string(s.size())+':'+s;
    };
    if (text.empty()) { field("undeclared"); return encoded; }
    const auto range=ParseConfigurationRange(text);
    const auto value=[&](const ConfigurationValue &v) { field(ConfigurationValueType(v)); field(NormalizeConfigurationValue(v)); };
    if (range.kind==ConfigurationRange::Enumeration) {
        field("enum"); field(std::to_string(range.values.size())); for (const auto &v:range.values) value(v);
    } else {
        field("interval"); field(range.lowerUnbounded ? "unbounded" : range.includeLower ? "closed" : "open");
        if (!range.lowerUnbounded) value(range.lower);
        field(range.upperUnbounded ? "unbounded" : range.includeUpper ? "closed" : "open");
        if (!range.upperUnbounded) value(range.upper);
    }
    return encoded;
}
}
