#ifndef GETDAQ_CATALOGREPLACEMENT_H
#define GETDAQ_CATALOGREPLACEMENT_H

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace GetDAQ {
struct CatalogReplacementEntry {
    std::string sourceId;
    std::string fileName;
    bool fromNewRun=false;
    // True only when the selected file belongs to the input catalog's sealed
    // receiver session; retained schema-2 files remain logical data but are
    // never mistaken for newly acquired files during chained replacement.
    bool currentSessionFile=false;
    // Populated by PlanReceiverCatalogReplacement() from a verified catalog.
    // The pure name-set planner leaves these fields empty/zero.
    std::string sourceCatalogDirectory,sourceStoredDirectory,sourceCatalogReceiptSha256;
    std::uint64_t bytes=0,device=0,inode=0;
};

// Build the logical file set for a replacement catalog. File-name collisions
// are scoped to one source; a new file wins an exact-name collision and every
// other old file remains represented. This inspects names only, never payloads.
std::vector<CatalogReplacementEntry> BuildCatalogReplacementPlan(
    const std::map<std::string,std::set<std::string>> &oldFiles,
    const std::map<std::string,std::set<std::string>> &newFiles);
}

#endif
