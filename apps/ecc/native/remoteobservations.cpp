#include "remoteobservations_p.h"

#include "remotereceivermanager.h"
#include "controlhttp.h"
#include "controlprotocol.h"
#include "eccprotocol.h"
#include "planio.h"
#include "supervisorcommon.h"
#include "configurationsha256.h"
#include "../../../common/text/utf8.h"

#include <algorithm>
#include <chrono>
#include <set>
#include <stdexcept>

namespace GetDAQ {
namespace {
using Clock=std::chrono::steady_clock;
using namespace SupervisorIO;

JsonValue Response(const SupervisorHttpResponse &response,unsigned int expected) {
    if (response.code!=expected) {
        const auto detail=response.body.substr(0,512);
        throw std::runtime_error("Remote supervisor HTTP "+std::to_string(response.code)+
            (detail.empty() ? std::string() : "; response="+detail)+"; request is not retried");
    }
    return ParseStrictJson(response.body);
}
}

std::string RemoteObservationError(const char *error) {
    auto message=EccErrorText(error);
    for (char &byte:message) if (static_cast<unsigned char>(byte)<32 || byte==127) byte=' ';
    if (message.size()>512) {
        message.resize(496); while (!message.empty() && !ValidUtf8(message)) message.pop_back();
        message+=" [truncated]";
    }
    return message;
}

std::string ObserveRemoteAcquisitionEvents(const RemoteReceiverEndpoint &endpoint,
    const std::string &controllerId,const std::string &instanceId,const std::string &planSha256,
    const std::string &senderId,unsigned int sourceIndex,const std::string &sessionId,int pid,bool fileBytes) {
    auto options=endpoint.control; options.timeoutMs=std::min(options.timeoutMs,1500U);
    const auto value=Response(SupervisorHttpClient(options).ReceiverEvents(sourceIndex,fileBytes),200);
    ExactSupervisorFields(value,{"schema","service","agent_id","controller_id","instance_id","plan_sha256","events"});
    if (Unsigned(value.At("schema"))!=1 || String(value.At("service"))!="getdaq-supervisor" ||
        String(value.At("agent_id"))!=endpoint.agentId || String(value.At("controller_id"))!=controllerId ||
        String(value.At("instance_id"))!=instanceId || String(value.At("plan_sha256"))!=planSha256)
        throw std::runtime_error("Receiver event service/plan identity differs from the bound ECC deployment");
    const auto &events=value.At("events");
    if(fileBytes) ExactSupervisorFields(events,{"source_id","session_id","pid","available","accepted_file_bytes","observed_at_unix_ms"});
    else ExactSupervisorFields(events,{"source_id","session_id","pid","available","acquisition_events","observed_at_unix_ms"});
    if (String(events.At("source_id"))!=senderId || String(events.At("session_id"))!=sessionId ||
        Unsigned(events.At("pid"))!=std::uint64_t(pid) || !Unsigned(events.At("observed_at_unix_ms"),true))
        throw std::runtime_error("Receiver event count is not bound to the original source/session/PID");
    const bool available=Boolean(events.At("available")); const auto count=Unsigned(events.At(fileBytes ? "accepted_file_bytes" : "acquisition_events"));
    if (!available && count) throw std::runtime_error("Unavailable receiver event counter carries a value");
    return "{\"agent_id\":"+EncodeJsonString(endpoint.agentId)+",\"plan_sha256\":"+EncodeJsonString(planSha256)+
        ",\"instance_id\":"+EncodeJsonString(instanceId)+",\"source_id\":"+EncodeJsonString(senderId)+
        ",\"session_id\":"+EncodeJsonString(sessionId)+",\"pid\":"+std::to_string(pid)+
        ",\"available\":"+(available ? "true" : "false")+",\""+(fileBytes ? "accepted_file_bytes" : "acquisition_events")+"\":"+EncodeJsonString(std::to_string(count))+
        ",\"observed_at_unix_ms\":"+EncodeJsonString(String(events.At("observed_at_unix_ms")))+"}";
}

std::string ObserveRemoteReceiverEndpoints(const RemoteReceiverOptions &options,
                                          const RemoteReceiverStatus &cachedStatus) {
    const auto deadline=Clock::now()+std::chrono::milliseconds(1500);
    std::string json="["; std::size_t receiverCount=0;
    for (std::size_t i=0;i<options.endpoints.size();++i) {
        const auto &endpoint=options.endpoints[i]; std::string instance,observation,message;
        const auto clientOptions=[&] {
            const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-Clock::now()).count();
            if (remaining<100) throw std::runtime_error("Receiver observation time budget exhausted; node not confirmed");
            auto client=endpoint.control; client.timeoutMs=unsigned(std::min<long long>(client.timeoutMs,remaining)); return client;
        };
        try {
            SupervisorHttpClient identityClient(clientOptions()); const auto identity=Response(identityClient.Identity(),200);
            ExactSupervisorFields(identity,{"schema","service","agent_id","controller_id","instance_id","plan_sha256","plan"});
            if (Unsigned(identity.At("schema"))!=1 || String(identity.At("service"))!="getdaq-supervisor" ||
                String(identity.At("agent_id"))!=endpoint.agentId || String(identity.At("controller_id"))!=options.controllerId ||
                String(identity.At("plan_sha256"))!=endpoint.planSha256) throw std::runtime_error("Receiver observation node identity/plan differs");
            instance=String(identity.At("instance_id")); Digest(instance);
            const auto plan=DecodeSupervisorPlan(identity.At("plan"));
            if (ConfigurationSha256(EncodeOptions(plan.options,plan.executableSha256))!=endpoint.planSha256)
                throw std::runtime_error("Receiver observation canonical plan differs");
            if (!cachedStatus.nodes[i].instanceId.empty() && cachedStatus.nodes[i].instanceId!=instance)
                throw std::runtime_error("Receiver observation instance differs from bound lifecycle; no takeover");
            SupervisorHttpClient observationClient(clientOptions()); const auto value=Response(observationClient.Receivers(),200);
            ExactSupervisorFields(value,{"schema","service","agent_id","controller_id","instance_id","plan_sha256","observation"});
            if (Unsigned(value.At("schema"))!=1 || String(value.At("service"))!="getdaq-supervisor" || String(value.At("agent_id"))!=endpoint.agentId ||
                String(value.At("controller_id"))!=options.controllerId || String(value.At("instance_id"))!=instance || String(value.At("plan_sha256"))!=endpoint.planSha256)
                throw std::runtime_error("Receiver observation response identity/instance differs");
            const auto decoded=DecodeSupervisorObservation(value.At("observation"));
            if (decoded.lifecycle.planSha256!=endpoint.planSha256 || receiverCount+decoded.lifecycle.receivers.size()>195)
                throw std::runtime_error("Receiver observation lifecycle plan/count differs");
            if (decoded.lifecycle.receivers.size()>plan.options.sources.size()) throw std::runtime_error("Receiver observation source count exceeds fixed plan");
            for (std::size_t source=0;source<decoded.lifecycle.receivers.size();++source)
                if (decoded.lifecycle.receivers[source].id!=plan.options.sources[source].id)
                    throw std::runtime_error("Receiver observation source differs from fixed plan");
            observation=EncodeSupervisorObservation(decoded);
            if (json.size()+observation.size()>768*1024) throw std::runtime_error("Receiver observation response byte budget exhausted");
            receiverCount+=decoded.lifecycle.receivers.size();
        } catch (const std::exception &e) { message=RemoteObservationError(e.what()); instance.clear(); observation.clear(); }
        if (json.back()!='[') json+=',';
        json+="{\"agent_id\":"+EncodeJsonString(endpoint.agentId)+",\"plan_sha256\":"+EncodeJsonString(endpoint.planSha256)+
            ",\"instance_id\":"+EncodeJsonString(instance)+",\"available\":"+(observation.empty() ? "false" : "true")+
            ",\"message\":"+EncodeJsonString(message)+",\"observation\":"+(observation.empty() ? "null" : observation)+"}";
    }
    return json+"]";
}

}
