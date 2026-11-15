#ifndef GETDAQ_SOURCE_OBSERVATION_FIXTURE_H
#define GETDAQ_SOURCE_OBSERVATION_FIXTURE_H
#include "strictjson.h"
#include "systemexperimentconfiguration.h"
namespace SourceObservationFixture {
inline std::string Row(const std::string &id,const std::string &session,const std::string &state="running",const std::string &phase="available") {
    const bool observed=phase=="available";
    return "{\"id\":"+GetDAQ::EncodeJsonString(id)+",\"protocol\":\"ICE\",\"phase\":"+GetDAQ::EncodeJsonString(phase)+",\"message\":\"\",\"local_state\":4,\"source_instance_id\":\""+std::string(64,'c')+"\",\"receiver_session\":"+GetDAQ::EncodeJsonString(session)+
        ",\"observed_at_unix_ms\":\""+(observed ? "18446744073709551615" : "0")+"\",\"state\":"+GetDAQ::EncodeJsonString(observed?state:"")+",\"error\":\"\",\"issued_bytes\":\""+(observed ? "9007199254740993" : "0")+
        "\",\"locally_sent_bytes\":\""+(observed ? "9007199254740993" : "0")+"\",\"start_issued\":"+(observed ? "true" : "false")+",\"stop_acknowledged\":false,\"drain_acknowledged\":false,\"finish_issued\":false,\"source_end_acknowledged\":false,\"failure_stop_issued\":false,\"failure_stop_acknowledged\":false}";
}
inline std::string Root(const std::string &rows,const std::string &instance=std::string(64,'a'),const std::string &snapshot=std::string(64,'b')) {
    return "{\"schema\":1,\"kind\":\"getdaq.ecc-sources\",\"instance_id\":"+GetDAQ::EncodeJsonString(instance)+",\"snapshot_sha256\":"+GetDAQ::EncodeJsonString(snapshot)+",\"configuration_sha256\":\""+std::string(64,'1')+"\",\"artifact_sha256\":\""+std::string(64,'2')+"\",\"controller_operation_id\":\"\",\"busy\":false,\"sources\":["+rows+"]}";
}
}
#endif
