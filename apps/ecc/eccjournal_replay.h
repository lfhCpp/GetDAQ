#ifndef GETDAQ_ECCJOURNAL_REPLAY_H
#define GETDAQ_ECCJOURNAL_REPLAY_H

#include "eccjournal.h"
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace GetDAQ {
namespace EccJournalInternal {

struct ReplayState {
    std::size_t bytes = 0;
    std::size_t records = 0;
    unsigned long long sequence = 0;
    std::string pending;
    EccStatus status;
    std::map<std::string, EccOperation> operations;
    std::vector<std::string> order;
};

ReplayState Replay(int fd, const std::string &scope);

} // namespace EccJournalInternal
} // namespace GetDAQ

#endif
