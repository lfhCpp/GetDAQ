#ifndef GETDAQ_SCANRUNTIMEIDENTITYINTERNAL_P_H
#define GETDAQ_SCANRUNTIMEIDENTITYINTERNAL_P_H

#include "scanruntimeidentity.h"
#include "strictjson.h"
#include "systemexperimentconfiguration.h"
#include <set>
#include <stdexcept>

namespace GetDAQ {
namespace ScanRuntimeIdentityInternal {

inline void Require(bool condition, const char *message)
{
    if (!condition) throw std::invalid_argument(message);
}

inline bool IsDigest(const std::string &value)
{
    return value.size() == 64 && value.find_first_not_of("0123456789abcdef") == std::string::npos;
}

inline void RequireDigest(const std::string &value, const char *message)
{
    Require(IsDigest(value), message);
}

inline std::string Quote(const std::string &value)
{
    return EncodeJsonString(value);
}

inline std::string Text(const JsonValue &value)
{
    Require(value.type == JsonValue::String,
            "Scan runtime identity contract field must be a string");
    return value.text;
}

inline unsigned int Number(const JsonValue &value, unsigned int maximum)
{
    Require(value.type == JsonValue::Number && !value.text.empty(),
            "Scan runtime identity point index must be an integer");
    unsigned int result = 0;
    for (char c : value.text) {
        Require(c >= '0' && c <= '9',
                "Scan runtime identity point index is not canonical decimal");
        const unsigned int digit = static_cast<unsigned int>(c - '0');
        Require(digit <= maximum && result <= (maximum - digit) / 10U,
                "Scan runtime identity point index is out of range");
        result = result * 10U + digit;
    }
    return result;
}

inline void ExactKeys(const JsonValue &value, const std::set<std::string> &expected)
{
    Require(value.type == JsonValue::Object,
            "Scan runtime identity contract must be an object");
    std::set<std::string> actual;
    for (const auto &member : value.members) actual.insert(member.first);
    Require(actual == expected,
            "Scan runtime identity contract has missing or unsupported fields");
}

inline void ValidateExpectationFields(const ScanRuntimeIdentityExpectation &expected)
{
    RequireDigest(expected.executionSnapshotSha256,
                  "Expected scan execution snapshot SHA-256 is invalid");
    Require(expected.pointIndex <= 255,
            "Expected scan point index is out of range");
    Require(!expected.serviceId.empty() && expected.serviceId.size() <= 128,
            "Expected ECC service ID is invalid");
    Require(!expected.clientId.empty() && expected.clientId.size() <= 128,
            "Expected ECC client ID is invalid");
    RequireDigest(expected.instanceId, "Expected ECC instance ID is invalid");
    RequireDigest(expected.snapshotSha256, "Expected ECC snapshot SHA-256 is invalid");
    RequireDigest(expected.inputBindingSha256, "Expected ECC input binding SHA-256 is invalid");
    RequireDigest(expected.configurationSha256, "Expected configuration SHA-256 is invalid");
    RequireDigest(expected.artifactSha256, "Expected artifact SHA-256 is invalid");
    RequireDigest(expected.dataBindingSha256, "Expected data binding SHA-256 is invalid");
    RequireDigest(expected.dataScopeSha256, "Expected data controller scope SHA-256 is invalid");
    RequireDigest(expected.dataEpoch, "Expected data controller epoch is invalid");
    Require(!expected.dataNodes.empty() && expected.dataNodes.size() <= 195,
            "Expected data node set is empty or exceeds its limit");
    std::set<std::string> agents;
    for (const ScanRuntimeDataNodeExpectation &node : expected.dataNodes) {
        Require(!node.agentId.empty() && node.agentId.size() <= 128 &&
                agents.insert(node.agentId).second,
                "Expected data node IDs are invalid or duplicated");
        RequireDigest(node.instanceId, "Expected data node instance ID is invalid");
        RequireDigest(node.planSha256, "Expected receiver plan SHA-256 is invalid");
    }
}

ScanRuntimeIdentityObservation StableIdentityObservation(
    const std::string &identityBytes,
    const std::string &statusBytes,
    const ScanRuntimeIdentityExpectation &expected);

}
}

#endif
