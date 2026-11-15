#include "configurationrepository_plan_p.h"

#include "../../common/experiment/configurationsha256.h"
#include "../../common/experiment/strictjson.h"

#include <stdexcept>

namespace GetDAQ {
namespace {

void RequirePlan(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

void RequireFields(const JsonValue &value,
                   std::initializer_list<const char *> names)
{
    if (value.type != JsonValue::Object || value.members.size() != names.size())
        throw std::invalid_argument("Unexpected repository plan fields");
    for (const char *name : names) value.At(name);
}

std::string PlanText(const JsonValue &value)
{
    if (value.type != JsonValue::String)
        throw std::invalid_argument("Repository plan requires string");
    return value.text;
}

void RequireDigest(const std::string &digest)
{
    if (digest.size() != 64 ||
        digest.find_first_not_of("0123456789abcdef") != std::string::npos)
        throw std::invalid_argument("Repository requires lowercase SHA-256");
}

std::vector<std::string> ObjectReferences(const JsonValue &value,
                                          std::size_t maximum)
{
    if (value.type != JsonValue::Array || value.items.empty() ||
        value.items.size() > maximum)
        throw std::invalid_argument("Invalid repository object list");
    std::vector<std::string> result;
    result.reserve(value.items.size());
    for (const auto &item : value.items) {
        const std::string digest = PlanText(item);
        RequireDigest(digest);
        result.push_back(digest);
    }
    return result;
}

} // namespace

PreparedConfigurationRepositoryCommit PrepareConfigurationRepositoryCommit(
    const std::string &planBytes,
    const std::function<std::string(const std::vector<std::string> &, std::size_t, std::size_t)> &assemble)
{
    const JsonValue plan = ParseStrictJson(planBytes);
    RequireFields(plan, {"schema_version", "kind", "publication_sha256",
                         "manifest_objects", "files"});
    if (plan.At("schema_version").type != JsonValue::Number ||
        plan.At("schema_version").text != "1" ||
        PlanText(plan.At("kind")) != "getdaq.configuration-upload-plan")
        throw std::invalid_argument("Unsupported repository plan");

    PreparedConfigurationRepositoryCommit prepared;
    prepared.publicationSha256 = PlanText(plan.At("publication_sha256"));
    RequireDigest(prepared.publicationSha256);
    prepared.planSha256 = ConfigurationSha256(planBytes);
    prepared.manifest = assemble(ObjectReferences(plan.At("manifest_objects"), 32),
                                 1024 * 1024, 0);
    RequirePlan(ConfigurationSha256(prepared.manifest) == prepared.publicationSha256,
                "Repository plan manifest SHA differs");
    prepared.publication = DecodeConfigurationPublication(prepared.manifest);

    const JsonValue &entries = plan.At("files");
    if (entries.type != JsonValue::Array ||
        entries.items.size() != prepared.publication.sources.size() + 3)
        throw std::invalid_argument("Repository plan file table differs");
    std::map<std::string, std::vector<std::string>> references;
    for (const auto &entry : entries.items) {
        RequireFields(entry, {"stored", "objects"});
        const std::string name = PlanText(entry.At("stored"));
        if (!references.emplace(name, ObjectReferences(entry.At("objects"), 2048)).second)
            throw std::invalid_argument("Duplicate repository plan file");
    }

    const auto content = [&](const PublicationFile &file) {
        const auto found = references.find(file.stored);
        if (found == references.end())
            throw std::invalid_argument("Missing repository plan role");
        std::string bytes = assemble(found->second, static_cast<std::size_t>(file.bytes),
                                     static_cast<std::size_t>(file.bytes));
        prepared.files.emplace(file.stored, bytes);
        return bytes;
    };
    prepared.artifact = VerifyConfigurationPublication(prepared.publication, content);
    return prepared;
}

} // namespace GetDAQ
