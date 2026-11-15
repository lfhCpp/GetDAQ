#include "catalogreplacement.h"
#include <iostream>
#include <stdexcept>
#include <map>
#include <set>

namespace {
void Require(bool value,const char *message) { if (!value) throw std::runtime_error(message); }
void Reject(const std::map<std::string,std::set<std::string>> &oldFiles,
            const std::map<std::string,std::set<std::string>> &newFiles) {
    bool rejected=false;
    try { GetDAQ::BuildCatalogReplacementPlan(oldFiles,newFiles); }
    catch (const std::invalid_argument &) { rejected=true; }
    Require(rejected,"Invalid replacement file-set metadata was accepted");
}
}

int main() {
    try {
        const std::map<std::string,std::set<std::string>> oldFiles={
            {"cobo-0",{"same.graw","old-only.graw"}},
            {"cobo-1",{"same.graw"}},
            {"retired-source",{"retired.graw"}}
        };
        const std::map<std::string,std::set<std::string>> newFiles={
            {"cobo-0",{"same.graw","new-only.graw"}},
            {"cobo-1",{}},
            {"new-source",{"same.graw"}}
        };
        const auto plan=GetDAQ::BuildCatalogReplacementPlan(oldFiles,newFiles);
        Require(plan.size()==6,"Unexpected merged logical file count");
        std::map<std::string,std::map<std::string,bool>> selected;
        for (const auto &entry:plan)
            Require(selected[entry.sourceId].insert(std::make_pair(entry.fileName,entry.fromNewRun)).second,
                    "Merged logical catalog contains a duplicate source/name entry");
        Require(selected["cobo-0"]["same.graw"],"New raw did not win exact same-source filename collision");
        Require(selected["cobo-0"]["new-only.graw"],"New-only raw was lost");
        Require(!selected["cobo-0"]["old-only.graw"],"Noncolliding old raw was not retained");
        Require(!selected["cobo-1"]["same.graw"],"Old raw from a source with no new file was not retained");
        Require(!selected["retired-source"]["retired.graw"],"Old raw from a retired source was not retained");
        Require(selected["new-source"]["same.graw"],"Filename collision leaked across source namespaces");

        Reject({{"",{"x.graw"}}},{});
        Reject({{"cobo-0",{"../escape.graw"}}},{});
        Reject({{}},{});
        std::cout<<"PASS exact-name replacement precedence/source isolation/old-file retention/metadata rejection\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr<<"FAIL "<<e.what()<<'\n';
        return 1;
    }
}
