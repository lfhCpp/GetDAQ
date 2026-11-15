#ifndef GETDAQ_EFFECTIVECONFIGURATIONRECORD_H
#define GETDAQ_EFFECTIVECONFIGURATIONRECORD_H
#include "strictjson.h"
namespace GetDAQ {
struct EffectiveConfigurationDigests { std::string effectiveSha256,snapshotSha256; };
// schema1有效快照的共享校验/摘要核心，无Qt/硬件或第三方库。
// checkDigests=false仅供生产者填入新摘要；结构/类型/范围/来源始终检查。
EffectiveConfigurationDigests CheckEffectiveConfigurationRecord(const JsonValue &record,bool checkDigests=true);
JsonValue DecodeEffectiveConfigurationRecord(const std::string &bytes);
}
#endif
