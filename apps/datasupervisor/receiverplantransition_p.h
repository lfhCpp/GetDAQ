#ifndef GETDAQ_RECEIVERPLANTRANSITION_P_H
#define GETDAQ_RECEIVERPLANTRANSITION_P_H

#include "receiverplantransition.h"
#include "supervisorio.h"
#include <cstddef>
#include <string>
#include <sys/stat.h>

namespace GetDAQ { namespace ReceiverPlanTransitionInternal {

void Require(bool value,const char *message);
void EnsureAbsent(const std::string &path);
void SealMetadata(const std::string &file);

class BoundFile {
    std::string path,bytes;
    SupervisorIO::Fd fd;
    struct stat identity;
public:
    BoundFile(const std::string &file,std::size_t maximum,unsigned int mode);
    void Check() const;
    const std::string &Bytes() const {return bytes;}
    int Descriptor() const {return fd.value;}
};

SupervisorPlanDocument ValidateNextPlan(const std::string &bytes,
                                        const DataSupervisorOptions &original,
                                        const std::string &executableSha256,
                                        bool requireAuditParent=true);

} }

#endif
