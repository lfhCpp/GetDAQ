#ifndef GETDAQ_TEST_EFFECTIVE_RECORD_FIXTURE_H
#define GETDAQ_TEST_EFFECTIVE_RECORD_FIXTURE_H
#include "effectiveconfigurationrecord.h"
#include "systemexperimentconfiguration.h"
namespace GetDAQTest {
// 空对象快照仅供低层字节/身份绑定夹具；真实XML发布另由进程联调验证。
inline std::string EmptyEffectiveSnapshot(const std::string &source,const std::string &sha,std::size_t bytes) {
    std::string record="{\"schema_version\":1,\"kind\":\"getdaq.effective-configuration-snapshot\",\"root_file\":"+GetDAQ::EncodeJsonString(source)+
        ",\"root_path\":\"Setup\",\"objects\":[\"Setup\"],\"scalars\":[],\"effective_sha256\":\"\",\"snapshot_sha256\":\"\",\"sources\":[{\"path\":"+
        GetDAQ::EncodeJsonString(source)+",\"sha256\":"+GetDAQ::EncodeJsonString(sha)+",\"bytes\":"+GetDAQ::EncodeJsonString(std::to_string(bytes))+"}]}";
    const auto result=GetDAQ::CheckEffectiveConfigurationRecord(GetDAQ::ParseStrictJson(record),false);
    for (const auto &field:std::vector<std::pair<std::string,std::string>>{{"effective_sha256",result.effectiveSha256},{"snapshot_sha256",result.snapshotSha256}}) {
        const auto marker=GetDAQ::EncodeJsonString(field.first)+":\"\""; const auto at=record.find(marker);
        record.replace(at,marker.size(),GetDAQ::EncodeJsonString(field.first)+':'+GetDAQ::EncodeJsonString(field.second));
    }
    return record;
}
}
#endif
