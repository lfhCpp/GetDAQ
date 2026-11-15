#ifndef NATIVERECOVERYSNAPSHOTPOLICY_H
#define NATIVERECOVERYSNAPSHOTPOLICY_H

#include "runconfig.h"

namespace GetDAQ {
bool MatchesNativeRecoverySnapshot(const RunConfig &saved, const RunConfig &current);
}

#endif
