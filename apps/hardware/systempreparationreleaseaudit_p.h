#ifndef GETDAQ_SYSTEMPREPARATIONRELEASEAUDIT_P_H
#define GETDAQ_SYSTEMPREPARATIONRELEASEAUDIT_P_H

#include "hardwareaudit.h"
#include "systempreparation.h"

namespace GetDAQ {

// Bounded, chained audit writer shared by the preparation release paths.
class SystemPreparationReleaseAudit {
public:
    SystemPreparationReleaseAudit(const std::string &path,const char *action,
                                  const SystemPreparationOptions &options);
    void Record(const std::string &row);
    std::string Begin(const std::string &id,const char *stage);
    void End(const std::string &id,const char *stage,bool complete=true);
    std::string ChainSha256() const { return fAudit.ChainSha256(); }

private:
    HardwareAudit fAudit;
    std::string fDirectory;
    unsigned int fSequence=0;
};

}

#endif
