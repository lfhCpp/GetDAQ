#include "configurationrange.h"
#include "configurationrangeinternal_p.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace GetDAQ {
namespace {
bool Space(char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n' || c=='\f' || c=='\v'; }
std::string Trim(const std::string &s) {
    std::size_t first=0,last=s.size();
    while (first<last && Space(s[first])) ++first;
    while (last>first && Space(s[last-1])) --last;
    return s.substr(first,last-first);
}
int Digit(char c) {
    if (c>='0' && c<='9') return c-'0';
    if (c>='a' && c<='f') return c-'a'+10;
    if (c>='A' && c<='F') return c-'A'+10;
    return -1;
}
bool IntegerLiteral(const std::string &s) {
    std::size_t i=!s.empty() && (s[0]=='+' || s[0]=='-') ? 1 : 0;
    if (i==s.size()) return false;
    for (;i<s.size();++i) if (s[i]<'0' || s[i]>'9') return false;
    return true;
}
bool RealLiteral(const std::string &s) {
    std::size_t i=!s.empty() && (s[0]=='+' || s[0]=='-') ? 1 : 0,digits=0;
    while (i<s.size() && s[i]>='0' && s[i]<='9') { ++digits; ++i; }
    if (i<s.size() && s[i]=='.') {
        ++i; while (i<s.size() && s[i]>='0' && s[i]<='9') { ++digits; ++i; }
    }
    if (!digits) return false;
    if (i<s.size() && (s[i]=='e' || s[i]=='E')) {
        ++i; if (i<s.size() && (s[i]=='+' || s[i]=='-')) ++i;
        const auto begin=i; while (i<s.size() && s[i]>='0' && s[i]<='9') ++i;
        if (i==begin) return false;
    }
    return i==s.size();
}
std::uint64_t Unsigned(const std::string &s,unsigned int base,std::uint64_t maximum) {
    if (s.empty()) throw std::invalid_argument("Missing configuration integer");
    std::uint64_t result=0;
    for (char c:s) {
        const int digit=Digit(c);
        if (digit<0 || unsigned(digit)>=base || result>(maximum-unsigned(digit))/base)
            throw std::invalid_argument("Configuration integer syntax/overflow");
        result=result*base+unsigned(digit);
    }
    return result;
}
struct Token { std::string value; bool quoted=false; };
std::vector<Token> Tokens(const std::string &s) {
    std::vector<Token> result; std::size_t i=0;
    while (i<s.size()) {
        while (i<s.size() && Space(s[i])) ++i;
        if (i==s.size()) {
            if (!result.empty()) throw std::invalid_argument("Missing configuration range item after comma");
            break;
        }
        Token token;
        if (i<s.size() && s[i]=='"') {
            token.quoted=true; ++i; bool closed=false;
            while (i<s.size()) {
                char c=s[i++];
                if (c=='"') { closed=true; break; }
                if (c=='\\') {
                    if (i==s.size()) throw std::invalid_argument("Incomplete configuration string escape");
                    c=s[i++];
                    switch (c) {
                    case 'a': c='\a'; break; case 'b': c='\b'; break; case 'f': c='\f'; break;
                    case 'n': c='\n'; break; case 'r': c='\r'; break; case 't': c='\t'; break;
                    case 'v': c='\v'; break; case '\\': case '"': case '\'': case '?': break;
                    case 'x': {
                        unsigned int byte=0,digits=0;
                        while (i<s.size() && Digit(s[i])>=0) {
                            const auto d=unsigned(Digit(s[i++]));
                            if (byte>(255-d)/16) throw std::invalid_argument("Configuration hex escape exceeds one byte");
                            byte=byte*16+d; ++digits;
                        }
                        if (!digits) throw std::invalid_argument("Missing configuration hex escape digits");
                        c=char(byte); break;
                    }
                    default:
                        if (c<'0' || c>'7') throw std::invalid_argument("Unsupported configuration string escape");
                        unsigned int byte=unsigned(c-'0'),digits=1;
                        while (digits<3 && i<s.size() && s[i]>='0' && s[i]<='7') { byte=byte*8+unsigned(s[i++]-'0'); ++digits; }
                        if (byte>255) throw std::invalid_argument("Configuration octal escape exceeds one byte");
                        c=char(byte);
                    }
                }
                token.value+=c;
            }
            if (!closed) throw std::invalid_argument("Unclosed configuration string");
            while (i<s.size() && Space(s[i])) ++i;
        } else {
            const auto begin=i;
            while (i<s.size() && s[i]!=',') {
                if (s[i]=='"' || s[i]=='{' || s[i]=='}' || s[i]=='[' || s[i]==']') throw std::invalid_argument("Unquoted configuration range delimiter");
                ++i;
            }
            token.value=Trim(s.substr(begin,i-begin));
            if (token.value.empty()) throw std::invalid_argument("Empty configuration range item");
        }
        if (result.size()>=4096) throw std::invalid_argument("Configuration range item limit exceeded");
        result.push_back(token);
        if (i==s.size()) break;
        if (s[i++]!=',' || i==s.size()) throw std::invalid_argument("Invalid configuration range separator");
    }
    return result;
}
ConfigurationValue Value(const Token &t) { return ParseConfigurationValue(t.value,t.quoted ? "string" : ""); }
}
using namespace ConfigurationRangeInternal;
ConfigurationValue ParseConfigurationValue(const std::string &text,const std::string &declared) {
    if (text.size()>1024*1024) throw std::invalid_argument("Configuration value byte limit exceeded");
    const auto s=Trim(text); std::string type=declared,folded=s;
    for (auto &c:folded) if (c>='A' && c<='Z') c=char(c-'A'+'a');
    const bool hex=s.size()>2 && s[0]=='0' && (s[1]=='x' || s[1]=='X');
    if (type.empty()) {
        if (hex) type="hex";
        else if (IntegerLiteral(s)) type="int";
        else if ((s.find('.')!=std::string::npos || s.find('e')!=std::string::npos || s.find('E')!=std::string::npos) && RealLiteral(s)) type="real";
        else if (folded=="true" || folded=="false") type="bool";
        else type="string";
    }
    ConfigurationValue result;
    if (type=="int") {
        if (!IntegerLiteral(s)) throw std::invalid_argument("Invalid configuration signed integer");
        const bool negative=s[0]=='-'; const std::size_t begin=s[0]=='-' || s[0]=='+' ? 1 : 0;
        const auto limit=std::uint64_t(std::numeric_limits<std::int64_t>::max())+unsigned(negative);
        const auto magnitude=Unsigned(s.substr(begin),10,limit);
        result.type=ConfigurationValue::Integer;
        result.integer=negative ? (magnitude==limit ? std::numeric_limits<std::int64_t>::min() : -std::int64_t(magnitude)) : std::int64_t(magnitude);
    } else if (type=="hex") {
        result.type=ConfigurationValue::Hexadecimal; result.hexadecimal=Unsigned(hex ? s.substr(2) : s,16,std::numeric_limits<std::uint64_t>::max());
    } else if (type=="real") {
        if (!RealLiteral(s)) throw std::invalid_argument("Invalid configuration finite real");
        std::istringstream stream(s); stream.imbue(std::locale::classic()); stream>>result.real;
        if (!stream || !stream.eof() || !std::isfinite(result.real)) throw std::invalid_argument("Invalid configuration finite real");
        result.type=ConfigurationValue::Real;
    } else if (type=="bool") {
        if (folded!="true" && folded!="false") throw std::invalid_argument("Boolean must be true or false");
        result.type=ConfigurationValue::Boolean; result.boolean=folded=="true";
    } else if (type=="string") { result.type=ConfigurationValue::String; result.text=text; }
    else throw std::invalid_argument("Unsupported scalar type: "+type);
    return result;
}
ConfigurationRange ParseConfigurationRange(const std::string &text) {
    if (text.size()>1024*1024) throw std::invalid_argument("Configuration range byte limit exceeded");
    const auto s=Trim(text); if (s.size()<2) throw std::invalid_argument("Invalid configuration range");
    ConfigurationRange range;
    const auto tokens=Tokens(s.substr(1,s.size()-2));
    if (s.front()=='{' && s.back()=='}') {
        for (const auto &token:tokens) {
            if (!token.quoted && std::any_of(token.value.begin(),token.value.end(),Space)) throw std::invalid_argument("Enumeration strings containing spaces must be quoted");
            const auto value=Value(token);
            if (!range.values.empty() && range.values.front().type!=value.type) throw std::invalid_argument("Configuration enumeration types differ");
            range.values.push_back(value);
        }
        std::sort(range.values.begin(),range.values.end(),Less);
        range.values.erase(std::unique(range.values.begin(),range.values.end(),Equal),range.values.end());
    } else if ((s.front()=='[' || s.front()==']') && (s.back()=='[' || s.back()==']') && tokens.size()==2) {
        range.kind=ConfigurationRange::Interval; range.includeLower=s.front()=='['; range.includeUpper=s.back()==']';
        range.lowerUnbounded=!tokens[0].quoted && tokens[0].value=="-inf";
        range.upperUnbounded=!tokens[1].quoted && tokens[1].value=="+inf";
        if (!range.lowerUnbounded) range.lower=Value(tokens[0]);
        if (!range.upperUnbounded) range.upper=Value(tokens[1]);
    } else throw std::invalid_argument("Invalid configuration range delimiters/item count");
    return range;
}

}
