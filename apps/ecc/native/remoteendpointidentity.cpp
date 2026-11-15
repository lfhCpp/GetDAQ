#include "remoteendpointidentity_p.h"
#include "configurationsha256.h"
#include "supervisorcommon.h"
#include "systemexperimentconfiguration.h"
#include "supervisorio.h"
#include <algorithm>
#include <arpa/inet.h>
#include <set>
#include <stdexcept>

namespace GetDAQ {
namespace RemoteReceiverManagerInternal {
using namespace SupervisorIO;
RemoteReceiverOptions CheckedOptions(RemoteReceiverOptions options) {
    Text(options.controllerId,128);
    if (options.endpoints.empty() || options.endpoints.size()>195 || options.commandTimeoutMs<100 || options.commandTimeoutMs>600000 ||
        options.pollIntervalMs<10 || options.pollIntervalMs>1000) throw std::invalid_argument("Invalid remote receiver endpoint/time policy");
    std::sort(options.endpoints.begin(),options.endpoints.end(),[](const RemoteReceiverEndpoint &a,const RemoteReceiverEndpoint &b) { return a.agentId<b.agentId; });
    std::set<std::string> ids,addresses;
    for (const auto &endpoint:options.endpoints) {
        Text(endpoint.agentId,128); Digest(endpoint.planSha256);
        in_addr address={}; char canonical[INET_ADDRSTRLEN];
        if (::inet_pton(AF_INET,endpoint.control.address.c_str(),&address)!=1 || !::inet_ntop(AF_INET,&address,canonical,sizeof(canonical)) ||
            canonical!=endpoint.control.address || endpoint.control.address=="0.0.0.0" || !endpoint.control.port || endpoint.control.port>65535 ||
            endpoint.control.timeoutMs<100 || endpoint.control.timeoutMs>60000)
            throw std::invalid_argument("Remote receiver declaration requires canonical IPv4/port/deadline");
        Path(endpoint.control.credentialFile); // Validate its declaration without opening the secret.
        if (endpoint.control.timeoutMs>options.commandTimeoutMs) throw std::invalid_argument("Remote HTTP timeout exceeds whole command budget");
        if (!ids.insert(endpoint.agentId).second || !addresses.insert(endpoint.control.address+':'+std::to_string(endpoint.control.port)).second)
            throw std::invalid_argument("Duplicate remote receiver agent/control endpoint");
    }
    return options;
}
std::string Binding(const RemoteReceiverOptions &options) {
    std::string bytes="getdaq.remote-receivers:1\n{\"controller_id\":"+EncodeJsonString(options.controllerId)+
        ",\"command_timeout_ms\":"+std::to_string(options.commandTimeoutMs)+",\"poll_interval_ms\":"+std::to_string(options.pollIntervalMs)+",\"peers\":[";
    for (const auto &peer:options.endpoints) {
        if (bytes.back()!='[') bytes+=',';
        bytes+="{\"agent_id\":"+EncodeJsonString(peer.agentId)+",\"plan_sha256\":"+EncodeJsonString(peer.planSha256)+
            ",\"address\":"+EncodeJsonString(peer.control.address)+",\"port\":"+std::to_string(peer.control.port)+",\"http_timeout_ms\":"+std::to_string(peer.control.timeoutMs)+"}";
    }
    return ConfigurationSha256(bytes+"]}\n"); // 不包含凭据值或短寿命实例/持有期。
}
}
std::string RemoteReceiverBindingSha256(const RemoteReceiverOptions &options) {
    return RemoteReceiverManagerInternal::Binding(RemoteReceiverManagerInternal::CheckedOptions(options));
}
} // namespace GetDAQ
