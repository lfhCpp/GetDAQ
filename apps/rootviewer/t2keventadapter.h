#ifndef GETDAQ_T2KEVENTADAPTER_H
#define GETDAQ_T2KEVENTADAPTER_H

#include "roottreereader.h"

#include <memory>

class TBranch;
class TTree;

namespace GetDAQ {

// Decodes the TT2K/T2KEvent matrix format into the viewer's bounded event model.
class T2KEventAdapter {
public:
    T2KEventAdapter();
    ~T2KEventAdapter();

    static bool Supports(const TBranch *branch);
    bool Bind(TTree *tree);
    bool Read(RootEvent *event, std::string *error);

private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};

} // namespace GetDAQ

#endif
