#ifndef GETDAQ_ROOT5TREEADAPTER_H
#define GETDAQ_ROOT5TREEADAPTER_H

#include "roottreereader.h"

#include <memory>

class TBranch;
class TTree;

namespace GetDAQ {

// Reads the lowercase get:: object names written by the ROOT 5-era viewer.
// The legacy classes live in a separate translation unit so they can coexist
// with the current uppercase GET:: classes in one ROOT 6 process.
class Root5TreeAdapter {
public:
    Root5TreeAdapter();
    ~Root5TreeAdapter();

    static bool Supports(const TBranch *branch);
    bool Bind(TTree *tree);
    bool Read(RootEvent *event, std::string *error);

private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};

}

#endif
