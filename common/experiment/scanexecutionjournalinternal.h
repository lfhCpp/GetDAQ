#ifndef GETDAQ_SCANEXECUTIONJOURNALINTERNAL_H
#define GETDAQ_SCANEXECUTIONJOURNALINTERNAL_H

#include "scanexecutionjournal.h"
#include "systemexperimentconfiguration.h"
#include "strictjson.h"
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

namespace GetDAQ {
namespace ScanExecutionJournalInternal {


inline void Require(bool condition, const char *message)
{
    if (!condition) throw std::invalid_argument(message);
}

inline bool IsSha256(const std::string &value)
{
    return value.size() == 64 &&
           value.find_first_not_of("0123456789abcdef") == std::string::npos;
}

inline bool IsSafeId(const std::string &value)
{
    if (value.empty() || value.size() > 64) return false;
    for (unsigned char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'))
            return false;
    return value != "." && value != "..";
}

inline bool IsSafeOperation(const std::string &value)
{
    if (value.empty() || value.size() > 64) return false;
    for (unsigned char c : value)
        if (!((c >= 'a' && c <= 'z') || c == '-')) return false;
    return true;
}

inline std::string ShutdownRequestBytes(const std::string &instanceId,
                                const std::string &snapshotSha256,
                                const std::string &handoffOperationId)
{
    return "{\"schema\":1,\"kind\":\"getdaq.ecc-service-shutdown\",\"instance_id\":" +
        EncodeJsonString(instanceId) + ",\"snapshot_sha256\":" +
        EncodeJsonString(snapshotSha256) + ",\"handoff_operation_id\":" +
        EncodeJsonString(handoffOperationId) + "}\n";
}

inline std::string ShutdownAcceptedBytes(const std::string &instanceId,
                                  const std::string &snapshotSha256,
                                  const std::string &handoffOperationId)
{
    return "{\"schema\":1,\"kind\":\"getdaq.ecc-service-shutdown-accepted\",\"instance_id\":" +
        EncodeJsonString(instanceId) + ",\"snapshot_sha256\":" +
        EncodeJsonString(snapshotSha256) + ",\"handoff_operation_id\":" +
        EncodeJsonString(handoffOperationId) + "}\n";
}

inline void ExactKeys(const JsonValue &value, const std::set<std::string> &keys)
{
    Require(value.type == JsonValue::Object, "Expected scan journal object");
    std::set<std::string> actual;
    for (const auto &member : value.members) actual.insert(member.first);
    Require(actual == keys, "Scan journal has missing or unsupported fields");
}

inline std::string Text(const JsonValue &value)
{
    Require(value.type == JsonValue::String, "Expected scan journal string");
    return value.text;
}

inline bool Boolean(const JsonValue &value)
{
    Require(value.type == JsonValue::Boolean, "Expected scan journal boolean");
    return value.text == "true";
}

inline unsigned int Number(const JsonValue &value, unsigned int maximum)
{
    Require(value.type == JsonValue::Number && !value.text.empty(),
            "Expected scan journal integer");
    unsigned int result = 0;
    for (char c : value.text) {
        Require(c >= '0' && c <= '9', "Invalid scan journal integer");
        const unsigned int digit = unsigned(c - '0');
        Require(digit <= maximum && result <= maximum / 10U &&
                (result < maximum / 10U || digit <= maximum % 10U),
                "Scan journal integer is out of range");
        result = result * 10U + digit;
    }
    return result;
}

inline std::uint64_t Number64(const JsonValue &value)
{
    Require(value.type == JsonValue::String && !value.text.empty(),
            "Expected scan baseline uint64 decimal string");
    Require(value.text.size() == 1 || value.text.front() != '0',
            "Scan baseline uint64 decimal string is not canonical");
    std::uint64_t result = 0;
    for (char c : value.text) {
        Require(c >= '0' && c <= '9', "Invalid scan baseline uint64 decimal string");
        const unsigned int digit = unsigned(c - '0');
        Require(result <= (std::numeric_limits<std::uint64_t>::max() - digit) / 10U,
                "Scan baseline uint64 value is out of range");
        result = result * 10U + digit;
    }
    return result;
}

inline bool IsBoundedText(const std::string &value, std::size_t maximum)
{
    return !value.empty() && value.size() <= maximum &&
           value.find_first_of("\r\n\t") == std::string::npos;
}

inline void ValidateEventCountBaseline(const ScanEventCountBaseline &baseline)
{
    Require(IsSha256(baseline.instanceId) && IsSha256(baseline.snapshotSha256) &&
            IsSha256(baseline.dataBindingSha256) && IsBoundedText(baseline.agentId, 128) &&
            IsSha256(baseline.planSha256) && IsSha256(baseline.receiverInstanceId) &&
            IsBoundedText(baseline.sourceId, 128) && IsSha256(baseline.sessionId) &&
            baseline.pid > 0 && baseline.available && baseline.observedAtUnixMs > 0,
            "Invalid or unavailable scan receiver event baseline");
}

inline bool SameEventCountBaseline(const ScanEventCountBaseline &left,
                            const ScanEventCountBaseline &right)
{
    return left.instanceId == right.instanceId &&
           left.snapshotSha256 == right.snapshotSha256 &&
           left.dataBindingSha256 == right.dataBindingSha256 &&
           left.agentId == right.agentId && left.planSha256 == right.planSha256 &&
           left.receiverInstanceId == right.receiverInstanceId &&
           left.sourceId == right.sourceId && left.sessionId == right.sessionId &&
           left.pid == right.pid && left.available == right.available &&
           left.acquisitionEvents == right.acquisitionEvents &&
           left.observedAtUnixMs == right.observedAtUnixMs;
}

inline ScanPointState ParseState(const std::string &value)
{
    const ScanPointState states[] = {
        ScanPointState::Planned, ScanPointState::Reviewed,
        ScanPointState::EccStarted, ScanPointState::Described,
        ScanPointState::Prepared, ScanPointState::Configured,
        ScanPointState::RunStarted, ScanPointState::RunStopped,
        ScanPointState::ReceiverFinished, ScanPointState::CatalogVerified,
        ScanPointState::Bound, ScanPointState::Retired,
        ScanPointState::NeedsReconciliation
    };
    for (ScanPointState state : states)
        if (value == ScanPointStateName(state)) return state;
    throw std::invalid_argument("Unknown scan journal state");
}

inline ScanPointState OperationTarget(ScanPointState current, const std::string &operation)
{
    const struct Mapping { ScanPointState from, to; const char *operation; } mappings[] = {
        {ScanPointState::Reviewed, ScanPointState::EccStarted, "start-ecc"},
        {ScanPointState::Reviewed, ScanPointState::EccStarted, "adopt-ecc"},
        {ScanPointState::EccStarted, ScanPointState::Described, "describe"},
        {ScanPointState::Described, ScanPointState::Prepared, "prepare"},
        {ScanPointState::Prepared, ScanPointState::Configured, "configure"},
        {ScanPointState::Configured, ScanPointState::RunStarted, "run-start"},
        {ScanPointState::RunStarted, ScanPointState::RunStopped, "run-stop"},
        {ScanPointState::RunStopped, ScanPointState::ReceiverFinished, "receiver-finish"},
        {ScanPointState::ReceiverFinished, ScanPointState::CatalogVerified, "catalog-verify"},
        {ScanPointState::CatalogVerified, ScanPointState::Bound, "bind"},
        {ScanPointState::Bound, ScanPointState::Retired, "retire"}
    };
    for (const Mapping &mapping : mappings)
        if (mapping.from == current && operation == mapping.operation) return mapping.to;
    throw std::invalid_argument("Scan operation is not valid for the current lifecycle state");
}

inline std::string Escape(const std::string &value)
{
    std::string result;
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { result += '\\'; result += char(c); }
        else if (c < 0x20) throw std::invalid_argument("Control byte in scan journal ID");
        else result += char(c);
    }
    return result;
}


} // namespace ScanExecutionJournalInternal
} // namespace GetDAQ

#endif
