#include "eccjournal_p.h"
#include "../../common/text/utf8.h"
#include <set>
#include <stdexcept>

namespace GetDAQ {
namespace EccJournalInternal {
const std::size_t MaxBytes=64*1024*1024,MaxLine=64*1024,MaxRecords=200000,MaxOperations=100000;

void TextLimit(const std::string &text) {
    if(text.empty() || text.size()>4096 || text.find('\0')!=std::string::npos)
        throw std::invalid_argument("Journal text must contain 1..4096 non-NUL UTF-8 bytes");
    if(!ValidUtf8(text))throw std::invalid_argument("Invalid UTF-8 in journal text");
}

void IdLimit(const std::string &id) {
    if(id.empty() || id.size()>128 || id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)
        throw std::invalid_argument("Operation ID must contain 1..128 ASCII letters, digits, '_' or '-'");
}

std::string Quote(const std::string &text) {
    static const char hex[]="0123456789abcdef";
    std::string out="\"";
    for(unsigned char c:text) {
        if(c=='\"' || c=='\\') { out+='\\';out+=char(c); }
        else if(c<32) { out+="\\u00";out+=hex[c>>4];out+=hex[c&15]; }
        else out+=char(c);
    }
    return out+'\"';
}

void Keys(const JsonValue &root,std::initializer_list<const char *> keys) {
    if(root.type!=JsonValue::Object)throw std::runtime_error("Journal record must be a JSON object");
    std::set<std::string> allowed;for(const char *key:keys)allowed.insert(key);
    if(root.members.size()!=allowed.size())throw std::runtime_error("Missing/unknown journal key");
    for(const auto &item:root.members)if(!allowed.count(item.first))throw std::runtime_error("Unknown journal key");
}

std::string Text(const JsonValue &node) {
    if(node.type!=JsonValue::String)throw std::runtime_error("Journal value must be a JSON string");
    return node.text;
}

unsigned long long Number(const JsonValue &node,bool sequence) {
    if(node.type!=(sequence ? JsonValue::String : JsonValue::Number))throw std::runtime_error("Invalid journal integer JSON type");
    const auto &text=node.text;
    if(text.empty() || text.size()>18 || text.find_first_not_of("0123456789")!=std::string::npos)
        throw std::runtime_error("Invalid journal integer");
    unsigned long long value=0;for(char c:text)value=value*10+static_cast<unsigned int>(c-'0');return value;
}

EccState State(const JsonValue &node) {
    const auto value=Number(node);
    if(value<1 || value>6)throw std::runtime_error("Journal state must be 1..6");
    return static_cast<EccState>(value);
}
}
}
