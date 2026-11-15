#include "strictjson.h"
#include <cstdint>
#include <fstream>
#include <stdexcept>
namespace GetDAQ {
const JsonValue &JsonValue::At(const std::string &key) const {
    const auto p = members.find(key);
    if (type != Object || p == members.end()) throw std::invalid_argument("Missing configuration key: "+key);
    return p->second;
}
void ValidateJsonUtf8(const std::string &s) {
    for (std::size_t i = 0; i < s.size();) {
        unsigned char first = s[i++]; if (first < 128) continue;
        unsigned int n, code, minimum;
        if (first >= 0xc2 && first <= 0xdf) { n=1; code=first&31; minimum=128; }
        else if (first >= 0xe0 && first <= 0xef) { n=2; code=first&15; minimum=2048; }
        else if (first >= 0xf0 && first <= 0xf4) { n=3; code=first&7; minimum=65536; }
        else throw std::invalid_argument("Invalid UTF-8 in configuration");
        if (s.size()-i < n) throw std::invalid_argument("Truncated UTF-8 in configuration");
        for (unsigned j=0; j<n; ++j) { unsigned char c=s[i++]; if ((c&0xc0)!=0x80) throw std::invalid_argument("Invalid UTF-8 continuation"); code=(code<<6)|(c&63); }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) throw std::invalid_argument("Invalid UTF-8 code point");
    }
}
namespace {
class Parser {
    const std::string &s;
    const JsonLimits limits;
    std::size_t p=0, nodes=0;
    [[noreturn]] void Error(const std::string &message) const { throw std::invalid_argument(message+" at JSON byte "+std::to_string(p)); }
    void Space() { while (p<s.size() && (s[p]==' ' || s[p]=='\n' || s[p]=='\r' || s[p]=='\t')) ++p; }
    bool Take(char c) { Space(); if (p<s.size() && s[p]==c) { ++p; return true; } return false; }
    void Need(char c) { if (!Take(c)) Error(std::string("Expected '")+c+"'"); }
    bool Digit() const { return p<s.size() && s[p]>='0' && s[p]<='9'; }
    unsigned Hex4() {
        unsigned code=0;
        for (unsigned i=0; i<4; ++i) {
            if (p==s.size()) Error("Truncated unicode escape");
            char c=s[p++]; unsigned x;
            if (c>='0' && c<='9') x=c-'0'; else if (c>='a' && c<='f') x=c-'a'+10; else if (c>='A' && c<='F') x=c-'A'+10; else Error("Invalid unicode escape");
            code=(code<<4)|x;
        }
        return code;
    }
    static void Unicode(std::string &out,unsigned c) {
        if (c<128) out+=char(c);
        else if (c<2048) { out+=char(0xc0|(c>>6)); out+=char(0x80|(c&63)); }
        else if (c<65536) { out+=char(0xe0|(c>>12)); out+=char(0x80|((c>>6)&63)); out+=char(0x80|(c&63)); }
        else { out+=char(0xf0|(c>>18)); out+=char(0x80|((c>>12)&63)); out+=char(0x80|((c>>6)&63)); out+=char(0x80|(c&63)); }
    }
    std::string String() {
        Need('"'); std::string out;
        while (p<s.size()) {
            unsigned char c=s[p++];
            if (c=='"') { ValidateJsonUtf8(out); return out; }
            if (c<32) Error("Unescaped control character");
            if (c!='\\') out+=char(c);
            else {
                if (p==s.size()) Error("Truncated escape");
                switch (s[p++]) {
                case '"': out+='"'; break; case '\\': out+='\\'; break; case '/': out+='/'; break;
                case 'b': out+='\b'; break; case 'f': out+='\f'; break; case 'n': out+='\n'; break; case 'r': out+='\r'; break; case 't': out+='\t'; break;
                case 'u': {
                    unsigned code=Hex4();
                    if (code>=0xd800 && code<=0xdbff) {
                        if (s.size()-p<2 || s[p++]!='\\' || s[p++]!='u') Error("Missing low surrogate");
                        const unsigned low=Hex4(); if (low<0xdc00 || low>0xdfff) Error("Invalid low surrogate");
                        code=0x10000+((code-0xd800)<<10)+(low-0xdc00);
                    } else if (code>=0xdc00 && code<=0xdfff) Error("Unpaired low surrogate");
                    Unicode(out,code); break;
                }
                default: Error("Invalid JSON escape");
                }
            }
            if (out.size()>limits.stringBytes) Error("Configuration string byte limit exceeded");
        }
        Error("Unterminated string");
    }
    JsonValue Value(unsigned depth) {
        Space(); if (depth>64 || ++nodes>4000000) Error("Configuration nesting/node limit exceeded");
        if (p==s.size()) Error("Missing JSON value");
        JsonValue v;
        if (s[p]=='{') {
            ++p; v.type=JsonValue::Object; if (Take('}')) return v;
            do {
                std::string key=String(); Need(':'); auto child=Value(depth+1);
                if (!v.members.emplace(std::move(key),std::move(child)).second) Error("Duplicate configuration key");
                if (v.members.size()>4096) Error("Excessive object members");
            } while (Take(','));
            Need('}');
        } else if (s[p]=='[') {
            ++p; v.type=JsonValue::Array; if (Take(']')) return v;
            do { v.items.push_back(Value(depth+1)); if (v.items.size()>limits.arrayItems) Error("Excessive array items"); } while (Take(','));
            Need(']');
        } else if (s[p]=='"') { v.type=JsonValue::String; v.text=String(); }
        else if (s[p]=='t' || s[p]=='f' || s[p]=='n') {
            const char *literal=s[p]=='t' ? "true" : s[p]=='f' ? "false" : "null";
            for (const char *q=literal; *q; ++q) if (p==s.size() || s[p++]!=*q) Error("Invalid literal");
            v.type=*literal=='n' ? JsonValue::Null : JsonValue::Boolean; v.text=literal;
        } else {
            v.type=JsonValue::Number; const auto start=p;
            if (s[p]=='-') ++p;
            if (!Digit()) Error("Invalid number");
            if (s[p]=='0') ++p; else while (Digit()) ++p;
            if (p<s.size() && s[p]=='.') { ++p; if (!Digit()) Error("Missing fraction"); while (Digit()) ++p; }
            if (p<s.size() && (s[p]=='e' || s[p]=='E')) {
                ++p; if (p<s.size() && (s[p]=='-' || s[p]=='+')) ++p;
                if (!Digit()) Error("Missing exponent");
                while (Digit()) ++p;
            }
            if (p-start>128) Error("Excessive number text");
            v.text=s.substr(start,p-start);
        }
        return v;
    }
public:
    Parser(const std::string &bytes,const JsonLimits &budget):s(bytes),limits(budget) {}
    JsonValue Parse() { auto result=Value(0); Space(); if (p!=s.size()) Error("Trailing JSON content"); return result; }
};
}
JsonValue ParseStrictJson(const std::string &bytes) {
    return ParseStrictJson(bytes,JsonLimits());
}
JsonValue ParseStrictJson(const std::string &bytes,const JsonLimits &limits) {
    if (bytes.empty() || bytes.size()>64*1024*1024) throw std::invalid_argument("Configuration must contain 1..64MiB JSON bytes");
    if (!limits.stringBytes || limits.stringBytes>4*1024*1024 || !limits.arrayItems || limits.arrayItems>200000)
        throw std::invalid_argument("Invalid JSON parser limits");
    ValidateJsonUtf8(bytes);
    return Parser(bytes,limits).Parse();
}
std::string ReadConfigurationFile(const std::string &path) {
    std::ifstream input(path,std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open configuration: "+path);
    std::string bytes(64*1024*1024+1,'\0'); input.read(&bytes[0],bytes.size()); bytes.resize(static_cast<std::size_t>(input.gcount()));
    if (input.bad() || bytes.size()>64*1024*1024) throw std::runtime_error("Unreadable/excessive configuration: "+path);
    return bytes;
}
}
