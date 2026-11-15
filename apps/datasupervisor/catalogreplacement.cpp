#include "catalogreplacement.h"
#include <stdexcept>

namespace GetDAQ {
namespace {
bool SafeSourceId(const std::string &value) {
    if (value.empty() || value.size()>128) return false;
    for (const unsigned char c:value) {
        if (c<32 || c==127) return false;
    }
    return true; // Source IDs are opaque identity text, never path components.
}
bool SafeRawName(const std::string &value) {
    if (value.size()<=5 || value.size()>255 || value.compare(value.size()-5,5,".graw")!=0) return false;
    for (const unsigned char c:value) {
        if (!((c>='A' && c<='Z') || (c>='a' && c<='z') ||
              (c>='0' && c<='9') || c=='_' || c=='-' || c=='.' || c==':')) return false;
    }
    return true;
}
void Validate(const std::map<std::string,std::set<std::string>> &files) {
    std::size_t count=0;
    for (const auto &source:files) {
        if (!SafeSourceId(source.first)) throw std::invalid_argument("Invalid catalog replacement source ID");
        for (const auto &name:source.second) {
            if (!SafeRawName(name)) throw std::invalid_argument("Invalid catalog replacement raw file name");
            if (++count>65536) throw std::invalid_argument("Catalog replacement file count exceeds 65536");
        }
    }
}
}

std::vector<CatalogReplacementEntry> BuildCatalogReplacementPlan(
    const std::map<std::string,std::set<std::string>> &oldFiles,
    const std::map<std::string,std::set<std::string>> &newFiles) {
    Validate(oldFiles);
    Validate(newFiles);
    std::set<std::string> sources;
    for (const auto &source:oldFiles) sources.insert(source.first);
    for (const auto &source:newFiles) sources.insert(source.first);

    std::vector<CatalogReplacementEntry> result;
    for (const auto &source:sources) {
        const auto oldSource=oldFiles.find(source);
        const auto newSource=newFiles.find(source);
        const std::set<std::string> empty;
        const auto &oldNames=oldSource==oldFiles.end() ? empty : oldSource->second;
        const auto &newNames=newSource==newFiles.end() ? empty : newSource->second;
        std::set<std::string> names=oldNames;
        names.insert(newNames.begin(),newNames.end());
        for (const auto &name:names) {
            CatalogReplacementEntry entry;
            entry.sourceId=source;
            entry.fileName=name;
            entry.fromNewRun=newNames.count(name)!=0;
            result.push_back(entry);
        }
    }
    if (result.size()>65536) throw std::invalid_argument("Merged catalog file count exceeds 65536");
    return result;
}
}
