#include "remotereceivermanager_p.h"
#include <stdexcept>

namespace GetDAQ {
namespace RemoteReceiverManagerInternal {

std::string EmbeddedJsonObject(const std::string &encoded) {
    if (encoded.empty() || encoded.back()!='\n') throw std::runtime_error("Encoded JSON object lacks its record terminator");
    const auto object=encoded.substr(0,encoded.size()-1);
    if (ParseStrictJson(object).type!=JsonValue::Object) throw std::runtime_error("Encoded JSON record is not an object");
    return object;
}

JsonValue Response(const SupervisorHttpResponse &response,unsigned int expected) {
    if (response.code!=expected) {
        const auto detail=response.body.substr(0,512);
        throw std::runtime_error("Remote supervisor HTTP "+std::to_string(response.code)+
            (detail.empty() ? std::string() : "; response="+detail)+"; request is not retried");
    }
    return ParseStrictJson(response.body);
}

void BoundFile(const std::string &path,const struct stat &identity,const std::string &bytes,std::size_t limit) {
    struct stat current;
    if (::lstat(path.c_str(),&current) || !Same(identity,current) || !S_ISREG(current.st_mode) || current.st_uid!=::geteuid() || (current.st_mode&0777)!=0600 || ReadFile(path,limit)!=bytes)
        throw std::runtime_error("Remote receiver controller/audit file changed");
}

} // namespace RemoteReceiverManagerInternal
} // namespace GetDAQ
