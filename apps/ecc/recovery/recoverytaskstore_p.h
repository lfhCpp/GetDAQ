#ifndef GETDAQ_COLD_RECOVERY_TASK_STORE_P_H
#define GETDAQ_COLD_RECOVERY_TASK_STORE_P_H

#include "coldhandoffprotocol.h"
#include "supervisorio.h"
#include <map>
#include <string>

namespace GetDAQ { namespace RecoveryPrivate {

struct Job {
    ColdHandoffOperation operation;
    std::string stateBytes;
};
using Jobs = std::map<std::string,Job>;

class RecoveryTaskStore {
public:
    static Jobs Load(const SupervisorIO::Directory &root, const std::string &policySha256);
    static void Check(const SupervisorIO::Directory &root, const Jobs &jobs);
    static void CreateRequest(const SupervisorIO::Directory &root, const ColdHandoffRequest &request);
    static void Save(const SupervisorIO::Directory &root, Job &job);
};

} }

#endif
