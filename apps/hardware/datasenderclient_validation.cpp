#include "datasenderclient.h"
#include "datasenderclient_validation_p.h"
#include "../../common/text/utf8.h"
#include "strictjson.h"
#include <arpa/inet.h>
#include <set>
#include <stdexcept>

namespace GetDAQ {
namespace DataSenderValidation {
void Text(const std::string &s,std::size_t maximum) {
    if (s.empty() || s.size() > maximum) throw std::invalid_argument("Invalid bounded data sender text");
    ValidateJsonUtf8(s);
    for (unsigned char c : s) if (c < 32 || c == 127) throw std::invalid_argument("Control byte in data sender text");
}

std::string Receiver(const std::string &s) {
    if (s.empty() || s.size() > 64) throw std::invalid_argument("Invalid bounded data receiver address");
    Text(s,64); const auto colon = s.find(':');
    if (colon == std::string::npos || s.find(':',colon+1) != std::string::npos)
        throw std::invalid_argument("Data receiver requires explicit IPv4:port");
    const auto ip = s.substr(0,colon), port = s.substr(colon+1);
    in_addr address;
    if (::inet_pton(AF_INET,ip.c_str(),&address) != 1 || port.empty() || port.size() > 5 ||
        port[0] == '0' || port.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("Invalid canonical data receiver IPv4:port");
    const auto host = ntohl(address.s_addr);
    if (host == 0 || host == 0xffffffffU || (host >> 28) == 14 || (host >> 24) == 0 || (host >> 24) >= 240)
        throw std::invalid_argument("Data receiver must be a unicast IPv4 address");
    if (std::stoul(port) > 65535) throw std::invalid_argument("Data receiver port exceeds 65535");
    return ip+':'+port;
}

void Scalars(const DataSenderOptions &o) {
    Text(o.senderId,128); Text(o.controlProxy,4096); Receiver(o.receiverAddress);
    if (o.protocol!="TCP" && o.protocol!="ZBUF" && o.protocol!="FDT" && o.protocol!="ICE") throw std::invalid_argument("Managed data sender protocol must be TCP, ZBUF, FDT or upgraded ICE");
    if (o.circularBufferMask == 0 || o.circularBufferMask > 15 || o.encodingMinor < 0 || o.encodingMinor > 1 ||
        o.timeoutMs < 100 || o.timeoutMs > 600000)
        throw std::invalid_argument("Invalid data sender mask/encoding/timeout");
    if (!o.exclusiveStoppedOwnerConfirmed) throw std::invalid_argument("Data sender requires exclusive stopped owner declaration");
}
}

void ValidateDataSenderOptions(const DataSenderOptions &o) {
    DataSenderValidation::Scalars(o);
    CanonicalDataSenderNodeKeys(o.controlProxy,o.timeoutMs);
}

void ValidateDataSenderSet(const std::vector<DataSenderOptions> &options) {
    if (options.empty() || options.size() > 195) throw std::invalid_argument("Data sender set requires 1..195 senders");
    std::set<std::string> ids, nodes, receivers;
    for (const auto &o : options) {
        DataSenderValidation::Scalars(o);
        if (!ids.insert(o.senderId).second || !receivers.insert(DataSenderValidation::Receiver(o.receiverAddress)).second)
            throw std::invalid_argument("Duplicate data sender ID, canonical control node or receiver");
        for (const auto &key : CanonicalDataSenderNodeKeys(o.controlProxy,30000))
            if (!nodes.insert(key).second) throw std::invalid_argument("Duplicate canonical data control node/overlapping endpoints");
    }
}
}
