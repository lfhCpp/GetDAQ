#ifndef GETDAQ_NATIVEECCPLANCOMPILER_P_H
#define GETDAQ_NATIVEECCPLANCOMPILER_P_H

#include "nativeecccoordinator.h"

namespace GetDAQ {
struct CompiledNativeEccPlan {
    SystemPreparationOptions bound;
    std::vector<DataSenderOptions> senders;
    std::string dataBinding;
    NativeEccPlan plan;
};

CompiledNativeEccPlan CompileNativeEccPlan(const NativeEccOptions &options,
                                           const std::string &dataLifecycleBindingSha256);
}

#endif
